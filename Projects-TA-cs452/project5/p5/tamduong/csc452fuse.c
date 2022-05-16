/*
	FUSE: Filesystem in Userspace
	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452
	Author: Tam Duong
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>


//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct csc452_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct csc452_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct csc452_file_directory) - sizeof(int)];
} ;

typedef struct csc452_root_directory csc452_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct csc452_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct csc452_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct csc452_directory) - sizeof(int)];
} ;

typedef struct csc452_directory_entry csc452_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE- sizeof(long))

struct csc452_disk_block
{
	//Space in the block can be used for actual data storage.
	char data[MAX_DATA_IN_BLOCK];
	//Link to the next block in the chain
	long nNextBlock;
};

typedef struct csc452_disk_block csc452_disk_block;

/*
* this will return the root directory from the mount point, which is .disk
*/ 
static void getRootDirectory(csc452_root_directory *root) {
	FILE *fp = fopen(".disk", "rb");
	if (fp != NULL) {
		fread(root, sizeof(csc452_root_directory), 1, fp);	
	}
	fclose(fp);

}

/*
	directoryIndex(directory, entry) will find the the directory
	fill the entry in parameter with information from disk and
	return the index of directory in root, or -1 if not found
*/
static int directoryIndex(char *directory, csc452_directory_entry *entry) {
	csc452_root_directory root;
	getRootDirectory(&root);
	int dirIndex = 0;
	for (; dirIndex < root.nDirectories; dirIndex++) {
		if (strcmp(directory, root.directories[dirIndex].dname) == 0) {
			break; 
		}
	}
	if (dirIndex >= root.nDirectories) {
		return -1; 
	}
	FILE *fp = fopen(".disk", "rb");
	if (fp == NULL) {
		printf("Cannot open root file. Something very wrong happen");
		return -1;
	}
	fseek(fp, root.directories[dirIndex].nStartBlock, SEEK_SET);
	fread(entry, sizeof(csc452_directory_entry), 1, fp);
	fclose(fp);
	return dirIndex; 
}

/*
	getFileSize(path) will get the size of the file in path
	and will return -1 if no file is found
*/
static int getFileSize(const char *path) {
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int pathLength = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
	csc452_directory_entry entry;  
	int dirIndex = directoryIndex(directory, &entry); 
	if (dirIndex == -1) return -1; 
	for (int i = 0; i < entry.nFiles; i++) {
		if (strcmp(filename, entry.files[i].fname) == 0) {
			if (pathLength == 3) {
				if (strcmp(extension, entry.files[i].fext) == 0) {
					return entry.files[i].fsize; 
				} 
			} else {
				return entry.files[i].fsize; 
			}
		}
	}
	return -1; 
}

