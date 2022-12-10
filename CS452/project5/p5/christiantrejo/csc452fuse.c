/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/

/*
Author: Christian Trejo 
Course: CSC452
Assignment: Project 5 - File Systems
Due: Wednesday, May 4, 2022, by 11:59pm
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

/*
To track the usage of the 10240 blocks (5MB disk and each block is 512 bytes), I will 
use a bitmap where each bit will correspond to one block. The bitmap will store ints 
(int are 4 bytes == 32 bits), so each int will track 32 blocks. A total of 384 ints (3 blocks 
worth of ints) will be used to track all block's usage. Bit shifting and masking will 
be used to get and set individual bits.
*/
unsigned int bitmap[384];		//Global for tracking open and used spots. 


/*
Purpose: Gets the bit from the bitmap corresponding to the given block number. 
	To get bit corresponding to a block number:
	1. Find the int holding the block's bit
	2. Find the number of shifts needed to get the bit to the LSB spot
	3. Shift the int by the number of shifts needed and then bitwise AND with 1
Parameters: blockNumber - block number to get bit value for; int type 
Return: bitValue - the bit value corresponding to the block number; -1 if errors; int type
*/
int getBitValue(int blockNumber){
	FILE *fp;
	int index, number, numShifts, bitValue;

	//Open file and read in bitmap
	fp = fopen(".disk", "rb");
	if(fp == NULL){
		fclose(fp);
		return -1;
	}
	fseek(fp, -3*BLOCK_SIZE, SEEK_END);		//Beginning of third to last block 
	fread(&bitmap, sizeof(bitmap), 1, fp);  //Set bitmap 

	//Get bit value
	index = blockNumber/32;					//Index of int in bitmap
	number = bitmap[index];					//Actual int in bitmap 
	numShifts = blockNumber%32;				//Number of shifts to get to bit
	bitValue = (number >> (31 - numShifts)) & 1;	//Bit value for block number

	fclose(fp);								//Close file
	return bitValue;
}


/*
Purpose: Sets the bit value to 1 in the bitmap for a given block number. 
Parameters: blockNumber - block number whose bit value will be set to 0; int type
Returns: 1 = success; -1 = error; int type
*/
int setBitValueTo1(int blockNumber){
	FILE *fp;
	int index, number, numShifts, bitSetValue;

	//Open file and read in bitmap
	fp = fopen(".disk", "rb+");
	if(fp == NULL){
		fclose(fp);
		return -1;
	}
	fseek(fp, -3*BLOCK_SIZE, SEEK_END);		//Beginning of third to last block 
	fread(&bitmap, sizeof(bitmap), 1, fp);  //Set bitmap 

	//Set bit value to 1
	index = blockNumber/32;					//Index of int in bitmap
	number = bitmap[index];					//Actual int in bitmap 
	numShifts = blockNumber%32;				//Number of shifts to get to bit
	bitSetValue = 1 << (31 - numShifts);	//Bit shifted to correct spot
	bitmap[index] = number | bitSetValue;	//Bitwise OR and save new int

	//Write bitmap to memory
	fseek(fp, -3*BLOCK_SIZE, SEEK_END);		//Beginning of third to last block
	fwrite(&bitmap, sizeof(bitmap), 1, fp);	//Write bitmap to memory 
	fclose(fp);								//Close file
	return 1;
}


