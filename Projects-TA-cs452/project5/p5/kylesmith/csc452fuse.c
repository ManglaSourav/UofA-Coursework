/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452

	Co-Author: Kyle Smith
	Class: CSC 452 - Project 5

*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

// size of a disk block
#define BLOCK_SIZE 512

// we'll use 8.3 filenames
#define MAX_FILENAME 8
#define MAX_EXTENSION 3

// How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

// The attribute packed means to not align these things
struct csc452_directory_entry
{
	int nFiles; // How many files are in this directory.
				// Needs to be less than MAX_FILES_IN_DIR

	struct csc452_file_directory
	{
		char fname[MAX_FILENAME + 1];				   // filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];				   // extension (plus space for nul)
		size_t fsize;								   // file size
		long nStartBlock;							   // where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR]; // There is an array of these

	// This is some space to get this to be exactly the size of the disk block.
	// Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct csc452_file_directory) - sizeof(int)];
};

typedef struct csc452_root_directory csc452_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct csc452_root_directory
{
	int nDirectories; // How many subdirectories are in the root
					  // Needs to be less than MAX_DIRS_IN_ROOT
	struct csc452_directory
	{
		char dname[MAX_FILENAME + 1];						 // directory name (plus space for nul)
		long nStartBlock;									 // where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT]; // There is an array of these

	// This is some space to get this to be exactly the size of the disk block.
	// Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct csc452_directory) - sizeof(int)];
};

typedef struct csc452_directory_entry csc452_directory_entry;

// How much data can one block hold?
#define MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))

struct csc452_disk_block
{
	// Space in the block can be used for actual data storage.
	char data[MAX_DATA_IN_BLOCK];
	// Link to the next block in the chain
	long nNextBlock;
};

typedef struct csc452_disk_block csc452_disk_block;

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	printf("Entering getattr \n");
	char dir[MAX_FILENAME + 1]; char fname[MAX_FILENAME + 1]; char ext[MAX_EXTENSION + 1];
	int retVal = 0;
	csc452_root_directory rootDir;

	if (strcmp(path, "/") == 0)
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	else
	{
		// Get our root
		FILE *D1 = fopen(".disk", "rb+");
		fread(&rootDir, sizeof(csc452_root_directory), 1, D1);
		// If the path does exist and is a directory:
		// stbuf->st_mode = S_IFDIR | 0755;
		// stbuf->st_nlink = 2;
		int pathRet = sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext); // Works good
		fclose(D1);

		if (pathRet == 1)
		{
			printf("Path ret == 1 \n");

			// Check if directory exists
			for (int i = 0; i < rootDir.nDirectories; i++)
			{
				if (strcmp(dir, rootDir.directories[i].dname) == 0)
				{
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					return 0;
				}
			}
			return -ENOENT;
		}
		// If the path does exist and is a file:
		// stbuf->st_mode = S_IFREG | 0666;
		// stbuf->st_nlink = 2;
		// stbuf->st_size = file size
		else if (pathRet > 1) // DOUBLE CHECK
		{
			printf("pathRet > 1 \n");
			int size = -1;
			csc452_directory_entry dirEntry;


			int dirPosition = -1;
			// Check if directory is not valid or found
			for (int i = 0; i < rootDir.nDirectories; i++)
			{
				if (strcmp(rootDir.directories[i].dname, dir) == 0)
				{
					dirPosition = i;
					break;
				}
			}
			// Get our directory
			if (dirPosition == -1) 
			{
				return -ENOENT;
			}
			printf("Got root \n");
			FILE *D3 = fopen(".disk", "rb+");
			fseek(D3, rootDir.directories[dirPosition].nStartBlock, SEEK_SET);
			fread(&dirEntry, sizeof(csc452_directory_entry), 1, D3);

			printf("Got entry \n");
			// Find file and get file size
			for (int i = 0; i < dirEntry.nFiles; i++)
			{
				if (strcmp(dirEntry.files[i].fname, fname) == 0) // Check file name
				{
					if (pathRet == 3) // Extension present
					{
						if (strcmp(dirEntry.files[i].fext, ext) == 0) // check extension
						{
							size = dirEntry.files[i].fsize;
						}
					}
					else
					{
						size = dirEntry.files[i].fsize;
					}
				}
			}

			fclose(D3);
			if (size > 0)
			{
				printf("Got size of %d\n", size);
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 2;
				size = (size_t)size;
				stbuf->st_size = size;
				return 0;
			}
		}

		
		else // Not sure if I need this anymore?
		{
			return -ENOENT;
		}
	}

