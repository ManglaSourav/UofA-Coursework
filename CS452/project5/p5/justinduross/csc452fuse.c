/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/
/*
 * File: csc452fuse.c
 * Author: Justin Duross
 * Purpose: Implementation of various filesystem system calls to make your own filesystem.
 * We are implementing sycalls such as mkdir, getattr, readdir, rmdir, and more. The "disk"
 * is a file called ".disk" that represents the physical hard disk. Our filesystem has a 
 * root directory, and in the root directory are other directories but not files. In the 
 * directories are only files.
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

#define BITMAP_BLOCKS 20
#define BITMAP_TOTAL_SIZE (BITMAP_BLOCKS * BLOCK_SIZE)
#define BITMAP_ENTRIES ((BITMAP_TOTAL_SIZE / sizeof(short)) - BITMAP_BLOCKS)

// Splits the path string into the directory, filename, and extension then
// returns the value of sscanf()
static int scanPath(const char *path, char *directory, char *filename, char *ext) {
	int scanval = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, ext);
	/*
	if (scanval == 1) {
		filename[0] = '\0';
		ext[0] = '\0';
	}
	else if (scanval == 2) {
		ext[0] = '\0';
		filename[MAX_FILENAME] = '\0';
	}
	*/
	directory[MAX_FILENAME] = '\0';
	filename[MAX_FILENAME] = '\0';
	ext[MAX_EXTENSION] = '\0';

	return scanval;
}

// Opens the disk and reads the first block which is the root
// directory struct and reads it into rootDir
static void getRootDir(csc452_root_directory *rootDir) {
	FILE *disk = fopen(".disk", "rb+");
	fseek(disk, 0, SEEK_SET);
	fread(rootDir, BLOCK_SIZE, 1, disk);
	fclose(disk);
}

// Iterates through the directories in rootDir and checks to see if
// any of them match the directory string passed in. If true then
// return the start block number for the directory, otherwise -1
static int dirExists(char *directory) {
	csc452_root_directory rootDir;
	getRootDir(&rootDir);

	int numDirs = rootDir.nDirectories;
	//printf("	numDirs: %d\n", numDirs);
	for (int i = 0; i < numDirs; i++) {
		//printf("	dirExists   dirName: %s\n", rootDir.directories[i].dname);
		if (strcmp(directory, rootDir.directories[i].dname) == 0) {
			return rootDir.directories[i].nStartBlock;
		}
	}
	return -1;
}

//Finds the block of the directory string passed in and reads the block
//into the directory entry struct
static void readDirEntry(char *directory, csc452_directory_entry *dirEntry) {
	long dirBlock = dirExists(directory);

	FILE *disk = fopen(".disk", "rb+");
	fseek(disk, dirBlock, SEEK_SET);
	fread(dirEntry, sizeof(csc452_directory_entry), 1, disk);
	fclose(disk);
}

//Writes the dirEntry struct into the dirBlock on the disk
static void writeDirEntry(csc452_directory_entry *dirEntry, long dirBlock) {
	FILE *disk = fopen(".disk", "rb+");
	fseek(disk, dirBlock, SEEK_SET);
	fwrite(dirEntry, BLOCK_SIZE, 1, disk);
	fclose(disk);
}

// Removes the directory at the passed in dirIndex in the rootDir struct.
// Does restructuring so the directories array is contiguous
static void removeDir(int dirIndex, csc452_root_directory *rootDir) {
	int numDirs = rootDir->nDirectories;

	if (dirIndex < numDirs - 1) {
		rootDir->directories[dirIndex] = rootDir->directories[numDirs - 1];
		strcpy(rootDir->directories[numDirs - 1].dname, "");
	}
	else {
		strcpy(rootDir->directories[dirIndex].dname, "");
	}
	rootDir->nDirectories -= 1;

}

// Checks to see if the file exists by iterating through the files in the specified
// directory. Returns the file size if exists, otherwise -1
static int fileExists(char *directory, char *filename, char *ext) {
	if (dirExists(directory) == -1) {
		return -1;
	}

	csc452_directory_entry dirEntry;
	readDirEntry(directory, &dirEntry);
	
	for (int i = 0; i < dirEntry.nFiles; i++) {
		printf("	fname: %s     fext: %s\n", dirEntry.files[i].fname, dirEntry.files[i].fext);
		if (strcmp(filename, dirEntry.files[i].fname) == 0 && strcmp(ext, dirEntry.files[i].fext) == 0) {
			return dirEntry.files[i].fsize;
		}
	}
	return -1;
}

