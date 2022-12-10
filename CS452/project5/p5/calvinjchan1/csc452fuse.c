/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/

/*
Filename: csc425fuse.c
Author: Ember Chan
Course: CSC452 Spr 2022
Description: Project 5 - simulates an I/O storage device
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//Max blocks not counting root or free space tracking
#define MAX_BLOCKS 10236

//First block of free space tracking
#define FS_BLOCK 10236

//Free Space Tracking
typedef struct fstracking {
	int nBlocks;
	char bitfield[BLOCK_SIZE*3-sizeof(int)];
} fstracking;

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

typedef struct blockData{
	csc452_root_directory root;
	int dirBlock;
	int dirIndex;
	csc452_directory_entry dirEntry;
	int fileBlock;
	int fileIndex;
	csc452_disk_block fileEntry;
} blockData;

typedef struct pathData {
	char *dname;
	char *fname;
	char *fext;
} pathData;

/* Break a path down in directory, filename and extension

Any of those fields not in the path will be an empty string
*/
void processPath(const char *path, pathData* pdata){
	int len = strlen(path);
	pdata->dname = malloc(len);
	pdata->fname = malloc(len);
	pdata->fext = malloc(len);
	*(pdata->dname) = '\0';
	*(pdata->fname) = '\0';
	*(pdata->fext) = '\0';
	sscanf(path, "/%[^/]/%[^.].%s", pdata->dname, pdata->fname, pdata->fext);
}

/*
Free all of the strings in the pathData struct
*/
void freePathData(pathData* pdata){
	free(pdata->dname);
	free(pdata->fname);
	free(pdata->fext);
}

/*
Return the lesser of two integers
*/
int min(int n1, int n2){
	if(n1 < n2){
		return n1;
	} else {
		return n2;
	}
}

/*
Return the greater of two integers
*/
int max(int n1, int n2){
	if(n1 > n2){
		return n1;
	} else {
		return n2;
	}
}

/*
Load the blocks specified by pdata into bdata,
if such blocks exist on disk.

If a file or directory doesn't exist, it's block will be -1
in the bdata struct
*/
void loadBlocks(int disk, pathData* pdata, blockData* bdata){
	bdata->dirBlock = -1;
	bdata->fileBlock = -1;
	//Get root
	lseek(disk, 0, SEEK_SET);
	read(disk, &(bdata->root), BLOCK_SIZE);
	//Search for directory
	int i = 0;
	while (i<bdata->root.nDirectories &&
		strcmp(bdata->root.directories[i].dname, pdata->dname) != 0){
		i++;
	}
	if (i == bdata->root.nDirectories){
		//We didn't find the directory
		return;
	}
	//load directory information
	bdata->dirIndex = i;
	bdata->dirBlock = bdata->root.directories[i].nStartBlock;
	lseek(disk, bdata->dirBlock * BLOCK_SIZE, SEEK_SET);
	read(disk, &(bdata->dirEntry), BLOCK_SIZE);

	i = 0;
	//Search for file
	while (i < bdata->dirEntry.nFiles && (
		strcmp(bdata->dirEntry.files[i].fname, pdata->fname) != 0 ||
		strcmp(bdata->dirEntry.files[i].fext, pdata->fext) != 0 )
		){
		i++;
	}
	if (i == bdata->dirEntry.nFiles){
		//We didn't find the file
		return;
	}

	//Load file information
	bdata->fileIndex = i;
	bdata->fileBlock = bdata->dirEntry.files[i].nStartBlock;
	lseek(disk, bdata->fileBlock * BLOCK_SIZE, SEEK_SET);
	read(disk, &(bdata->fileEntry), BLOCK_SIZE);
}

//Returns bit i in the given bitfield
int getBit(char *bitfield, int i){
	int character = i/8;
	int offset = 1 % 8;
	return (*(bitfield + character) >> offset) & 1;
}

//set bit i in the given bitfield to value
void setBit(char *bitfield, int i, int value){
	int character = i/8;
	int offset = 1 % 8;
	if(value == 0){
		*(bitfield + character) &= ~(1 << offset);
	} else {
		*(bitfield + character) |= (1 << offset);
	}
}