/*
Purpose: Sets the bit value to 0 in the bitmap for a given block number. 
Parameters: blockNumber - block number whose bit value will be set to 0; int type
Returns: 1 = success; -1 = error; int type
*/
int setBitValueTo0(int blockNumber){
	FILE *fp;
	int index, number, numShifts, bitSetValue;

	//If, bit value already 0
	if(getBitValue(blockNumber) == 0){
		return 1;
	}

	//Else, bit value is 1 currently. Use XOR.
	//Open file and read in bitmap
	fp = fopen(".disk", "rb+");
	if(fp == NULL){
		return -1;
	}
	fseek(fp, -3*BLOCK_SIZE, SEEK_END);		//Beginning of third to last block 
	fread(&bitmap, sizeof(bitmap), 1, fp);  //Set bitmap 

	//Set bit value to 1
	index = blockNumber/32;					//Index of int in bitmap
	number = bitmap[index];					//Actual int in bitmap 
	numShifts = blockNumber%32;				//Number of shifts to get to bit
	bitSetValue = 1 << (31 - numShifts);	//Bit shifted to correct spot
	bitmap[index] = number ^ bitSetValue;	//Bitwise OR and save new int

	//Write bitmap to memory
	fseek(fp, -3*BLOCK_SIZE, SEEK_END);		//Beginning of third to last block
	fwrite(&bitmap, sizeof(bitmap), 1, fp);	//Write bitmap to memory 
	fclose(fp);								//Close file
	return 1;
}


/*
Purpose: Find and return the next open block number. 
Parameters: None
Return: next open block that can be used; if -1 returned, no open blocks; int type
*/
int getNextOpenSpot(){
	int i;

	//Iterate through all 10240 blocks skipping root block and last three bitmap blocks 
	for(i = 1; i < 10237; i++){
		if(getBitValue(i) == 0){		//If block is empty, return its number 
			return i;
		}
	}

	return -1;							//Else, return -1 to signal that disk is full
}


/*
Directories will be stored in our .disk file as a single block-sized csc452_directory_entry structure
per subdirectory. Each directory entry is only to take up a single disk block so we are limited to a
fixed number of files per directory. Each file entry in the directory has a filename in 8.3 (name.extension)
format. We also record the total size of the file, and the location of the file's first block on disk.
*/
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


/*
In our file system the root only contains other directoreis, so we will use block 0 of .disk to hold the
directory entry of the root.
The root has a array of subdirectories (csc452_directory objects) in which each entry corresponds
to a subdirectory's name and starting block index. We are limiting our root to be one block in size, so
there is a limit of how many subdirectories we can create (MAX_DIRS_IN_ROOT).
Each subdirectory will have an entry in the directories array with its name and the block index
of the subdirectory's directory entry.
*/
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


