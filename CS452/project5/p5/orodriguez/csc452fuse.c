/*
FUSE: Filesystem in Userspace
Author: Orlando Rodriguez
Class: CSC 452
Date: May 4th, 2022

Implemented syscalls for FUSE
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

/*
 * This function handles the loading of the root from the disk. 
 * Returns the root directory struct
 */
static csc452_root_directory* csc452_opendisk() {
    FILE* handle = fopen(".disk", "rb"); // make sure this is the rihgt file to open
    fseek(handle, 0, SEEK_SET);
    csc452_root_directory* root = (csc452_root_directory*) calloc(1, sizeof(csc452_root_directory));
    fread(root, BLOCK_SIZE, 1, handle);
    fclose(handle);
    return root;
}

/*
 * This struct handles keeping track of all the written and unwritten blocks
 */
struct csc452_memory_tracker {
    char memoryArray[1280];
};

typedef struct csc452_memory_tracker csc452_memory_tracker;


/*
 * This function returns the memory tracking struct from the .disk
 */
static csc452_memory_tracker* get_memory() {
	csc452_memory_tracker* memory = (csc452_memory_tracker*) calloc(1, sizeof(csc452_memory_tracker));
	FILE* handle = fopen(".disk", "rb"); // make sure this is the rihgt file to open
	fseek(handle, 10236 * BLOCK_SIZE, SEEK_SET);
	fread(memory, 2 * BLOCK_SIZE + 256, 1, handle);
	fclose(handle);
	return memory;
}

/*
 * This function writes a copy of the memory tracker back to its original location on .disk
 */
static void write_memory(csc452_memory_tracker* memory) {
	FILE* handle = fopen(".disk", "rb+"); // make sure this is the rihgt file to open
	fseek(handle, 10236 * BLOCK_SIZE, SEEK_SET);
	fwrite(memory, 2 * BLOCK_SIZE + 256, 1, handle);
	free(memory);
	fclose(handle);
}

/*
 * This function initializes memory to reflect the presence of root and the memory tracker
 */
static void memory_init() {
    csc452_memory_tracker* memory = get_memory();
    memory->memoryArray[0] = 0x80;
    memory->memoryArray[1279] = 0x7;
    write_memory(memory);
}

/*
 * Checks if a block has been written to in memory
 */
static int is_written(int block) {
	csc452_memory_tracker* memory = get_memory();
	int charIndex = block / 8;
	int charIndexOffset = block % 8;
	char memoryArrayBit = (memory->memoryArray[charIndex] >> (7 - charIndexOffset)) & 1;
	if (memoryArrayBit == 1)
		return 1;
	else
		return 0;
}

/*
 * Flips a block between 1 and 0 on .disk
 */
static void flip_block(int block) {
	csc452_memory_tracker* memory = get_memory();
	int charIndex = block / 8;
	int charIndexOffset = block % 8;
	memory->memoryArray[charIndex] ^= (1 << (7 - charIndexOffset));
	write_memory(memory);
}

/*
 * Checks if memory needs to be initialized
 */
static int needs_init() {
	csc452_memory_tracker* memory = get_memory();
	if (memory->memoryArray[0] == 0) 
		return 1;
	else 
		return 0;
}

/*
 * Looks for the next free block
 */
static long next_free_block() {
	for (int i = 0; i < 10240; i++) {
		if (!is_written(i))
			return i;
	}
	return -1;
}

/*
 * Writes a root structure back to where it belongs in .disk
 */
static void write_root_to_block(csc452_root_directory* root) {
	FILE* handle = fopen(".disk", "rb+"); // make sure this is the rihgt file to open
	fseek(handle, 0, SEEK_SET);
	fwrite(root, BLOCK_SIZE, 1, handle);
	fclose(handle);
}

/*
 * Reads a directory struct from .disk
 */
static csc452_directory_entry* read_directory_from_block(long block) {
	FILE* handle = fopen(".disk", "rb"); // make sure this is the rihgt file to open
	fseek(handle, block * BLOCK_SIZE, SEEK_SET);
	csc452_directory_entry* directory = (csc452_directory_entry*) calloc(1, sizeof(csc452_directory_entry));
	fread(directory, BLOCK_SIZE, 1, handle);
	fclose(handle);
	return directory;
}

