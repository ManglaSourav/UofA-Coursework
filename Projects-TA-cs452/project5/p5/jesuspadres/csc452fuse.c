#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
/**
	Project 5: File Systems
  Authors: Jesus Padres, Jonathan Misurda
  Class: Csc 452
	Instructor: Jonathan Misurda

  Purpose: This program uses FUSE to create our own file system, managed via a
	single file that represents our disk device. Through FUSE and our implementation,
	it will be possible to interact with our newly created file system using standard
	UNIX/Linux programs in a transparent way.
*/

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

csc452_root_directory *root = NULL;
int nextBlock = 1;

/**
 * Method isValidPath
 *
 * Purpose: checks the path string to make sure it is a valid path
 *
 * @param path is a char array of a file path
 *
 * @return int 0 if only directory, 1 if file, 2 if not valid
 */
static int isValidPath(const char *path) {
	int retVal = 0;

	if (path[0] != '/') {
		return 2;
	}

	int i = 1;
	for (; path[i]; i++) {
		if (path[i] == '/') {
			return 1;
		}
	}
	return 0;
}

/**
 * Method getDirName
 *
 * Purpose: gets the directory name given a path string
 *
 * @param path is a char array of a file path
 *
 * @return char * a char array representing the directory name
 */
static char *getDirName(const char *path) {
	int start = 1;
	int end = 1;
	for (; path[end]; end++) {
		if (path[end] == '/') {
			break;
		}
	}

	char dir[end];
	memcpy(dir, &path[1], end-1);
	dir[end-1] = '\0';

	return dir;
}

/**
 * Method getFileName
 *
 * Purpose: gets the file name given a path string
 *
 * @param path is a char array of a file path
 *
 * @return char * a char array representing the file name
 */
static char *getFileName(const char *path) {
	int start = 1;
	for (; path[start]; start++) {
		if (path[start] == '/') {
			break;
		}
	}
	int end = start;
	for (; path[end]; end++) {
		if (path[end] == '.') {
			break;
		}
	}

	char fn[end-start + 1];
	memcpy(fn, &path[start], end-start);
	fn[end-start] = '\0';

	return fn;
}

/**
 * Method getFileExt
 *
 * Purpose: gets the file extension given a path string
 *
 * @param path is a char array of a file path
 *
 * @return char * a char array representing the file extension
 */
static char *getFileExt(const char *path) {
	int start = 1;
	for (; path[start]; start++) {
		if (path[start] == '.') {
			break;
		}
	}
	int end = start;
	for (; path[end]; end++) {
		if (path[end] == '\0') {
			break;
		}
	}

	char fn[end-start + 1];
	memcpy(fn, &path[start], end-start);
	fn[end-start] = '\0';

	return fn;
}

/**
 * Method getDir
 *
 * Purpose: gets the directory struct given a directory name
 *
 * @param dir is a char array of a directory name
 *
 * @return csc452_directory_entry * directory struct representing the directory dir
 */
static csc452_directory_entry *getDir(const char *dir) {
	if (root == NULL) {
		return NULL;
	}

	int i;
	for (i = 0; i < root->nDirectories; i++) {
		if (strcmp(dir, root->directories[i].dname) == 0) {
			return *root->directories[i].nStartBlock;
		}
	}

	return NULL;
}

/**
 * Method getFile
 *
 * Purpose: gets the file struct given a file name and its parent directory name
 *
 * @param dir is a char array of a directory name
 * @param fn is a char array of a file name
 * @param fx is a char array of a file extension
 *
 * @return csc452_file_directory * file struct representing the file at /dir/fn.fx
 */