/*
	nextFreeAddress() will get the next free address from the end
	of the disk
*/
static long nextFreeAddress() {
	FILE *fp = fopen(".disk", "rb+");
	fseek(fp, 0, SEEK_END);
	
	long maxiFile = ftell(fp) - BLOCK_SIZE;
	// SEEK_END because the free block are at the end
	fseek(fp, (-1)*BLOCK_SIZE, SEEK_END); 
	long temp;
	fread(&temp, sizeof(long), 1, fp); 
	long address = temp + BLOCK_SIZE; 
	if (address >= maxiFile) {
		fclose(fp); 
		return -1; 
	}
	fseek(fp, (-1)*BLOCK_SIZE, SEEK_END); 
	fwrite(&address, sizeof(long), 1, fp); 
	fclose(fp); 
	return address; 

}
/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int retVal = 0;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	if (strcmp(path, "/") == 0) {
		// is root
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		int pathLength = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
		if (pathLength == 2 || pathLength == 3) {
			// is a file
			int fsize = getFileSize(path);
			if (fsize != -1) {
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 2;
				stbuf->st_size = (size_t) fsize;
				return 0;
			}
		} if (pathLength == 1) {
			// is a directory
			csc452_directory_entry entry;
			if (directoryIndex(directory, &entry) != -1) {
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				return 0;
			} 
		}
		printf("ERROR: directory error\n");
		retVal = -ENOENT;
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

	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	
	if (strcmp(path, "/") != 0) {
		int pathLength = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
		if (pathLength == 1) {
			csc452_directory_entry entry;  
			int dirIndex = directoryIndex(directory, &entry); 
			if (dirIndex == -1) {
				return -ENOENT; 
			} 
			for (int i = 0; i < entry.nFiles; i++) {
				filler(buf, entry.files[i].fname, NULL, 0);
			}
		} else {
			return -ENOENT;	
		}
	} else {
		//read from root
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		csc452_root_directory root;
		getRootDirectory(&root);
		for(int i = 0; i < root.nDirectories; i++)
			filler(buf, root.directories[i].dname, NULL, 0);
	}

	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 * Since this is a two level file system, we can be sure that the directory 
 * will be always below the root one level. 
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) mode;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int pathLength = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
	
	// wrong format
	if (pathLength != 1) {
		return -EPERM; 
	} 
	// name is too long
	if (strlen(directory) > 8) {
		return -ENAMETOOLONG; 
	}

	// directory exist
	csc452_directory_entry entry;
	if (directoryIndex(directory, &entry) != -1) {
		return -EEXIST; 
	}
	csc452_root_directory root;
	getRootDirectory(&root);
	if (root.nDirectories < MAX_DIRS_IN_ROOT) {
		FILE *fp = fopen(".disk", "rb+");
		long address = nextFreeAddress(); 
		if (address == -1) { 
			printf("Out of disk space");
			fclose(fp);
			return -EDQUOT; 
		}
		strcpy(root.directories[root.nDirectories].dname, directory);
		root.directories[root.nDirectories].nStartBlock = address;
		root.nDirectories += 1; 
		fwrite(&root, sizeof(csc452_root_directory), 1, fp); 
		fclose(fp);
	} else {
		printf("Reached max directory\n");
		return -ENOSPC;
	}
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
	(void) path;
	(void) mode;
    (void) dev;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int pathLength = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);	
	if (pathLength != 3) 
		return -EPERM; 
	if (strlen(filename) > 8 || strlen(extension) > 3)
		return -ENAMETOOLONG; 
	if (getFileSize(path) != -1)
		return -EEXIST;
	// assuming that getatt already handle if a path is a directory
	csc452_directory_entry entry;  
	int dirIndex = directoryIndex(directory, &entry); 
	csc452_root_directory root; 
	getRootDirectory(&root); 
	FILE *fp = fopen(".disk", "rb+");
	if (entry.nFiles < MAX_FILES_IN_DIR) {
		long address = nextFreeAddress(); 
		if (address == -1) { 
			printf("Out of disk space");
			fclose(fp); 
			return -EDQUOT; 
		}
		
		entry.files[entry.nFiles].fsize = 0; 
		entry.files[entry.nFiles].nStartBlock = address;
		strcpy(entry.files[entry.nFiles].fname, filename);
		strcpy(entry.files[entry.nFiles].fext, extension);
		entry.nFiles++; 
		// write the updated directory into the disk. 
		
		fseek(fp, root.directories[dirIndex].nStartBlock, SEEK_SET); 
		fwrite(&entry, sizeof(csc452_directory_entry), 1, fp);
		
	} else {
		printf("Maximum file in a directory reached. File creation fail\n");
		return -ENOSPC; 
	}
	fclose(fp); 
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset.
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//return success, or error

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//return success, or error

	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	  (void) path;

	  return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
        (void) path;
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
	(void) path;
	(void) size;

    return 0;
}

/*
 * Called when we open a file
 *
 */
static int csc452_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int csc452_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations csc452_oper = {
    .getattr	= csc452_getattr,
    .readdir	= csc452_readdir,
    .mkdir		= csc452_mkdir,
    .read		= csc452_read,
    .write		= csc452_write,
    .mknod		= csc452_mknod,
    .truncate	= csc452_truncate,
    .flush		= csc452_flush,
    .open		= csc452_open,
    .unlink		= csc452_unlink,
    .rmdir		= csc452_rmdir
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &csc452_oper, NULL);
}
