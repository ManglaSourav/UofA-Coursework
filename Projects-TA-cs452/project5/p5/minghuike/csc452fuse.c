/*
	Author: 	Minghui Ke
	Assignment:	CSC 452 Project 5
	Purpose:	Using fuse to create a disk devicd in user space manerging by this file.
				It can support like mkdir, ls, read, write etc.
*/

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

struct bitmap
{
	int map[128];
};

typedef struct bitmap bitmap;

static void getRoot(csc452_root_directory *root) {
	FILE *fp = fopen(".disk", "rb");
	// get root block
	if (fp != NULL) fread(root, sizeof(csc452_root_directory), 1, fp);	
	else printf("root not exist.\n");
	
	fclose(fp);
}

static int getDir(csc452_directory_entry *dir, char *dirName) {

	csc452_root_directory root;
	getRoot(&root);

	int idx;
	for (idx = 0; idx < root.nDirectories; idx++) {
		if (strcmp(dirName, root.directories[idx].dname) == 0) break;
	}

	if (idx == root.nDirectories) return -1;

	FILE *fp = fopen(".disk", "rb");
	if (fp == NULL) {
		printf("Can't open root.\n");
		fclose(fp);
		return -1;
	}

	fseek(fp, root.directories[idx].nStartBlock*BLOCK_SIZE, SEEK_SET);
	fread(dir, sizeof(csc452_directory_entry), 1, fp);
	fclose(fp);

	return idx; 
}

static int fileSize(char *dirName, char *fileName, char *extension) {
	csc452_directory_entry dir;  
	int dirIndex = getDir(&dir, dirName); 
	if (dirIndex == -1) {
		return -1; 
	} 

	for (int i = 0; i < dir.nFiles; i++) {
		if (strcmp(fileName, dir.files[i].fname) == 0 &&
			strcmp(extension, dir.files[i].fext) == 0) {
			return dir.files[i].fsize; 
		}
	}
	return -1; 
}

static long getNextFree() {
	bitmap curr;
	FILE *fp = fopen(".disk", "rb+"); 
	fseek(fp, -BLOCK_SIZE, SEEK_END);
	long index = 0;
	while (1) {
		fread(&curr, sizeof(bitmap), 1, fp);
		for (int i = 0; i < 128; i++) {
			unsigned int temp = 1;
			int j = 0;
			while (j < 31) {
				if ((temp & curr.map[i]) == 0){
					curr.map[i] |= temp;
					if (index != 0) {
						fwrite(&curr, sizeof(bitmap), 1, fp);
						fclose(fp);
						return index;
					}
				}
				index++;
				temp = temp << 1;
				j++;
			}
		}
		fseek(fp, -BLOCK_SIZE, SEEK_CUR);
	}
	fclose(fp);
	return -1;
}