	return retVal;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
						  off_t offset, struct fuse_file_info *fi)
{
	printf("Entering readdir \n");
	char dir[MAX_FILENAME + 1]; char fname[MAX_FILENAME + 1]; char ext[MAX_EXTENSION + 1];
	int pathInput;
	csc452_root_directory rootDir;
	// Since we're building with -Wall (all warnings reported) we need
	// to "use" every parameter, so let's just cast them to void to
	// satisfy the compiler
	(void)offset;
	(void)fi;

	// A directory holds two entries, one that represents itself (.)
	// and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0)
	{
		// csc452_root_directory rootDir; // double check!!!!

		FILE *D1 = fopen(".disk", "rb+");
		if (D1 != NULL)
		{
			fread(&rootDir, sizeof(csc452_root_directory), 1, D1);
		}

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		// Fill directory
		for (int i = 0; i < rootDir.nDirectories; i++)
		{
			filler(buf, rootDir.directories[i].dname, NULL, 0);
		}
		fclose(D1);
	}
	else if ( (pathInput = sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext) == 1) ) {
		csc452_directory_entry dirEntry;

		// Get our root
		FILE *D2 = fopen(".disk", "rb+");
		fread(&rootDir, sizeof(csc452_root_directory), 1, D2);
		

		int dirPosition = 0;
		// Check if directory is not valid or found
		for (int i = 0; i < rootDir.nDirectories; i++)
		{
			if (strcmp(dir, rootDir.directories[i].dname) == 0)
			{
				dirPosition = i;
				break;
			}
		}
		// Get our directory
		if (dirPosition != 1)
		{
			return -ENOENT;
		}
		fclose(D2);

		FILE *D3 = fopen(".disk", "rb+");
		fseek(D3, rootDir.directories[dirPosition].nStartBlock, SEEK_SET);
		fread(&dirEntry, sizeof(csc452_directory_entry), 1, D3);

		// Fill entry
		for (int i = 0; i < dirEntry.nFiles; i++)
		{
			filler(buf, dirEntry.files[i].fname, NULL, 0);
		}
		fclose(D3);

	}
	else {
		return -ENOENT;
	}

	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	printf("Entering mkdir \n");
	(void)mode;
	char dir[MAX_FILENAME + 1]; char fname[MAX_FILENAME + 1]; char ext[MAX_EXTENSION + 1];
	int pathInput = sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext); // Works good

	if (strlen(dir) > 8)
	{
		return -ENAMETOOLONG;
	}
	else if (pathInput != 1)
	{
		return -EPERM;
	}

	// Create root start
	csc452_root_directory rootDir;
	FILE *D1 = fopen(".disk", "rb+");
	fread(&rootDir, sizeof(csc452_root_directory), 1, D1);


	int dirExists = 0;
	// Check if directory exists
	for (int i = 0; i < rootDir.nDirectories; i++)
	{
		if (strcmp(dir, rootDir.directories[i].dname) == 0)
		{
			dirExists = 1;
			break;
		}
	}

	if (dirExists == 1)
	{ // Directory exists
		return -EEXIST;
	}
	printf("Before new block creation (mkdir) \n");
	fclose(D1);

	// Get next open block of memory
	long tempBlock, nextBlock;

	FILE *D2 = fopen(".disk", "rb+");
	fseek(D2, 0, SEEK_SET);

	fseek(D2, (-1) * BLOCK_SIZE, SEEK_SET);
	fread(&tempBlock, sizeof(long), 1, D2);

	nextBlock = tempBlock + BLOCK_SIZE;

	// Write our next block into disk
	fseek(D2, BLOCK_SIZE, SEEK_SET);
	fwrite(&nextBlock, sizeof(long), 1, D2);

	fclose(D2);

	// Assign block address and add our directory to root
	long addr = nextBlock;

	// Add dir to end of list of dirs
	strcpy(rootDir.directories[rootDir.nDirectories].dname, dir);

	rootDir.directories[rootDir.nDirectories].nStartBlock = addr;

	rootDir.nDirectories += 1;

	printf("After new block creation (mkdir) \n");

	FILE *D3 = fopen(".disk", "rb+");
	fwrite(&rootDir, sizeof(csc452_root_directory), 1, D3);
	
	fclose(D3);

	return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 * Note that the mknod shell command is not the one to test this.
 * mknod at the shell is used to create "special" files and we are
 * only supporting regular files.
 *
 */