//Removes the file from the directory struct. Ensures old file index
//is empty and the files in the array are contiguous
static void removeFile(int i, csc452_directory_entry *dirEntry) {
	int numFiles = dirEntry->nFiles;
	if (i < numFiles - 1) {
		dirEntry->files[i] = dirEntry->files[numFiles - 1];
		strcpy(dirEntry->files[numFiles - 1].fname, "");
		strcpy(dirEntry->files[numFiles - 1].fext, "");

	}
	else {
		strcpy(dirEntry->files[i].fname, "");
		strcpy(dirEntry->files[i].fext, "");
	}
	dirEntry->nFiles -= 1;
}

// Gets the starting block for the file that is passed in. Returns the starting
// block number for the file if found, otherwise -1
static long getFileBlockNum(char *directory, char *filename, char *ext) {
	csc452_directory_entry dirEntry;
	readDirEntry(directory, &dirEntry);
	for (int i = 0; i < dirEntry.nFiles; i++) {
		if (strcmp(filename, dirEntry.files[i].fname) == 0 && strcmp(ext, dirEntry.files[i].fext) == 0) {
			return dirEntry.files[i].nStartBlock;
		}
	}
	return -1;
}

// Updates the file size of the specified file path to the fileSize parameter
static void setFileSize(char *directory, char *filename, char *ext, int fileSize) {
	csc452_directory_entry dirEntry;
	readDirEntry(directory, &dirEntry);
	long dirBlock = dirExists(directory);

	for (int i = 0; i < dirEntry.nFiles; i++) {
		if (strcmp(dirEntry.files[i].fname, filename) == 0 && strcmp(dirEntry.files[i].fext, ext) == 0) {
			dirEntry.files[i].fsize = fileSize;
			writeDirEntry(&dirEntry, dirBlock);
			return;
		}
	}
}

// Iterates through the bitmap on the disk until it finds the first free block and
// returns that block
static long getFreeBlock() {
	FILE *disk = fopen(".disk", "rb+");
	
	fseek(disk, -BITMAP_TOTAL_SIZE + sizeof(short), SEEK_END);
	
	short bitmapVal;
	fread(&bitmapVal, sizeof(short), 1, disk);
	for (int i = 1; i < BITMAP_ENTRIES; i++) {
		//fread(&bitmapVal, sizeof(short), 1, disk);
		if (bitmapVal == 0) {
			return i * BLOCK_SIZE;
		}
		fread(&bitmapVal, sizeof(short), 1, disk);
	}
	fclose(disk);
	return -1;
}

// Gets the bitmap value for a specific blockNum specified and returns
// the value
static short getBlockVal(long blockNum) {
	short bitval;
	FILE *disk = fopen(".disk", "rb+");
	short blockIndex = blockNum / BLOCK_SIZE;
	fseek(disk, (-BITMAP_TOTAL_SIZE + (sizeof(short) * blockIndex)), SEEK_END);
	fread(&bitval, sizeof(short), 1, disk);
	fclose(disk);
	return bitval;
}

