/**
 * @file csc452fuse.c
 * @author Nicholas Leluan
 * @brief 
 * @version 0.1
 * @date 2022-04-24
 * 
 * @copyright Copyright (c) 2022
 * 
 */

/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/

#define	FUSE_USE_VERSION 26

#include <fuse.h> //NEED TO COMMENT THIS BACK IN IN THE VM
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

/* 
WHAT I ADD
*/
long FILE_SIZE;
long getFileSize();
long isDirectoryInSystem(const char *directory);
void removeDirFromRootDirectories(const char *dirToRemove);
long isFileInSystem(const char *directory, const char*file, const char *ext);
struct csc452_file_directory * getFileDirectory(long fileStartBlock);
struct csc452_directory_entry * getDirectoryEntry(long dirStartBlock);
struct csc452_root_directory * getRoot();
struct csc452_disk_block * getBitMap();
struct csc452_disk_block * getDiskBlock(long nStartBlock);
void setBitInBitMap(long index,int val);
void zeroOutChunkOfFileStartingAtIndex(int index);

int getNextFreeBlock();

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

#define fileSize  (5 << 20) // 5MB
#define bitsInFile  (fileSize / BLOCK_SIZE)
#define  bytesInFile  (bitsInFile / 8)



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

#define TOTAL_BLOCKS (MAX_DIRS_IN_ROOT*MAX_FILES_IN_DIR)

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

