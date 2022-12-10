/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


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

int getNextBlock() {
	char blocks[1280];
	FILE *file = fopen(".disk", "r+b");
	fseek(file, -3 * BLOCK_SIZE, SEEK_END);
	fread(blocks, 1279, 1, file);
	int count = 0;
	int i;
	for (i = 0; i < 1280; i++) {
		char letter = blocks[i];
		int j;
		int found = 0;
		for (j = 0; j < 8; j++) {
			if (letter & 0x0) {
				found = 1;
				break;
			}
			count++;
		}
		if (found) break;
	}
	fclose(file);
	if (count == 1280) {
		return -1;
	}
	return count;
}

void markUsedBlock(int blockNum) {
	char blocks[1280];
	FILE *file = fopen(".disk", "r+b");
	fseek(file, -3 * BLOCK_SIZE, SEEK_END);
	fread(blocks, 1279, 1, file);
	int count = 0;
	int i;
	for (i = 0; i < 1280; i++) {
		char letter = blocks[i];
		int j;
		for (j = 0; j < 8; j++) {
			if (count == blockNum) {
				letter = letter & 0x1;
				blocks[i] = letter | blocks[i];

			}
			letter = letter >> 1;
			count++;
		}
	}
	fseek(file, -3 * BLOCK_SIZE, SEEK_END);
	fwrite(&blocks, BLOCK_SIZE * 3, 1, file);
	fclose(file);
}

struct csc452_root_directory * getRoot() {
	struct csc452_root_directory *root  = (csc452_root_directory * )malloc(sizeof(struct csc452_root_directory));
	FILE *file = fopen(".disk", "r+b");
	fread(root, BLOCK_SIZE, 1, file);
	fclose(file);
	return root;
}

struct csc452_directory_entry * getDirectory(int blockNum) {
	FILE *file = fopen(".disk", "rb");
	fseek(file, blockNum * BLOCK_SIZE, SEEK_SET);
	struct csc452_directory_entry *entry = (struct csc452_directory_entry *)malloc(sizeof(csc452_directory_entry));
	fread(entry, BLOCK_SIZE, 1, file);
	fclose(file);
	return entry;
}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf) {
	int res = 0;
	// marks the root as used, just in case;
	markUsedBlock(0);
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	struct csc452_root_directory *root = getRoot();
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		// Checks if path exists
		if (filename == NULL) {
			int i = 0;
			while (i < MAX_DIRS_IN_ROOT) {
				if (strcmp(directory, root->directories[i].dname) == 0) {
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					break;
				}
				i++;
			}
			if (i == MAX_DIRS_IN_ROOT) 
				res = -ENOENT;
		}
		//If the path does exist and is a file:
		else {
			int i = 0;
			while (strcmp(directory, root->directories[i].dname) != 0 && i < MAX_DIRS_IN_ROOT) {
				i++;
			}
			// if directory not found
			if (i == MAX_DIRS_IN_ROOT) {
				free(root);
				return -ENOENT;
			}
			struct csc452_directory_entry *dir = getDirectory(i);
			i = 0;
			while (i < MAX_FILES_IN_DIR) {
				struct csc452_file_directory entry = dir->files[i];
				if (strcmp(filename, entry.fname) == 0) {
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 1;
					stbuf->st_size = entry.fsize;
					break;
				}
				i++;
			}
			if (i == MAX_FILES_IN_DIR) {
				res = -ENOENT;
			}
		}
	}
	free(root);
	return res;
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

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
	}
	else {
		// All we have _right now_ is root (/), so any other path must
		// not exist. 
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
	if (strlen(path) > MAX_FILENAME)
	{
		return -ENAMETOOLONG;
	}

	struct stat * stats = (struct stat *)malloc(sizeof(struct stat));
	int error = csc452_getattr(path, stats);
	if (error == 0)
	{
		return -EEXIST;
	}
	// Set EPERM
	free(stats);

	
	struct csc452_root_directory *root = getRoot();
	// if too many files uwu
	if (root->nDirectories >= MAX_DIRS_IN_ROOT) {
		// change the value to whatever daddy D misurda wants
		return -1;
	}

	int availableBlock = getNextBlock();
	if (availableBlock == -1) return -1;

	// Gets the root, increases the number;
	FILE *file = fopen(".disk", "r+b");
	root->nDirectories++;
	struct csc452_directory newDir = root->directories[availableBlock];
	strcpy(newDir.dname, path);
	newDir.nStartBlock = availableBlock;
	fseek(file, 0, SEEK_SET);
	fwrite(root, BLOCK_SIZE, 1, file);

	// Write the directory
	struct csc452_directory_entry *dir = getDirectory(availableBlock);
	dir->nFiles = 0;
	markUsedBlock(availableBlock);
	fseek(file, -availableBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(dir, BLOCK_SIZE, 1, file);
	free(root);
	free(dir);
	fclose(file);
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
	
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
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
