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

#define TRUE 1
#define FALSE 2

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

/// Help Function to implement the fuse filesystem

static void get_root(csc452_root_directory *root); 
static int get_dir_index(char *directory);
static int get_file_index(csc452_directory_entry entry,char* filename,char* extension,int token);
static int get_file_size(char *directory, char *filename, char *extension,int token);
static int get_dir_block(csc452_directory_entry *entry, char *directory);
static long find_free_block();

// to keep track of the free block
typedef struct
{
	uint8_t bits[2*BLOCK_SIZE]; // 2 * 512 * 8 = 4MB  we only can use 4MB
}BitMap;
BitMap bitmap; 

static int alreadyInitBitMap;

/**
 * @brief Get the root object
 * 
 * @param root 
 * @return  
 */
static void get_root(csc452_root_directory *root) {
	FILE *fp = fopen("/home/deniffer/work/.disk", "rb");
	if (fp != NULL) {
		fread(root, sizeof(csc452_root_directory), 1, fp);
		fclose(fp);
	}
}

/**
 * @brief Get the dir index on root array.
 *  if not exist dir return -1
 * @param directory 
 * @return int 
 */
static int get_dir_index(char *directory) {
	csc452_root_directory root;
	get_root(&root);
	for (int i = 0; i < root.nDirectories; i++) {
		if (strcmp(directory, root.directories[i].dname) == 0) {
			return i; 
		}
	}
	return -1;
}

static int get_file_index(csc452_directory_entry entry,char* filename,char* extension,int token) {
	int i; 
	for (i = 0; i < entry.nFiles; i++) {
		if (strcmp(filename, entry.files[i].fname) == 0) {
			if (token == 2) { // filename without extension
				return i;
			}else if (strcmp(extension, entry.files[i].fext) == 0) { // filename with extension
				return i;
			}
		}
	}
	return -1;
}

/**
 * @brief Get the dir block object
 * 
 * @param entry 
 * @param directory 
 * @return int 
 */
static int get_dir_block(csc452_directory_entry *entry, char *directory) {
	csc452_root_directory root;
	get_root(&root);
	int index = get_dir_index(directory); 
	if (index == -1) {
		return -1; 
	}
	FILE *fp = fopen("/home/deniffer/work/.disk", "rb");
	if (fp == NULL) { 
		printf("Disk not exist ! \n");
		return -1;
	}
	// read dir block into entry.
	fseek(fp, root.directories[index].nStartBlock, SEEK_SET);
	fread(entry, sizeof(csc452_directory_entry), 1, fp);
	fclose(fp);
	
	return 0; 
}

/**
 * @brief Get the file size 
 *  if file not exist return -1
 * @param directory 
 * @param filename 
 * @param extension 
 * @return int 
 */
static int get_file_size(char *directory, char *filename, char *extension,int token) {
	csc452_directory_entry entry;  
	if (get_dir_block(&entry, directory) == -1) { // directory simply not exist in the first place
		return -1; 
	}
	for (int i = 0; i < entry.nFiles; i++) {
		if (strcmp(filename, entry.files[i].fname) == 0) {
			if (token == 3) { // the path does have extension, we do one more compare
				if (strcmp(extension, entry.files[i].fext) == 0) {
					return entry.files[i].fsize; 
				} 
			} else { // if it is only name, we return the size right away
				return entry.files[i].fsize; 
			}
		}
	}
	
	return -1;
}

/**
 * @brief Using bitmap to keep track of the free block in the disk ,return the 
 * 
 * @return long 
 */