// solomente files 
struct csc452_disk_block
{
	//Space in the block can be used for actual data storage.
	char data[MAX_DATA_IN_BLOCK];
	//Link to the next block in the chain
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
	int res = 0;
	char directory[100]; // path leading up to the pwd
	char filename[100]; // the name of file
	char extension[100]; // if this is missing, then 'filename' is a directory!
	// root path
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755; /*File type and mode*/
		stbuf->st_nlink = 2; /* number of hard links - how many other directories this
							file can be found in:: is this 2 because its in root and 
							some other directory???*/
	} 
	// not root
	else  {
		// retScan == 3 -> /path/filename.extension
		// retScan == 1 -> /[root]/directory 
		int retScan = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		// handle error here for res? if it is <= 1?
		int isPathValid; // if > 0 will be long stark block value
		//If the path DOES exist and is a directory:
		if(retScan == 1){
			if((isPathValid = isDirectoryInSystem(directory)) == -ENOENT){
				return isPathValid; // will be -ENOENT here
			}
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		}
		//If the path DOES exist and is a file:
		else if(retScan == 3){
			if((isPathValid = isFileInSystem(directory,filename,extension)) == -ENOENT){
				return isPathValid; // will be -ENOENT here
			}
			//struct csc452_file_directory *file = getFileDirectory(isPathValid);
			struct csc452_directory_entry *dirEntry = getDirectoryEntry(isDirectoryInSystem(directory));
			int fileIndex;
			for(fileIndex = 0; fileIndex < MAX_FILES_IN_DIR; fileIndex++){
				// found a valid match in the directory (should be there)
				if(strcmp(extension, dirEntry->files[fileIndex].fext) == 0 &&
						strcmp(filename,dirEntry->files[fileIndex].fname) == 0 &&
							dirEntry->files[fileIndex].nStartBlock > 0){
								break; // fileIndex will now be where we want to make changes
							}
			}
			stbuf->st_mode = S_IFREG | 0666; // regular file | full permissons
			stbuf->st_nlink = 2;
			//stbuf->st_size = file->fsize; // we will need the struct to get this if file
			stbuf->st_size = dirEntry->files[fileIndex].fsize;
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

	(void) offset;
	(void) fi;

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		struct csc452_root_directory *root = getRoot();
		for(int i = 0; i < MAX_DIRS_IN_ROOT; i++){
			// removing a dir sets this to -1; 0 is unintialized
			if (root->directories[i].nStartBlock > 0)
			{
				filler(buf,root->directories[i].dname,NULL,0);
			}
		}

	}
	else {
		// FROM HERE, confirm directory path is valid, if so, list 
		// any of the files the directory may have 
		char dirBuffer[100];
		char fileBuffer[100];
		char extBuffer[100];
		int retScan  = sscanf(path, "/%[^/]/%[^.].%s", dirBuffer, fileBuffer, extBuffer);
		if(retScan == 1){
			long dirStartBlock = isDirectoryInSystem(dirBuffer);
			if(dirStartBlock == -ENOENT){
				return -ENOENT;
			}
			struct csc452_directory_entry *dirEntry = getDirectoryEntry(dirStartBlock);
			for(int x = 0; x < MAX_FILES_IN_DIR;x++){
				if(dirEntry->files[x].nStartBlock > 0){
					char buffer[100];
					snprintf(buffer,sizeof(buffer),"%s.%s",dirEntry->files[x].fname,dirEntry->files[x].fext);
					filler(buf,buffer,NULL,0);
				}
			}
		}
		//path leads to a file or a directory not in root
		else{
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
	char dirBuffer[100];
	char fileBuffer[100];
	char extBuffer[100];
	int retScan = sscanf(path, "/%[^/]/%[^.].%s", dirBuffer, fileBuffer, extBuffer);
	// 1 = /dir or /dir/
	// 2 = /dir/foo --> illegal for mkdir
	// 3 = /dir/foo.ext --> illegal for mkdir
	if(retScan > 1){
		//directory is not under the root or is a file
		return -EPERM;
	}
	if(strlen(dirBuffer) > MAX_FILENAME){
		// directory name is too large (> 8)
		return -ENAMETOOLONG;
	}
	// check if the directory is already in the system
	int check = isDirectoryInSystem(dirBuffer);
	if(check != -ENOENT){
		return -EEXIST; // directory is already in the system
	}
	// FILE *fp = fopen(".disk","rb+");
	struct csc452_root_directory *root = getRoot();
	struct csc452_directory_entry *newDir = malloc(sizeof(csc452_directory_entry));
	//struct csc452_directory *d = malloc(sizeof(csc452_directory));
	if(!newDir){
		fprintf(stderr,"Error creating new directory struct\n");
		exit(0);
	}
	int nextFreeIndex = getNextFreeBlock();
	// Should we check if this is going to write over root or bitmap?
	if(nextFreeIndex < 0){
		return -ENOMEM;
	}
	// set and write the new directory in disk
	int x;
	// this searches the directories array and finds the
	// next empty space if there is one
	for(x = 0; x < MAX_DIRS_IN_ROOT; x++){
		if(root->directories[x].nStartBlock == 0 || root->directories[x].nStartBlock == -1){
			break;
		}
	}
	if(x == MAX_DIRS_IN_ROOT){
		return -ENOMEM;
	}
	root->directories[x].nStartBlock = nextFreeIndex * BLOCK_SIZE; // IS THIS OFF BY ONE?
	strncpy(root->directories[x].dname,dirBuffer,MAX_FILENAME);
	root->nDirectories++;
	newDir->nFiles = 0;
	FILE *fp = fopen(".disk","rb+");
	fseek(fp,(nextFreeIndex * BLOCK_SIZE),SEEK_SET);
	//zeroOutChunkOfFileStartingAtIndex(nextFreeIndex*BLOCK_SIZE);
	fwrite(newDir,sizeof(struct csc452_directory_entry),1,fp);
	// re-write the root back into disk
	fseek(fp,0,SEEK_SET); //get to the beginning
	fwrite(root,sizeof(csc452_root_directory),1,fp);
	setBitInBitMap(nextFreeIndex,1);
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
	//long isDirInSystem;
	char dirBuffer[100];
	char fileBuffer[100];
	char extBuffer[100];
	int check  = sscanf(path, "/%[^/]/%[^.].%s", dirBuffer, fileBuffer, extBuffer);
	// happens with /file.ext
	if(check == 1 ){
		return -EPERM;
	}
	// strings exceed the 8.3 length (file.ext)
	if(strlen(dirBuffer) > MAX_FILENAME || strlen(extBuffer) > MAX_EXTENSION){
		return -ENAMETOOLONG;
	}

	int dirStart = isDirectoryInSystem(dirBuffer); // returns start block if found
	struct csc452_directory_entry *dirEntry = getDirectoryEntry(dirStart);
	int freeInDirFiles;
	for(freeInDirFiles = 0; freeInDirFiles < MAX_FILES_IN_DIR; freeInDirFiles++){
		// sentinel value; the file index is free if the value is 0 or negative (-1)
		if(dirEntry->files[freeInDirFiles].nStartBlock == 0 ||
			dirEntry->files[freeInDirFiles].nStartBlock == -1){
			break;
		}
	}
	if(freeInDirFiles > MAX_FILES_IN_DIR){
		return -ENOMEM;
	}
	FILE *fp = fopen(".disk","rb+");
	int nextFreeBlock = getNextFreeBlock(); // this will be the index where there
	// was a 0 in the bit map, needs to be multiplied by BLOCK_SIZE
	//newFile->nStartBlock = nextFreeBlock * BLOCK_SIZE;
	struct csc452_disk_block *newBlock = malloc(sizeof(csc452_disk_block));
	fseek(fp,nextFreeBlock * BLOCK_SIZE,SEEK_SET);
	fwrite(newBlock,sizeof(csc452_disk_block),1,fp);
	setBitInBitMap(nextFreeBlock,1);
	dirEntry->files[freeInDirFiles].nStartBlock = nextFreeBlock * BLOCK_SIZE;
	//dirEntry->files[freeInDirFiles].fsize = 0; // this will need to change when we write the file
	strncpy(dirEntry->files[freeInDirFiles].fname,fileBuffer,MAX_FILENAME);
	strncpy(dirEntry->files[freeInDirFiles].fext,extBuffer,MAX_EXTENSION);
	dirEntry->nFiles++;
	fseek(fp,dirStart,SEEK_SET);
	fwrite(dirEntry,sizeof(csc452_directory_entry),1,fp);
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
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	char dirBuffer[100];
	char fileBuffer[100];
	char extBuffer[100];
	int check = sscanf(path, "/%[^/]/%[^.].%s", dirBuffer, fileBuffer, extBuffer);
	//check to make sure path exists
	if(check != 3){
		return -EISDIR;
	}
	//check that size is > 0
	if(check <= 0){
		return -ENOENT; // what dowe return here?
	}

	//check to make sure path exists
	int fileCheck = isFileInSystem(dirBuffer,fileBuffer,extBuffer);
	if (fileCheck == -ENOENT){
		return -EEXIST;
	}
	struct csc452_directory_entry *dirEntry = getDirectoryEntry(isDirectoryInSystem(dirBuffer));
	int fileLoc;
	for(fileLoc = 0; fileLoc < MAX_FILES_IN_DIR; fileLoc++){
		if(strcmp(extBuffer, dirEntry->files[fileLoc].fext) == 0 &&
						strcmp(fileBuffer,dirEntry->files[fileLoc].fname) == 0 &&
							dirEntry->files[fileLoc].nStartBlock > 0){
								break; // fileIndex will now be where we want to make changes
							}
	}
	//check that offset is <= to the file size
	if(dirEntry->files[fileLoc].fsize < offset){
		return -EFBIG;
	}
	// this will be the first block, may be chained to other blocks 
	struct csc452_disk_block *currFileBlock = getDiskBlock(dirEntry->files[fileLoc].nStartBlock);
	//read in data
	int write = 0;
	//char subString[size - offset];
	//printf("cfb: %lu de->%lu\n",currFileBlock->nNextBlock,dirEntry->files[fileLoc].nStartBlock);
	while(1){
		// go through data until you reach null or max data that can be stored
		// in block
		for(int x = 0; x < MAX_DATA_IN_BLOCK; x++){
			// make sure that we arent writing NULL and that our writes are lower than
			// our size
			//currFileBlock->data[x+offset] != '\0' && 
			if(write < size){
				buf[write] = currFileBlock->data[x];
				write++;
			}
		}
		offset = 0;
		currFileBlock = getDiskBlock(currFileBlock->nNextBlock);
		if(currFileBlock->nNextBlock <= 0 || write > size){
			break;
		}
		// might need to check here if the currFileBlock exists and break from
		// outer loop if it does not
	}
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
	char dirBuffer[100];
	char fileBuffer[100];
	char extBuffer[100];
	sscanf(path, "/%[^/]/%[^.].%s", dirBuffer, fileBuffer, extBuffer);

	//check to make sure path exists -  IS THIS NEEDED?
	int check = isFileInSystem(dirBuffer,fileBuffer,extBuffer);
	if (check == -ENOENT){
		return -EEXIST;
	}
	//check that size is > 0
	if(size <= 0){
		return -ENODATA; 
	}
	//check that offset is <= to the file size
	long dirLoc = isDirectoryInSystem(dirBuffer);
	struct csc452_directory_entry *dirEntry = getDirectoryEntry(dirLoc);
	// loop through bit by bit of the data and write the corresponding index of 
	// passed in buffer;
	int blockLoc;
	int fileIndex; // will be the index of file on the directory files array
	// us this to update values to the entry and write back when done.
	for(fileIndex = 0; fileIndex < MAX_FILES_IN_DIR; fileIndex++){
				// found a valid match in the directory (should be there)
				if(strcmp(extBuffer, dirEntry->files[fileIndex].fext) == 0 &&
						strcmp(fileBuffer,dirEntry->files[fileIndex].fname) == 0 &&
							dirEntry->files[fileIndex].nStartBlock > 0){
								break; // fileIndex will now be where we want to make changes
							}
			}
	int x = 0;
	int writes = 0; 
	struct csc452_disk_block  *fileData = getDiskBlock(dirEntry->files[fileIndex].nStartBlock);
	long currNStartBlock = dirEntry->files[fileIndex].nStartBlock;
	if(offset > dirEntry->files[fileIndex].fsize){
		return -EFBIG;
	}
	// has null terminator? dont write?
	while(writes < size){
		// reached the end of the data block; need a new one
		if((x+offset) > MAX_DATA_IN_BLOCK){
			// if this returns negative, we have run out of disk space
			if((blockLoc = getNextFreeBlock()) < 0){
				return -ENOSPC;
			}
			// will now start at the beginning of a file;
			x = 0;
			offset = 0;
			// UPDATE current disk block and write data back to .disk
			setBitInBitMap(blockLoc,1);
			fileData->nNextBlock = blockLoc * BLOCK_SIZE;
			FILE *fpInner = fopen(".disk","rb+");
			fseek(fpInner,currNStartBlock,SEEK_SET);
			fwrite(fileData,sizeof(csc452_disk_block),1,fpInner);
			fclose(fpInner);
			//GET next disk block of file
			fileData = getDiskBlock(blockLoc * BLOCK_SIZE);
			currNStartBlock = blockLoc * BLOCK_SIZE;
			
		}
		// x starts at zero; offset starts at passed in value; 
		// if we need to create another block, offset becomes 0
		fileData->data[x+offset] = buf[writes]; // write to the data bffer of current file disk block
		dirEntry->files[fileIndex].fsize++; // increase the size of the file metadata
		writes++; // total writes, compares in loop stops when we have written all data 
		x++; // index of where we need to write in the data
	}
	FILE *fp = fopen(".disk","rb+");
	// UPDATE THE FILE DIRECTORY
	fseek(fp,dirLoc,SEEK_SET);
	fwrite(dirEntry,sizeof(struct csc452_directory_entry),1,fp);
	// UPDATE THE FILE BLOCK -> should be last one written to if multiple blocks
	// were needed
	fseek(fp,currNStartBlock,SEEK_SET);
	fwrite(fileData,sizeof(csc452_disk_block),1,fp);
	fclose(fp);
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path){
	long isDirInSystem;
	char dirBuffer[100];
	char fileBuffer[100];
	char extBuffer[100];
	if((sscanf(path, "/%[^/]/%[^.].%s", dirBuffer, fileBuffer, extBuffer)) != 1){
			return -ENOTDIR; // passed in path does not lead to directory
	}
	isDirInSystem = isDirectoryInSystem(dirBuffer);
	struct csc452_directory_entry *dirEntry = getDirectoryEntry(isDirInSystem);
	if(dirEntry->nFiles > 0){
		return -ENOTEMPTY; // directory is not empty; cannot delete
	}
	removeDirFromRootDirectories(dirBuffer);
	setBitInBitMap(isDirInSystem/BLOCK_SIZE,0);
	//zeroOutChunkOfFileStartingAtIndex(isDirInSystem);
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{

    (void) path;
	char dirBuffer[100];
	char fileBuffer[100];
	char extBuffer[100];
	int check  = sscanf(path, "/%[^/]/%[^.].%s", dirBuffer, fileBuffer, extBuffer);
	if(check != 3){
		return -EISDIR;
	}
	if( (isFileInSystem(dirBuffer,fileBuffer,extBuffer) <= 0)){
		return -ENOENT;
	}
	long startDir = isDirectoryInSystem(dirBuffer); // returns the block where the
	// directory is on disk
	struct csc452_directory_entry *dirEntry = getDirectoryEntry(startDir);
	int fileIndex;
	long startIndex = 0;
	for(fileIndex = 0; fileIndex < MAX_FILES_IN_DIR; fileIndex++){
		if(strcmp(extBuffer, dirEntry->files[fileIndex].fext) == 0 &&
					strcmp(fileBuffer,dirEntry->files[fileIndex].fname) == 0 &&
						dirEntry->files[fileIndex].nStartBlock > 0){
							break; // fileIndex will now be where we want to make changes
						}
	}
	startIndex = dirEntry->files[fileIndex].nStartBlock;
	//int savedFileIndex = dirEntry->files[fileIndex].nStartBlock;
	struct csc452_disk_block *fileBlock = malloc(sizeof(csc452_disk_block));
	int flag = 1;
	while(flag){
		fileBlock = getDiskBlock(startIndex);
		setBitInBitMap(startIndex/BLOCK_SIZE,0); // free the space in the bitmap
		startIndex = fileBlock->nNextBlock;
		if(startIndex <= 0){
			flag = 0;
		}
	}
	dirEntry->files[fileIndex].nStartBlock = -1;
	dirEntry->files[fileIndex].fsize = 0;
	
	dirEntry->nFiles--;
	FILE *fp = fopen(".disk","rb+");
	fseek(fp,startDir,SEEK_SET);
	fwrite(dirEntry,sizeof(csc452_directory_entry),1,fp);
	fclose(fp);
    return 0;
}


/**
 * @brief Will check if the passed in path leads to a valid file or directory
 * 
 * @param directory - the passed in directory to find in root
 * @return  -EONENT if directory not found; return location of file if found
 */
long isDirectoryInSystem(const char *directory){
	struct csc452_root_directory *root = getRoot();
	for(int i = 0; i < MAX_DIRS_IN_ROOT;i++){
		// found a match;
		if(strcmp(root->directories[i].dname,directory) == 0 && 
					root->directories[i].nStartBlock > 0 ){
			return root->directories[i].nStartBlock;
		}
	}
	// did not find the directory
	return -ENOENT;
}
/**
 * @brief Will check if the passed in path leads to a valid file or directory
 * 
 * @param type - will either be 2 or 3; 2 means directory 3 means file
 * @param path - the passed in path to check 
 * @return -EONENT if file/directory not found; long val of the start block of file
 */
long isFileInSystem(const char *directory, const char*file, const char *ext){
	//csc452_root_directory *root = getRoot();
	int res = -ENOENT; // this will only change if dir and file exist
	int dirStartBlock = isDirectoryInSystem(directory);
	struct csc452_directory_entry *dirEntry = malloc(sizeof(csc452_directory_entry));
	//printf("SEARCHING FOR: %s in %s dir",file,directory);
	dirEntry = getDirectoryEntry(dirStartBlock);
	//printf("Dir # files: %d\n",dirEntry->nFiles);
	// handle if x is too large? Shouldnt be becasue we confirmed directory is in System..
	int f,e; //filename,extension
	for(int i = 0; i < dirEntry->nFiles; i++){
		//printf("COMP: %s.%s with %s.%s\n",file,ext,dirEntry->files[i].fname,dirEntry->files[i].fext);
		f = strcmp(dirEntry->files[i].fname,file);
		e = strcmp(dirEntry->files[i].fext,ext);
		// file name and extension match
		if(f == 0 && e == 0){
			res = dirEntry->files[i].nStartBlock;
			break;
		}
	}
	return res;

}

/**
 * @brief Get the Root object
 * 
 * @return csc452_root_directory 
 */
struct csc452_root_directory * getRoot(){
	FILE *fp = fopen(".disk","rb+");
	if(!fp){
		exit(1);
	}
	struct csc452_root_directory *root = malloc(sizeof(csc452_root_directory));
	fread(root,sizeof(csc452_root_directory),1,fp);
	// there is not a root directory, create one and set up bitmap
	if(root->nDirectories == 0){
		//root = malloc(sizeof(csc452_root_directory));
		if(root == NULL){
			fprintf(stderr,"Error in allocating memory for root directory\n");
			exit(0);
		}
		struct csc452_disk_block *block;
		for(int i = 3; i > 0 ; i--){
			block = malloc(sizeof(csc452_disk_block));
			block->nNextBlock = bytesInFile - (BLOCK_SIZE * i); // this means last block will be == bytesInBlock
			if(i == 3){
				memset(block->data,0,MAX_DATA_IN_BLOCK);
				block->data[0] = 1; //the root
			}
			else if(i == 2){
				memset(block->data,0,MAX_DATA_IN_BLOCK);
			}
			else if(i == 1){
				// last block is half in size!
				memset(block->data,0,MAX_DATA_IN_BLOCK/2);
				block->data[(MAX_DATA_IN_BLOCK/2)-1] = 1; // bitmap block #1
			 	block->data[(MAX_DATA_IN_BLOCK/2)-2] = 1; // bitmap block #2
			 	block->data[(MAX_DATA_IN_BLOCK/2)-3] = 1; // bitmap block #3
			}
			fseek(fp,-(BLOCK_SIZE*i),SEEK_END);
			fwrite(block,sizeof(csc452_disk_block),1,fp);
		}
	}
	// there is a root directory, simply return it
	fclose(fp);
	return root;
}

/**
 * @brief Get the Directory Entry object
 * 
 * @param dirStartBlock 
 * @return struct csc452_directory_entry 
 */
struct csc452_directory_entry * getDirectoryEntry(long dirStartBlock){
	FILE *fp = fopen(".disk","rb+");
	struct csc452_directory_entry *dir = malloc(sizeof(csc452_directory_entry));
	// this should get our fp to the beginning byte of the directory we want to read
	fseek(fp,dirStartBlock,SEEK_SET);
	fread(dir,sizeof(csc452_directory_entry),1,fp);
	fclose(fp);
	return dir;

}

/**
 * @brief Get the File Directory object
 * 
 * @param fileStartBlock long value of where the file's block starts on the .disk file
 * @return struct csc452_file_directory 
 */
struct csc452_file_directory * getFileDirectory(long fileStartBlock){
	FILE *fp = fopen(".disk","rb");
	struct csc452_file_directory *file = malloc(sizeof(struct csc452_file_directory));
	int check = fseek(fp,fileStartBlock,SEEK_SET);
	if(check != 0){
		fprintf(stderr,"Something went wrong using fseek() in getFileDirectory\n");
		exit(0);
	}
	fread(file,sizeof(struct csc452_file_directory),1,fp);
	fclose(fp);
	return file;
}

/**
 * @brief Get the Next Free Block index in the bitmap
 * 
 * @return int value of the index at which an entire BLOCK is represented
 */
int getNextFreeBlock(){
	FILE *fp = fopen(".disk","rb+");
	int index = 0;
	int block = 1; // [1,2,3] blocks; 3 is half full!
	struct csc452_disk_block *bitmap = getBitMap();
	//char *bits;
	int counter;
	while(1){
		//bits = bitmap->data;
		if(block != 3){
			counter = MAX_DATA_IN_BLOCK;
		}else{
			counter = (MAX_DATA_IN_BLOCK / 2); // this block is half full
		}
		for(int i = 0; i < counter;i++){
			// rememebr the last block is only half full!
			if(bitmap->data[i] == 0){
				fclose(fp);
				return index;
			}
			index++;
		}
		block++;
		// we went through the enture linked bitmap and did not find an
		// empty space.
		if (block > 3){
			break;
		}
		// did not find in this block, move to next
		fseek(fp,bitmap->nNextBlock,SEEK_SET); 
		fread(bitmap,sizeof(csc452_disk_block),1,fp);

	}
	// shouldnt get here, but if we do, a free space wasnt found
	fclose(fp);
	return -1;
}

/**
 * @brief Get the Bit Map object
 * 
 * @return struct csc452_disk_block* 
 */
struct csc452_disk_block *getBitMap(){
	FILE *fp = fopen(".disk","rb+");
	struct csc452_disk_block *bm = malloc(sizeof(csc452_disk_block));
	fseek(fp,-(BLOCK_SIZE*3),SEEK_END);
	fread(bm,sizeof(csc452_disk_block),1,fp);
	fclose(fp);
	return bm;
}

/**
 * @brief Set the Bit In Bit Map object
 * 
 * @param index 
 * @param val 
 * TODO: NOT 100% this works
 */
void setBitInBitMap(long index,int val){
	FILE *fp = fopen(".disk","rb+");
	struct csc452_disk_block *block = malloc(sizeof(csc452_disk_block));
	int blockOffset,seek;
	// max_data_in_block = 504 
	if(index < MAX_DATA_IN_BLOCK){
		blockOffset = 1;
		seek = 3;
	}else if(index < MAX_DATA_IN_BLOCK *2){
		blockOffset = 2;
		seek = 2;
	}else{
		blockOffset = 3;
		seek = 1;
	}
	fseek(fp,-(seek *BLOCK_SIZE),SEEK_END);
	fread(block,sizeof(csc452_disk_block),1,fp);
	block->data[(index - (BLOCK_SIZE * (blockOffset-1)))] = val;
	fseek(fp,-(seek *BLOCK_SIZE),SEEK_END);
	fwrite(block,sizeof(csc452_disk_block),1,fp);
	fclose(fp);
	
}

/**
 * @brief 
 * 
 * @param dirToRemove 
 */
void removeDirFromRootDirectories(const char *dirToRemove){
	FILE *fp = fopen(".disk","rb+");
	struct csc452_root_directory *root = getRoot();
	int index = 0;
	for(int i = 0; i < MAX_DIRS_IN_ROOT; i++){
		if(strcmp(dirToRemove,root->directories[i].dname) == 0 &&
					root->directories[i].nStartBlock > 0){
			root->directories[i].nStartBlock = -1; // sentinal value
		}
		index++;
	}
	root->nDirectories--;
	fseek(fp,0,SEEK_SET); // being paranoid
	fwrite(root,sizeof(csc452_root_directory),1,fp);
	fclose(fp);


}

void zeroOutChunkOfFileStartingAtIndex(int index){
	FILE *fp = fopen(".disk","rb+");
	fseek(fp,index,SEEK_SET);
	char buf[1];
	memset(buf,0,1);
	// want to write "0" in the file BLOCK_SIZE times
	for(int x = 0 ; x < BLOCK_SIZE; x++){
		fwrite(buf,sizeof(char),1,fp);
	}
	fclose(fp);

}

/**
 * @brief Get the Disk Block object
 * 
 * @param nStartBlock 
 */
struct csc452_disk_block *getDiskBlock(long nStartBlock){
	struct csc452_disk_block *block = malloc(sizeof(csc452_disk_block));
	FILE *fp = fopen(".disk","rb+");
	fseek(fp,nStartBlock,SEEK_SET);
	fread(block,sizeof(csc452_disk_block),1,fp);
	fclose(fp);
	return block;
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