static long freeBlock(long index) {
	// 512 bytes * 8 bits = 4096 bits
	int temp = (index / 4096) +1;
	bitmap curr;
	FILE *fp = fopen(".disk", "rb+"); 
	fseek(fp, -temp*BLOCK_SIZE, SEEK_END);
	fread(&curr, sizeof(bitmap), 1, fp);
	temp = (index % 4096) / 32;
	int mask = 1 << (index % 4096) % 32;
	curr.map[temp] = (curr.map[temp] & ~mask) | ((0 << (index % 4096) % 32) & mask);
	fwrite(&curr, sizeof(bitmap), 1, fp);
	fclose(fp);
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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		int num = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		
		if (num == 1) { //If the path does exist and is a directory:

			csc452_directory_entry dir;
			int idx = getDir(&dir, directory);
			if (idx != -1) {
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				return res;
			}

		} else if (num == 3) { //If the path does exist and is a file:
			int size;
			if ((size = fileSize(directory, filename, extension)) != -1) {
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 2;
				stbuf->st_size = (size_t) size;
				return res;
			}
		}
		// Else return that path doesn't exist
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
		getRoot(&root);
		for(int i = 0; i < root.nDirectories; i++) {
			filler(buf, root.directories[i].dname, NULL, 0);
		}
	}
	else {

		int num = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		if (num == 1) { //If the path does exist and is a directory:
			
			csc452_directory_entry dir;  
			int idx = getDir(&dir, directory); 
			if (idx == -1) return -ENOENT; 

			filler(buf, ".", NULL,0);
			filler(buf, "..", NULL, 0);
			for (int i = 0; i < dir.nFiles; i++) {
				char file[13];
				sprintf(file, "%s.%s", dir.files[i].fname, dir.files[i].fext);
				filler(buf, file, NULL, 0);
			}
		} else return -ENOENT;
		
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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];

	int num = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	// if the directory is not under the root dir only
	if (num != 1) return EPERM;  
	// if the name is beyond 8 chars
	if (strlen(directory) > 8) return ENAMETOOLONG;
	// if the directory already exists
	csc452_directory_entry dir;  
	if (getDir(&dir, directory) != -1) return EEXIST;

	csc452_root_directory root;
	getRoot(&root);
	if (root.nDirectories >= MAX_DIRS_IN_ROOT) {
		printf("Exceed maximum, Can't make directory.\n");
		// The user's quota of disk blocks or inodes on the file system has been exhausted.
		return ENOSPC;
	} else {

		csc452_directory_entry newDir;
		newDir.nFiles = 0;
		strcpy(root.directories[root.nDirectories].dname, directory);

		FILE *fp = fopen(".disk", "rb+");
		long next = getNextFree();
		fseek(fp, (next)*BLOCK_SIZE, SEEK_SET);
		// write the new directory
		fwrite(&newDir, sizeof(csc452_directory_entry), 1, fp);
		fclose(fp);

		root.directories[root.nDirectories++].nStartBlock = next;

		// write the root
		fp = fopen(".disk", "rw+");
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
	int num = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// if the file is trying to be created in the root dir or else
	if (num != 3) return EPERM; 
	//  if the name is beyond 8.3 chars
	if (strlen(filename) > 8 || strlen(extension) > 3) return ENAMETOOLONG; 
	// if the file already exists
	if (fileSize(directory, filename, extension) != -1) return EEXIST;

	csc452_directory_entry dir;  
	int idx = getDir(&dir, directory);
	if (idx == -1) {
		return ENOENT;
	}
	if (dir.nFiles >= MAX_FILES_IN_DIR) {
		return ENOSPC; 

	}

	strcpy(dir.files[dir.nFiles].fname, filename);
	strcpy(dir.files[dir.nFiles].fext, extension);
	dir.files[dir.nFiles].fsize = 0;
	FILE *fp = fopen(".disk", "rw+");

	long next = getNextFree();
	dir.files[dir.nFiles++].nStartBlock = next;

	// write the directory
	csc452_root_directory root;
	getRoot(&root);
	fseek(fp, (root.directories[idx].nStartBlock)*BLOCK_SIZE, SEEK_SET);
	fwrite(&dir, sizeof(csc452_directory_entry), 1, fp);
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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1]; 
	
	int num = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	//check to make sure path exists
	if (num == 1) {
		csc452_directory_entry dir;  
		if (getDir(&dir, directory) != -1) return EISDIR;
	}
	else if (num == 3) {
		size_t fsize = fileSize(directory, filename, extension);
		if (fsize != -1) {
			// check that size is > 0
			if (size == 0) return 0;
			// check that offset is <= to the file size
			if (offset > fsize) return EINVAL;

			csc452_directory_entry dir; 
			getDir(&dir, directory); 
			int i;
			for (i = 0; i < dir.nFiles; i++) {
				if (strcmp(filename, dir.files[i].fname) == 0 &&
					strcmp(extension, dir.files[i].fext) == 0) {
					break;
				}
			}
			FILE *fp = fopen(".disk", "rb"); 
			csc452_disk_block fileBlock;
			fseek(fp, dir.files[i].nStartBlock*BLOCK_SIZE, SEEK_SET);
			fread(&fileBlock, sizeof(csc452_disk_block), 1, fp); 

			// move to the correct blcok
			long fileIndex = offset;
			while (fileIndex > MAX_DATA_IN_BLOCK) {
				fseek(fp, fileBlock.nNextBlock*BLOCK_SIZE, SEEK_SET); 
				fread(&fileBlock, sizeof(csc452_disk_block), 1, fp); 
				fileIndex -= MAX_DATA_IN_BLOCK;
			}

			// read in data
			if (size > fsize) size = fsize;
			long bufIndex = 0;
			while (size > 0) {
				if (fileIndex == MAX_DATA_IN_BLOCK) {
					fseek(fp, fileBlock.nNextBlock*BLOCK_SIZE, SEEK_SET); 
					fread(&fileBlock, sizeof(csc452_disk_block), 1, fp);
					fileIndex = 0;
				}
				buf[bufIndex++] = fileBlock.data[fileIndex++];
				size--;
			}
			fclose(fp);
			return fsize;
		}
	}
	return ENOENT;
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

	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1]; 
	
	int num = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	//check to make sure path exists
	if (num == 1) {
		csc452_directory_entry dir;  
		int idx = getDir(&dir, directory);
		if (idx != -1) return EISDIR;
	}
	else if (num == 3) {
		size_t fsize = fileSize(directory, filename, extension);
		if (fsize != -1) {
			// check that size is > 0
			if (size == 0) return 0;
			// check that offset is <= to the file size
			if (offset > fsize) return EFBIG;

			csc452_directory_entry dir; 
			int idx = getDir(&dir, directory); 
			int i;
			for (i = 0; i < dir.nFiles; i++) {
				if (strcmp(filename, dir.files[i].fname) == 0 &&
					strcmp(extension, dir.files[i].fext) == 0) {
					break;
				}
			}

			FILE *fp = fopen(".disk", "rb+"); 

			csc452_disk_block fileBlock;
			fseek(fp, dir.files[i].nStartBlock*BLOCK_SIZE, SEEK_SET);
			fread(&fileBlock, sizeof(csc452_disk_block), 1, fp); 

			// move to the correct blcok
			long fileIndex = offset;
			long currBlock = dir.files[i].nStartBlock;
			while (fileIndex > MAX_DATA_IN_BLOCK) {
				currBlock = fileBlock.nNextBlock;
				fseek(fp, fileBlock.nNextBlock*BLOCK_SIZE, SEEK_SET); 
				fread(&fileBlock, sizeof(csc452_disk_block), 1, fp); 
				fileIndex -= MAX_DATA_IN_BLOCK;
			}

			// write data
			long left = size;
			long bufIndex = 0;
			while (left > 0) {
				if (fileIndex == MAX_DATA_IN_BLOCK) {

					// write current block
					long next = getNextFree();
					fileBlock.nNextBlock = next;
					fseek(fp, currBlock*BLOCK_SIZE, SEEK_SET); 
					fwrite(&fileBlock, sizeof(csc452_disk_block), 1, fp);
					// begin next block
					currBlock = fileBlock.nNextBlock;
					fseek(fp, fileBlock.nNextBlock*BLOCK_SIZE, SEEK_SET); 
					fread(&fileBlock, sizeof(csc452_disk_block), 1, fp);
					fileIndex = 0;
				}
				fileBlock.data[fileIndex++] = buf[bufIndex++];
				left--;
			}
			// write current block
			fileBlock.nNextBlock = 0;
			fseek(fp, currBlock*BLOCK_SIZE, SEEK_SET); 
			fwrite(&fileBlock, sizeof(csc452_disk_block), 1, fp);
			// write the change of file size
			dir.files[i].fsize += size;
			csc452_root_directory root;
			getRoot(&root);
			fseek(fp, root.directories[idx].nStartBlock*BLOCK_SIZE, SEEK_SET); 
			fwrite(&dir, sizeof(csc452_directory_entry), 1, fp);

			fclose(fp);
			return size;
		}
	}

	return ENOENT;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{

	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];

	int num = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	if (num != 1) return ENOTDIR;

	csc452_directory_entry dir;  
	int idx = getDir(&dir, directory);

	if (idx == -1) return ENOENT;
	if (dir.nFiles != 0) return ENOTEMPTY;

	csc452_root_directory root;
	getRoot(&root);
	long rm = 0;
	for (int i = 0; i < root.nDirectories; i++) {
		if (strcmp(root.directories[i].dname, directory) == 0) {
			rm = root.directories[i].nStartBlock;
			for (; i < root.nDirectories-1; i++) {
				root.directories[i] = root.directories[i+1];
			}
			break;
		}
	}
	root.nDirectories--;
	FILE *fp = fopen(".disk", "rb+");
	// write the root
	fwrite(&root, sizeof(csc452_root_directory), 1, fp); 
	fclose(fp);
	freeBlock(rm);
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

	int num = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	if (num == 1) {
		csc452_directory_entry dir;  
		int idx = getDir(&dir, directory);
		if (idx != -1) return EISDIR;
	}
	else if (num == 3) {
		size_t fsize = fileSize(directory, filename, extension);
		if (fsize != -1) {

			csc452_directory_entry dir; 
			int idx = getDir(&dir, directory); 
			long rm;
			for (int i = 0; i < dir.nFiles; i++) {
				if (strcmp(filename, dir.files[i].fname) == 0 &&
					strcmp(extension, dir.files[i].fext) == 0) {
					rm = dir.files[i].nStartBlock;
					for (; i < dir.nFiles-1; i++) {
						dir.files[i] = dir.files[i+1];
					}
					break;
				}
			}
			dir.nFiles--;

			csc452_root_directory root;
			getRoot(&root);

			FILE *fp = fopen(".disk", "rb+"); 
			// write directory
			fseek(fp, root.directories[idx].nStartBlock*BLOCK_SIZE, SEEK_SET);
			fwrite(&dir, sizeof(csc452_directory_entry), 1, fp); 

			csc452_disk_block fileBlock;
			fseek(fp, rm*BLOCK_SIZE, SEEK_SET);
			fread(&fileBlock, sizeof(csc452_disk_block), 1, fp); 

			// mark block free
			while (rm != 0) {
				freeBlock(rm);
				rm = fileBlock.nNextBlock;
				fseek(fp, rm*BLOCK_SIZE, SEEK_SET);
				fread(&fileBlock, sizeof(csc452_disk_block), 1, fp); 
			}
			return 0;
		}
	}

	return ENOENT;
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