static long find_free_block() {
	if (alreadyInitBitMap != TRUE) {
		FILE* fp = fopen("/home/deniffer/work/.disk","rb");
		if (fp != NULL){
			fseek(fp,BLOCK_SIZE,SEEK_SET);
			fread(&bitmap,sizeof(bitmap),1,fp);
			fclose(fp);	
		}
		alreadyInitBitMap = TRUE;
	}
	uint8_t index;
	int i=0;
	int find = FALSE;
	long address;
	for(;i< 2*BLOCK_SIZE;i++) {
		uint8_t value = bitmap.bits[i];
		if (value == 255) { // this cell is full
			continue;
		} else {
			uint8_t bitFlag = 1;
			index = 0;
			while(TRUE){
				// find first zero bit
				if (!(bitFlag & value)){
					bitmap.bits[i] |= bitFlag;
					find = TRUE;
					break;
				}
				bitFlag = bitFlag << 1;
				index ++;
			}
			break;
		}
	}
	if (find){
		// i stand for cell, index means which block
		// 3 is the offset ,since we don't use the first three block  1Block for root, 2Block for bitMap 
		address = ((i * 8) + index + 3) * 512;
		FILE* fp = fopen("/home/deniffer/work/.disk","rb+");
		if (fp != NULL){
			fseek(fp,BLOCK_SIZE,SEEK_SET);
			fwrite(&bitmap,sizeof(bitmap),1,fp);
			fclose(fp);
		}
	} else {
		address = -1;
	}
	return address;
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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		int token = sscanf(path,"/%[^/]/%[^.].%s",directory, filename,extension);
		// request to a dir
		if(token == 1) { 
			if(get_dir_index(directory) != -1) { // check for dir exist
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				return res;
			}
		} else if (token > 1) { //If the path does exist and is a file:
			int fileSize = get_file_size(directory,filename,extension,token);
			if (fileSize >= 0) {
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 2;
				stbuf->st_size = (size_t) fileSize;
				return res;
			}
		}
		//Else return that path doesn't exist
		res = -ENOENT;
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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		csc452_root_directory root;
		get_root(&root);
		for (int i = 0; i < root.nDirectories ; i++) {
			filler(buf,root.directories[i].dname,NULL,0);
		}
	}
	else {
		int token = sscanf(path,"/%[^/]/%[^.].%s",directory, filename,extension);
		if (token == 1) {
			csc452_directory_entry entry;
			if (get_dir_block(&entry,directory) == -1) {
				return -ENOENT;
			}
			char fullName[MAX_FILENAME + MAX_EXTENSION + 3];
			for (int i = 0; i < entry.nFiles ; i++) {
				memset(fullName,0,MAX_FILENAME + MAX_EXTENSION + 3);
				if(!strcmp(entry.files[i].fext,"")){
					sprintf(fullName,"%s",entry.files[i].fname);
				}else {
					sprintf(fullName,"%s.%s",entry.files[i].fname,entry.files[i].fext);
				}
				filler(buf, fullName, NULL, 0);
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
	(void) path;
	(void) mode;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int token = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
	// Exception Control
	if (token != 1) {		
		return -EPERM;
	}
	if (strlen(directory) > 8) {
		return -ENAMETOOLONG;
	}
	if (get_dir_index(directory) != -1){
		return -EEXIST;
	}

	csc452_root_directory root;
	get_root(&root);
	if(root.nDirectories >= MAX_DIRS_IN_ROOT)
		return -ENOSPC;
	else {
		long address = find_free_block();
		if (address == -1) 
			return -EDQUOT;
		// copy dir strcuture into root array;
		strcpy(root.directories[root.nDirectories].dname,directory);
		// record the start block in nStartBlock field
		root.directories[root.nDirectories].nStartBlock = address;
		root.nDirectories ++;
		// write root  back into the disk
		FILE *fp = fopen("/home/deniffer/work/.disk", "rb+");
		fwrite(&root, sizeof(csc452_root_directory), 1, fp); 
		fclose(fp);
	}
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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int token = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (token < 2) { 
		return -EPERM; 
	} 
	if (strlen(filename) > 8 || strlen(extension) > 3) {
		return -ENAMETOOLONG; 
	}
	if (get_file_size(directory, filename, extension, token) != -1) {
		return -EEXIST;
	}
		
	csc452_directory_entry entry;  
	get_dir_block(&entry, directory);
	if (entry.nFiles >= MAX_FILES_IN_DIR) {
		printf("File creation fail Already have max file in this directory\n");
		return -ENOSPC; 
	} else {
		long address = find_free_block(); 
		if (address == -1) { 
			printf("Out of disk space");
			return -EDQUOT; 
		}
		strcpy(entry.files[entry.nFiles].fname, filename);
		strcpy(entry.files[entry.nFiles].fext, extension);
		entry.files[entry.nFiles].fsize = 0; 
		entry.files[entry.nFiles].nStartBlock = address; 
		entry.nFiles += 1; 
		// write back to disk
		csc452_root_directory root; 
		get_root(&root);
		int dir_index = get_dir_index(directory);
		FILE *fp = fopen("/home/deniffer/work/.disk", "rb+");
		fseek(fp, root.directories[dir_index].nStartBlock, SEEK_SET); 
		fwrite(&entry, sizeof(csc452_directory_entry), 1, fp);
		fclose(fp); 
	}
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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1]; 
	int token = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	//check to make sure path exists
	if (token == 1) {
		if (get_dir_index(directory) != -1) {
			return -EISDIR; 
		}	
	} 
	//check that size is > 0
	size_t fsize = get_file_size(directory, filename, extension, token);  
	if (fsize == -1) {
		return -ENOENT; 
	}
	if (size == 0) {
		return size; 
	}
	//check that offset is <= to the file size
	if (offset > fsize) {
		return -EINVAL; 
	}
	//check that offset is <= to the file size
	csc452_directory_entry entry; 
	get_dir_block(&entry, directory);  
	int i = get_file_index(entry,filename,extension,token); 
	FILE *fp = fopen("/home/deniffer/work/.disk", "rb"); 
	csc452_disk_block file_block; 
	fseek(fp, entry.files[i].nStartBlock, SEEK_SET); 
	fread(&file_block, sizeof(csc452_disk_block), 1, fp);
	int begin_index = offset; 
	while (begin_index > MAX_DATA_IN_BLOCK) {
		fseek(fp, file_block.nNextBlock, SEEK_SET); 
		fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
		begin_index = begin_index - MAX_DATA_IN_BLOCK; 
	}
	if (size > fsize) {
		size = fsize; 
	}
	//read in data
	size_t left_to_read = size; 
	int buf_index = 0; 
	int file_block_left = MAX_DATA_IN_BLOCK - begin_index; 
	while (left_to_read > 0) {
		// move to the next one when data in this block is done.
		if (file_block_left == 0) {
			fseek(fp, file_block.nNextBlock, SEEK_SET); 
			fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
			file_block_left = MAX_DATA_IN_BLOCK; 
			begin_index = 0; 
		} 
		buf[buf_index] = file_block.data[begin_index]; 
		left_to_read--; 
		file_block_left--; 
		begin_index++; 
		buf_index++; 
	}
	//return success, or error
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
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int token = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (token == 1) {
		if (get_dir_index(directory) != -1) {
			return -EISDIR; 
		}	
	} 
	//check that size is > 0
	size_t fsize = get_file_size(directory, filename, extension, token);  
	if (fsize == -1) {
		return -ENOENT; 
	}
	if (size == 0) {
		return size; 
	}
	//check that offset is <= to the file size
	if (offset > fsize) {
		return -EFBIG;
	}
	// get file index inside the directory entry
	csc452_directory_entry entry;
	get_dir_block(&entry,directory);
	int i = get_file_index(entry,filename,extension,token);
	// get file start block 
	long startAddress = entry.files[i].nStartBlock;
	FILE *fp = fopen("/home/deniffer/work/.disk", "rb+"); 
	csc452_disk_block file_block; 
	fseek(fp, startAddress, SEEK_SET);
	fread(&file_block, sizeof(csc452_disk_block), 1, fp);
	// check if need add file size 
	size_t size_to_write = offset + size - fsize;
	if (size_to_write > 0) {
		entry.files[i].fsize += size_to_write; 
		csc452_root_directory root;
		get_root(&root); 
		// locate into the dir startBlock && update block
		fseek(fp, root.directories[get_dir_index(directory)].nStartBlock, SEEK_SET); 
		fwrite(&entry, sizeof(csc452_directory_entry), 1, fp); 
	} 
	// find write position
	int write_index = offset;
	while (write_index > MAX_DATA_IN_BLOCK) {
		startAddress =  file_block.nNextBlock; 
		fseek(fp, startAddress, SEEK_SET); 
		fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
		write_index  -= MAX_DATA_IN_BLOCK; 
	}
	int remain_block_size = MAX_DATA_IN_BLOCK - write_index;
	long need_write_bytes = size;
	int buf_index = 0;
	while (need_write_bytes > 0 ){
		if(remain_block_size == 0) {
			// need write into new data block
			if (file_block.nNextBlock == 0 ){
				long address = find_free_block();
				if (address == -1) {
					return -EFBIG;
				}
				file_block.nNextBlock = address;
			}
			// update old file_block into disk
			fseek(fp,startAddress,SEEK_SET);
			fwrite(&file_block, sizeof(csc452_disk_block), 1, fp);
			// set position to next file block
			startAddress = file_block.nNextBlock;
			fseek(fp, startAddress, SEEK_SET);
			fread(&file_block, sizeof(csc452_disk_block), 1, fp);
			remain_block_size = MAX_DATA_IN_BLOCK;
			write_index = 0;
		}
		// write buf data into file_block
		file_block.data[write_index] = buf[buf_index];
		need_write_bytes --;
		remain_block_size --;
		write_index ++ ;
		buf_index ++;
	}
	// update last file block into disk
	fseek(fp, startAddress, SEEK_SET); 
	fwrite(&file_block, sizeof(csc452_disk_block), 1, fp);
	fclose(fp);
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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int token = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (token == 1) {
		csc452_directory_entry entry;
		if (get_dir_block(&entry,directory) == -1){
			return -ENOENT;
		}
		if (entry.nFiles != 0){
			return -ENOTEMPTY;
		}
		// delete directory from root
		int index = get_dir_index(directory);
		csc452_root_directory root;
		get_root(&root);
		root.nDirectories --;
		// let all directories behind this one move foreword
		for(int i=index;i<root.nDirectories;i++){
			strcpy(root.directories[index].dname,root.directories[index+1].dname);
			root.directories[index].nStartBlock = root.directories[index+1].nStartBlock;
		}

		FILE *fp = fopen("/home/deniffer/work/.disk", "rb+");
		fwrite(&root, sizeof(csc452_root_directory), 1, fp); 
		fclose(fp);
	} else {
		return -ENOTDIR;
	}

	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
    (void) path;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int token = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (token == 1) {
		if (get_dir_index(directory) != -1) {
			return -EISDIR; 
		}	
	}
	size_t fsize = get_file_size(directory,filename,extension,token);
	if (fsize < 0) {
		return -ENOENT;
	}
	csc452_directory_entry entry;
	get_dir_block(&entry,directory);
	int file_index = get_file_index(entry,filename,extension,token);
	entry.nFiles --;
	// move forward that the one behind this file
	for(int i=file_index;i<entry.nFiles;i++){
		strcpy(entry.files[i].fname,entry.files[i+1].fname);
		strcpy(entry.files[i].fext,entry.files[i+1].fext);
		entry.files[i].nStartBlock = entry.files[i+1].nStartBlock ;
		entry.files[i].fsize = entry.files[i+1].fsize;
	}
	// write back to disk
	csc452_root_directory root; 
	get_root(&root);
	int dir_index = get_dir_index(directory);
	FILE *fp = fopen("/home/deniffer/work/.disk", "rb+");
	fseek(fp, root.directories[dir_index].nStartBlock, SEEK_SET); 
	fwrite(&entry, sizeof(csc452_directory_entry), 1, fp);
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