int availableBlocks(int disk){
	fstracking fs;
	lseek(disk, FS_BLOCK * BLOCK_SIZE, SEEK_SET);
	read(disk, &fs, sizeof(fstracking));
	return MAX_BLOCKS - fs.nBlocks;
}

//Returns a free block, and marks at filled in the free space tracking
//Disk is the file descriptor for the the disk
//-1 is returned if the storage is full
int getFreeBlock(int disk){
	fstracking fs;
	lseek(disk, FS_BLOCK * BLOCK_SIZE, SEEK_SET);
	read(disk, &fs, sizeof(fstracking));
	if(fs.nBlocks == MAX_BLOCKS){
		return -1;
	}
	//Find a free block where we can put the directory
	int i = 1;
	while(getBit(fs.bitfield, i) == 1){
		i++;
	}
	setBit(fs.bitfield, i, 1);
	fs.nBlocks ++;
	//Write bitfield back to memory
	lseek(disk, FS_BLOCK * BLOCK_SIZE, SEEK_SET);
	write(disk, &fs, sizeof(fstracking));
	return i;
}

//Mark the given block as no longer in use for the free space tracking
void freeBlock(int disk, int i){
	fstracking fs;
	lseek(disk, FS_BLOCK * BLOCK_SIZE, SEEK_SET);
	read(disk, &fs, sizeof(fstracking));
	setBit(fs.bitfield, i, 0);
	fs.nBlocks --;
	//Write bitfield back to memory
	lseek(disk, FS_BLOCK * BLOCK_SIZE, SEEK_SET);
	write(disk, &fs, sizeof(fstracking));

}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	//Seg fault on non-exist dir
	int res = 0;

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		pathData pdata;
		processPath(path, &pdata);
		int disk = open(".disk", O_RDONLY);
		blockData bdata;
		loadBlocks(disk, &pdata, &bdata);

		if (strcmp(pdata.dname, "") != 0 
			&& strcmp(pdata.fname, "") == 0
			&& bdata.dirBlock != -1){

			//If the path does exist and is a directory:
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		} else if (strcmp(pdata.fname, "") != 0 && bdata.fileBlock != -1){
			//If the path does exist and is a file:
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = bdata.dirEntry.files[bdata.fileIndex].fsize;
		} else {
			res = -ENOENT;
		}
		freePathData(&pdata);
		close(disk);
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
	//Root directory
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		int disk = open(".disk", O_RDONLY);
		csc452_root_directory root;
		read(disk, &root, BLOCK_SIZE);
		int i;
		for(i = 0; i<root.nDirectories; i++){
			filler(buf, root.directories[i].dname, NULL, 0);
		}
		close(disk);
	}
	else {
		//Subdirectory
		//Check that path exists and is a directory
		struct stat stbuf;
		if(csc452_getattr(path, &stbuf) == -ENOENT || 
			stbuf.st_mode != (S_IFDIR | 0755)){ //dir doesn't exist
			return -ENOENT;
		}
		//setup
		pathData pdata;
		processPath(path, &pdata);
		int disk = open(".disk", O_RDONLY);
		blockData bdata;
		loadBlocks(disk, &pdata, &bdata);

		//fillers
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		int i;
		for(i = 0; i<bdata.dirEntry.nFiles; i++){
			char *fname = bdata.dirEntry.files[i].fname;
			char *fext = bdata.dirEntry.files[i].fext;
			if(strcmp(fext, "") == 0){ //extensionless files
				filler(buf, fname, NULL, 0);
				continue;
			}
			char file[13];
			file[0] = '\0';
			strcat(file, fname);
			strcat(file, ".");
			strcat(file, fext);
			filler(buf, file, NULL, 0);
		}
		freePathData(&pdata);
		close(disk);
	}
	return 0;
}