/*
 * Writes a directory structure back to where it belongs in .disk
 */
static void write_directory_to_block(csc452_directory_entry* directory, long block) {
	FILE* handle = fopen(".disk", "rb+"); // make sure this is the rihgt file to open
	fseek(handle, block * BLOCK_SIZE, SEEK_SET);
	fwrite(directory, BLOCK_SIZE, 1, handle);
	fclose(handle);
}

/*
 * Reads a file struct from .disk
static csc452_disk_block* read_file_from_block(long block) {
	FILE* handle = fopen(".disk", "rb"); // make sure this is the rihgt file to open
	fseek(handle, block * BLOCK_SIZE, SEEK_SET);
	csc452_disk_block* file = (csc452_disk_block*) calloc(1, sizeof(csc452_disk_block));
	fread(file, BLOCK_SIZE, 1, handle);
	fclose(handle);
	return file;
}
 */

/*
 * Writes a file structure back to where it belongs in .disk
static void write_file_to_block(csc452_disk_block* file, long block) {
	FILE* handle = fopen(".disk", "rb+"); // make sure this is the rihgt file to open
	fseek(handle, block * BLOCK_SIZE, SEEK_SET);
	fwrite(file, BLOCK_SIZE, 1, handle);
	fclose(handle);
}
 */

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int flag = 0;
	if (needs_init())
		memory_init();
	int res = 0;

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		char* directory = (char*) calloc(MAX_FILENAME + 1, sizeof(char));
		char* filename = (char*) calloc(MAX_FILENAME + 1, sizeof(char));
		char* extension = (char*) calloc(MAX_EXTENSION + 1, sizeof(char));
		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	
		csc452_root_directory* root = csc452_opendisk();
		// If path is a directory
		if (strcmp(filename, "") == 0) {
			for (int i = 0; i < MAX_DIRS_IN_ROOT; i++) {
				if (strcmp(root->directories[i].dname, directory) == 0) {
					// If it exists
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					flag = 1;
					break;
				}
			}
			if (!flag)
				res = -ENOENT;
		} else {
			// path is a file
			int i;
			for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
				if (strcmp(root->directories[i].dname, directory) == 0) {
					break;
				}
			}
			// Directory path doesn't exist
			if (i == MAX_DIRS_IN_ROOT)
				res = -ENOENT;
			else {
				// Directory path does exist
				csc452_directory_entry* dEntry = read_directory_from_block(root->directories[i].nStartBlock);
				for (i = 0; i < MAX_FILES_IN_DIR; i++) {
					if (strcmp(dEntry->files[i].fname, filename) == 0 && 
							strcmp(dEntry->files[i].fext, extension) == 0) {
						// File path does exist
						stbuf->st_mode = S_IFREG | 0666;
						stbuf->st_nlink = 2;
						stbuf->st_size = dEntry->files[i].fsize;
						break;
					}
				}
				// File path doesn't exist
				if (i == MAX_FILES_IN_DIR)
					res = -ENOENT;
			}
		}
		//If the path does exist and is a directory:
		//stbuf->st_mode = S_IFDIR | 0755;
		//stbuf->st_nlink = 2;

		//If the path does exist and is a file:
		//stbuf->st_mode = S_IFREG | 0666;
		//stbuf->st_nlink = 2;
		//stbuf->st_size = file size
		
		//Else return that path doesn't exist
		//res = -ENOENT;
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
	if (needs_init())
		memory_init();
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	char* directory = (char*) calloc(MAX_FILENAME + 1, sizeof(char));
	char* filename = (char*) calloc(MAX_FILENAME + 1, sizeof(char));
	char* extension = (char*) calloc(MAX_EXTENSION + 1, sizeof(char));
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	csc452_root_directory* root = csc452_opendisk();

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);

		for (int i = 0; i < MAX_DIRS_IN_ROOT; i++) {
			if (strcmp(root->directories[i].dname, "") != 0) 
				filler(buf, root->directories[i].dname, NULL, 0);
		}
	}
	else {
		int i;
		for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
			if (strcmp(root->directories[i].dname, directory) == 0) 
				break;
		}
		// Folder does not exist
		if (i == MAX_DIRS_IN_ROOT)
			return -ENOENT;

		// Does exist
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);

		csc452_directory_entry* dEntry = read_directory_from_block(root->directories[i].nStartBlock);
		for (int j = 0; j < MAX_FILES_IN_DIR; j++) {
			if (strcmp(dEntry->files[i].fname, "") != 0) {
				char fullFileName[MAX_FILENAME + MAX_EXTENSION];
				char fileE[MAX_EXTENSION];
				strcpy(fullFileName, filename);
				strcpy(fileE, extension);
				strcat(fullFileName, fileE);
				filler(buf, fullFileName, NULL, 0);
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
	(void) mode;

	if (needs_init())
		memory_init();
	csc452_root_directory* root = csc452_opendisk();

	char* directory = (char*) calloc(MAX_FILENAME + 1, sizeof(char));
	char* filename = (char*) calloc(MAX_FILENAME + 1, sizeof(char));
	char* extension = (char*) calloc(MAX_EXTENSION + 1, sizeof(char));
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// Checks if the directory already exists
	for (int i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		if (strcmp(directory, root->directories[i].dname) == 0)
			return -EEXIST;
	}

	// Checks if the directory is in root or not
	if (strcmp(filename, "") != 0)
	    return -EPERM;
	
	// Checks directory name length
	if (strlen(directory) > MAX_FILENAME)
		return -ENAMETOOLONG;

	// If it is truly a new valid directory do the following
	root->nDirectories++;
	int i;
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		if (strcmp(root->directories[i].dname, "") == 0)
			break;
	}

	strcpy(root->directories[i].dname, directory);
	root->directories[i].nStartBlock = next_free_block();
	flip_block(root->directories[i].nStartBlock); // Write to memory

	write_root_to_block(root); // Write root back to file

	// Make new directory entry and add it to disk
	csc452_directory_entry newDir;
	newDir.nFiles = 0;
	write_directory_to_block(&newDir, root->directories[i].nStartBlock);

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
 * Didn't get to finish this one completely
 *
 */
static int csc452_rmdir(const char *path)
{
	fprintf(stderr, "Entered rmdir at all\n");
	(void) path;
	if (needs_init())
		memory_init();

	fprintf(stderr, "RMDIR PT1\n");
	char* directory = (char*) calloc(MAX_FILENAME + 1, sizeof(char));
	char* filename = (char*) calloc(MAX_FILENAME + 1, sizeof(char));
	char* extension = (char*) calloc(MAX_EXTENSION + 1, sizeof(char));
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	fprintf(stderr, "RMDIR PT2\n");

	if (strcmp(filename, "") != 0)
		return -ENOTDIR;

	csc452_root_directory* root = csc452_opendisk();
	int i;
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		if (strcmp(root->directories[i].dname, directory) == 0)
			break;
	}
	
	if (i == MAX_DIRS_IN_ROOT)
		return -ENOENT;

	fprintf(stderr, "RMDIR PT3\n");

	csc452_directory_entry* dEntry = read_directory_from_block(root->directories[i].nStartBlock);

	for (i = 0; i < MAX_FILES_IN_DIR; i++) {
		fprintf(stderr, "RMDIR PT3b < %s >\n", dEntry->files[i].fname);
		if (strcmp(dEntry->files[i].fname, "") != 0)
			return -ENOTEMPTY;
	}

	fprintf(stderr, "RMDIR PT4\n");

	// If directory can be removed
	
	// Edit and write root back to block
	long tempBlock;
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		if (strcmp(root->directories[i].dname, directory) == 0) {
			tempBlock = root->directories[i].nStartBlock;
			for (int c = 0; c < MAX_FILENAME; c++) {
				root->directories[i].dname[c] = 0;
			}
			root->directories[i].nStartBlock = 0;
			break;
		}
	}
	write_root_to_block(root);

	fprintf(stderr, "RMDIR PT5\n");

	// Mark memory block as unused
	flip_block(tempBlock);

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
