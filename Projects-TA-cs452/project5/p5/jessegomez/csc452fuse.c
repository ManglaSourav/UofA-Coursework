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

// Maximum possible path with two /s and one . 
#define MAX_PATH 256

static void getDirectory(const char *path, char *ret)
{
	char pathcpy[MAX_PATH+1];
	strncpy(pathcpy, path, MAX_PATH);
	pathcpy[MAX_PATH] = '\0';
	printf("Copied path %s\n", pathcpy);
	char *token = strtok(pathcpy, "/");
	strcpy(ret, token);
}

static void getFilename(const char *path, char *ret)
{
	char pathcpy[MAX_PATH+1];
	strncpy(pathcpy, path, MAX_PATH);
	pathcpy[MAX_PATH] = '\0';
	char *token = strtok(pathcpy, "/");
	if (token) {
		token = strtok(NULL, ".");
	}
	if (token){
		strcpy(ret, token);
	}
}

static void getExtension(const char *path, char *ret)
{
	char pathcpy[MAX_PATH+1];
	strncpy(pathcpy, path, MAX_PATH);
	pathcpy[MAX_PATH] = '\0';
	char *token = strtok(pathcpy, "/");
	if (token) {
		token = strtok(NULL, ".");
	}
	if (token) {
		token = strtok(NULL, ".");
	}
	if (token) {
		strcpy(ret, token);
	}
}

static int countDir(const char *path)
{
	int count;
	char pathcpy[MAX_PATH];
	strcpy(pathcpy, path); 
	int i;
	for (i=0; i < strlen(pathcpy); i++) {
		if (pathcpy[i] == '/') {
			count++;
		}
	}
	return count;
}

static int dirExists(const char *path)
{
	FILE *fp;
	int i;
	char searchName[MAX_FILENAME+1];
       	getDirectory(path, searchName);
	printf("Got directory %s\n", searchName);
	csc452_root_directory root;
	fp = fopen(".disk", "rb");
	if (fp == NULL){
		perror("Did not open file: ");
	}
	printf("c 6\n");
	fread(&root, sizeof(csc452_root_directory), 1, fp);
	printf("d 5\n");
	fclose(fp);
	printf("c 5\n");
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++){
		char curName[MAX_FILENAME+1];
		if (i == 0) {
			printf("name: %s\n", root.directories[i].dname);
		}
		if (strlen(root.directories[i].dname) > 0){
			strcpy(curName, root.directories[i].dname);
			printf("name: %s\n", curName);	
			if (strcmp(searchName, curName) == 0){
				return 1;
			}
		}
	}
	return 0;
}

static int fileExists(const char *path)
{
	FILE *fp;
	int i, j;
	char searchName[MAX_FILENAME+1];
	getDirectory(path, searchName);
	char fileName[MAX_FILENAME+1];
	getFilename(path, fileName);
	char ext[MAX_EXTENSION+1];
	getExtension(path, ext);
	csc452_root_directory root;
	fp = fopen(".drive", "rb");
	fread(&root, BLOCK_SIZE, 1, fp);
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++){
		char *curName = root.directories[i].dname;
		if (strcmp(searchName, curName) == 0){
			fseek(fp, BLOCK_SIZE * root.directories[i].nStartBlock, SEEK_SET);
			csc452_directory_entry dirEntry;
			fread(&dirEntry, BLOCK_SIZE, 1, fp);
			for (j=0; j < MAX_FILES_IN_DIR; j++) {
				if (strcmp(dirEntry.files[j].fname, fileName) == 0){
					fclose(fp);
					return 1;
				}
			}
		}
	}
	fclose(fp);
	return 0;
}

static size_t getFileLength(const char *path)
{
	FILE *fp;
	int i, j;
	char searchName[MAX_FILENAME+1];
	getDirectory(path, searchName);
	char fileName[MAX_FILENAME+1];
	getFilename(path, fileName);
	char ext[MAX_EXTENSION+1];
	getExtension(path, ext);
	csc452_root_directory root;
	fp = fopen(".drive", "rb");
	fread(&root, BLOCK_SIZE, 1, fp);
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++){
		char *curName = root.directories[i].dname;
		if (strcmp(searchName, curName) == 0){
			fseek(fp, BLOCK_SIZE * root.directories[i].nStartBlock, SEEK_SET);
			csc452_directory_entry dirEntry;
			fread(&dirEntry, BLOCK_SIZE, 1, fp);
			for (j=0; j < MAX_FILES_IN_DIR; j++) {
				if (strcmp(dirEntry.files[j].fname, fileName) == 0){
					fclose(fp);
					return dirEntry.files[j].fsize;
				}
			}
		}
	}
	fclose(fp);
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

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		printf("%s %d %d %d\n", path, countDir(path), dirExists(path), fileExists(path));	
		//If the path does exist and is a directory:
		if (countDir(path) == 1 && dirExists(path)){
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		}
		//If the path does exist and is a file:
		else if (countDir(path) == 2 && fileExists(path)){
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = getFileLength(path);
		}	
		else {
		//Else return that path doesn't exist
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
	(void) mode;
	printf("catch 1\n");
	if (dirExists(path)){
		return -EEXIST;
	}
	printf("c 2\n");
	if (countDir(path) > 1){
		return -EPERM;
	}
	printf("c 3\n");
	char name[256];
	getDirectory(path, name);
	if (strlen(name) > MAX_FILENAME){
		return -ENAMETOOLONG;
	}
	printf("c 4\n");
	FILE *fp = fopen(".disk", "ab+");
	csc452_root_directory root;
	fseek(fp, 0, SEEK_SET);
	fread(&root, BLOCK_SIZE, 1, fp);
	int curNumDir = root.nDirectories;
	int i;
	int nextBlock = curNumDir+1;
	int freeIndex = -1;
	for (i=0; i < MAX_DIRS_IN_ROOT; i++){
		if (root.directories[i].nStartBlock == 0L && freeIndex == -1){
			freeIndex = i;
		}
	}
	root.nDirectories++;
	root.directories[freeIndex].nStartBlock = nextBlock;
	strcpy(root.directories[freeIndex].dname, name);

	fseek(fp, 0, SEEK_SET);
	printf("%d %lu %s\n", root.nDirectories, root.directories[freeIndex].nStartBlock, root.directories[freeIndex].dname);
	int writeRet = fwrite(&root, BLOCK_SIZE, 1, fp);
	printf("Successfully wrote %d %d.\n", writeRet, freeIndex);
	fclose(fp);
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
