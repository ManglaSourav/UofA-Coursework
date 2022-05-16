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



/**
 * @author Amin Sennour
 * @purpose provide implementations of the system calls needed for FUSE to 
 * 	        operate a file system. 
 */



/*
 * ---------------------------- START STRUCTURES ----------------------------
 */



// constant for parsing the path
#define PATH_PARSE "/%9[^/]/%9[^.].%4s"

// disk file path
#define DISK ".disk"

// size of the disk in bytes
#define DISK_SIZE 5242880  

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

// how many blocks on the disk
#define NUM_BLOCKS DISK_SIZE / BLOCK_SIZE
// store the size of an integer in bits
#define INT_SIZE 32
// the number of integers that will be in the bitmap 
#define BITMAP_NUM_INTS NUM_BLOCKS / INT_SIZE 
// how many blocks to store the bitmap 
#define BITMAP_BLOCKS BITMAP_NUM_INTS * 4 / BLOCK_SIZE + 1
// determine how many bytes the bitmap will take up
#define BITMAP_SIZE (BITMAP_BLOCKS) * BLOCK_SIZE
// always store the bitmap in the last BITMAP_BLOCKS blocks 
#define BITMAP_FIRST_BLOCK_INDEX NUM_BLOCKS - BITMAP_BLOCKS - 1
// structure to keep track of free blocks  
typedef struct free_space_bitmap {
	// divide by 32 since each integer contains 32 bits
	int bitmap[BITMAP_NUM_INTS];

	// padding so it fills all it's blocks 
	char padding[BITMAP_SIZE - ((NUM_BLOCKS / INT_SIZE) * 4)];
} free_space_bitmap;



/*
 * ----------------------------- END STRUCTURES -----------------------------
 */ 



/**
 * @brief read an indexed block from disk 
 * 
 * @param i the index of the block to read
 * @return void* a pointer to the block in memory 
 */
static void *readBlock(long i) {
	// if the passed block is invalid then simply return null
	if (i < 0) return NULL;

    // malloc a BLOCK_SIZE region in memory to store the block
	void *ret = malloc(BLOCK_SIZE);

	// open the file
	FILE *fp;
	fp = fopen(DISK, "rb");
	// seek to the start of the ith block
	fseek(fp, BLOCK_SIZE * (i), SEEK_SET);
	// read the block 
	fread(ret, BLOCK_SIZE, 1, fp);
	// close the file 
	fclose(fp);
	// return a pointer to the ith block
	return ret;
}


/**
 * @brief write a block of data to block on disk
 * 
 * @param i the index of the block on disk to write the data from memory to
 * @param data the data to write to disk 
 */
static void writeBlock(long i, void *data) {
    // open the file
	FILE *fp;
	fp = fopen(DISK, "r+b");
	// seek to the start of the ith block
	fseek(fp, BLOCK_SIZE * (i), SEEK_SET);
	// read the block 
	fwrite(data, BLOCK_SIZE, 1, fp);
	// close the file 
	fclose(fp);
}


/**
 * @brief read the bitmap from disk
 * 
 * @return void* the bitmap 
 */
static void *readBitmap() {
	// malloc a BLOCK_SIZE region in memory to store the block
	void *ret = malloc(BITMAP_SIZE);

	// open the file
	FILE *fp;
	fp = fopen(DISK, "rb");
	// seek to the start of the ith block
	fseek(fp, BLOCK_SIZE * (BITMAP_FIRST_BLOCK_INDEX), SEEK_SET);
	// read the block 
	fread(ret, BITMAP_SIZE, 1, fp);
	// close the file 
	fclose(fp);
	// return a pointer to the ith block
	return ret;
}


/**
 * @brief write the bitmap to it's blocks on 
 * 
 * @param data the bitmap to write
 */
