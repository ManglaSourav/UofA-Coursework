/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/
/**
 * Author: Samantha Mathis
 * Class: CSC 452
 * 
 * file: csc452fuse.c
 * 
 * Purpose: This program creates syscalls that implement a file system using the FUSE linux kernel extension
 * It creates a two-level directory system, with restrictions/simplifications. 
 **/ 

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

int block_status[10240];
//block_status[0] = 1;

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
/**
 * Purpose: To keep track of the free block space
 **/
struct block_status
{
	int blocks[512 / sizeof(int)];
};

/**
 * Purpose: To find the root
 * return the root directory object
 **/
struct csc452_root_directory* find_root(){
	char *buffer = malloc(sizeof(char)*512);
	FILE *fp;
	fp = fopen(".disk", "rb");
	fseek(fp, 0, SEEK_SET);
	fread(buffer, sizeof(char) * 512, 1, fp);
	fclose(fp);
	return (csc452_root_directory*) buffer;
}

/**
 * Purpose: To find the directory entry given an offset
 * param: offset, which is where the the directory entry lives
 * return the directory entry object
 **/
struct csc452_directory_entry* find_directory(long offset){
	char *buffer = malloc(sizeof(char)*512);
	FILE *fp;
	fp = fopen(".disk", "rb");
	fseek(fp, offset, SEEK_SET);
	fread(buffer, sizeof(char) *512, 1, fp);
	fclose(fp);
	return (csc452_directory_entry*) buffer;
}

/**
 * Purpose: To find the disk block of where the file are stored at given an offset
 * param: offset, which is where the the file on disk lives
 * return the disk block object
 **/
struct csc452_disk_block* find_file(long offset){
	char *buffer = malloc(sizeof(char)*512);
	FILE *fp;
	fp = fopen(".disk", "rb");
	fseek(fp, offset, SEEK_SET);
	fread(buffer, sizeof(char)*512, 1, fp);
	fclose(fp);
	return (csc452_disk_block*) buffer;

}

/**
 * Purpose: To find the block status (struct created to hold where the next available block is) from where its being stored on disk
 * param: index, which is where the the file on disk lives
 * return the block status object
 **/
struct block_status* get_block_status(int index) {
	char *buffer = malloc(sizeof(char)*512);
	FILE *fp;
	fp = fopen(".disk", "rb");
	fseek(fp, (10237L * 512L) + (index * 512), SEEK_SET);
	fread(buffer, sizeof(char)*512, 1, fp);
	fclose(fp);
	return (struct block_status*) buffer;
}

/**
 * Purpose: To write/update root onto disk
 * param: root, which is a root_directory object being written back onto disk
 **/
void write_root(struct csc452_root_directory* root){
	FILE* fp;
	fp = fopen(".disk", "rb+");
	fseek(fp, 0, SEEK_SET);
	fwrite(root, sizeof(char)*512, 1, fp);
	fclose(fp);
}

/**
 * Purpose: To write/update directory entry onto disk
 * param: direct, which is a directory entry object being written back onto disk
 * param: offset, which is the location of where to write the entry
 **/
void write_entry(struct csc452_directory_entry* direct, long offset){
	FILE *fp;
	fp = fopen(".disk", "rb+");
	fseek(fp, offset, SEEK_SET);
	fwrite(direct, sizeof(char)*512, 1, fp);
	fclose(fp);
	
}

/**
 * Purpose: To write/update block from a file onto disk
 * param: block, which is a disk block object being written back onto disk
 * param: offset, which is the location of where to write the block
 **/
void write_file(struct csc452_disk_block* block, long offset){
	FILE *fp;
	fp = fopen(".disk", "rb+");
	fseek(fp, offset, SEEK_SET);
	fwrite(block, sizeof(char) *512, 1, fp);
	fclose(fp);

}

/**
 * Purpose: To write/update block status onto disk
 * param: status, which contains the block status object, indicating if that block is available or not
 * param: index, which is the location of where to write the block status
 **/
void write_block_status(struct block_status* status, int index) {
	FILE *fp;
	fp = fopen(".disk", "rb+");
	fseek(fp, (10237L * 512L) + (index * 512), SEEK_SET);
	fwrite(status, sizeof(char) * 512, 1, fp);
	fclose(fp);
}

