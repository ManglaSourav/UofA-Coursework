/**
 * @file csc452fuse.c
 * @author Luke Broadfoot (lucasxavier@email.arizona.edu)
 * @brief Implements a basic file system in user space using FUSE
 *
 * FUSE: Filesystem in Userspace
 * 		 gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452
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
// 1KB * 5KB == 5MB || 5242880B
#define DISK_SIZE 5242880

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
typedef struct csc452_directory_entry {
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
} csc452_directory_entry;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

typedef struct csc452_root_directory {
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
} csc452_root_directory;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE- sizeof(long))

typedef struct csc452_disk_block {
	//Space in the block can be used for actual data storage.
	char data[MAX_DATA_IN_BLOCK];
	//Link to the next block in the chain
	long nNextBlock;
} csc452_disk_block;

// holds the parsed file path
typedef struct csc452_path {
	int matches;
	char dir[MAX_FILENAME + 1], fName[MAX_FILENAME + 1], eName[MAX_EXTENSION + 1];
} csc452_path;

// a bitmap for tracking free blocks, I don't think I implemented it well though
typedef struct csc452_bitmap {
	unsigned int x : 1;
} __attribute__((packed)) csc452_bitmap[BLOCK_SIZE * 3];

// there are 10240 512 blocks in 5MB, and I'm using 3 for my csc452_bitmap
// I'm not quite sure which numbers to believe because running od -x .disk says there are 24000 blocks
// and that my bitmap starts at block 23775 instead of 10237 from what I thought
#define MAX_FREE_BLOCKS (DISK_SIZE / BLOCK_SIZE) - 3

/**
 * @brief Takes a file path and a pointer to a csc452_path struct and attempts to parse the path and place
 * the results in the struct
 * 
 * @param path a file path
 * @param res where the parsed data is stored
 * @return int returns the result of sscanf (-1, 1, 2, 3)
 */
static int parsePath(const char *path, csc452_path *res) {
	res->matches = sscanf(path, "/%[^/]/%[^.].%s", res->dir, res->fName, res->eName);
	return res->matches;
}

/**
 * @brief places the first block of the file into a csc452_root_directory struct
 * 
 * @param res the location to store the loaded block
 * @return int 0 if .disk does not exist, 1 if it does
 */
static int loadRoot(csc452_root_directory *res) {
	FILE *fp = fopen(".disk", "rb");
	if (fp == NULL) { return 0; }
	fread(res, BLOCK_SIZE, 1, fp);
	fclose(fp);
	return 1;
}

/**
 * @brief attempts to find the directory dir in the .disk file
 * 
 * @param dir the directory name
 * @param res the location to store the loaded block (can be NULL if you don't want the block)
 * @return long 0 if the directory does not exist, else returns the block number
 */