static void writeBitmap(void *data) {
	// open the file
	FILE *fp;
	fp = fopen(DISK, "r+b");
	// seek to the start of the ith block
	fseek(fp, BLOCK_SIZE * (BITMAP_FIRST_BLOCK_INDEX), SEEK_SET);
	// read the block 
	fwrite(data, BITMAP_SIZE, 1, fp);
	// close the file 
	fclose(fp);
}


/**
 * @brief free a given block in the free space bitmap 
 * 
 * @param index the block to free
 */
static void freeBlock(long index) {
	free_space_bitmap *bitmap_block = readBitmap();

	int i = index / INT_SIZE;
	int p = index % INT_SIZE;

	unsigned int flag = ~(1 << p);
	bitmap_block->bitmap[i] = bitmap_block->bitmap[i] & flag;

	// store the bitmap
	writeBitmap(bitmap_block);
}


/**
 * @brief recursive file to free all of the blocks associated with a file
 * 
 * @param fBlock the starting block of the file to free
 */
static void freeFile(long fBlock) {
	// if the block is invalid then return
	if (fBlock < 0) return;
	// read the file block
	csc452_disk_block *file = readBlock(fBlock);
	// free the current block
	freeBlock(fBlock);
	// free the next block
	freeFile(file->nNextBlock);
}


/**
 * @brief alloc a block in the free space bitmap 
 * 
 * @param index the block to alloc 
 * @return int -1 if the block is already alloced 0 otherwise
 */
static int allocBlock(long index) {
	free_space_bitmap *bitmap_block = readBitmap();

	int i = index / INT_SIZE;
	int p = index % INT_SIZE;

	unsigned int flag = 1 << p;

	// if the block is already alloced then return -1
	if ((bitmap_block->bitmap[i] & flag) == 1) return -1;

	bitmap_block->bitmap[i] = bitmap_block->bitmap[i] | flag;

	// store the bitmap
	writeBitmap(bitmap_block);
	return 0;
}


/**
 * @brief Get the first free block 
 * 
 * @return int the index of the first free block or -1 if there are no free 
 * 		   blocks
 */
static int getFirstFreeBlock() {
	// search the space between the 0th index and the bitmap block index 
	// (which is assumed to be the last block)
	free_space_bitmap *bitmap_block = readBitmap();
	
	for (int block = 1; block < BITMAP_FIRST_BLOCK_INDEX; block++) {
		int i = block / INT_SIZE;
		int p = block % INT_SIZE;

		unsigned int flag = 1 << p;

		if ((bitmap_block->bitmap[i] & flag) == 0) return block;
	}

	return -1;
}


/**
 * @brief simple function to allocate a new element in a file block linked list
 * 
 * @return long if an empty block is found then the index of the new block, otherwise -1
 */
static long allocNewFileBlock() {
	// get a free block 
	long newBlockIndex = getFirstFreeBlock();
	// an invalid index means there's no free space, return error
	if (newBlockIndex < 0) return -1;
	// allocate that block
	allocBlock(newBlockIndex);
	// create the new struct to store at the block 
	csc452_disk_block newDiskBlock;
	newDiskBlock.nNextBlock = -1;
	// write the new block to disk 
	writeBlock(newBlockIndex, &newDiskBlock);
	// return the index
	return newBlockIndex;
}


/**
 * @brief Get the block containing the directory struct 
 * 
 * @param dirName the name of the directory 
 * @return long -1 if the directory is not found, the block containing the dir
 * 			    otherwise
 */
static long getDirBlock(char *dirName) {
	// retrieve the root node of the filesystem 
	csc452_root_directory *root = readBlock(0);

	// iterate through the directories 
	for (int i = 0; i < root->nDirectories; i++) {
		// if dirName the name of one of the directories match return success
		if (strcmp(dirName, root->directories[i].dname) == 0) {
			return root->directories[i].nStartBlock;
		}
	}

	// if the loop completes without returning then the dir does not exist 
	return -1;
}


/**
 * @brief Get the start block of a given file
 * 
 * @param dirName the directory containing the file
 * @param fname the file name
 * @param fext the file extension
 * @return long -1 if the file is not found, the block containing the file
 * 			    data otherwise.
 */
