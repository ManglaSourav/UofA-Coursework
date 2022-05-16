/*
* File: csc452fuse.c
* Author: Kaden Herr
* Date Created: April 23, 2022
* Last Editted: May 2, 2022
* Purpose: Implement a file system.
* Note: Skeleton code provided by Dr. Jonathan Misurda.
* Creating Bit Map:
* http://www.mathcs.emory.edu/~cheung/Courses/255/Syllabus/1-C-intro/bit-array.html
*/

/*

FUSE: Filesystem in Userspace


gcc -Wall `pkg-config fuse --cflags --libs` -D_FILE_OFFSET_BITS=64 csc452fuse.c -o csc452


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

#define NUM_OF_BLOCKS 10240 // 5MB_disk_space / BLOCK_SIZE
#define BITMAP_SIZE 320 //NUM_OF_BLOCKS / INT_SIZE (need a bit for each block)
/*
* It will take 3 blocks to store the bitmap:
* 1280 = BITMAP_SIZE(320) * 4 bytes (an int is 4 bytes)
* BLOCK_SIZE is 512 so 3*512 = 1536
* This make the valid number of blocks now 10237
* 
* We can use the below code to find the bitmap in the .disk file:
* fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
*/
#define VALID_NUM_BLOCKS 10237
#define BITMAP_NBLOCKS 3
#define BITMAP_BIT_SIZE 1280

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))


/*****************************************************************************
***************************** Structs for Data *******************************
*****************************************************************************/

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


/*****************************************************************************
***************************** Bitmap Functions *******************************
*****************************************************************************/

// sizeof() returns num of bytes. There are 8 bits in byte
#define INT_SIZE (int) (sizeof(int)*8)

/*
* int isFree(int, int*) - Test is a block in memory is free or not.
*/
int isFree(int blockNum, int* bitmap) {
    int intToCheck = bitmap[blockNum/INT_SIZE];
    int bitPos = blockNum % INT_SIZE;
    unsigned int flag = 1;
    flag = flag << bitPos;

    if(intToCheck & flag) {
        return 0;// Space not free
    } else {
        return 1;// Space is free
    }
}


/*
* void markFree(int, int**) - Mark a block as free space in the bitmap.
*/
void markFree(int blockNum, int* bitmap) {
    int pos = blockNum/INT_SIZE;
    int bitPos = blockNum % INT_SIZE;
    unsigned int flag = 1;
    flag = flag << bitPos;
    flag = ~flag;

    bitmap[pos] = bitmap[pos] & flag;
}


/*
* void markFull(int, int**) - Mark a block as filled in the bitmap.
*/
void markFull(int blockNum, int* bitmap) {
    int pos = blockNum/INT_SIZE;
    int bitPos = blockNum % INT_SIZE;
    unsigned int flag = 1;
    flag = flag << bitPos;

    bitmap[pos] = bitmap[pos] | flag;
}


/*
* int nextFree(int*) - Return the index of the next free space in the
* given bitmap. If there is no free space, return 0.
*/
int nextFree(int *bitmap) {
    int i;
    for(i=1; i<VALID_NUM_BLOCKS; i++) {
        if(isFree(i,bitmap)) {
            return i;
        }
    }

    // No free space
    return 0;
}


/*****************************************************************************
***************************** Helper Functions *******************************
*****************************************************************************/

/*
* int isValidPath(char*, char*, char*) - Test that the directory, filename,
* and extention are valid lengths.
*/
int isValidPath(char *dir, char *filename, char *ext) {
    if(strlen(dir) > MAX_FILENAME || strlen(filename) > MAX_FILENAME ||
        strlen(ext) > MAX_EXTENSION) {
        return 0;
    } else {
        return 1;
    }
}


/*
* int max(int, int) - Return the max of the two integers
*/
int max(int a, int b) {
    if(a >= b) {
        return a;
    } else {
        return b;
    }
}