/**
 * Purpose: To find if a string maps to an existing directory
 * param: directory, which char* of the name of a potential directory we are searching for
 * return true (1) or false (0) if there is a match of the directory in the root;
 **/
int is_directory(char *directory){
	struct csc452_root_directory* root = find_root();
	int i;
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++){
		if (strcmp(directory, root->directories[i].dname) == 0){
			return 1;
		}
	}
	return 0;
}

/**
 * Purpose: To find the first free block to write to
 * return index, which is the location of the first free block in the block status object array
 **/
int find_first_free_block() {
	int i;
	int index = -1;
	// We have 3 blocks set aside for keeping track of free blocks.
	for (i = 0; i < 3; i++) {
		int j;
		// Each block has 128 integers (4 bytes each).
		for (j = 0; j < 128; j++) {
			int k;
			// Each integer has 32 bytes.
			for (k = 0; k < 32; k++) {
				index++;
				struct block_status *status = get_block_status(i);
				// Skip the root block, as that will always be used.
				// Don't go past the available number of blocks, as our 3 blocks for the array span more space than number of disks we have.
				if (!(i == 0 && j == 0 && k == 0) && index < 10237) {
					// Extract the single bit.
					int cont = status->blocks[j];
					int curr_block_status = cont << k >> 31;
					// If it's available, set it to not be available.
					if (curr_block_status == 0) {
						status->blocks[j] = status->blocks[j] | (1 << (31 - k));
						write_block_status(status, i);
						return index;
					}
				}
			}
		}
	}
	return -1;
}

/**
 * Purpose: To find if a string maps to an existing file
 * param: filename, which char* of the name of a potential file we are searching for
 * param: directory, which char* of the name of the extension for the file we are searching for
 * param: directory, which char* of the name of a potential directory we are looking in
 * return true (1) or false (0) if there is a match of the file in the directory in root;
 **/