static long getFileBlock(char *dirName, char *fname, char *fext) {
	
	csc452_directory_entry *dir = readBlock(getDirBlock(dirName));

	// if the directory doesn't exist then neither does the file 
	if (dir == NULL) {
		return -1;
	}

	for (int i = 0; i < dir->nFiles; i++) {
		// check the name and extension 
		int matchingName = strcmp(fname, dir->files[i].fname);
		int matchingExtension = strcmp(fext, dir->files[i].fext);
		// if they both match then return the block 
		if (matchingName == 0 && matchingExtension == 0) {
			return dir->files[i].nStartBlock;
		}
	}

	// if the loop completes without returning then the file does not exist 
	return -1;
}


/**
 * @brief read in the nth block in a file linked list from curr
 * 
 * @param currBlock the block index of the starting block 
 * @param n the index to move to 
 * @return long the block index of the nth block, or -1 if the block isn't in the list
 */
static long getNthFileBlock(long currBlock, long n) {
	csc452_disk_block *curr = readBlock(currBlock);
	while (n-- > 0 && curr != NULL) {
		currBlock = curr->nNextBlock;
		// free the previous blocks as we search 
		free(curr);
		curr = readBlock(currBlock);
	}
	return currBlock;
}


/**
 * @brief Get the size of a given file
 * 
 * @param dirName the directory containing the file
 * @param fname the file name
 * @param fext the file extension
 * @return long -1 if the file is not found, the file size otherwise
 */
static long getFileSize(char *dirName, char *fname, char *fext) {
	
	csc452_directory_entry *dir = readBlock(getDirBlock(dirName));

	// if the directory doesn't exist then neither does the file 
	if (dir == NULL) {
		return -1;
	}

	for (int i = 0; i < dir->nFiles; i++) {
		// check the name and extension 
		int matchingName = strcmp(fname, dir->files[i].fname);
		int matchingExtension = strcmp(fext, dir->files[i].fext);
		// if they both match then return the size 
		if (matchingName == 0 && matchingExtension == 0) {
			return dir->files[i].fsize;
		}
	}

	// if the loop completes without returning then the file does not exist 
	return -1;
}


/**
 * @brief Get the index of a file within a directory 
 * 
 * @param fname the name of the file
 * @param fext the extension of the file 
 * @param dir the directory containing the file
 * @return int -1 if the file isn't in the directory, the index otherwise
 */
static int getFileIndex(char *fname, char *fext, csc452_directory_entry *dir) {
	// search the directory 
	for (int i = 0; i < dir->nFiles; i++) {
		// check the name and extension 
		int matchingName = strcmp(fname, dir->files[i].fname);
		int matchingExtension = strcmp(fext, dir->files[i].fext);
		// if they both match then return the size 
		if (matchingName == 0 && matchingExtension == 0) {
			return i;
		}
	}

	// if the loop completes without returning then the file does not exist 
	return -1;
}


/**
 * @brief check if a directory is empty or not 
 * 
 * @param dir the directory to check the contents of
 * @return int 1 true, int 0 false
 */
static int isDirEmpty(csc452_directory_entry *dir) {
	// iterate through the dir
	for (int i = 0; i < dir->nFiles; i++) {
		// if the mapping is not invalid return false, the dir is not empty
		if (dir->files[i].nStartBlock != -1) {
			return 0;
		}
	}
	// return true, the dir is empty, if the above loop terminates
	return 1;
}


/**
 * @brief simple structure to allow the passing and returning of both a disk
 * 		  block and it's index in a files linked list 
 */
typedef struct disk_block_and_metadata
{
	csc452_disk_block *disk_block;
	long block_index;
	long index;
	csc452_directory_entry *dir;
	long fileIndexInDir;
} disk_block_and_metadata;