static long dirExists(const char *dir, csc452_directory_entry *res) {
	csc452_root_directory root;
	if (!loadRoot(&root)) { return 0; }
	FILE *fp = fopen(".disk", "rb");
	for (int i = 0; i < root.nDirectories; i++) {
		if (strcmp(dir, root.directories[i].dname) == 0) {
			// checks if the user passed NULL as res or not
			if (res) {
				fseek(fp, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
				fread(res, BLOCK_SIZE, 1, fp);
			}
			fclose(fp);
			return root.directories[i].nStartBlock;
		}
	}
	fclose(fp);
	return 0;
}
/**
 * @brief attempts to find a file in the .disk file
 * 
 * @param path a csc452_path struct containing the parsed path
 * @param res the location to store the loaded file data (can be NULL if you don't want the data)
 * @return size_t -1 if the file does not exist, else returns the size of the file
 */
static size_t fileExists(csc452_path *path, struct csc452_file_directory *res) {
	if (path->matches != 3) { return -1; }
	csc452_directory_entry dir;
	if (!dirExists(path->dir, &dir)) { return -1; }
	for (int i = 0; i < dir.nFiles; i++) {
		if (strcmp(path->fName, dir.files[i].fname) == 0) {
			if (strcmp(path->eName, dir.files[i].fext) == 0) {
				// checks if res != NULL, and puts the data in to return
				if (res) { memcpy(res, &dir.files[i], sizeof(struct csc452_file_directory)); }
				return dir.files[i].fsize;
			}
		}
	}
	return -1;
}

/**
 * @brief attempts to find the next free block
 * 
 * @return long -1 if there are no free blocks, else returns the next free block number
 */
static long getFreeBlock() {
	csc452_root_directory root;
	csc452_bitmap bitmap;
	loadRoot(&root);
	FILE *fp = fopen(".disk", "rb+");
	// loads the bitmap
	fseek(fp, (-3)*BLOCK_SIZE, SEEK_END);
	fread(&bitmap, BLOCK_SIZE, 3, fp);
	// this is a set up, if .disk was not previously initialized, set the first bit in the bitmap
	// to 1 meaning the root occupies the first block
	if (root.nDirectories == 0) { bitmap[0].x = 1; }
	// loops over all free blocks to find the first available one
	for (int i = 0; i < MAX_FREE_BLOCKS; i++) {
		if (bitmap[i].x == 0) {
			bitmap[i].x = 1;
			fseek(fp, (-3)*BLOCK_SIZE, SEEK_END);
			fwrite(&bitmap, BLOCK_SIZE, 3, fp);
			fclose(fp);
			return (long) i;
		}
	}
	return -1;
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
	size_t fileSize;

	// base case for root directory
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		// parses the path
		csc452_path parsed;
		parsePath(path, &parsed);
		// matches is 1 if it only matched for the directory, check that the directory exists
		if (parsed.matches == 1 && dirExists(parsed.dir, NULL)) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		}
		// matches is 3 for a file, and checks that the file exists
		else if (parsed.matches == 3 && (fileSize = fileExists(&parsed, NULL)) != -1) {
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = fileSize;
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
		csc452_root_directory root;
		// loadRoot returns false (0) if it could not load .disk
		if (!loadRoot(&root)) { return -ENOENT; }
		// looks over the directories in the root and adds them to the buffer
		for (int i = 0; i < root.nDirectories; i++) {
			filler(buf, root.directories[i].dname, NULL, 0);
		}
	}
	else {
		// converts the path into a parsed struct
		csc452_path parsed;
		parsePath(path, &parsed);
		// matches == 1 means it is a directory
		if (parsed.matches == 1) {
			csc452_directory_entry dir;
			// dirExists returns false (0) if the directory doesn't exist
			if (!dirExists(parsed.dir, &dir)) { return -ENOENT; }
			filler(buf, ".", NULL,0);
			filler(buf, "..", NULL, 0);
			char fileName[MAX_FILENAME + MAX_EXTENSION + 2];
			// loops over the files in the directory and adds them to the buffer
			for (int i = 0; i < dir.nFiles; i++) {
				sprintf(fileName, "%s.%s", dir.files[i].fname, dir.files[i].fext);
				filler(buf, fileName, NULL, 0);
			}
		} else {
			return -ENOENT;
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
	(void) mode;
	long freeBlock;

	// guard clauses checks that the path is within 8 characters (the plus 2 is for \0 and /)
	if (strlen(path) > MAX_FILENAME + 2) { return -ENAMETOOLONG; }
	// we don't want users making new roots
	if (strcmp(path, "/") == 0) { return -EEXIST; }
	// I was having a bug and putting it on the heap fixed it.
	csc452_path *parsed = malloc(BLOCK_SIZE);
	parsePath(path, parsed);
	// checks that the directory does not already exists
	if (dirExists(parsed->dir, NULL)) { return -EEXIST; }
	// checks that root isn't full
	csc452_root_directory *root = malloc(BLOCK_SIZE);
	loadRoot(root);
	if (root->nDirectories >= MAX_DIRS_IN_ROOT) { return -ENOSPC; }
	// checks for free a free block
	if ((freeBlock = getFreeBlock()) == -1) { return -ENOSPC; }
	// puts the new directory metadata into the root
	strcpy(root->directories[root->nDirectories].dname, parsed->dir);
	root->directories[root->nDirectories++].nStartBlock = freeBlock;
	// writes out the root
	FILE *fp = fopen(".disk", "rb+");
	fwrite(root, BLOCK_SIZE, 1, fp);
	fclose(fp);
	free(parsed); free(root);
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
	long dirBlock, freeBlock;

	// checks that the path is within spec
	if (strlen(path) > (MAX_FILENAME * 2) + MAX_EXTENSION + 4) { return -ENAMETOOLONG; }

	// checks that the path is valid
	csc452_path parsed;
	if (parsePath(path, &parsed) != 3) { return -EPERM; }
	// checks that the directory exists
	csc452_directory_entry dir;
	if ((dirBlock = dirExists(parsed.dir, &dir)) == 0) { return -ENOENT; }
	// checks that the files does not already exist
	if (fileExists(&parsed, NULL) != -1) { return -EEXIST; }
	// checks that the directory isn't full
	if (dir.nFiles >= MAX_FILES_IN_DIR) { return -ENOSPC; }
	// checks that there is a free block to give out
	if ((freeBlock = getFreeBlock()) == -1) { return -ENOSPC; }
	// puts the new file metadata into the directory
	strcpy(dir.files[dir.nFiles].fname, parsed.fName);
	strcpy(dir.files[dir.nFiles].fext, parsed.eName);
	dir.files[dir.nFiles++].nStartBlock = freeBlock;
	// writes the directory out
	FILE *fp = fopen(".disk", "rb+");
	fseek(fp, dirBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&dir, BLOCK_SIZE, 1, fp);
	fclose(fp);
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

	// checks that the path is to a file
	csc452_path parsed;
	if (parsePath(path, &parsed) == 1) { return -EISDIR; }
	// checks that the file exists
	struct csc452_file_directory file;
	if (fileExists(&parsed, &file) == -1) { return -ENOENT; }
	// return early if there is nothing to read
	if (size == 0) { return size; }
	// error out if they try to read something that doesn't exist
	if (file.fsize < offset) { return -EINVAL; }
	// grab the first block of data
	FILE *fp = fopen(".disk", "rb");
	csc452_disk_block cur;
	fseek(fp, file.nStartBlock * BLOCK_SIZE, SEEK_SET);
	fread(&cur, BLOCK_SIZE, 1, fp);
	// find where to start reading
	int blockOffset = offset % MAX_DATA_IN_BLOCK;
	// updates size to be how many bytes it can read
	if (size > file.fsize) { size = file.fsize; }
	// if the offset is not in the first block, loop until we are in the block it wants
	for (int i = 0; i < offset / MAX_DATA_IN_BLOCK; i++) {
		fseek(fp, cur.nNextBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur, BLOCK_SIZE, 1, fp);
	}
	// loops over the block(s) reading all the data into the buffer
	for (size_t i = 0; i < size; i++) {
		// we when finish reading a block
		if (blockOffset >= MAX_DATA_IN_BLOCK) {
			// if there are no more blocks, return early
			if (cur.nNextBlock == 0) { 
				fclose(fp);
				return size;
			}
			// otherwise load the next block
			fseek(fp, cur.nNextBlock * BLOCK_SIZE, SEEK_SET);
			fread(&cur, BLOCK_SIZE, 1, fp);
			blockOffset = 0;
		}
		// copies the block data into the buffer byte by byte
		buf[i] = cur.data[blockOffset++];
	}
	fclose(fp);
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
	long freeBlock, curBlockNo;

	// checks that the path is to a file
	csc452_path parsed;
	if (parsePath(path, &parsed) == 1) { return -EISDIR; }
	struct csc452_file_directory file;
	// checks that the file exists
	if (fileExists(&parsed, &file) == -1) { return -ENOENT; }
	// return early if there is nothing to write
	if (size == 0) { return size; }
	// error out if it tries to start writing beyond the file size
	if (file.fsize < offset) { return -EFBIG; }
	FILE *fp = fopen(".disk", "rb+");
	// grabs the first block of the file
	csc452_disk_block cur;
	fseek(fp, file.nStartBlock * BLOCK_SIZE, SEEK_SET);
	fread(&cur, BLOCK_SIZE, 1, fp);
	// determines where to start writing
	int blockOffset = offset % MAX_DATA_IN_BLOCK;
	curBlockNo = file.nStartBlock;
	// if offset is beyond the first block, loop until it in the next block
	for (int i = 0; i < offset / MAX_DATA_IN_BLOCK; i++) {
		curBlockNo = cur.nNextBlock;
		fseek(fp, cur.nNextBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur, BLOCK_SIZE, 1, fp);
	}
	fclose(fp);
	// copies the data from buf into the block(s) on disk
	for (size_t i = 0; i <= size; i++) {
		// if we pill up a single block go to the next
		if (blockOffset >= MAX_DATA_IN_BLOCK) {
			// if there is no next block, get a free one
			if (cur.nNextBlock == 0) {
				if ((freeBlock = getFreeBlock()) == -1) { return -ENOSPC; }
				cur.nNextBlock = freeBlock;
			}
			// writes out the current block and loads the next block
			FILE *fp = fopen(".disk", "rb+");
			fseek(fp, curBlockNo * BLOCK_SIZE, SEEK_SET);
			fwrite(&cur, BLOCK_SIZE, 1, fp);
			curBlockNo = cur.nNextBlock;
			fseek(fp, cur.nNextBlock * BLOCK_SIZE, SEEK_SET);
			fread(&cur, BLOCK_SIZE, 1, fp);
			fclose(fp);
			blockOffset = 0;
		}
		// copies from buf into the current block
		cur.data[blockOffset++] = buf[i];
	}
	// updates the file metadata if we need to
	fp = fopen(".disk", "rb+");
	if (file.fsize < size + offset) {
		parsePath(path, &parsed);
		csc452_directory_entry dir;
		// gets the directory block that the file belongs to and updates the metadata
		long dirBlock = dirExists(parsed.dir, &dir);
		for (int i = 0; i < dir.nFiles; i++) {
			if (strcmp(dir.files[i].fname, parsed.fName) == 0) {
				if (strcmp(dir.files[i].fext, parsed.eName) == 0) {
					dir.files[i].fsize = size + offset;
					fseek(fp, dirBlock * BLOCK_SIZE, SEEK_SET);
					fwrite(&dir, BLOCK_SIZE, 1, fp);
					break;
				}
			}
		}
	}
	// writes the last block out to disk
	fseek(fp, curBlockNo * BLOCK_SIZE, SEEK_SET);
	fwrite(&cur, BLOCK_SIZE, 1, fp);
	fclose(fp);
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	// checks that the path is valid
	csc452_path parsed;
	if (parsePath(path, &parsed) != 1) { return -ENOTDIR; }
	// checks that the directory exists
	csc452_directory_entry dir;
	if (!dirExists(parsed.dir, &dir)) { return -ENOENT; }
	// checks that the directory is empty
	if (dir.nFiles != 0) { return -ENOTEMPTY; }
	// probably unnecessary check that .disk exists
	csc452_root_directory root;
	if (!loadRoot(&root)) { return -ENOENT; }
	int i;
	// grabs the bitmap
	csc452_bitmap bitmap;
	FILE *fp = fopen(".disk", "rb+");
	fseek(fp, (-3)*BLOCK_SIZE, SEEK_END);
	fread(&bitmap, BLOCK_SIZE, 3, fp);
	// loops until we find the directory
	for (i = 0; i < root.nDirectories; i++) {
		if (strcmp(parsed.dir, root.directories[i].dname) == 0) {
			// 'erase' the directory name, and set all values to 0
			memset(root.directories[i].dname, 0, MAX_FILENAME + 1);
			bitmap[root.directories[i].nStartBlock].x = 0;
			root.directories[i].nStartBlock = 0;
			break;
		}
	}
	// move down the directories after it
	for (int j = i+1; j < root.nDirectories; j++) {
		strcpy(root.directories[j-1].dname, root.directories[j].dname);
		root.directories[j-1].nStartBlock = root.directories[j].nStartBlock;
	}
	root.nDirectories--;
	// writes out the root and bitmap to disk
	fseek(fp, 0, SEEK_SET);
	fwrite(&root, BLOCK_SIZE, 1, fp);
	fseek(fp, (-3)*BLOCK_SIZE, SEEK_END);
	fwrite(&bitmap, BLOCK_SIZE, 3, fp);
	fclose(fp);
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
	// I had a bug where it didn't work on the stack so I put it on the heap
	csc452_path *parsed = malloc(BLOCK_SIZE);
	// checks that it is a valid path / not a directory
	if (parsePath(path, parsed) == 1) { return -EISDIR; }
	// checks that the file exists
	struct csc452_file_directory file;
	if ((fileExists(parsed, &file)) == -1) { return -ENOENT; }
	csc452_directory_entry *dir = malloc(BLOCK_SIZE);
	// grabs the directory block number and loads the directory into dir
	long dirBlock =	dirExists(parsed->dir, dir);
	int i;
	// finds the file metdata in the directory and 'erases' it
	for (i = 0; i < dir->nFiles; i++) {
		if (strcmp(dir->files[i].fname, parsed->fName) == 0) {
			if (strcmp(dir->files[i].fext, parsed->eName) == 0) {
				memset(dir->files[i].fname, 0, MAX_FILENAME + 1);
				memset(dir->files[i].fext, 0, MAX_EXTENSION + 1);
				dir->files[i].fsize = 0;
				dir->files[i].nStartBlock = 0;
				break;
			}
		}
	}
	// moves down the files after it
	for (int j = i+1; j < dir->nFiles; j++) {
		strcpy(dir->files[j-1].fname, dir->files[j].fname);
		strcpy(dir->files[j-1].fext, dir->files[j].fext);
		dir->files[j-1].fsize = dir->files[j].fsize;
		dir->files[j-1].nStartBlock = dir->files[j].nStartBlock;
	}
	dir->nFiles--;
	FILE *fp = fopen(".disk", "rb+");
	csc452_bitmap bitmap;
	// writes the updated directory block to disk
	fseek(fp, dirBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(dir, BLOCK_SIZE, 1, fp);
	// loads the bitmap
	fseek(fp, (-3)*BLOCK_SIZE, SEEK_END);
	fread(&bitmap, BLOCK_SIZE, 3, fp);
	// loads the first block of the file
	csc452_disk_block cur;
	fseek(fp, file.nStartBlock * BLOCK_SIZE, SEEK_SET);
	fread(&cur, BLOCK_SIZE, 1, fp);
	csc452_disk_block empty;
	// a 'dummy' blank block
	memset(&empty, 0, BLOCK_SIZE);
	long curBlock = file.nStartBlock, nextBlock = cur.nNextBlock;
	// updates the bitmap to say that that block is free
	bitmap[curBlock].x = 0;
	// writes the dummy block over the old block
	fseek(fp, curBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&empty, BLOCK_SIZE, 1, fp);
	// goes through all linked blocks and 'frees' them
	while (nextBlock) {
		curBlock = nextBlock;
		nextBlock = cur.nNextBlock;
		bitmap[curBlock].x = 0;
		// loads the next block
		fseek(fp, curBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur, BLOCK_SIZE, 1, fp);
		// then writes over it
		fseek(fp, curBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&empty, BLOCK_SIZE, 1, fp);
	}
	// writes out the bitmap
	fseek(fp, (-3)*BLOCK_SIZE, SEEK_END);
	fwrite(&bitmap, BLOCK_SIZE, 3, fp);
	fclose(fp);
	free(parsed); free(dir);
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