/*
Files will be stored alongside the directories in .disk. Data blocks are 512-byte structs of
the following below format.
*/
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
	FILE *fp;								//File pointer
	char directoryName[strlen(path)];		//Directory name
	char filenameName[strlen(path)];		//File name
	char extension[strlen(path)];		//Extension for file
	struct csc452_root_directory root;		//Root directory 
	struct csc452_directory_entry directory;	//Desired directory 
	int res = 0;
	int fileLoc, startBlock, i;
	int retval;

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {

		
		retval = sscanf(path, "/%[^/]/%[^.].%s", directoryName, filenameName, extension);
		
		//Read in root
		fp = fopen(".disk", "rb+");							//Open .disk file
		fseek(fp, 0, SEEK_SET);								//Front of file
		fread(&root, sizeof(csc452_root_directory), 1, fp);	//Read root struct
		fclose(fp);
		
		//If root contains no directories, then path does not exist
		if(root.nDirectories == 0){
			res = -ENOENT;
			return res;
		}

		//Determine if directory exists 
		startBlock = -1;
		for(i = 0; i < root.nDirectories; i++){
			//Compare name of directory and given directory
			if(strcmp(root.directories[i].dname, directoryName) == 0){
				startBlock = root.directories[i].nStartBlock;
				break; 
			}
		}

		//If directory does NOT exist 
		if(startBlock == -1){
			res = -ENOENT;
			return res;
		} 

		//Directory exists:
		//If the path does exist and is only a directory (no filename):
		if(retval == 1){
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return res;
		} 

		//Read in directory
		fp = fopen(".disk", "rb+");	//Open .disk file
		fseek(fp, startBlock*BLOCK_SIZE , SEEK_SET);		//Go to starting block of directory
		fread(&directory, sizeof(csc452_directory_entry), 1, fp);	//Read in directory 
		fclose(fp);											

		//Find file 
		fileLoc = -1;
		for(i = 0; i < directory.nFiles; i++){		//Iterate through files and compare filenames
			if(strcmp(directory.files[i].fname, filenameName) == 0){		//If filename in directory
				if(strcmp(directory.files[i].fext, extension) == 0){
					fileLoc = i;						//Save index in list 
					break;
				}
			}
		}
		
		//If the path does exist and is a file
		if(fileLoc != -1){	
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = directory.files[fileLoc].fsize;
		} else {		//Path does not exist
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
	FILE *fp;
	csc452_root_directory root;
	csc452_directory_entry directoryEntry;
	char filenameName[strlen(path)];		//File name
	char extension[strlen(path)];			//Extension for file
	char directoryName[strlen(path)];		//Directory Name
	int i, retval, dirIndex;

	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	retval = sscanf(path, "/%[^/]/%[^.].%s", directoryName, filenameName, extension);
	if(retval > 1){			//If directory path is invalid
		return -ENOENT;
	}

	fp = fopen(".disk", "rb+");							//Open .disk file
	fseek(fp, 0, SEEK_SET);								//Front of file
	fread(&root, sizeof(csc452_root_directory), 1, fp);	//Read root struct
	fclose(fp);

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);

		//Read through directories in root and add to buf. 
		for(i = 0; i < root.nDirectories; i++){
			filler(buf, root.directories[i].dname, NULL, 0);
		}

	}
	else {
		// All we have _right now_ is root (/), so any other path must
		// not exist. 

		//get directory 
		//Read through directories in root and get directory. 
		dirIndex = -1;
		for(i = 0; i < root.nDirectories; i++){
			if(strcmp(root.directories[i].dname, directoryName) == 0){
				dirIndex = root.directories[i].nStartBlock;
				break;
			}
		}

		//if directory not found, return -ENOENT
		if(dirIndex == -1){
			return -ENOENT;
		}

		//print directory contents 
		fp = fopen(".disk", "rb+");
		fseek(fp, dirIndex*BLOCK_SIZE, SEEK_SET);		//Jump to directory in file; 
		fread(&directoryEntry, sizeof(directoryEntry), 1, fp);		//Read in directory 
		fclose(fp);

		for(i = 0; i < directoryEntry.nFiles; i++){
			filler(buf, directoryEntry.files[i].fname, NULL, 0);
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
	FILE *fp;
	csc452_root_directory root;
	char filenameName[strlen(path)];		//File name
	char extension[strlen(path)];			//Extension for file
	char directoryName[strlen(path)];		//Directory name
	int openSpot, i, retval;
	csc452_directory_entry newDir;

	(void) path;
	(void) mode;

	//scan for directory Name first 
	retval = sscanf(path, "/%[^/]/%[^.].%s", directoryName, filenameName, extension);

	//If the directory is not under the root directory only 
	if(retval > 1){
		return -EPERM;
	}

	//Read in root
	fp = fopen(".disk", "rb+");							//Open .disk file
	fseek(fp, 0, SEEK_SET);								//Front of file
	fread(&root, sizeof(csc452_root_directory), 1, fp);	//Read root struct
	fclose(fp);

	//If too many directories in root, return -1
	// if(root.nDirectories >= MAX_DIRS_IN_ROOT){
	// 	return -1;
	// }

	//Check if directory name is too long (>8 chars)
	if(strlen(directoryName) > MAX_FILENAME){
		return -ENAMETOOLONG;
	}

	//Check if directory already exists
	for(i = 0; i < root.nDirectories; i++){
		if(strcmp(root.directories[i].dname, directoryName) == 0){
			return -EEXIST;
		}
	}

	//find next open spot in bitmap and then set spot to 1
	openSpot = getNextOpenSpot();
	setBitValueTo1(openSpot);

	//Set nFiles in new csc452_directory_entry object
	newDir.nFiles = 0;

	//Update Root with entry and increase count 
	strcpy(root.directories[root.nDirectories].dname, directoryName);
	root.directories[root.nDirectories].nStartBlock = openSpot;
	root.nDirectories = root.nDirectories + 1;

	//Update disk: root, directory_entry and bitmap 
	fp = fopen(".disk", "rb+");									//Open .disk file
	fseek(fp, 0, SEEK_SET);										//Beginning of file
	fwrite(&root, sizeof(csc452_root_directory), 1, fp);		//Write root to memory 

	fseek(fp, openSpot*BLOCK_SIZE, SEEK_SET);					//Starting block of open block
	fwrite(&newDir, sizeof(csc452_directory_entry), 1, fp);		//Write new directory entry to memory 
	fclose(fp);													//Close file

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
	FILE *fp;
	char filename[strlen(path)];			//File name
	char extension[strlen(path)];			//Extension for file
	char directoryName[strlen(path)];		//Directory name
	csc452_directory_entry directory;
	csc452_root_directory root;
	csc452_disk_block fileBlock;
	int i, retval, startBlock, openSpot;

	(void) path;
	(void) mode;
	(void) dev;

	//Get directory name, filename, and extension 
	retval = sscanf(path, "/%[^/]/%[^.].%s", directoryName, filename, extension);

	//If file is trying to be created in the root dir
	if(retval != 3){
		return -EPERM;
	}
	
	//If name is beyond 8.3 chars (8 char filename, 3 char file extension)
	if(strlen(filename) > 8 || strlen(extension) > 3){
		return -ENAMETOOLONG;
	}

	//Read in root
	fp = fopen(".disk", "rb+");							//Open .disk file
	fseek(fp, 0, SEEK_SET);								//Front of file
	fread(&root, sizeof(csc452_root_directory), 1, fp);	//Read root struct
	fclose(fp);

	//Find directory
	startBlock = -1; 
	for(i = 0; i < root.nDirectories; i++){
		if(strcmp(root.directories[i].dname, directoryName) == 0){
			startBlock = root.directories[i].nStartBlock;
			break;
		}
	}
	if(startBlock == -1){								//If directory does not exist 
		return -ENOENT;
	}

	//Read in directory entry
	fp = fopen(".disk", "rb+");							//Open .disk file
	fseek(fp, startBlock*BLOCK_SIZE, SEEK_SET);			//Go to starting block of directory
	fread(&directory, sizeof(csc452_directory_entry), 1, fp);	//Read in directory 
	fclose(fp);

	//Check if file exists already (check filename and extension)
	for(i = 0; i < directory.nFiles; i++){
		if(strcmp(directory.files[i].fname, filename) == 0){
			if(strcmp(directory.files[i].fext, extension) == 0){
				return -EEXIST;
			}
		}
	}

	//Add file:
	//Find next open spot in bitmap and then set spot to 1 
	openSpot = getNextOpenSpot();
	setBitValueTo1(openSpot);

	//Add file info to directory's files[] 
	strcpy(directory.files[directory.nFiles].fname, filename);
	strcpy(directory.files[directory.nFiles].fext, extension);
	directory.files[directory.nFiles].fsize = 0;
	directory.files[directory.nFiles].nStartBlock = openSpot;
	
	//Update directory's file count
	directory.nFiles = directory.nFiles + 1;

	//Set file block's nNextBlock = -1 and clear data[]. 
	fileBlock.nNextBlock = -1;
	memset(fileBlock.data, 0, MAX_DATA_IN_BLOCK);

	//Write Directory's contents and File's contents to disk 
	fp = fopen(".disk", "rb+");
	fseek(fp, startBlock*BLOCK_SIZE, SEEK_SET);
	fwrite(&directory, sizeof(csc452_directory_entry), 1, fp); 

	fseek(fp, openSpot*BLOCK_SIZE, SEEK_SET);
	fwrite(&fileBlock, sizeof(csc452_disk_block), 1, fp);
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
	FILE *fp;
	char filename[strlen(path)];			//File name
	char extension[strlen(path)];			//Extension for file
	char directoryName[strlen(path)];		//Directory name

	csc452_root_directory root;
	csc452_directory_entry directory;
	csc452_disk_block currFileBlock;

	int retval, startBlock, fileIndex;
	int i, skipBlocks, totalRead, startLoc;
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//Get directory name, filename, and extension 
	retval = sscanf(path, "/%[^/]/%[^.].%s", directoryName, filename, extension);
	
	if(retval < 3){							//If the path is a directory
		return -EISDIR;
	}

	//Read in root
	fp = fopen(".disk", "rb+");							//Open .disk file
	fseek(fp, 0, SEEK_SET);								//Front of file
	fread(&root, sizeof(csc452_root_directory), 1, fp);	//Read root struct
	fclose(fp);

	//Find directory
	startBlock = -1; 
	for(i = 0; i < root.nDirectories; i++){
		if(strcmp(root.directories[i].dname, directoryName) == 0){
			startBlock = root.directories[i].nStartBlock;
			break;
		}
	}
	if(startBlock == -1){					//If directory does not exist 
		return -ENOENT;
	}

	//Read in directory entry
	fp = fopen(".disk", "rb+");						//Open .disk file
	fseek(fp, startBlock*BLOCK_SIZE, SEEK_SET);		//Go to starting block of directory
	fread(&directory, sizeof(csc452_directory_entry), 1, fp);	//Read in directory 
	fclose(fp);

	//Find file
	fileIndex = -1;							//Index of file in directory's files[]
	for(i = 0; i < directory.nFiles; i++){
		if(strcmp(directory.files[i].fname, filename) == 0){
			if(strcmp(directory.files[i].fext, extension) == 0){
				fileIndex = i;
				break;
			}
		}
	}
	if(fileIndex == -1){		//If file does not exist 
		return -ENOENT;
	}

	//Check that size is > 0
	if(size <= 0){
		return -1;
	}

	//Check that offset is <= to the file size
	if(offset > directory.files[fileIndex].fsize){
		return -EFBIG;
	}


	//Get to correct spot in file: offset/MAX_DATA_IN_BLOCK for blocks to skip
	//							   offset%MAX_DATA_IN_BLOCK for spot in data[]
	skipBlocks = offset/MAX_DATA_IN_BLOCK;	//Number of blocks to skip
	startLoc   = offset%MAX_DATA_IN_BLOCK;	//Starting loc in data[] in disk_block 
	totalRead  = 0;							//Total number of chars read in

	fp = fopen(".disk", "rb+");
	fseek(fp, directory.files[fileIndex].nStartBlock*BLOCK_SIZE, SEEK_SET);
	fread(&currFileBlock, sizeof(csc452_disk_block), 1, fp);

	//Get to correct disk_block to start writing 
	for(i = 0; i < skipBlocks; i++){		
		fseek(fp, currFileBlock.nNextBlock*BLOCK_SIZE, SEEK_SET);
		fread(&currFileBlock, sizeof(csc452_disk_block), 1, fp);
	}
	fclose(fp);

	//Start reading data[] starting at offset until you reach end of block data[] OR 
	//	until you have read all necessary chars 
	for(i = 0; (i < MAX_DATA_IN_BLOCK - startLoc) && (totalRead < size); i++){
		buf[i] = currFileBlock.data[startLoc+i];
		totalRead++;
	}

	//If need to read more data, load next disk_block and continue reading
	while(totalRead < size){
		fp = fopen(".disk", "rb+");
		fseek(fp, currFileBlock.nNextBlock*BLOCK_SIZE, SEEK_SET);
		fread(&currFileBlock, sizeof(csc452_disk_block), 1, fp);
		fclose(fp);

		for(i = 0; (i < MAX_DATA_IN_BLOCK) && (totalRead < size); i++){
			buf[totalRead] = currFileBlock.data[i];
			totalRead++;
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
	FILE *fp;
	char filename[strlen(path)];			//File name
	char extension[strlen(path)];			//Extension for file
	char directoryName[strlen(path)];		//Directory name

	csc452_root_directory root;
	csc452_directory_entry directory;
	csc452_disk_block currFileBlock;

	int i, retval, startBlock, openSpot, fileIndex;
	int skipBlocks, totalRead, currentBlockNumber, startLoc;
	(void) fi;

	//Get directory name, filename, and extension 
	retval = sscanf(path, "/%[^/]/%[^.].%s", directoryName, filename, extension);

	//Read in root
	fp = fopen(".disk", "rb+");							//Open .disk file
	fseek(fp, 0, SEEK_SET);								//Front of file
	fread(&root, sizeof(csc452_root_directory), 1, fp);	//Read root struct
	fclose(fp);

	//Find directory
	startBlock = -1; 
	for(i = 0; i < root.nDirectories; i++){
		if(strcmp(root.directories[i].dname, directoryName) == 0){
			startBlock = root.directories[i].nStartBlock;
			break;
		}
	}
	if(startBlock == -1){			//If directory does not exist 
		return -ENOENT;
	}

	//Read in directory entry
	fp = fopen(".disk", "rb+");						//Open .disk file
	fseek(fp, startBlock*BLOCK_SIZE, SEEK_SET);		//Go to starting block of directory
	fread(&directory, sizeof(csc452_directory_entry), 1, fp);	//Read in directory 
	fclose(fp);

	//Find file
	fileIndex = -1;				//Index of file in directory's files[]
	for(i = 0; i < directory.nFiles; i++){
		if(strcmp(directory.files[i].fname, filename) == 0){
			if(strcmp(directory.files[i].fext, extension) == 0){
				fileIndex = i;
				break;
			}
		}
	}
	if(fileIndex == -1){		//If file does not exist 
		return -ENOENT;
	}

	//Check that size is > 0
	if(size <= 0){
		return -1;
	}

	//TODO: NEEDS TO HANDLE APPENDS
	//Check that offset is <= to the file size
	if(offset > directory.files[fileIndex].fsize){
		return -EFBIG;
	}

	//Get to correct spot in file: offset/MAX_DATA_IN_BLOCK for blocks to skip
	//							   offset%MAX_DATA_IN_BLOCK for spot in data[]
	skipBlocks = offset/MAX_DATA_IN_BLOCK;	//Number of blocks to skip
	startLoc   = offset%MAX_DATA_IN_BLOCK;	//Starting loc in data[] in disk_block 
	totalRead  = 0;							//Total number of chars written in
	currentBlockNumber = directory.files[fileIndex].nStartBlock;	//Track which block number currently in

	fp = fopen(".disk", "rb+");
	fseek(fp, directory.files[fileIndex].nStartBlock*BLOCK_SIZE, SEEK_SET);
	fread(&currFileBlock, sizeof(csc452_disk_block), 1, fp);

	//Get to correct disk_block to start writing 
	for(i = 0; i < skipBlocks; i++){		
		currentBlockNumber = currFileBlock.nNextBlock;
		fseek(fp, currFileBlock.nNextBlock*BLOCK_SIZE, SEEK_SET);
		fread(&currFileBlock, sizeof(csc452_disk_block), 1, fp);
	}
	fclose(fp);
	
	//Fill remaining space in disk block 
	for(i = 0; (i < MAX_DATA_IN_BLOCK - startLoc) && (totalRead < size); i++){
		currFileBlock.data[startLoc+i] = buf[i];
		totalRead++;
	}

	//If we dont need more memory, just write currFileBlock out to memory
	if(totalRead >= size){
		fp = fopen(".disk", "rb+");
		fseek(fp, currentBlockNumber*BLOCK_SIZE, SEEK_SET);
		fwrite(&currFileBlock, sizeof(csc452_disk_block), 1, fp);
		fclose(fp);
	}

	//If need more space, write remaining message into new disk blocks 
	while(totalRead < size){				
		openSpot = getNextOpenSpot();						//Get open spot
		if(openSpot == -1){									//If disk is full
			return -ENOSPC;
		}
		setBitValueTo1(openSpot);							//Else, set spot to taken

		currFileBlock.nNextBlock = openSpot;				//Update disk_block's nNextBlock
		fp = fopen(".disk", "rb+");							//Save disk_block
		fseek(fp, currentBlockNumber*BLOCK_SIZE, SEEK_SET);
		fwrite(&currFileBlock, sizeof(csc452_disk_block), 1, fp);
		fclose(fp);
		
		memset(currFileBlock.data, 0, MAX_DATA_IN_BLOCK);	//Reset fields in disk_block
		currFileBlock.nNextBlock = -1;
		currentBlockNumber = openSpot; 						//Update current block number 

		for(i = 0; totalRead < size && i < MAX_DATA_IN_BLOCK; i++){		//Read in data
			currFileBlock.data[i] = buf[totalRead];
			totalRead++;
		}
	}

	fp = fopen(".disk", "rb+");								//Write out disk block 
	fseek(fp, currentBlockNumber*BLOCK_SIZE, SEEK_SET);
	fwrite(&currFileBlock, sizeof(csc452_disk_block), 1, fp);
	
	if(directory.files[fileIndex].fsize < offset+size){		//Update directory entry if file grew in size 
		directory.files[fileIndex].fsize = offset+size;
		fseek(fp, startBlock*BLOCK_SIZE, SEEK_SET);
		fwrite(&directory, sizeof(csc452_directory_entry), 1, fp);
	}
	fclose(fp); 

	return directory.files[fileIndex].fsize;				//Return file size on success
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	FILE *fp;
	char filenameName[strlen(path)];		//File name
	char extension[strlen(path)];			//File extension
	char directoryName[strlen(path)];		//Directory Name 
	csc452_root_directory root;
	csc452_directory_entry directory;
	int i, retval, startBlock, index;

	//scan for directory name first 
	retval = sscanf(path, "/%[^/]/%[^.].%s", directoryName, filenameName, extension);

	//If the path is not a directory
	if(retval > 1){
		return -ENOTDIR;
	}

	//If the directory to remove is root, return error 
	if(strcmp(path, "/") == 0){
		return -ENOTDIR;
	}

	//Find directory in root
	fp = fopen(".disk", "rb+");
	fseek(fp, 0, SEEK_SET);
	fread(&root, sizeof(csc452_root_directory), 1, fp);
	fclose(fp);

	startBlock = -1;		//Directory's starting block
	index = -1;				//Index of directory in root's directories[]
	for(i = 0; i < root.nDirectories; i++){
		if(strcmp(root.directories[i].dname, directoryName) == 0){
			startBlock = root.directories[i].nStartBlock;
			index = i;
			break;
		}
	}

	//If directory does NOT exist 
	if(startBlock == -1){
		return -ENOENT;
	} 

	//Directory Exists:
	//Read in directory entry
	fp = fopen(".disk", "rb+");		//Open .disk file
	fseek(fp, startBlock*BLOCK_SIZE, SEEK_SET);		//Go to starting block of directory
	fread(&directory, sizeof(csc452_directory_entry), 1, fp);	//Read in directory 
	fclose(fp);

	//If directory is not empty
	if(directory.nFiles > 0){
		return -ENOTEMPTY;
	}

	//Directory is empty, delete it - Update bitmap, shift directories over to fill in gap in root's directories[]
	setBitValueTo0(startBlock);

	for(i = 0; i < root.nDirectories - index - 1; i++){		//Move entries left to consolidate root's directories[] 
		strcpy(root.directories[index+i].dname, root.directories[index+i+1].dname);
		root.directories[index+i].nStartBlock = root.directories[index+i+1].nStartBlock;
	}

	root.nDirectories = root.nDirectories - 1;				//Decrease count of directories in root

	//Write root to file 
	fp = fopen(".disk", "rb+");
	fseek(fp, 0, SEEK_SET);
	fwrite(&root, sizeof(csc452_root_directory), 1, fp);
	fclose(fp);
	
	return 0;		//Success
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
    FILE *fp;
	char filename[strlen(path)];			//File name
	char extension[strlen(path)];			//Extension for file
	char directoryName[strlen(path)];		//Directory name

	csc452_root_directory root;
	csc452_directory_entry directory;
	csc452_disk_block currFileBlock;

	int i, retval, startBlock, fileIndex, currentBlockNumber;

	//Get directory name, filename, and extension 
	retval = sscanf(path, "/%[^/]/%[^.].%s", directoryName, filename, extension);

	if(retval < 3){							//If path is a directory
		return -EISDIR;
	}

	//Read in root
	fp = fopen(".disk", "rb+");							//Open .disk file
	fseek(fp, 0, SEEK_SET);								//Front of file
	fread(&root, sizeof(csc452_root_directory), 1, fp);	//Read root struct
	fclose(fp);

	//Find directory
	startBlock = -1; 
	for(i = 0; i < root.nDirectories; i++){
		if(strcmp(root.directories[i].dname, directoryName) == 0){
			startBlock = root.directories[i].nStartBlock;
			break;
		}
	}
	if(startBlock == -1){			//If directory does not exist 
		return -ENOENT;
	}

	//Read in directory entry
	fp = fopen(".disk", "rb+");						//Open .disk file
	fseek(fp, startBlock*BLOCK_SIZE, SEEK_SET);		//Go to starting block of directory
	fread(&directory, sizeof(csc452_directory_entry), 1, fp);	//Read in directory 
	fclose(fp);

	//Find file
	fileIndex = -1;				//Index of file in directory's files[]
	for(i = 0; i < directory.nFiles; i++){
		if(strcmp(directory.files[i].fname, filename) == 0){
			if(strcmp(directory.files[i].fext, extension) == 0){
				fileIndex = i;
				break;
			}
		}
	}
	if(fileIndex == -1){		//If file does not exist 
		return -ENOENT;
	}

	//Starting block of file 
	currentBlockNumber = directory.files[fileIndex].nStartBlock;

	//Iterate through all disk_blocks, mark them as free in bitmap
	while(currentBlockNumber != -1){
		fp = fopen(".disk", "rb+");						//Read in file block
		fseek(fp, currentBlockNumber*BLOCK_SIZE, SEEK_SET);
		fread(&currFileBlock, sizeof(csc452_disk_block), 1, fp);
		fclose(fp);

		setBitValueTo0(currentBlockNumber);				//Set its bit value = 0
		currentBlockNumber = currFileBlock.nNextBlock;	//Next block 
	}

	//Update directory's files[] - shift files over to the left to fill in gap 
	for(i = 0; i < directory.nFiles-fileIndex-1; i++){
		strcpy(directory.files[fileIndex+i].fname, directory.files[fileIndex+i+1].fname);
		strcpy(directory.files[fileIndex+i].fext, directory.files[fileIndex+i+1].fext);
		directory.files[fileIndex+i].fsize = directory.files[fileIndex+i+1].fsize;
		directory.files[fileIndex+i].nStartBlock = directory.files[fileIndex+i+1].nStartBlock;
	}
	//Update directory's number of files 
	directory.nFiles = directory.nFiles - 1;

	//Write directory to .disk
	fp = fopen(".disk", "rb+");									//Open .disk file
	fseek(fp, startBlock*BLOCK_SIZE, SEEK_SET);					//Go to starting block of directory
	fwrite(&directory, sizeof(csc452_directory_entry), 1, fp);	//Write directory to disk
	fclose(fp);

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