/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	pathData pdata;
	processPath(path, &pdata);
	//Make sure we are creating a directory under the root
	if(strcmp(pdata.dname, "") == 0 || strcmp(pdata.fname, "") != 0){
		freePathData(&pdata);
		return -EPERM;
	}
	//Make sure directory name is good
	if (strlen(pdata.dname) > MAX_FILENAME){
		freePathData(&pdata);
		return -ENAMETOOLONG;
	}
	//Make sure directory doesn't already exist
	struct stat stbuf;
	if (csc452_getattr(path, &stbuf) != -ENOENT){
		freePathData(&pdata);
		return -EEXIST;
	}
	int disk = open(".disk", O_RDWR);

	//Get root node
	lseek(disk, 0, SEEK_SET);
	csc452_root_directory  root;
	read(disk, &root, BLOCK_SIZE);

	//Make sure we have room for a new directory
	if(root.nDirectories == MAX_DIRS_IN_ROOT){
		freePathData(&pdata);
		close(disk);
		return -ENOSPC;
	}
	if(availableBlocks(disk) < 1){
		freePathData(&pdata);
		close(disk);
		return -ENOSPC;
	}

	//Make directory
	int dirBlock = getFreeBlock(disk);

	//Set values for new directory
	root.nDirectories ++;
	strcpy(root.directories[root.nDirectories - 1].dname, pdata.dname);
	root.directories[root.nDirectories - 1].nStartBlock = dirBlock;

	csc452_directory_entry dentry;
	dentry.nFiles = 0;

	printf("saving with %d dirs\n", root.nDirectories);
	//Write back to disk
	lseek(disk, 0, SEEK_SET);
	write(disk, &root, BLOCK_SIZE);
	lseek(disk, dirBlock*BLOCK_SIZE, SEEK_SET);
	write(disk, &dentry, BLOCK_SIZE);

	freePathData(&pdata);
	close(disk);
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
	pathData pdata;
	processPath(path, &pdata);

	if(strcmp(pdata.fname, "") == 0){
		freePathData(&pdata);
		return -EPERM;
	}
	struct stat stbuf;
	if(csc452_getattr(path, &stbuf) != -ENOENT){
		freePathData(&pdata);
		return -EEXIST;
	}
	if (strlen(pdata.fname) > MAX_FILENAME || strlen(pdata.fext) > MAX_EXTENSION){
		freePathData(&pdata);
		return -ENAMETOOLONG;
	}

	int disk = open(".disk", O_RDWR);
	blockData bdata;
	loadBlocks(disk, &pdata, &bdata);
	if(bdata.dirBlock == -1){
		//Directory doesn't exist
		freePathData(&pdata);
		close(disk);
		return -ENOTDIR;
	}
	if(bdata.dirEntry.nFiles == MAX_FILES_IN_DIR || availableBlocks(disk) < 1){
		//No more room for for more files
		freePathData(&pdata);
		close(disk);
		return -ENOSPC;
	}
	bdata.dirEntry.nFiles ++;
	int fileIndex = bdata.dirEntry.nFiles - 1;
	strcpy(bdata.dirEntry.files[fileIndex].fname, pdata.fname);
	strcpy(bdata.dirEntry.files[fileIndex].fext, pdata.fext);
	bdata.dirEntry.files[fileIndex].fsize = 0;

	int fileBlock = getFreeBlock(disk);
	bdata.dirEntry.files[fileIndex].nStartBlock = fileBlock;
	lseek(disk, bdata.dirBlock *BLOCK_SIZE, SEEK_SET);
	write(disk, &bdata.dirEntry, BLOCK_SIZE);

	//Make 1st block of file
	csc452_disk_block file;
	file.nNextBlock = 0;
	lseek(disk, fileBlock * BLOCK_SIZE, SEEK_SET);
	write(disk, &file, BLOCK_SIZE);

	//Don't get yelled at by the complier
	(void) mode;
	(void) dev;

	freePathData(&pdata);
	close(disk);

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
	struct stat stbuf;
	if(csc452_getattr(path, &stbuf) != 0){
		return -ENOENT;
	}
	//check that size is > 0
	if (size <= 0){
		return -EINVAL;
	}
	//check that we are not a directory
	if(strcmp(path, "/") == 0 || stbuf.st_mode == (S_IFDIR | 0755)){
		return -EISDIR;
	}
	//check that offset is <= to the file size
	int disk = open(".disk", O_RDONLY);
	pathData pdata;
	processPath(path, &pdata);
	blockData bdata;
	loadBlocks(disk, &pdata, &bdata);
	size_t fileSize = bdata.dirEntry.files[bdata.fileIndex].fsize;
	if(offset > fileSize){
		freePathData(&pdata);
		close(disk);
		return -EINVAL;
	}
	//read in data
	int startBlock = offset/MAX_DATA_IN_BLOCK;
	int inBlockOffset = offset%MAX_DATA_IN_BLOCK;
	int i = 0;
	csc452_disk_block cur = bdata.fileEntry;
	while (i != startBlock){
		lseek(disk, cur.nNextBlock * BLOCK_SIZE, SEEK_SET);
		read(disk, &cur, BLOCK_SIZE);
		i++;
	}
	int bytesToRead = min(size, fileSize-offset);
	int bytesLeft = bytesToRead;
	int j;
	int bufOffset = 0;
	while(bytesLeft != 0){
		j = min(bytesLeft, MAX_DATA_IN_BLOCK - inBlockOffset);
		memcpy(buf + bufOffset, cur.data + inBlockOffset, j);
		bufOffset += j;
		bytesLeft -= j;
		lseek(disk, cur.nNextBlock * BLOCK_SIZE, SEEK_SET);
		read(disk, &cur, BLOCK_SIZE);
		inBlockOffset = 0;
	}
	freePathData(&pdata);
	close(disk);
	return bytesToRead;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	//write data
	//return success, or error
	
	//check to make sure path exists
	struct stat stbuf;
	if(csc452_getattr(path, &stbuf) != 0){
		return -ENOENT;
	}
	//check that size is > 0
	if (size <= 0){
		return -EINVAL;
	}
	//check that we are not a directory
	if(strcmp(path, "/") == 0 || stbuf.st_mode == (S_IFDIR | 0755)){
		return -EISDIR;
	}
	//check that offset is <= to the file size
	int disk = open(".disk", O_RDWR);
	pathData pdata;
	processPath(path, &pdata);
	blockData bdata;
	loadBlocks(disk, &pdata, &bdata);
	size_t fileSize = bdata.dirEntry.files[bdata.fileIndex].fsize;
	if(offset > fileSize){
		freePathData(&pdata);
		close(disk);
		return -EINVAL;
	}
	//Check that we have enough space
	int spaceInFileBlock = BLOCK_SIZE-fileSize%BLOCK_SIZE;

	int sizeNeeded = size + offset - fileSize;
	if((sizeNeeded - spaceInFileBlock) % BLOCK_SIZE == 0){
		sizeNeeded = sizeNeeded + BLOCK_SIZE;
	}
	if(sizeNeeded > availableBlocks(disk) * BLOCK_SIZE + spaceInFileBlock){
		freePathData(&pdata);
		close(disk);
		return -EFBIG;
	}

	int newFileSize = max(fileSize, offset+size);

	//Write down new file size
	bdata.dirEntry.files[bdata.fileIndex].fsize = newFileSize;
	lseek(disk, bdata.dirBlock * BLOCK_SIZE, SEEK_SET);
	write(disk, &(bdata.dirEntry), BLOCK_SIZE);

	//Find place to start writing
	int startBlock = offset/MAX_DATA_IN_BLOCK;
	int inBlockOffset = offset%MAX_DATA_IN_BLOCK;
	int i = 0;
	csc452_disk_block cur = bdata.fileEntry;
	int curBlock = bdata.dirEntry.files[bdata.fileIndex].nStartBlock;
	while (i != startBlock){
		curBlock = cur.nNextBlock;
		lseek(disk, curBlock * BLOCK_SIZE, SEEK_SET);
		read(disk, &cur, BLOCK_SIZE);
		i++;
	}
	//Write loop
	int bytesLeft = size;
	int j;
	int bufOffset = 0;
	int fileOffset = offset;
	csc452_disk_block nextBlock;
	while(bytesLeft != 0){
		j = min(bytesLeft, MAX_DATA_IN_BLOCK - inBlockOffset);
		memcpy(cur.data + inBlockOffset, buf + bufOffset, j);
		bufOffset += j;
		bytesLeft -= j;
		fileOffset += j;

		//Get a new block if we're at the end of the linked list
		if(cur.nNextBlock == 0){
			nextBlock = cur;
			cur.nNextBlock = getFreeBlock(disk);
		} else {
			lseek(disk, cur.nNextBlock * BLOCK_SIZE, SEEK_SET);
			read(disk, &nextBlock, BLOCK_SIZE);
		}

		//Write block back
		printf("Wrote to block: %d\n", curBlock);
		lseek(disk, curBlock * BLOCK_SIZE, SEEK_SET);
		write(disk, &cur, BLOCK_SIZE);

		//Get next block
		curBlock = cur.nNextBlock;
		cur = nextBlock;
		inBlockOffset = 0;
	}

	freePathData(&pdata);
	close(disk);

	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	struct stat stbuf;
	if(strcmp(path, "/") == 0){
		return -EPERM;
	}
	if(csc452_getattr(path, &stbuf) == -ENOENT){
		return -ENOENT;
	}
	if(stbuf.st_mode != (S_IFDIR | 0755)){
		return -ENOTDIR;
	}

	pathData pdata;
	processPath(path, &pdata);
	int disk = open(".disk", O_RDWR);
	blockData bdata;
	loadBlocks(disk, &pdata, &bdata);

	if(bdata.dirEntry.nFiles > 0){ //Directory isn't empty
		freePathData(&pdata);
		close(disk);
		return -ENOTEMPTY;
	}
	freeBlock(disk, bdata.dirBlock);
	int entrySize = sizeof(struct csc452_directory);
	memmove((bdata.root.directories + bdata.dirIndex),
		(bdata.root.directories+1+bdata.dirIndex),
		(bdata.root.nDirectories - bdata.dirIndex - 1)*entrySize);
	bdata.root.nDirectories --;
	lseek(disk, 0, SEEK_SET);
	write(disk, &(bdata.root), BLOCK_SIZE);

	freePathData(&pdata);
	close(disk);
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
	//Check that file exists
	struct stat stbuf;
	if(csc452_getattr(path, &stbuf) == -ENOENT){
		return -ENOENT;
	}
	//Make sure that it is indeed a file
	if(stbuf.st_mode != (S_IFREG | 0666)){
		return -EISDIR;
	}
	//Delete file
	int disk = open(".disk", O_RDWR);
	pathData pdata;
	processPath(path, &pdata);
	blockData bdata;
	loadBlocks(disk, &pdata, &bdata);
	size_t fileSize = bdata.dirEntry.files[bdata.fileIndex].fsize;

	//need to delete fileSize/MAX_DATA_IN_BLOCK + 1 entires
	int i;
	csc452_disk_block cur = bdata.fileEntry;
	int blockToFree = bdata.fileBlock;
	for(i = 0; i < fileSize/MAX_DATA_IN_BLOCK + 1; i++){
		freeBlock(disk, blockToFree);
		blockToFree = cur.nNextBlock;
		lseek(disk, blockToFree * BLOCK_SIZE, SEEK_SET);
		read(disk, &cur, BLOCK_SIZE);
	}
	//Remove entry in directory array
	int entrySize = sizeof(struct csc452_file_directory);
	i = bdata.fileIndex;
	memmove((bdata.dirEntry.files + i), (bdata.dirEntry.files+1+i),
		(bdata.dirEntry.nFiles - i - 1)*entrySize);
	bdata.dirEntry.nFiles --;
	lseek(disk, bdata.dirBlock * BLOCK_SIZE, SEEK_SET);
	write(disk, &bdata.dirEntry, BLOCK_SIZE);

	freePathData(&pdata);
	close(disk);
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