/**
 * @brief wrapper to treat reading from the file as like reading from a 
 * 		  standard linked list.
 * 		  note, only seqential reads are support, attempts to read nonsequentially may cause
 * 		  infinite loops
 * 
 * @param curr structure (defined above) representing the curret disk block and associated metadata
 * @param byteIndex index of the byte to read
 * @param buf the buffer to read into
 * @param bufIndex the buffer index to read into 
 * @return int -1 if there is an error, 0 otherwise
 */
static int readByte(disk_block_and_metadata *curr, long byteI, char *buf, long bufI) {
	// determine which block (in the files linked list) the byte index lives
	long indexInFileBlockList = byteI / MAX_DATA_IN_BLOCK;
	// determine if index is in the current block 
	// if it is in the next block use curr->nNextBlock to fetch it
	// otherwise the index indicates a non sequential read - terminate 
	if (indexInFileBlockList != curr->index) {
		// non sequential reads are not supported, if one is detected then terminate 
		if (indexInFileBlockList != curr->index + 1) return -1;
		// read the next block 
		curr->disk_block = readBlock(curr->disk_block->nNextBlock);
		curr->index = curr->index + 1;
		// if the block is null then we've reached the ent of the file 
		if (curr->disk_block == NULL) return 0;
	}

	// determine the index within the block where the indexed byte is located 
	long indexInBlock = byteI % MAX_DATA_IN_BLOCK;
	// write that byte to buf at bufIndex 
	buf[bufI] = curr->disk_block->data[indexInBlock];
	// return success
	return 0;
}


/**
 * @brief method to make sequentially writing bytes to disk like writing bytes to a linked list
 * 	 	  note, this function will save change to disk for all affected blocks except the last 
 * 		  one - therefore the caller must manually write the final value of curr->disk_block
 * 		  to curr->block_index after calling. 
 * 		  note, this function will update curr->dir to reflect changes to the file size, but will
 * 		  NOT save these changes to disk - this must be done by the caller 
 * 
 * @param curr structure (defined above) representing the curret disk block and associated metadata
 * @param byteI the index in the files linked list to write the byte
 * @param toWrite the byte to write
 * @return int 0 if the function successfully write the byte, error otherwise
 */
static int writeByte(disk_block_and_metadata *curr, long byteI, char toWrite) {
	// determine which block (in the files linked list) the byte index lives
	long indexInFileBlockList = byteI / MAX_DATA_IN_BLOCK;
	// determine if index is in the current block 
	// if it is in the next block use curr->nNextBlock to fetch it
	// if curr->nNextBlock is -1 alloc a new block
	if (indexInFileBlockList != curr->index) {
		// non sequential writes are not supported, if one is detected then return an error 
		if (indexInFileBlockList != curr->index + 1) return -1;
		// if the current disk block is the end of the file alloc a new block 
		if (curr->disk_block->nNextBlock < 0) curr->disk_block->nNextBlock = allocNewFileBlock();
		// if the next block still doesn't exist then we are out of space 
		if (curr->disk_block->nNextBlock < 0) return -ENOSPC;
		// write out the current block (since we are moving to the next one)
		writeBlock(curr->block_index, curr->disk_block);
		// load the next block
		curr->block_index = curr->disk_block->nNextBlock;
		// free the previous block before we replace it 
		free(curr->disk_block);
		// replace the previous block
		curr->disk_block = readBlock(curr->block_index);
		curr->index = curr->index + 1;
	}

	// determine the index within the block where the indexed byte is located 
	long indexInBlock = byteI % MAX_DATA_IN_BLOCK;
	// write that byte to buf at bufIndex 
	curr->disk_block->data[indexInBlock] = toWrite;

	// if the current byte index (plus one due to 0 indexing) is larger than the current file
	// size then update the file size to reflect another byte having been written
	if (byteI + 1 > curr->dir->files[curr->fileIndexInDir].fsize) {
		curr->dir->files[curr->fileIndexInDir].fsize = byteI + 1;
	}

	// return success
	return 0;
}



/*
 * ----------------------------- START SYSCALLS -----------------------------
 */ 



