/*
	Author: Emilio Santa Cruz
	Description: Creates an file system run with FUSE

	Bugs: Created directories can not be populated.
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

// ll node for files
struct fileNode{
	char name[13];
	struct fileNode *next;
	int size;
};

// ll node for directories
struct directoryNode{
	char name[9];
	struct directoryNode *next;
	struct fileNode *fileHead;
	struct fileNode *fileTail;
};
// defs
typedef struct fileNode directoryNode;

// globals for directory ll
struct directoryNode *head = NULL;
struct directoryNode *tail = NULL;

/*
Finds matching directories that match with path. Returns the node
if found, NULL if not.
*/
struct directoryNode* findDirectory(const char *path){
	struct directoryNode *curr = head;
	path++;
	printf("test: %s\n", path);
	while(curr != NULL){
		printf("%s vs %s\n", curr->name, path);
		if(strcmp(curr->name, path) == 0){
			return curr;
		}
		curr = curr->next;
	}
	printf("dir not found\n");
	return NULL;
}

/*
Finds the file that matches path in dir. Returns 1 if found, 0 if not.
*/
int findFile(struct directoryNode* dir, const char *path){
	printf("%d", 1);
	struct fileNode* curr = dir->fileHead;
	printf("%d", 2);
	while(curr != NULL){
		printf("FILE: %s vs %s", curr->name, path);
		if(strcmp(curr->name, path) == 0){
			return 1;
		}
		curr = curr->next;
	}

	return 0;
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	struct directoryNode* currDir = findDirectory(path);

	path++;

	if (strcmp(path, "") == 0) {
		printf("%d\n", 1);
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if(currDir != NULL && strcmp(currDir->name, path) == 0){
		printf("%d\n", 2);
		//If the path does exist and is a directory:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if(currDir != NULL && findFile(currDir, path)){
		//If the path does exist and is a file:
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 2;
		stbuf->st_size = 1;
		
		//Else return that path doesn't exist
	} else{
		res = -ENOENT;
	}

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
		struct directoryNode *curr = head;
		while(curr != NULL){
			filler(buf, curr->name, NULL, 0);
			curr = curr->next;
		}
	}
	else {
		// All we have _right now_ is root (/), so any other path must
		// not exist. 
		return -ENOENT;
	}

	return 0;
}

/*
Checks if directory is root level, 1 if not, 0 if so
*/
int checkLevel(const char *path){
	int i = 0;
	int level = 0;
	while(i != strlen(path)){
		if(strcmp(path, "/") == 0){
			level++;
		}
		if(level > 1){
			return 1;
		}
		i++;
	}

	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	path++;
	if(strlen(path) > 8){
		return -ENAMETOOLONG;
	} else if(checkLevel(path)){
		return -EPERM;
	} else if(findDirectory(path)){
		return -EEXIST;
	}
	struct directoryNode* new = (struct directoryNode*) malloc(sizeof(struct directoryNode));
	strcpy(new->name, path);

	// prepopulation of directory
	new->fileHead = (struct fileNode*) malloc(sizeof(struct fileNode));
	strcpy(new->fileHead->name, ".");
	new->fileTail = (struct fileNode*) malloc(sizeof(struct fileNode));
	strcpy(new->fileTail->name, "..");
	new->fileHead->next = new->fileTail;

	if(head == NULL){
		head = new;
		tail = new;
	} else{
		tail->next = new;
		tail = new;
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