// Writes a bitmap value newVal for a specific blockToSet
static void setBlock(long blockToSet, short newVal) {
	FILE *disk = fopen(".disk", "rb+");
	short entryNum = blockToSet / BLOCK_SIZE;
	fseek(disk, -BITMAP_TOTAL_SIZE + (entryNum * sizeof(short)), SEEK_END);
	fwrite(&newVal, sizeof(short), 1, disk);
	fclose(disk);
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
	//printf("HELLO THERE!!!\n");
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		char directory[MAX_FILENAME + 1] = "";
		char filename[MAX_FILENAME + 1] = "";
		char ext[MAX_EXTENSION + 1] = "";
		int scanval = scanPath(path, directory, filename, ext);
		//printf("	DIR NAME: %s\n", directory);
		//If the path does exist and is a directory:
		//stbuf->st_mode = S_IFDIR | 0755;
		//stbuf->st_nlink = 2;
		//int exists = dirExists(directory);
		//printf("	dirExists: %d\n", exists);
		if (scanval == 1 && dirExists(directory) != -1) {
			printf("	path exists and is a dir\n");
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return res;
		}

		//If the path does exist and is a file:
		//stbuf->st_mode = S_IFREG | 0666;
		//stbuf->st_nlink = 2;
		//stbuf->st_size = file size
		int fileSize = fileExists(directory, filename, ext);
		printf("	fileSize: %d\n", fileSize);
		if (fileSize != -1) {
			printf("	path exists and is a file\n");
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = fileSize;
		}
		
		//Else return that path doesn't exist
		else {
			printf("	path does not exist\n");
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

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char ext[MAX_EXTENSION + 1] = "";
	scanPath(path, directory, filename, ext);

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);

		csc452_root_directory rootDir;
		getRootDir(&rootDir);

		for (int i = 0; i < rootDir.nDirectories; i++) {
			filler(buf, rootDir.directories[i].dname, NULL, 0);
		}
	}
	else if (dirExists(directory) != -1) {
		//printf("	ADD FILES IN DIR\n");
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		csc452_directory_entry dirEntry;
		readDirEntry(directory, &dirEntry);

		for (int i = 0; i < dirEntry.nFiles; i++) {
			char fileNameExt[MAX_FILENAME + MAX_EXTENSION + 2];
			strcpy(fileNameExt, dirEntry.files[i].fname);
			strcat(fileNameExt, ".");
			strcat(fileNameExt, dirEntry.files[i].fext);
			filler(buf, fileNameExt, NULL, 0);
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
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char ext[MAX_EXTENSION + 1] = "";
	int scanval = scanPath(path, directory, filename, ext);
	//int scanval = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, ext);
	//directory[MAX_FILENAME + 1] = '\0';
	if (strlen(directory) > MAX_FILENAME) {
		return -ENAMETOOLONG;
	}
	//printf("DIR NAME: %s\n", directory);
	//printf("FILE NAME: %s\n", filename);
	//printf("EXT: %s\n", ext);
	if (scanval > 1) {
		return -EPERM;
	}
	if (strlen(directory) > MAX_FILENAME) {
		printf("DIR NAME TOO LONG!!!\n");
		return -ENAMETOOLONG;
	}
	if (dirExists(directory) != -1) {
		printf("DIR ALREADY EXISTS!!!\n");
		return -EEXIST;
	}

	csc452_root_directory rootDir;
	getRootDir(&rootDir);

	if (rootDir.nDirectories >= MAX_DIRS_IN_ROOT) {
		return -1;
	}

	rootDir.nDirectories += 1;

	long newBlock = getFreeBlock();
	setBlock(newBlock, -1);
	
	csc452_directory_entry dirEntry;
	dirEntry.nFiles = 0;
	strcpy(rootDir.directories[rootDir.nDirectories-1].dname, directory);
	rootDir.directories[rootDir.nDirectories-1].nStartBlock = newBlock;

	FILE *disk = fopen(".disk", "rb+");
	fseek(disk, 0, SEEK_SET);
	fwrite(&rootDir, BLOCK_SIZE, 1, disk);
	fclose(disk);
	writeDirEntry(&dirEntry, newBlock);

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

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char ext[MAX_EXTENSION + 1] = "";
	scanPath(path, directory, filename, ext);

	if (strcmp(path, "/") == 0) {
		return -EPERM;
	}
	if (strlen(filename) > MAX_FILENAME || strlen(ext) > MAX_EXTENSION) {
		return -ENAMETOOLONG;
	}
	if (fileExists(directory, filename, ext) != -1) {
		return -EEXIST;
	}

	csc452_directory_entry dirEntry;
	readDirEntry(directory, &dirEntry);
	long fileBlock = getFreeBlock();

	dirEntry.nFiles += 1;
	dirEntry.files[dirEntry.nFiles-1].nStartBlock = fileBlock;
	strcpy(dirEntry.files[dirEntry.nFiles-1].fname, filename);
	if (strlen(ext) > 0) {
		strcpy(dirEntry.files[dirEntry.nFiles-1].fext, ext);
	}
	else {
		strcpy(dirEntry.files[dirEntry.nFiles-1].fext, "");
	}
	
	dirEntry.files[dirEntry.nFiles-1].fsize = 0;

	writeDirEntry(&dirEntry, dirExists(directory));
	setBlock(fileBlock, -1);
	
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
	
	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char ext[MAX_EXTENSION + 1] = "";
	int scanval = scanPath(path, directory, filename, ext);

	//check to make sure path exists
	if (scanval == 1) {
		return -EISDIR;
	}
	int fileSize = fileExists(directory, filename, ext);
	if (fileSize == -1) {
		return -ENOENT;
	}
	//check that size is > 0
	if (size <= 0) {
		return -1;
	}
	//check that offset is <= to the file size
	if (offset > fileSize) {
		return -1;
	}
	//read in data
	int fileBlock = getFileBlockNum(directory, filename, ext);
	int fileBlockNum = fileBlock / BLOCK_SIZE;

	FILE *disk = fopen(".disk", "r");
	fseek(disk, fileBlockNum * BLOCK_SIZE, SEEK_SET);
	for (int i = 0; i < (fileSize/BLOCK_SIZE) + 1; i++) {
		fread(buf + (i*BLOCK_SIZE), BLOCK_SIZE, 1, disk);
		fileBlockNum = getBlockVal(fileBlockNum);
	}
	fclose(disk);



	//return success, or error

	return size;
}

// Writes a specific block specified with blockNum, uses diskBlock
// struct to write to that block
static void writeBlock(long blockNum, csc452_disk_block *diskBlock) {
	FILE *disk = fopen(".disk", "rb+");
	fseek(disk, blockNum, SEEK_SET);
	fwrite(diskBlock, BLOCK_SIZE, 1, disk);
	fclose(disk);
}

// Uses the current diskBlock struct and writes the data in that 
// struct to the next free block on disk and updates the bitmap
static void copyToBuf(long prevBlock, csc452_disk_block *diskBlock) {
	long nextBlock = getFreeBlock();
	if (prevBlock != 0) {
		setBlock(prevBlock, nextBlock/BLOCK_SIZE);
	}
	prevBlock = nextBlock;
	writeBlock(nextBlock, diskBlock);
	setBlock(nextBlock, -1);
	
}

static void getDiskBlock(long blockNum, csc452_disk_block *diskBlock) {
	FILE *disk = fopen(".disk", "rb+");
	fseek(disk, BLOCK_SIZE * blockNum, SEEK_SET);
	fread(diskBlock, BLOCK_SIZE, 1, disk);
	fclose(disk);
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
	int returnSize = size;
	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char ext[MAX_EXTENSION + 1] = "";
	scanPath(path, directory, filename, ext);

	int fileSize = fileExists(directory, filename, ext);
	//check to make sure path exists
	if (fileSize == -1) {
		return -1;
	}	
	//check that size is > 0
	if (size <= 0) {
		return -1;
	}
	//check that offset is <= to the file size
	if (offset > fileSize) {
		return -EFBIG;
	}
	//write data
	int newFileSize = size + fileSize;
	setFileSize(directory, filename, ext, newFileSize);
	
	int offsetNum = offset / BLOCK_SIZE;
	long fileBlock = getFileBlockNum(directory, filename, ext);
	long fileBlockNum = fileBlock / BLOCK_SIZE;
	for (int i = 0; i < offsetNum; i++) {
		fileBlockNum = getBlockVal(fileBlockNum * BLOCK_SIZE);
		if (fileBlockNum != -1) {
			fileBlockNum = getBlockVal(fileBlockNum * BLOCK_SIZE);
		}
	}

	csc452_disk_block diskBlock;
	getDiskBlock(fileBlock, &diskBlock);
	int offsetMod = offset % BLOCK_SIZE;
	if (strlen(diskBlock.data) + size <= BLOCK_SIZE) {
		strncpy(diskBlock.data + offsetMod, buf, size);
		writeBlock(fileBlockNum * BLOCK_SIZE, &diskBlock);
	}
	else {
		strncpy(diskBlock.data + offsetMod, buf, BLOCK_SIZE - offsetMod);
		writeBlock(fileBlock, &diskBlock);
		buf += BLOCK_SIZE - offsetMod;
		size = size - (BLOCK_SIZE - offsetMod);
		FILE *disk = fopen(".disk", "rb+");
		while (getBlockVal(fileBlock) != -1) {
			fileBlock = getBlockVal(fileBlock);
			fseek(disk, fileBlock, SEEK_SET);
			if (size <= BLOCK_SIZE) {
				strncpy(diskBlock.data, buf, size);
				fwrite(&diskBlock, BLOCK_SIZE, 1, disk);
				size = 0;
			}
			else {
				strncpy(diskBlock.data, buf, BLOCK_SIZE);
				fwrite(&diskBlock, BLOCK_SIZE, 1, disk);
				size -= BLOCK_SIZE;
				buf += BLOCK_SIZE;
	
			}

		}
		fclose(disk);
		long lastBlock = 0;
		while (size > 0) {
			if (size <= BLOCK_SIZE) {
				strncpy(diskBlock.data, buf, size);
				copyToBuf(lastBlock, &diskBlock);
				buf += BLOCK_SIZE;
				return returnSize;
			}
			else {
				strncpy(diskBlock.data, buf, BLOCK_SIZE);
				size -= BLOCK_SIZE;
			}
			copyToBuf(lastBlock, &diskBlock);
			buf += BLOCK_SIZE;
		}
	}


	//return success, or error

	return returnSize;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char ext[MAX_EXTENSION + 1] = "";
	int scanval = scanPath(path, directory, filename, ext);

	if (scanval > 1) {
		return -ENOTDIR;
	}
	if (dirExists(directory) == -1) {
		return -ENOENT;
	}

	csc452_directory_entry dirEntry;
	readDirEntry(directory, &dirEntry);

	if (dirEntry.nFiles > 0) {
		return -ENOTEMPTY;
	}

	csc452_root_directory rootDir;
	getRootDir(&rootDir);

	for (int i = 0; i < rootDir.nDirectories; i++) {
		if (strcmp(rootDir.directories[i].dname, directory) == 0) {
			removeDir(i, &rootDir);
			setBlock(rootDir.directories[i].nStartBlock, 0);

			FILE *disk = fopen(".disk", "rb+");
			fseek(disk, 0, SEEK_SET);
			fwrite(&rootDir, BLOCK_SIZE, 1, disk);
			fclose(disk);

			return 0;
		}
	}

	return -1;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
        (void) path;

	char directory[MAX_FILENAME + 1] = "";
	char filename[MAX_FILENAME + 1] = "";
	char ext[MAX_EXTENSION + 1] = "";
	
	int scanval = scanPath(path, directory, filename, ext);
	if (scanval == 1) {
		return -EISDIR;
	}
	if (fileExists(directory, filename, ext) == -1) {
		return -ENOENT;
	}

	csc452_directory_entry dirEntry;
	long dirBlock = dirExists(directory);
	long fileBlock;
	//printf("	MADE IT\n");
	//printf("	nFiles: %d\n", dirEntry.nFiles);
	for (int i = 0; i < dirEntry.nFiles; i++) {
		//printf("	nFiles: %d\n", dirEntry.nFiles);
		//printf("!!!	fname: %s	fext: %s\n", dirEntry.files[i].fname, dirEntry.files[i].fext);
		//printf("!!!	filename: %s    ext: %s\n", filename, ext);
		if (strcmp(dirEntry.files[i].fname, filename) == 0 && strcmp(dirEntry.files[i].fext, ext) == 0) {
			//printf("	CORRECT FILE\n");
			fileBlock = dirEntry.files[i].nStartBlock;
			removeFile(i, &dirEntry);
			//printf("	REMOVED FILE\n");
			short blockNum = getBlockVal(fileBlock);
			if (blockNum == -1) {
				setBlock(fileBlock, 0);
			}
			else {
				int currIndex = blockNum;
				while (currIndex != -1) {
					int nextBlockIndex = getBlockVal(blockNum * BLOCK_SIZE);
					setBlock(blockNum, 0);
					currIndex = nextBlockIndex;
				}
				if (currIndex == -1) {
					setBlock(currIndex * BLOCK_SIZE, 0);
				}
			}
			
			writeDirEntry(&dirEntry, dirBlock);
			return 0;
		}
	}



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