/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} else  {
		// plus two on all of these for the null term and extra space to allow 
		// overlong input to be detected
		char dname[MAX_FILENAME + 2] = {0};
		char fname[MAX_FILENAME + 2] = {0};
		char fext[MAX_EXTENSION + 2] = {0};
		// scan the path to extract the directory, filename, and extension
		sscanf(path, PATH_PARSE, dname, fname, fext);

		// success case where he path is a valid directory
		if (fname[0] == 0 && fext[0] == 0 && getDirBlock(dname) != -1) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return 0;
		}

		// success case where the path is a valid file
		if (getFileBlock(dname, fname, fext) != -1) {
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = getFileSize(dname, fname, fext);
			return 0;
		}

		// if this case finishes without returning success then return error
		return -ENOENT;	
	}

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

	// a directory holds two entries, one that represents itself (.) 
	// and one that represents the directory above us (..)
	filler(buf, ".", NULL,0);
	filler(buf, "..", NULL, 0);

	if (strcmp(path, "/") == 0) {
		// get the root 
		csc452_root_directory *root = readBlock(0);
		// if root is NULL then there has been an error 
		if (root == NULL) return -ENOENT;
		// iterate through the directories and fill them into the buffer
		for (int i = 0; i < root->nDirectories; i++) {
			if (root->directories[i].nStartBlock != -1) {
				filler(buf, root->directories[i].dname, NULL,0);
			}
		}
	} else {
		// plus two on all of these for the null term and extra space to allow 
		// overlong input to be detected
		char dname[MAX_FILENAME + 2] = {0};
		char fname[MAX_FILENAME + 2] = {0};
		char fext[MAX_EXTENSION + 2] = {0};
		// scan the path to extract the directory, filename, and extension
		sscanf(path, PATH_PARSE, dname, fname, fext);

		// if the name or extension are filled then the path isn't a directory
		if (fname[0] != 0 || fext[0] != 0) return -ENOENT;
		
		// get the directory 
		csc452_directory_entry *dir = readBlock(getDirBlock(dname));

		// if dir is NULL then the path referenced an invalid directory
		if (dir == NULL) return -ENOENT;

		// iterate through the files and fill them into the buffer
		for (int i = 0; i < dir->nFiles; i++) {
			if (dir->files[i].nStartBlock != -1) {
				// +2 because one char for '.' and one for terminus
				char name[] = "";
				// copy fname into name
				strcat(name,dir->files[i].fname);
				// if the file has an extension then copy that to 
				if (strlen(dir->files[i].fext) > 0) {
					strcat(name,".");
					strcat(name,dir->files[i].fext);
				}
				// fill name into the buffer 
				filler(buf, name, NULL, 0);
			}
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

	// plus two on all of these for the null term and extra space to allow 
	// overlong input to be detected
	char dname[MAX_FILENAME + 2] = {0};
	char fname[MAX_FILENAME + 2] = {0};
	char fext[MAX_EXTENSION + 2] = {0};
	// scan the path to extract the directory, filename, and extension
	sscanf(path, PATH_PARSE, dname, fname, fext);

	// if the name or extension are filled then the path goes past the root
	if (fname[0] != 0 || fext[0] != 0) return -EPERM;
	// ensure the name is a valid length
	if (strlen(dname) > MAX_FILENAME) return -ENAMETOOLONG;
	// check if the directory exists
	if (getDirBlock(dname) != -1) return -EEXIST;

	// get the root 
	csc452_root_directory *root = readBlock(0);

	// get the index of a free block
	long directoryStartBlock = getFirstFreeBlock();
	// if we get an invalid index then there are no free blocks - out of space
	if (directoryStartBlock < 0) return -ENOSPC;
	// allocate that block
	allocBlock(directoryStartBlock);
	// create the new directory with 0 files and write it to the disk
	struct csc452_directory_entry newDir;
	newDir.nFiles = 0;
	writeBlock(directoryStartBlock, &newDir);

	// search for an entry with a negative start block to replace
	for (int i = 0; i < root->nDirectories; i++) {
		if (root->directories[i].nStartBlock == -1) {
			// copy the name
			strcpy(root->directories[i].dname, dname);
			// set the directory start block 
			root->directories[i].nStartBlock = directoryStartBlock;
			// write out the updated root 
			writeBlock(0, root);
			// return success
			return 0;
		}
	}

	// if there are no invalid entries then we will need to access a new one 
	// validate that the are unclaimed directory slots 
	// if there aren't any then return a quota error 
	if (root->nDirectories >= MAX_DIRS_IN_ROOT) return -EDQUOT;
	root->nDirectories++;
	int i = root->nDirectories-1;
	// copy the name
	strcpy(root->directories[i].dname, dname);
	// set the directory start block 
	root->directories[i].nStartBlock = directoryStartBlock;
	// write out the new root 
	writeBlock(0, root);
	// return success
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

	// plus two on all of these for the null term and extra space to allow 
	// overlong input to be detected
	char dname[MAX_FILENAME + 2] = {0};
	char fname[MAX_FILENAME + 2] = {0};
	char fext[MAX_EXTENSION + 2] = {0};
	// scan the path to extract the directory, filename, and extension
	sscanf(path, PATH_PARSE, dname, fname, fext);

	// if the name and extension are not filled then the file is being created
	// in the root - return permission error 
	if (fname[0] == 0 && fext[0] == 0) return -EPERM;
	// ensure the name is a valid length
	if (strlen(fname) > MAX_FILENAME) return -ENAMETOOLONG;
	// ensure the extension is a valid length
	if (strlen(fext) > MAX_EXTENSION) return -ENAMETOOLONG;
	// check if the file already exists
	if (getFileBlock(dname, fname, fext) != -1) return -EEXIST;

	// get the directory 
	long dBlock = getDirBlock(dname);
	csc452_directory_entry *dir = readBlock(dBlock);

	// get the index of a free block
	long fileStartBlock = getFirstFreeBlock();
	// if we get an invalid index then there are no free blocks - out of space
	if (fileStartBlock < 0) return -ENOSPC;
	// allocate that block
	allocBlock(fileStartBlock);
	// create the new directory with 0 files and write it to the disk
	struct csc452_disk_block newFile;
	// mark the new block as the last in the file 
	newFile.nNextBlock = -1;
	writeBlock(fileStartBlock, &newFile);

	// search for an entry with a negative start block to replace
	for (int i = 0; i < dir->nFiles; i++) {
		if (dir->files[i].nStartBlock == -1) {
			// copy the name
			strcpy(dir->files[i].fname, fname);
			// copy the extension 
			strcpy(dir->files[i].fext, fext);
			dir->files[i].fsize = 0;
			dir->files[i].nStartBlock = fileStartBlock;
			// write out the updated directory 
			writeBlock(dBlock, dir);
			// return success
			return 0;
		}
	}

	// if there are no invalid entries then we will need to access a new one 
	// validate that the are unclaimed file slots 
	// if there aren't any then return a quota error 
	if (dir->nFiles >= MAX_FILES_IN_DIR) return -EDQUOT;
	dir->nFiles++;
	int i = dir->nFiles - 1;
	// copy the name
	strcpy(dir->files[i].fname, fname);
	// copy the extension 
	strcpy(dir->files[i].fext, fext);
	dir->files[i].fsize = 0;
	dir->files[i].nStartBlock = fileStartBlock;
	// write out the updated directory 
	writeBlock(dBlock, dir);
	// return success
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

	// plus two on all of these for the null term and extra space to allow 
	// overlong input to be detected
	char dname[MAX_FILENAME + 2] = {0};
	char fname[MAX_FILENAME + 2] = {0};
	char fext[MAX_EXTENSION + 2] = {0};
	// scan the path to extract the directory, filename, and extension
	sscanf(path, PATH_PARSE, dname, fname, fext);

	// the root is a directory
	if (strcmp(path, "/") == 0) return -EISDIR;
	// if both the file name and file extension are not filled then the 
	// path refrences a directory
	if (fname[0] == 0 && fext[0] == 0) return -EISDIR;

	// get the file block 
	long fBlock = getFileBlock(dname, fname, fext);
	// if the block is invalid then the file doesn't exist - return error
	if (fBlock < 0) return -ENOENT;
	// if size is not > 0 return error 
	if (size <= 0) return -1;
	// get the file size
	size_t fileSize = getFileSize(dname, fname, fext);
	// if offset is outside of the file then return error
	if (offset > fileSize) return -EFBIG;

	// attempt to read size bytes from the file 
	long bufIndex = 0;
	long byteIndex = offset;
	// define the starting block and metadata structure 
	// ignore dir, fileIndex and block_index as they're not needed for reading 
	disk_block_and_metadata *curr = malloc(sizeof(disk_block_and_metadata));
	curr->disk_block = readBlock(fBlock);
	curr->index = 0;
	// while the buffer is not full and we haven't reached the end of the file
	// read the next byte into the next buffer index 
	int err = 0;
	while (bufIndex < size && curr->disk_block != NULL && err >= 0) {
		err = readByte(curr, byteIndex, buf, bufIndex);
		bufIndex++;
		byteIndex++;
	}

	// return the buffer index - indicating how many bytes we read 
	return bufIndex;
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

	// plus two on all of these for the null term and extra space to allow 
	// overlong input to be detected
	char dname[MAX_FILENAME + 2] = {0};
	char fname[MAX_FILENAME + 2] = {0};
	char fext[MAX_EXTENSION + 2] = {0};
	// scan the path to extract the directory, filename, and extension
	sscanf(path, PATH_PARSE, dname, fname, fext);

	// the root is a directory
	if (strcmp(path, "/") == 0) return -EISDIR;
	// if both the file name and file extension are not filled then the 
	// path refrences a directory
	if (fname[0] == 0 && fext[0] == 0) return -EISDIR;

	// get the file block 
	long fBlock = getFileBlock(dname, fname, fext);
	// if the block is invalid then the file doesn't exist - return error
	if (fBlock < 0) return -ENOENT;
	// if size is not > 0 return error 
	if (size <= 0) return -1;
	// if offset is outside of the file then return error
	if (offset > getFileSize(dname, fname, fext)) return -EFBIG;

	// get the directory containing the file (so we can update the file size)
	long dBlock = getDirBlock(dname);
	csc452_directory_entry *dir = readBlock(dBlock);
	// find and store the index of the file in the directory
	int fileIndex = getFileIndex(fname, fext, dir);
	
	// determine the starting block of the write
	long startIndex = offset / MAX_DATA_IN_BLOCK;
	long startBlock = getNthFileBlock(fBlock, startIndex);
	// if the starting block isn't found then we might be sitting on a block edge 
	// try getting startIndex - 1, allocing a new block, and then getting startIndex again 
	if (startBlock < 0) {
		long holdBlock = getNthFileBlock(fBlock, startIndex - 1);
		csc452_disk_block *hold = readBlock(holdBlock);
		// if hold is null then we aren't on an edge and there is something wrong with write's 
		// offset 
		if (hold == NULL) return -EFBIG;
		// if there is a block after hold then something has gone wrong 
		if (hold->nNextBlock > 0) return -EFBIG;
		// if hold exists and doesn't have a next block then alloc a new block 
		hold->nNextBlock = allocNewFileBlock();
		// if we can't alloc a block then we are out of space 
		if (hold->nNextBlock < 0) return -ENOSPC;
		// store the new start block
		startBlock = hold->nNextBlock;
		// write the hold block to connect the linked list 
		writeBlock(holdBlock, hold);
		// free the startBlock - 1 block
		free(hold);
	} 
	// load the start block 
	csc452_disk_block *start = readBlock(startBlock);

	// attempt to write size bytes to the file 
	long bufIndex = 0;
	long byteIndex = offset;
	// define the starting block and metadata structure  
	disk_block_and_metadata *curr = malloc(sizeof(disk_block_and_metadata));
	curr->disk_block = start;
	curr->block_index = startBlock;
	curr->index = startIndex;
	curr->dir = dir;
	curr->fileIndexInDir = fileIndex;
	// while there are still byte to write write the next byte to disk
	int err = 0;
	while (bufIndex < size && err >= 0) {
		err = writeByte(curr, byteIndex, buf[bufIndex]);
		bufIndex++;
		byteIndex++;
	}
	// if the loop terminates due to an out of space error return error
	if (err == -ENOSPC) return -ENOSPC;

	// write the final block of the file 
	writeBlock(curr->block_index, curr->disk_block);
	// write the directory block
	writeBlock(dBlock, curr->dir);
	// free the metadata structure
	free(curr->dir);
	free(curr->disk_block);
	free(curr);
	// return the buffer index - indicating how many bytes we wrote 
	return bufIndex;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;

	// plus two on all of these for the null term and extra space to allow 
	// overlong input to be detected
	char dname[MAX_FILENAME + 2] = {0};
	char fname[MAX_FILENAME + 2] = {0};
	char fext[MAX_EXTENSION + 2] = {0};
	// scan the path to extract the directory, filename, and extension
	sscanf(path, PATH_PARSE, dname, fname, fext);

	// the root can't be removed, so return not a directory 
	if (strcmp(path, "/") == 0) return -ENOTDIR;
	// if the name or extension are filled then the path goes past the root
	// and therefore isn't a directory 
	if (fname[0] != 0 || fext[0] != 0) return -ENOTDIR;
	
	// get the directory 
	long dBlock = getDirBlock(dname);
	csc452_directory_entry *dir = readBlock(dBlock);
	// if dir is NULL then the path referenced an invalid directory
	if (dir == NULL) return -ENOENT;
	// if the dir is not empty then return error 
	if (!isDirEmpty(dir)) return -ENOTEMPTY;

	// retrieve the root 
	csc452_root_directory *root = readBlock(0);
	// search root for the directory entry
	for (int i = 0; i < root->nDirectories; i++) {
		// mark it's start block as -1 indicating that it's an invalid entry
		if (strcmp(root->directories[i].dname, dname) == 0) {
			root->directories[i].nStartBlock = -1;
		}
	}
	// free the directory 
	freeBlock(dBlock);
	// write the updated root 
	writeBlock(0, root);
	// return success 
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
    (void) path;

	// plus two on all of these for the null term and extra space to allow 
	// overlong input to be detected
	char dname[MAX_FILENAME + 2] = {0};
	char fname[MAX_FILENAME + 2] = {0};
	char fext[MAX_EXTENSION + 2] = {0};
	// scan the path to extract the directory, filename, and extension
	sscanf(path, PATH_PARSE, dname, fname, fext);

	// the root can't be removed, so return is a directory 
	if (strcmp(path, "/") == 0) return -EISDIR;
	// if both the file name and file extension are not filled then the 
	// path refrences a directory
	if (fname[0] == 0 && fext[0] == 0) return -EISDIR;

	// get the file block 
	long fBlock = getFileBlock(dname, fname, fext);
	// if the block is invalid then the file doesn't exist - return error
	if (fBlock < 0) return -ENOENT;
	// free the file from disk 
	freeFile(fBlock);

	// get the directory 
	long dBlock = getDirBlock(dname);
	csc452_directory_entry *dir = readBlock(dBlock);
	// iterate through the file 
	for (int i = 0; i < dir->nFiles; i++) {
		// check the name and extension 
		int matchingName = strcmp(fname, dir->files[i].fname);
		int matchingExtension = strcmp(fext, dir->files[i].fext);
		// if they both match then mark the file invalid 
		if (matchingName == 0 && matchingExtension == 0) {
			dir->files[i].nStartBlock = -1;
		}
	}
	// write the updated directory 
	writeBlock(dBlock, dir);
	// return success
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