static csc452_file_directory *getFile(const char *dir, const char *fn, const char *fx) {
	if (root == NULL) {
		return NULL;
	}

	csc452_directory_entry *directory = NULL;

	int i;
	for (i = 0; i < root->nDirectories; i++) {
		if (strcmp(dir, root->directories[i].dname) == 0) {
			directory = *root->directories[i].nStartBlock;
		}
	}

	for (i = 0; i < directory->nFiles; i++) {
		if (strcmp(fn, directory->files[i].fname) == 0 && strcmp(fx, directory->files[i].fext) == 0) {
			return directory->files[i];
		}
	}

	return NULL;
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

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {

		char *dir = getDirName(path);
		char *fn = getFileName(path);
		char *fx = getFileExt(path);

		csc452_file_directory *file = getFile(dir, fn, fx);

		//If the path does exist and is a directory:
		if (getDir(dir) != NULL) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		} else if (file != NULL) {	//If the path does exist and is a file:
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = file.fsize;
		} else {
			res = -ENOENT;
		}
	}

	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls
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
	char *dir = getDirName(path);
	csc452_directory_entry *directory = getDir(dir);

	//A directory holds two entries, one that represents itself (.)
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
	} else if (directory != NULL) {
		for (i = 0; i < directory->nFiles; i++) {
			if (strcmp(fn, directory->files[i].fname) == 0 && strcmp(fx, directory->files[i].fext) == 0) {
				filler(buf, directory->files[i].fname, NULL, 0);
			}
		}
	} else {
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
	(void) mode;

	char *dir = path;
	if (isValidPath(path) == 0) {
		dir = getDirName(path);
	}

	csc452_directory_entry *newDir = *(&root + (nextBlock * BLOCK_SIZE));
	root->directories[root->nDirectories].dname = dir;
	root->nDirectories++;
	nextBlock++;

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
	(void) mode;
  (void) dev;

	char *dir = path;
	char *fn = path;
	char *fx = path;
	if (isValidPath(path) == 1) {
		dir = getDirName(path);
		fn = getFileName(path);
		fx = getFileExt(path);
	}

	csc452_directory_entry *directory = getDir(dir);

	csc452_file_directory *newFile = *(&root + (nextBlock * BLOCK_SIZE));
	directory->files[directory->nFiles].fname = fn;
	directory->files[directory->nFiles].fext = fx;
	directory->files[directory->nFiles].fsize = 0;
	directory->nStartBlock = &newFile;
	directory->nFiles++;
	nextBlock++;

	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) fi;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//return success, or error
	char *dir = path;
	char *fn = path;
	char *fx = path;
	if (isValidPath(path) == 1) {
		dir = getDirName(path);
		fn = getFileName(path);
		fx = getFileExt(path);
	}

	csc452_file_directory *file = getFile(dir, fn, fx);

	if (file->fsize > 0 && file->fsize > offset) {
		csc452_disk_block *block = *file->nNextBlock;
		memcpy(buf, &block->data[offset], size);
	}

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	char *dir = path;
	char *fn = path;
	char *fx = path;
	if (isValidPath(path) == 1) {
		dir = getDirName(path);
		fn = getFileName(path);
		fx = getFileExt(path);
	}

	csc452_file_directory *file = getFile(dir, fn, fx);

	csc452_disk_block *block = *(&root + (nextBlock * BLOCK_SIZE));
	memcpy(block->data, &buf[offset], size);
	file->nStartBlock = &block;
	file->fsize = size;

	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
		char *dir = getDirName(path);
		free(getDir(dir));

	  return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
	char *dir = path;
	char *fn = path;
	char *fx = path;
	if (isValidPath(path) == 1) {
		dir = getDirName(path);
		fn = getFileName(path);
		fx = getFileExt(path);
	}

	free(getFile(dir, fn, fx));

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
	int fd = open(".disk", O_RDWR, S_IRUSR | S_IWUSR);
	struct stat sb;
	fstat(fd, &sb);

	void *disk = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	root = disk;
	root->nDirectories = 0;

	int retVal = fuse_main(argc, argv, &csc452_oper, NULL);

	munmap(disk, sb.st_size);
	close(fd);

	return retVal;
}
