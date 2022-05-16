/*
	FUSE: Filesystem in Userspace
	Denson Gothi
	CSC452
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
#include <unistd.h>

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

static int is_dir(const char *path) {
	int i;
	for (i = 0; i < csc452_root_directory->nDirectories; i++) {
		struct csc452_directory curr_dir = csc452_root_directory->directories[i];
		if (strcmp(curr_dir->dname, path+1) == 0) {
			return 1;
		}
	}
	return 0;
}

static int is_file(const char *path) {
	int i;
	int inRoot = 1;
	path++;
	int res = 1;
	for (i = 0; i < strlen(path); i++) {
		if (strcmp(path[i], "/") == 0) {
			inRoot = 0;
			if ( i + 1 != strlen(path) ) {
				res = 0;
			}
		}
	}
	return res;
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

	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();

	if (strcmp(path, "/") == 0 || is_dir(path)) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		
		if (is_file(path) == 0) {
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = BLOCK_SIZE;
		} else {
			res = -ENOENT;
		}
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

		int n = csc452_root_directory->nDirectories;
		int i;
		for (i = 0; i < n; i++) {
			filler(buf, csc452_root_directory->directories[i]->dname, NULL, 0);
		}
	}
	else {
		if (is_dir(path)) {
			filler(buf, ".", NULL, 0);
			filler(buf, "..", NULL, 0);
			int num_files = csc452_directory_entry->nFiles;
			int j;
			for (j = 0; j < num_files; j++) {
				filler(buf, csc452_directory_entry->files[i]->fname, NULL, 0);
			}
		} else {
			return -ENOENT;
		}
	}

	return 0;
}

static int is_under_root(const char *path) {
	int i;
	path++;
	for (i = 0; i < strlen(path); i++) {
		if (strcmp(path[i], "/") == 0) {
			if (i != strlen(path)) {
				return 0;
			}
		}
	}
	return 1;
}

static int length_dir_name(const char *path) {
	int i = 0;
	path++;
	while (i <= 8) {
		if (strcmp(path[i], "/") == 0) {
			return 1;
		}
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
	if (!is_under_root(path)) {
		return EPERM;
	}

	if (!length_dir_name(path)) {
		return ENAMETOOLONG;
	}

	if (is_dir(path)) {
		return EEXIST;
	}

	int to_add = csc452_root_directory->nDirectories;
	struct csc452_directory next_dir = malloc(sizeof(csc452_directory));
	strcpy(next_dir->dname, path+1);
	next_dir->nStartBlock = to_add*BLOCK_SIZE;
	csc452_root_directory->directories[to_add] = next_dir;
	csc452_root_directory->nDirectories++;

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

	int start = 0;
	int actual = 0;
	int dot = 0;
	int dot_count = 0;
	path++;
	int len = strlen(path);
	for (i = 0; i < len; i++) {
		if (strcmp(path[i], "/") {
			start = 1;
		} else if (strcmp(path[i], ".") {
			dot = 1;
			start = 0;
		} else if (start) {
			actual++;
		} else if (dot) {
			dot_count++;
		}
	}

	if (start > 8 || dot_count > 3) {
		return ENAMETOOLONG;
	}

	if (start == 0) {
		return EPERM;
	}

	if (is_file(path--) == 0) {
		return EEXIST;
	}
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

	if (is_dir(path)) {
		return EISDIR;
	}
	//check to make sure path exists
	if (is_file(path) == 0) {
		if (size > 0) {
			if (offset <= size) {
				filler(buf, size, NULL, 0);
			}
		}
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
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	if (offset > size) {
		return EFBIG;
	}

	if (is_file(path) == 0) {
		if (size > 0) {
			if (offset <= size) {
				//TODO
			}
		}
	}

	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	if (is_dir(path)) {
		csc452_root_directory->nDirectories--;
	}
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