static int csc452_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void)path;
	(void)mode;
	(void)dev;
	// Get path input
	char dir[MAX_FILENAME + 1]; char fname[MAX_FILENAME + 1]; char ext[MAX_EXTENSION + 1];
	int pathInput = sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext);
	printf("Entering mknod \n");

	// Check Input path
	if (strlen(dir) > 8)
	{
		return -ENAMETOOLONG;
	}
	else if (pathInput == 1 || pathInput == 2) // we only deal in subdirectories for this call
	{
		return -EPERM;
	}

	csc452_root_directory rootDir;
	csc452_directory_entry dirEntry;

	// Get the root dir
	FILE *D1 = fopen(".disk", "rb+");
	fread(&rootDir, sizeof(csc452_root_directory), 1, D1);

	int dirPosition = -1;
	// Check if directory exists
	for (int i = 0; i < rootDir.nDirectories; i++)
	{
		if (strcmp(dir, rootDir.directories[i].dname) == 0)
		{
			dirPosition = i;
		}
	}
	
	fclose(D1);

	printf("GOT PAST ROOT \n");
	FILE *D2 = fopen(".disk", "rb+");
	fseek(D2, rootDir.directories[dirPosition].nStartBlock, SEEK_SET);
	fread(&dirEntry, sizeof(csc452_directory_entry), 1, D2);

	for (int i = 0; i < dirEntry.nFiles; i++)
	{
		if (strcmp(dirEntry.files[i].fname, fname) == 0)
		{
			// File exists already
			return -EEXIST;
		}
	}
	fclose(D2);


	// Get next open block for file
	long tempBlock, nextBlock;
	FILE *D3 = fopen(".disk", "rb+");
	fseek(D3, 0, SEEK_SET);

	fseek(D3, BLOCK_SIZE, SEEK_SET);
	fread(&tempBlock, sizeof(long), 1, D3);

	nextBlock = tempBlock + BLOCK_SIZE;

	// Write our next block into disk
	fseek(D3, BLOCK_SIZE, SEEK_SET);
	fwrite(&nextBlock, sizeof(long), 1, D3);

	long addr = nextBlock; // Our Free memory address


	printf("UPDATING FILES\n");

	// Update dir entry
	// Add to directory entry attributes, add address
	dirEntry.files[dirEntry.nFiles].nStartBlock = addr; // Add to end

	strcpy(dirEntry.files[dirEntry.nFiles].fext, ext);

	strcpy(dirEntry.files[dirEntry.nFiles].fname, fname);
	
	dirEntry.nFiles += 1;

	dirEntry.files[dirEntry.nFiles].fsize = 1;

	fclose(D3);

	// Write back to disk
	FILE *D4 = fopen(".disk", "rb+");
	fseek(D4, rootDir.directories[dirPosition].nStartBlock, SEEK_SET);
	fwrite(&dirEntry, sizeof(csc452_directory_entry), 1, D4);
	
	printf("FILES WRITTEN\n");

	fclose(D4);
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
					   struct fuse_file_info *fi)
{
	(void)buf;
	(void)offset;
	(void)fi;
	(void)path;
	//int sizeRead;
	// Get path input
	char dir[MAX_FILENAME + 1]; char fname[MAX_FILENAME + 1]; char ext[MAX_EXTENSION + 1];
	int pathInput = sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext);
	// check that offset is <= to the file size
	// read in data
	// return success, or error
	csc452_root_directory rootDir;
	csc452_directory_entry dirEntry;

	printf("Entering read \n");

	if (pathInput == 1 || pathInput == 2) // Check if dir
	{
		return -EISDIR;
	}


	// Get the root
	FILE *D1 = fopen(".disk", "rb+");
	fread(&rootDir, sizeof(csc452_root_directory), 1, D1);

	int dirPosition = -1;
	// Check if directory exists
	for (int i = 0; i < rootDir.nDirectories; i++)
	{
		if (strcmp(dir, rootDir.directories[i].dname) == 0)
		{
			dirPosition = i; // we found the directory name
		}
	}
	FILE *D2 = fopen(".disk", "rb+");
	fseek(D2, rootDir.directories[dirPosition].nStartBlock, SEEK_SET);
	fread(&dirEntry, sizeof(csc452_directory_entry), 1, D2);

	int filePos = -1;
	for (int i = 0; i < dirEntry.nFiles; i++)
	{
		if (strcmp(dirEntry.files[i].fname, fname) == 0)
		{
			// File exists
			filePos = i;
		}
	}
	
	if (filePos == -1)
	{ 
		// File doesn't exist
		return -ENOENT;
	}
	fclose(D2);



	// IMPLEMENT READ LOGIC
	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
						off_t offset, struct fuse_file_info *fi)
{
	(void)buf;
	(void)offset;
	(void)fi;
	(void)path;

	// Get path input
	char dir[MAX_FILENAME + 1]; char fname[MAX_FILENAME + 1]; char ext[MAX_EXTENSION + 1];
	int pathInput = sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext);
	printf("Entering write \n");

	if (pathInput ==0) // Check if dir
	{
		return -1;
	}
	// check to make sure path exists
	// check that size is > 0
	// check that offset is <= to the file size
	// write data
	// return success, or error
	csc452_root_directory rootDir;
	csc452_directory_entry dirEntry;

	// Get the root
	FILE *D1 = fopen(".disk", "rb+");
	fread(&rootDir, sizeof(csc452_root_directory), 1, D1);

	int dirPosition = -1;
	// Check if directory exists
	for (int i = 0; i < rootDir.nDirectories; i++)
	{
		if (strcmp(dir, rootDir.directories[i].dname) == 0)
		{
			dirPosition = i; // we found the directory name
		}
	}
	FILE *D2 = fopen(".disk", "rb+");
	fseek(D2, rootDir.directories[dirPosition].nStartBlock, SEEK_SET);
	fread(&dirEntry, sizeof(csc452_directory_entry), 1, D2);

	int filePos = -1;
	for (int i = 0; i < dirEntry.nFiles; i++)
	{
		if (strcmp(dirEntry.files[i].fname, fname) == 0)
		{
			// File exists
			filePos = i;
		}
	}

	if (filePos == -1)
	{ 
		// File doesn't exist
		filePos = dirEntry.nFiles;
	}
	fclose(D2);
	




	// TODO: IMPLEMENT WRITE LOGIC
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void)path;

	// Get path input
	char dir[MAX_FILENAME + 1]; char fname[MAX_FILENAME + 1]; char ext[MAX_EXTENSION + 1];
	int pathInput = sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext);
	csc452_root_directory rootDir;

	printf("Entering rmdir \n");
	// Get root dir
	FILE *D1 = fopen(".disk", "rb+");
	fread(&rootDir, sizeof(csc452_root_directory), 1, D1);

	if (pathInput != 1)
	{
		return -ENOTDIR;
	}
	int dirPosition = -1;

	// Check if directory exists
	for (int i = 0; i < rootDir.nDirectories; i++)
	{
		if (strcmp(dir, rootDir.directories[i].dname) == 0)
		{
			dirPosition = i; // we found the directory name
		}
	}

	if (dirPosition == -1)
	{ 
		// Directory doesn't exist
		return -ENOENT;
	}
	printf("DIRECTORY FOUND\n");

	// Remove block information
	strcpy(rootDir.directories[dirPosition].dname, "");

	rootDir.nDirectories -= 1;

	printf("After new block deletion (rmdir) \n");
	fclose(D1);

	FILE *D2 = fopen(".disk", "rb+");
	fwrite(&rootDir, sizeof(csc452_root_directory), 1, D2);
	printf("DIRECTORY REMOVED\n");
	fclose(D2);

	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
	(void)path;

	// Get path input
	char dir[MAX_FILENAME + 1]; char fname[MAX_FILENAME + 1]; char ext[MAX_EXTENSION + 1];
	int pathInput = sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext);
	csc452_root_directory rootDir;
	csc452_directory_entry dirEntry;

	if (pathInput == 1 || pathInput == 2) // Check if dir
	{
		return -EISDIR;
	}

	// Get the root dir
	FILE *D1 = fopen(".disk", "rb+");
	fread(&rootDir, sizeof(csc452_root_directory), 1, D1);


	int dirPosition = -1;
	// Check if directory exists
	for (int i = 0; i < rootDir.nDirectories; i++)
	{
		if (strcmp(dir, rootDir.directories[i].dname) == 0)
		{
			dirPosition = i;
		}
	}

	fclose(D1);

	printf("GOT PAST ROOT \n");
	FILE *D2 = fopen(".disk", "rb+");
	fseek(D2, rootDir.directories[dirPosition].nStartBlock, SEEK_SET);
	fread(&dirEntry, sizeof(csc452_directory_entry), 1, D2);

	int filePos = -1;
	for (int i = 0; i < dirEntry.nFiles; i++)
	{
		if (strcmp(dirEntry.files[i].fname, fname) == 0)
		{
			// File exists
			filePos = i;
		}
	}
	fclose(D2);
	if (filePos == -1)
	{ 
		// File doesn't exist
		return -ENOENT;
	}

	printf("GOT FILE\n");

	// Update dir entry
	// Delete and update file count
	strcpy(dirEntry.files[filePos].fext, "");

	strcpy(dirEntry.files[filePos].fname, "");
	
	dirEntry.nFiles -= 1;

	printf("FILE REMOVED\n");
	
	return 0;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int csc452_truncate(const char *path, off_t size)
{
	(void)path;
	(void)size;

	return 0;
}

/*
 * Called when we open a file
 *
 */
static int csc452_open(const char *path, struct fuse_file_info *fi)
{
	(void)path;
	(void)fi;
	/*
		//if we can't find the desired file, return an error
		return -ENOENT;
	*/

	// It's not really necessary for this project to anything in open

	/* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

		return -EACCES;
	*/

	return 0; // success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int csc452_flush(const char *path, struct fuse_file_info *fi)
{
	(void)path;
	(void)fi;

	return 0; // success!
}

// register our new functions as the implementations of the syscalls
static struct fuse_operations csc452_oper = {
	.getattr = csc452_getattr,
	.readdir = csc452_readdir,
	.mkdir = csc452_mkdir,
	.read = csc452_read,
	.write = csc452_write,
	.mknod = csc452_mknod,
	.truncate = csc452_truncate,
	.flush = csc452_flush,
	.open = csc452_open,
	.unlink = csc452_unlink,
	.rmdir = csc452_rmdir};

// Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &csc452_oper, NULL);
}