/*****************************************************************************
***************************** Program Code ***********************************
*****************************************************************************/

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
    int i;
    int dirFound = 0;
	int res = 0;
    char directory[10] = "";
    char filename[10] = "";
    char extension[5] = "";
    sscanf(path, "/%9[^/]/%9[^.].%4s",directory, filename, extension);

    // Check if the path given could be valid (correct name lengths)
    if(isValidPath(directory,filename,extension) == 0) {
        return -ENOENT;
    }

    // Create a root directory struct on the stack
    csc452_root_directory rootDir;
    // Open the .disk file
    FILE *fp = fopen(".disk","r");
    if(fp == NULL) {
        printf("Disk could not be read\n");
        return 1;
    }
    // Fill the root directory struct from the .disc file
    fread(&rootDir,BLOCK_SIZE,1,fp);


    // Test the path //

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
        // See if it is a directory
        for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
            if(strcmp(rootDir.directories[i].dname,directory) == 0) {
                dirFound = 1;
                break;
            }
        }

        if(dirFound) {
            // If the path does exist and is a directory:
            if(strcmp(filename,"") == 0) {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
                fclose(fp);
                return 0;
            }

            // Create a directory entry struct
            csc452_directory_entry dirEntry;
            // Find information about the directory entry in the .disk file
            fseek(fp, rootDir.directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
            // Read the info into the struct
            fread(&dirEntry,BLOCK_SIZE,1,fp);

            // Test if any of the files names match the given file name
            for(i=0; i<MAX_FILES_IN_DIR; i++) {
                if(strcmp(dirEntry.files[i].fname,filename) == 0 &&
                    strcmp(dirEntry.files[i].fext,extension) == 0) {
                    //If the path does exist and is a file:
                    stbuf->st_mode = S_IFREG | 0666;
                    stbuf->st_nlink = 2;
                    stbuf->st_size = dirEntry.files[i].fsize;
                    fclose(fp);
                    return 0;
                }
            }
        }
        //Else return that path doesn't exist
		res = -ENOENT;
	}

    fclose(fp);
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi) {

	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

    int i;
    int dirFound = 0;
    char directory[10] = "";
    char filename[10] = "";
    char extension[5] = "";
    char temp[MAX_FILENAME+MAX_EXTENSION+2] = ""; // plus 2 for "." and "\0"
    sscanf(path, "/%9[^/]/%9[^.].%4s",directory, filename, extension);

    // Check that the path is to a directory and not a file
    if(strcmp(filename,"") != 0) {
        return -ENOENT;
    }

    // Create a root directory struct on the stack
    csc452_root_directory rootDir;
    // Open the .disk file
    FILE *fp = fopen(".disk","r");
    if(fp == NULL) {
        printf("Disk could not be read\n");
        return 1;
    }
    // Fill the root directory struct from the .disc file
    fread(&rootDir,BLOCK_SIZE,1,fp);

	// If the path is the root, then read contents of root directory.
	if(strcmp(path, "/") == 0) {
        //A directory holds two entries, one that represents itself (.) 
        //and one that represents the directory above us (..)
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);

        // List the directories
        for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
            // If it is a directory, list it 
            if(strcmp(rootDir.directories[i].dname,"") != 0) {
                filler(buf, rootDir.directories[i].dname, NULL,0);
            }
        }
        fclose(fp);
        return 0;
	}

    // Check that the directory exists
    for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
        if(strcmp(rootDir.directories[i].dname,directory) == 0) {
            dirFound = 1;
            break;
        }
    }

    if(dirFound == 0) {
        // Directory not found
        fclose(fp);
        return -ENOENT;
    }

    // Create a directory entry struct
    csc452_directory_entry dirEntry;
    // Find information about the directory entry in the .disk file
    fseek(fp, rootDir.directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
    // Read the info into the struct
    fread(&dirEntry,BLOCK_SIZE,1,fp);

    // Add the symbols that represent the directory and the one above
    filler(buf, ".", NULL,0);
    filler(buf, "..", NULL, 0);

    // Add all of the file names in the directory
    for(i=0; i<dirEntry.nFiles; i++) {
        if(strcmp(dirEntry.files[i].fname,"") != 0) {
            // Add the file to the filler
            if(strcmp(dirEntry.files[i].fext,"") == 0) {
                // No extention
                filler(buf, dirEntry.files[i].fname, NULL,0);
            } else {
                // Has extention
                strncat(temp,dirEntry.files[i].fname,MAX_FILENAME);
                strcat(temp,".");
                strncat(temp,dirEntry.files[i].fext,MAX_EXTENSION);
                filler(buf, temp, NULL,0);
            }
        }
    }
    // Close the .disk file
    fclose(fp);
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode) {
	(void) mode;

    int i;
    char directory[10] = "";
    char filename[10] = "";
    char extension[5] = "";
    sscanf(path, "/%9[^/]/%9[^.].%4s",directory, filename, extension);

    // Check if the path given could be valid (correct name lengths)
    if(isValidPath(directory,filename,extension) == 0) {
        return -ENAMETOOLONG;
    }
    // Check that the directory is under the root directory only
    if(strcmp(filename,"") != 0) {
        return -EPERM;
    }

    // Open the .disk file
    FILE *fp = fopen(".disk","r+");
    if(fp == NULL) {
        return 1;
    }
    // Create a root directory struct on the stack
    csc452_root_directory rootDir;
    // Fill the root directory struct from the .disc file
    fread(&rootDir,BLOCK_SIZE,1,fp);
    
    // Check that the directory does not already exist
    for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
        if(strcmp(rootDir.directories[i].dname,directory) == 0) {
            fclose(fp);
            return -EEXIST;
        }
    }

    // Check that adding a directory will not exceed max directory count
    if(rootDir.nDirectories+1 > MAX_DIRS_IN_ROOT) {
        fclose(fp);
        return -ENOSPC;
    }

    // Create a bitmap of the free disk space
    int bitmap[BITMAP_SIZE];
    // Load the bitmap information from disk
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fread(bitmap,BITMAP_BIT_SIZE,1,fp);

    // Scan the bitmap for the next free chunk to create the directory in
    int bitIndex = nextFree(bitmap);
    if(bitIndex == 0) {
        // No more free space left on the disk
        fclose(fp);
        return -ENOSPC;
    }

    // Find the first free spot in the directories array
    for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
        if(strcmp(rootDir.directories[i].dname,"") == 0) {
            break;
        }
    }
    // Create the new directory
    strcpy(rootDir.directories[i].dname,directory);
    rootDir.directories[i].nStartBlock = bitIndex;
    // Increment the directory count
    rootDir.nDirectories = rootDir.nDirectories+1;
    // Mark the block as full in the bitmap
    markFull(bitIndex,bitmap);


    // Write everything back to disk:

    // Root directory
    rewind(fp);
    fwrite(&rootDir,BLOCK_SIZE,1,fp);
    // Bitmap
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fwrite(bitmap,BITMAP_BIT_SIZE,1,fp);

    // Everything succeeded
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
static int csc452_mknod(const char *path, mode_t mode, dev_t dev) {
	(void) mode;
    (void) dev;

    int i;
    int dirIndex = -1;
    char directory[10] = "";
    char filename[10] = "";
    char extension[5] = "";
    sscanf(path, "/%9[^/]/%9[^.].%4s",directory, filename, extension);

    // Check if the path given could be valid (correct name lengths)
    if(isValidPath(directory,filename,extension) == 0) {
        return -ENAMETOOLONG;
    }
    // Check that the file is under a sub-directory
    if(strcmp(filename,"") == 0) {
        return -EPERM;
    }

    // Open the .disk file
    FILE *fp = fopen(".disk","r+");
    if(fp == NULL) {
        return 1;
    }
    // Create a root directory struct on the stack
    csc452_root_directory rootDir;
    // Fill the root directory struct from the .disc file
    fread(&rootDir,BLOCK_SIZE,1,fp);
    
    // Check that the directory exists
    for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
        if(strcmp(rootDir.directories[i].dname,directory) == 0) {
            dirIndex = i;
            break;
        }
    }

    if(dirIndex == -1) {
        // The directory does not exist
        fclose(fp);
        return -ENOENT;
    }

    // Create a directory entry struct
    csc452_directory_entry dirEntry;
    // Find information about the directory entry in the .disk file
    fseek(fp, rootDir.directories[dirIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    // Read the info into the struct
    fread(&dirEntry,BLOCK_SIZE,1,fp);

    // Check that the file does not already exist 
    for(i=0; i<dirEntry.nFiles; i++) {
        if(strcmp(dirEntry.files[i].fname,filename) == 0 &&
            strcmp(dirEntry.files[i].fext,extension) == 0) {
            fclose(fp);
            return -EEXIST;
        }
    }

    // Check that adding a file will not exceed max file count
    if(dirEntry.nFiles+1 > MAX_FILES_IN_DIR) {
        fclose(fp);
        return -ENOSPC;
    }

    // Create a bitmap of the free disk space
    int bitmap[BITMAP_SIZE];
    // Load the bitmap information from disk
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fread(bitmap,BITMAP_BIT_SIZE,1,fp);

    // Scan the bitmap for the next free chunk to create the file in
    int bitIndex = nextFree(bitmap);
    if(bitIndex == 0) {
        // No more free space left on the disk
        fclose(fp);
        return -ENOSPC;
    }


    // Create the new file
    strcpy(dirEntry.files[dirEntry.nFiles].fname,filename);
    strcpy(dirEntry.files[dirEntry.nFiles].fext,extension);
    dirEntry.files[dirEntry.nFiles].nStartBlock = bitIndex;
    dirEntry.files[dirEntry.nFiles].fsize = 0;
    // Increment this directory's file counter
    dirEntry.nFiles = dirEntry.nFiles+1;
    // Mark the block as full in the bitmap
    markFull(bitIndex,bitmap);


    // Write everything back to disk:

    // Directory entry
    fseek(fp,rootDir.directories[dirIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    fwrite(&dirEntry,BLOCK_SIZE,1,fp);
    // Bitmap
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fwrite(bitmap,BITMAP_BIT_SIZE,1,fp);

    // Everything succeeded
    fclose(fp);
    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi) {
	(void) fi;

    int i;
    int dirIndex = -1;
    int fileIndex = -1;
    char directory[10] = "";
    char filename[10] = "";
    char extension[5] = "";
    sscanf(path, "/%9[^/]/%9[^.].%4s",directory, filename, extension);

    // Check that the path leads to a file and not a directory
    if(strcmp(filename,"") == 0) {
        return -EISDIR;
    }

    // Open the .disk file
    FILE *fp = fopen(".disk","r+");
    if(fp == NULL) {
        return 1;
    }
    // Create a root directory struct on the stack
    csc452_root_directory rootDir;
    // Fill the root directory struct from the .disc file
    fread(&rootDir,BLOCK_SIZE,1,fp);
    
    // Check that the directory exists
    for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
        if(strcmp(rootDir.directories[i].dname,directory) == 0) {
            dirIndex = i;
            break;
        }
    }

    if(dirIndex == -1) {
        // The directory does not exist
        fclose(fp);
        return -ENOENT;
    }

    // Create a directory entry struct
    csc452_directory_entry dirEntry;
    // Find information about the directory entry in the .disk file
    fseek(fp, rootDir.directories[dirIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    // Read the info into the struct
    fread(&dirEntry,BLOCK_SIZE,1,fp);

    // Check that the file exists 
    for(i=0; i<dirEntry.nFiles; i++) {
        if(strcmp(dirEntry.files[i].fname,filename) == 0 &&
            strcmp(dirEntry.files[i].fext,extension) == 0) {
            fileIndex = i;
            break;
        }
    }

    if(fileIndex == -1) {
        // The file does not exist
        fclose(fp);
        return -ENOENT;
    }

    // Check that size is greater than 0
    if(size <= 0) {
        fclose(fp);
        return -EFBIG;
    }

    // Check that offset is less than or equal to file size
    if(offset > dirEntry.files[fileIndex].fsize) {
        fclose(fp);
        return -EFBIG;
    }

    // Create a disk block struct that represents the file space
    csc452_disk_block fileEntry;
    // Find information about the file entry in the .disk file
    fseek(fp, dirEntry.files[fileIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    // Read the info into the struct
    fread(&fileEntry,BLOCK_SIZE,1,fp);


    // Find which data block the offset lives in
    int relativeOffset = offset;
    while(relativeOffset >= MAX_DATA_IN_BLOCK) {
        // Find the next file block entry in the .disk file
        fseek(fp, fileEntry.nNextBlock*BLOCK_SIZE,SEEK_SET);
        // Read the info into the struct
        fread(&fileEntry,BLOCK_SIZE,1,fp);
        relativeOffset = relativeOffset - MAX_DATA_IN_BLOCK;
    }

    // Start writing into the buffer from the file
    int j = 0;
    while(j<size) {

        for(i=relativeOffset; i<MAX_DATA_IN_BLOCK && j<size; i++) {
            buf[j] = fileEntry.data[i];
            j++;
        }

        // Find the next block of data
        fseek(fp, fileEntry.nNextBlock*BLOCK_SIZE,SEEK_SET);
        // Read the info into the struct
        fread(&fileEntry,BLOCK_SIZE,1,fp);

        relativeOffset = 0;
    }
    
    // Close the .disk file
    fclose(fp);

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi) {
	(void) fi;

    int i,j,retval,blockIndex,bitIndex;
    int dirIndex = -1;
    int fileIndex = -1;
    char directory[10] = "";
    char filename[10] = "";
    char extension[5] = "";
    sscanf(path, "/%9[^/]/%9[^.].%4s",directory, filename, extension);


    // Open the .disk file
    FILE *fp = fopen(".disk","r+");
    if(fp == NULL) {
        return 1;
    }
    // Create a root directory struct on the stack
    csc452_root_directory rootDir;
    // Fill the root directory struct from the .disc file
    fread(&rootDir,BLOCK_SIZE,1,fp);
    
    // Check that the directory exists
    for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
        if(strcmp(rootDir.directories[i].dname,directory) == 0) {
            dirIndex = i;
            break;
        }
    }

    if(dirIndex == -1) {
        // The directory does not exist
        fclose(fp);
        return -ENOENT;
    }

    // Create a directory entry struct
    csc452_directory_entry dirEntry;
    // Find information about the directory entry in the .disk file
    fseek(fp, rootDir.directories[dirIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    // Read the info into the struct
    fread(&dirEntry,BLOCK_SIZE,1,fp);

    // Check that the file exists 
    for(i=0; i<dirEntry.nFiles; i++) {
        if(strcmp(dirEntry.files[i].fname,filename) == 0 &&
            strcmp(dirEntry.files[i].fext,extension) == 0) {
            fileIndex = i;
            break;
        }
    }

    if(fileIndex == -1) {
        // The file does not exist
        fclose(fp);
        return -ENOENT;
    }

    // Check that size is greater than 0
    if(size <= 0) {
        fclose(fp);
        return -EFBIG;
    }

    // Check that offset is less than or equal to file size
    if(offset > dirEntry.files[fileIndex].fsize) {
        fclose(fp);
        return -EFBIG;
    }

    // Create a bitmap of the free disk space
    int bitmap[BITMAP_SIZE];
    // Load the bitmap information from disk
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fread(bitmap,BITMAP_BIT_SIZE,1,fp);


    // Create a disk block struct that represents the file space
    csc452_disk_block fileEntry;
    // Find information about the file entry in the .disk file
    fseek(fp, dirEntry.files[fileIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    // Read the info into the struct
    fread(&fileEntry,BLOCK_SIZE,1,fp);

    // Save the block number of the file
    blockIndex = dirEntry.files[fileIndex].nStartBlock;
    // Find which data block the offset lives in
    int relativeOffset = offset;
    while(relativeOffset >= MAX_DATA_IN_BLOCK) {
        // Save the block index of the block we are about to access
        blockIndex = fileEntry.nNextBlock;
        // Find the next file block entry in the .disk file
        fseek(fp, blockIndex*BLOCK_SIZE,SEEK_SET);
        // Read the info into the struct
        fread(&fileEntry,BLOCK_SIZE,1,fp);
        relativeOffset -= MAX_DATA_IN_BLOCK;
    }

    
    int noFreeSpace = 0;
    j = 0;
    do {
        // Start writing into the buffer
        for(i=relativeOffset; i<MAX_DATA_IN_BLOCK && j<size; i++) {
            fileEntry.data[i] = buf[j];
            j++;
        }

        if(j<size) {
            // We need more file space
            // Scan the bitmap for the next free block to store the data in
            bitIndex = nextFree(bitmap);
            if(bitIndex == 0) {
                // No more free space left on the .disk file
                noFreeSpace = 1;
                break;
            }
            // Mark the bit as full
            markFull(bitIndex,bitmap);

            // Set the next block in the file chain
            fileEntry.nNextBlock = bitIndex;
            // Write the file data into the .disk file
            fseek(fp, blockIndex*BLOCK_SIZE,SEEK_SET);
            fwrite(&fileEntry,BLOCK_SIZE,1,fp);
            // Set the new block index
            blockIndex = bitIndex;
            // Read in the next block
            fseek(fp, blockIndex*BLOCK_SIZE,SEEK_SET);
            fread(&fileEntry,BLOCK_SIZE,1,fp);
            // Reset the relative offset to 0 so we start writing at the
            // beginning of the block
            relativeOffset = 0;
        }
    } while(j<size);


    // Check everything was written to the file and we didn't run out of space
    if(noFreeSpace) {
        dirEntry.files[fileIndex].fsize = max(j+offset,dirEntry.files[fileIndex].fsize);
        retval = -ENOSPC;
    } else {
        // Set the file size to reflect the newly added data
        dirEntry.files[fileIndex].fsize = max(size+offset,dirEntry.files[fileIndex].fsize);
        retval = size;
    }

    // Write the data back into the .disk file:

    // Directory entry
    fseek(fp, rootDir.directories[dirIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    fwrite(&dirEntry,BLOCK_SIZE,1,fp);

    // File entry
    fseek(fp, blockIndex*BLOCK_SIZE,SEEK_SET);
    fwrite(&fileEntry,BLOCK_SIZE,1,fp);

    // Bitmap
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fwrite(bitmap,BITMAP_BIT_SIZE,1,fp);

    // Close the .disk file
    fclose(fp);
	return retval;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path) {
	int i;
    int dirFound = 0;
    char directory[10] = "";
    char filename[10] = "";
    char extension[5] = "";
    sscanf(path, "/%9[^/]/%9[^.].%4s",directory, filename, extension);

    // Check that it is a directory and not a file
    if(strcmp(filename,"") != 0) {
        return -ENOTDIR;
    }

    // Open the .disk file
    FILE *fp = fopen(".disk","r+");
    if(fp == NULL) {
        return 1;
    }
    // Create a root directory struct on the stack
    csc452_root_directory rootDir;
    // Fill the root directory struct from the .disc file
    fread(&rootDir,BLOCK_SIZE,1,fp);


    // Find the directory
    for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
        if(strcmp(rootDir.directories[i].dname,directory) == 0) {
            dirFound = 1;
            break;
        }
    }

    if(dirFound == 0) {
        // The directory does not exist
        fclose(fp);
        return -ENOENT;
    }

    // Create a directory entry struct
    csc452_directory_entry dirEntry;
    // Find information about the directory entry in the .disk file
    fseek(fp, rootDir.directories[i].nStartBlock*BLOCK_SIZE,SEEK_SET);
    // Read the info into the struct
    fread(&dirEntry,BLOCK_SIZE,1,fp);

    if(dirEntry.nFiles > 0) {
        // The directory is not empty
        fclose(fp);
        return -ENOTEMPTY;
    }

    // Create a bitmap of the free disk space
    int bitmap[BITMAP_SIZE];
    // Load the bitmap information from disk
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fread(bitmap,BITMAP_BIT_SIZE,1,fp);


    // Remove directory:

    // Mark the chunk that hold the directory as free
    markFree(rootDir.directories[i].nStartBlock,bitmap);
    // Remove directory name from root
    strcpy(rootDir.directories[i].dname,"\0");
    rootDir.directories[i].nStartBlock = 0;
    // Decrement directory counter
    rootDir.nDirectories = rootDir.nDirectories-1;


    // Write everything back to disk:

    // Root directory
    rewind(fp);
    fwrite(&rootDir,BLOCK_SIZE,1,fp);
    // Bitmap
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fwrite(bitmap,BITMAP_BIT_SIZE,1,fp);


    // Close the .disk file
    fclose(fp);
    return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path) {
    int i, nCurBlock, nNextBlock;
    int dirIndex = -1;
    int fileIndex = -1;
    char directory[10] = "";
    char filename[10] = "";
    char extension[5] = "";
    sscanf(path, "/%9[^/]/%9[^.].%4s",directory, filename, extension);

    // Check that the path is a file and not a directory
    if(strcmp(filename,"") == 0) {
        return -EISDIR;
    }

    // Open the .disk file
    FILE *fp = fopen(".disk","r+");
    if(fp == NULL) {
        return 1;
    }
    // Create a root directory struct on the stack
    csc452_root_directory rootDir;
    // Fill the root directory struct from the .disc file
    fread(&rootDir,BLOCK_SIZE,1,fp);
    
    // Check that the directory exists
    for(i=0; i<MAX_DIRS_IN_ROOT; i++) {
        if(strcmp(rootDir.directories[i].dname,directory) == 0) {
            dirIndex = i;
            break;
        }
    }

    if(dirIndex == -1) {
        // The directory does not exist
        fclose(fp);
        return -ENOENT;
    }

    // Create a directory entry struct
    csc452_directory_entry dirEntry;
    // Find information about the directory entry in the .disk file
    fseek(fp, rootDir.directories[dirIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    // Read the info into the struct
    fread(&dirEntry,BLOCK_SIZE,1,fp);

    // Check that the file exists 
    for(i=0; i<dirEntry.nFiles; i++) {
        if(strcmp(dirEntry.files[i].fname,filename) == 0 &&
            strcmp(dirEntry.files[i].fext,extension) == 0) {
            fileIndex = i;
            break;
        }
    }

    if(fileIndex == -1) {
        // The file does not exist
        fclose(fp);
        return -ENOENT;
    }

    // Create a bitmap of the free disk space
    int bitmap[BITMAP_SIZE];
    // Load the bitmap information from disk
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fread(bitmap,BITMAP_BIT_SIZE,1,fp);

    // Create a disk block struct that represents the file space
    csc452_disk_block fileEntry;

    // Save its block number
    nCurBlock = dirEntry.files[fileIndex].nStartBlock;

    // Remove each block of the file:
    do {
        // Read the current block from the .disk file
        fseek(fp, nCurBlock*BLOCK_SIZE,SEEK_SET);
        fread(&fileEntry,BLOCK_SIZE,1,fp);
        // Save next block number
        nNextBlock = fileEntry.nNextBlock;
        // Clear current block
        strcpy(fileEntry.data,"\0");
        fileEntry.nNextBlock = 0;
        // Write back current block
        fseek(fp, nCurBlock*BLOCK_SIZE,SEEK_SET);
        fwrite(&fileEntry,BLOCK_SIZE,1,fp);
        // Mark the current block as free in the bitmap
        markFree(nCurBlock,bitmap);
        // Change the current block to be the next block of the file
        nCurBlock = nNextBlock;
    } while(nCurBlock != 0);


    if(dirEntry.nFiles-1 != fileIndex) {
        // If this is not the last file in the list, then take the last file
        // and overwrite this files meta data with the meta data from the last
        // file in the list.
        strcpy(dirEntry.files[fileIndex].fname,dirEntry.files[dirEntry.nFiles-1].fname);
        strcpy(dirEntry.files[fileIndex].fext,dirEntry.files[dirEntry.nFiles-1].fext);
        dirEntry.files[fileIndex].fsize = dirEntry.files[dirEntry.nFiles-1].fsize;
        dirEntry.files[fileIndex].nStartBlock = dirEntry.files[dirEntry.nFiles-1].nStartBlock;
    }

    // Decrement the file counter for the directory
    dirEntry.nFiles -= 1;


    // Write everything back to the .disk file:

    // Root directory
    //rewind(fp);
    //fwrite(&rootDir,BLOCK_SIZE,1,fp);

    // Directory that held the file file
    fseek(fp, rootDir.directories[dirIndex].nStartBlock*BLOCK_SIZE,SEEK_SET);
    fwrite(&dirEntry,BLOCK_SIZE,1,fp);
    // Bitmap
    fseek(fp,-BITMAP_BIT_SIZE,SEEK_END);
    fwrite(bitmap,BITMAP_BIT_SIZE,1,fp);

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