int is_file(char *filename, char *extension, char *directory){
	struct csc452_root_directory* root = find_root();
	int i;
	//Finds the directory in root
	for (i = 0; i < MAX_DIRS_IN_ROOT;i++){
		if (strcmp(directory, root->directories[i].dname)== 0){
			break;
		}
	}
	//if it looped through root and didn't find a matching directory
	if (i == MAX_DIRS_IN_ROOT){
		return 0;
	// if there is a matching directory loop through the entries for the filename/extension
	}else{
		long offset = root->directories[i].nStartBlock;
		struct csc452_directory_entry* entry = find_directory(offset);

		int j;
		for (j = 0; j < MAX_FILES_IN_DIR;j++){
			if (strcmp(filename, entry->files[j].fname) == 0){
				if (strcmp(extension, entry->files[j].fext) == 0){
					return 1;
				}
			}
		}
	}
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
		char directory[MAX_FILENAME + 1];
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];

		int found = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		int d_exists = is_directory(directory);
		int f_exists = is_file(filename, extension, directory);
		//Checks if path doesnt exists and is a directory
		if (found==1 && d_exists == 1){
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		//Checks if path does exists and is a file
		}else if (found ==3 && f_exists == 1){
			struct csc452_root_directory *root = find_root();
			int i; 
			for (i = 0; i < MAX_DIRS_IN_ROOT;i++){
				if (strcmp(directory, root->directories[i].dname)==0){
					break;
				}
			}
			csc452_directory_entry *entry = find_directory(root->directories[i].nStartBlock);
			int j;
			for(j = 0; j < MAX_FILES_IN_DIR;j++){
				if (strcmp(filename, entry->files[j].fname)==0){
					if (strcmp(extension, entry->files[j].fext)==0){
						break;
					}
				}
			}
			off_t file_size = entry->files[j].fsize;
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = file_size;
		//if the path does not exist
		}else{
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
		//Finds the directory by searching through root for a match
		struct csc452_root_directory *root = find_root();
		int i;
		for (i = 0; i < MAX_DIRS_IN_ROOT; i++){
			if (strcmp(root->directories[i].dname,"\0")==0){
				continue;
			}
			filler(buf, strdup(root->directories[i].dname), NULL, 0);
		}
	}
	else {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		char directory[MAX_FILENAME + 1];
		sscanf(path, "/%s", directory);
		if (strchr(directory, '/') != NULL){
			return -ENOENT;
		}
		// All we have _right now_ is root (/), so any other path must
		// not exist. 
		struct csc452_root_directory *root = find_root();
		int i;
		for (i = 0; i < MAX_DIRS_IN_ROOT;i++){
			if (strcmp(directory, root->directories[i].dname)==0){
				break;
			}
		}
		long offset = root->directories[i].nStartBlock;
		struct csc452_directory_entry* entry = find_directory(offset);
		int j;
		for (j = 0; j < MAX_FILES_IN_DIR;j++){
			char final[13];
			int k;
			for (k = 0; k < 13; k++) {
				final[k] = '\0';
			}
			//Puts file name together
			strcat(final, entry->files[j].fname);
			strcat(final, ".");
			strcat(final, entry->files[j].fext);
			filler(buf, strdup(final), NULL, 0);
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
	char directory[MAX_FILENAME + 1];
	sscanf(path, "/%s", directory);
	//checks if trying to create directory outside of root
	if (strchr(directory, '/') != NULL){
		return -EPERM;
	}
	//If name of directory is too long
	else if (strlen(directory) > 8){
		return -ENAMETOOLONG;
	}else if (is_directory(directory)==1){
		return -EEXIST;
	}
	//searches through root until it finds an spot to add the directory entry
	struct csc452_root_directory *root = find_root();
	int i;
	for (i=0;i<MAX_DIRS_IN_ROOT;i++){
		if (strcmp(root->directories[i].dname, "\0")==0){
			break;
		}
	}
	strcpy(root->directories[i].dname, directory);
	int blockNum = find_first_free_block();
	long offset = 512L * (long) blockNum;
	root->directories[i].nStartBlock = offset;
	root->nDirectories++;
	write_root(root);
	(void) path;
	(void) mode;

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
	char directory[MAX_FILENAME +1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION +1];
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	int f_exists = is_file(filename, extension, directory);
	//Checks to make sure you are adding a file into a directory not into root
	if (strlen(filename) == 0 && strlen(extension)==0){
		return -EPERM;
	//Checks if the filename and extensions aren't too long
	}else if (strlen(filename) > 8 || strlen(extension) > 3){
		return -ENAMETOOLONG;
	//Checks if the file already exists
	}else if (f_exists == 1){
		return -EEXIST;
	}
	//Finds the root inorder to find the where the directory is located
	struct csc452_root_directory *root = find_root();
	int i;
	for (i = 0; i < root->nDirectories;i++){
		if (strcmp(root->directories[i].dname, directory) == 0){
			break;
		}
	}
	long offset = root->directories[i].nStartBlock;
	//finds the directory entry based on the StartBlock from root 
	//in order to search through the files for a spot to add the file information
	struct csc452_directory_entry *entry = find_directory(offset);
	int j;
	for (j = 0; j < MAX_FILES_IN_DIR; j++){
		if (strcmp(entry->files[j].fname, "\0")==0){
			break;
		}
	}
	strcpy(entry->files[j].fname, filename);
	strcpy(entry->files[j].fext, extension);
	int blockNum = find_first_free_block();
	entry->files[j].nStartBlock = 512L * (long)blockNum;
	block_status[blockNum] = 1;
	entry->files[j].fsize = 0;
	entry->nFiles++;
	write_entry(entry, offset);
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
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	int f_exists = is_file(filename, extension, directory);
	//Checks if the file does not exist
	if (f_exists != 1){
		int d_exists = is_directory(directory);
		//Checks if the directory exists
		if (d_exists == 1){
			return -EISDIR;
		}
	}
	//Searches through root to find where the matching directory name is stored
	struct csc452_root_directory *root = find_root();
	int i;
	for(i = 0; i < root->nDirectories;i++){
		if (strcmp(directory, root->directories[i].dname)==0){
			break;
		}
	}
	//Searches through the directory entry to find where the matching filename/extension are stored
	struct csc452_directory_entry *entry = find_directory(root->directories[i].nStartBlock);
	int j;
	for (j = 0; entry->nFiles;j++){
		if (strcmp(filename, entry->files[j].fname)==0){
			if (strcmp(extension, entry->files[j].fext)==0){
				break;
			}
		}
	}
	//Checks that size is greater than 0 and that the offset is less than the file size for the file
	//at the directory entry
	if (size > 0 && offset <= entry->files[j].fsize){
		//reads in teh data
		long file_offset = entry->files[j].nStartBlock;
		int current_pos = MAX_DATA_IN_BLOCK;
		//Needs to loop through each block and grab the data that was written for the file
		while (current_pos < offset){
			struct csc452_disk_block *block = find_file(file_offset);
			file_offset = block->nNextBlock;
			current_pos += MAX_DATA_IN_BLOCK;
		}
		//if there are left overs that dont take up a full block
		int initial_start = offset % MAX_DATA_IN_BLOCK;
		int test_index = 0;
		int buf_index = 0;
		long mfsize = entry->files[j].fsize;
		while (mfsize > 0){
		    struct csc452_disk_block *block = find_file(file_offset);
			int k;
			for (k = initial_start; k < MAX_DATA_IN_BLOCK; k++){
				if (block->data[k] == '\0'){
					buf[buf_index] = '\0';
					break;
				}
				buf[buf_index] = block->data[k];
				buf_index++;
			}
			mfsize -= MAX_DATA_IN_BLOCK;
			initial_start = 0;
			file_offset = block->nNextBlock;
			test_index++;
		}
	}

	size = entry->files[j].fsize;

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
	char directory[MAX_FILENAME +1];
	char filename[MAX_FILENAME +1];
	char extension[MAX_EXTENSION +1];
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	int f_exists = is_file(filename, extension, directory);
	//Checks if the file doesn't exist
	if (f_exists != 1){
		return -ENOENT;
	//Checks that the size is greater than 0
	}else if (size > 0){
		//Searches through root to find where the matching directory is stored
		struct csc452_root_directory *root = find_root();
		int i;
		for (i = 0; i < root->nDirectories; i++){
			if (strcmp(root->directories[i].dname, directory)==0){
				break;
			}
		}
		//Searches through the directory entry to find where the matching filename is stored 
		// after finding the directory entry from the startblock offset
		struct csc452_directory_entry *entry = find_directory(root->directories[i].nStartBlock);
		int j;
		for (j = 0; j < entry->nFiles;j++){
			if (strcmp(filename, entry->files[j].fname)==0){
				if (strcmp(extension, entry->files[j].fext)==0){
					break;
				}
			}
	
		}
	
		//checks if offset is greater than the filesize
		if (offset > entry->files[j].fsize){
			return -EFBIG;
		}
		int current_pos = MAX_DATA_IN_BLOCK;
		long prev_offset = -1;
		long file_offset = entry->files[j].nStartBlock;
		//Loops through the block data for the file to write	
		while (current_pos < offset){
			struct csc452_disk_block * block = find_file(file_offset);
			prev_offset = file_offset;
			file_offset = block->nNextBlock;
			current_pos += MAX_DATA_IN_BLOCK;
		}
		//Fixes the last block's file offset from -1
		if (file_offset == -1) {
			struct csc452_disk_block *block = find_file(prev_offset);
			int blockNum = find_first_free_block();
			long newOffset = 512L * blockNum;
			block->nNextBlock = newOffset;
			write_file(block, prev_offset);
			file_offset = newOffset;
		}
		//left over data, that doesn't take up a full block
		int initial_start = offset % MAX_DATA_IN_BLOCK;
		size_t msize = size;
		int buf_index =0;
		while (msize > MAX_DATA_IN_BLOCK){
			struct csc452_disk_block *block = find_file(file_offset);
			int k;
			for(k = initial_start; k < MAX_DATA_IN_BLOCK;k++){
				block->data[k] = buf[buf_index];
				buf_index += 1;
			}
			initial_start = 0;
			msize -= MAX_DATA_IN_BLOCK;
	
			int blockNum = find_first_free_block();
			if (blockNum == -1){
				return -ENOSPC;
			}
			long new_offset = 512L * (long) blockNum;
			block->nNextBlock = new_offset;
			write_file(block, file_offset);
			file_offset = new_offset;
			block_status[blockNum] = 1;
		}

		if (msize != 0){
			struct csc452_disk_block *block = find_file(file_offset);
			memcpy(block->data, buf, msize);
			block->nNextBlock = -1;
			write_file(block, file_offset);
		}
		//Resets the file size in the directory entry object
		if (offset > 0) {
			entry->files[j].fsize += size;
		} else {
			entry->files[j].fsize = size;
		}
		//updates entry
		write_entry(entry, root->directories[i].nStartBlock);
	}
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	int f_exists = is_file(filename, extension, directory);
	int d_exists = is_directory(directory);
	//Checks if directory doesn't exist
	if (d_exists != 1){
		return -ENOENT;
	}
	//searches through root to find where the matching directory name is stored
	struct csc452_root_directory *root = find_root();
	int i;
	for (i = 0; i < root->nDirectories; i++){
		if (strcmp(directory, root->directories[i].dname)==0){
			break;
		}
	}
	//searches through the directory entry object to find where the matching filename name is stored
	// after grabbing the directory entry based on the startblock offset
	struct csc452_directory_entry *entry = find_directory(root->directories[i].nStartBlock);
	//Checks if the directory isn't empty
	if (entry->nFiles != 0){
		return -ENOTEMPTY;
	//Checks if its not a directory if a file exists
	}else if (f_exists == 1){
		return -ENOTDIR;
	}
	// Clear the directory entry of data.
	int j;
	for (j = 0; j < strlen(root->directories[i].dname);j++){
		root->directories[i].dname[j] = '\0';
	}
	// Mark that block as being available.
	root->directories[i].nStartBlock = 0;
	// Write the cleared entry to disk.
	write_entry(entry, root->directories[i].nStartBlock);
	root->nDirectories--;
	// Update the root entry in disk.
	write_root(root);
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
    (void) path;
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	int f_exists = is_file(filename, extension, directory);
	int d_exists = is_directory(directory);
	//Checks if the path is directory
	if  (d_exists == 1 && strlen(filename)==0 && strlen(extension)==0){
		return -EISDIR;
	}
	//Checks if the file doesn't exist
	if (f_exists != 1){
		return -ENOENT;
	}
	//searches through root to find where the matching directory name is stored
	struct csc452_root_directory *root = find_root();
	int i;
	for (i =0; i < root->nDirectories;i++){
		if (strcmp(directory, root->directories[i].dname)==0){
			break;
		}
	}
	//searches through the directory entry object to find where the matching filename name is stored
	// after grabbing the directory entry based on the startblock offset
	struct csc452_directory_entry *entry = find_directory(root->directories[i].nStartBlock);
	int j = 0;
	for (j = 0; j < entry->nFiles; j++){
		if (strcmp(filename, entry->files[j].fname)==0){
			if (strcmp(extension, entry->files[j].fext)==0){
				break;
			}
		}
	}
	// Clear the block fields of data.
	struct csc452_disk_block *block = find_file(entry->files[j].nStartBlock);
	block->nNextBlock = 0;
	int k;
	//setting the data to null characters to clear the block
	for (k = 0; k < MAX_DATA_IN_BLOCK;k++){
		block->data[k] = '\0';
	}
	// Write the cleared block back to disk.
	write_file(block, entry->files[j].nStartBlock);
	entry->nFiles--;
	// Clear the file details in the entry.
	int l;
	for (l = 0; l < MAX_FILENAME; l++) {
		entry->files[j].fname[l] = '\0';
	}
	for (l = 0; l < MAX_EXTENSION; l++) {
		entry->files[j].fext[l] = '\0';
	}
	entry->files[j].fsize = 0;
	// Mark the block as free again.
	block_status[entry->files[j].nStartBlock / 512] = 1;
	entry->files[j].nStartBlock = 0;
	// Update the directory entry.
	write_entry(entry, root->directories[j].nStartBlock);	
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
