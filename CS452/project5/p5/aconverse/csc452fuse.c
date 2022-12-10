/*
 * Author: Amber Charlotte Converse
 * File: csc452_fuse.c
 * Purpose: This file implements system calls in user space to create a virtual
 * 	file system in user space using the application FUSE. The file can be
 * 	compiled using the following command:
 * 		
 * 	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452
 *
 * 	This program requires a blank file in the same directory called ".disk"
 * 	with the same size as defined in the macro DRIVE_SIZE.
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
#include <math.h>

//size of a disk block
#define	BLOCK_SIZE 512

// Silly macros
#define str(x) helperstr(x)
#define helperstr(x) #x

#define DRIVE_SIZE (5 * 1048576) // 5MB
#define NUM_BLOCKS (DRIVE_SIZE / BLOCK_SIZE)
#define BITMAP_SIZE ((int) ceil((double) (NUM_BLOCKS / 8) / (double) BLOCK_SIZE)) // in blocks

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

typedef struct csc452_directory_entry csc452_directory_entry;
typedef struct csc452_file_directory csc452_file_directory;

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
};

typedef struct csc452_root_directory csc452_root_directory;
typedef struct csc452_directory csc452_directory;

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
 * This struct handles keeping track of free blocks. This struct is holds a
 * bitmap with a bit for each block in the system. If the bit is 0, that block
 * is free. If a bit is 1, that block is occupied. I believe the size of this
 * struct is justified as it avoids lost storage upon deletion and takes up
 * less than 0.2% of the storage space for a 5MB drive. Even 20 blocks saved
 * makes up for the size of this struct on a 5MB drive.
 */
typedef struct csc452_bitmap {
	char bitmap[NUM_BLOCKS / 8];
} csc452_bitmap;

/*
 * This function searches for the given directory entry and file directory
 * using a given path. If the path leads to a directory, 1 is returned and only
 * the dir and dirBlock parameters are filled. If the path leads to a file, 2 is
 * returned all parameters are filled. If the path is invalid or does not
 * reference an existing file or directory, -1 is returned and none of the
 * parameters are filled.
 *
 * PARAM:
 * path (const char*): the path to the directory or file
 * dir (csc452_directory_entry*): a pointer to a block of memory sufficient to
 * 	hold a csc452_directory struct to be populated if the path leads to a
 * 	valid directory
 * file (csc_452_file_directory*): a pointer to a block of memory sufficient
 * 	to hold a csc452_file_directory struct to be populated if the path leads
 * 	to a valid file
 * dirBlock (long*): a pointer to a long where the block of dir will be stored
 *
 * RETURN:
 * int: 1 if a valid directory was found, 2 if a valid directory and file were
 * 	found, and -1 if the path was invalid or did not reference an existing
 * 	file or directory
 */
int search(const char *path, csc452_directory_entry *dir,
		csc452_file_directory *file, long* dirBlock) {
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	int num_read;
	if ((num_read = sscanf(path, "/%" str(MAX_FILENAME) "[^/]/%" str(MAX_FILENAME) 
				    "[^.].%" str(MAX_EXTENSION) "s",
			directory, filename, extension)) == 0) {
		return -1;
	}
	FILE *fp = fopen(".disk", "rb");
	csc452_root_directory root;
	fread(&root, BLOCK_SIZE, 1, fp);
	long dir_start = -1;
	for (int i = 0; i < root.nDirectories; i++) {
		csc452_directory cur = root.directories[i];
		if (strcmp(cur.dname, directory) == 0) {
			dir_start = cur.nStartBlock;
			break;	
		}
	}
	if (dir_start == -1) {
		fclose(fp);
		return -1;
	}
	fseek(fp, dir_start*BLOCK_SIZE, SEEK_SET);
	csc452_directory_entry fdir;
	fread(&fdir, BLOCK_SIZE, 1, fp);
	fclose(fp);
	if (num_read == 1) {
		*dir = fdir;
		*dirBlock = dir_start;
		return 1;
	}
	for (int i = 0; i < fdir.nFiles; i++) {
		csc452_file_directory cur = fdir.files[i];
		if (strcmp(cur.fname, filename) == 0 &&
				strcmp(cur.fext, extension) == 0) {
			*dir = fdir;
			*file = cur;
			*dirBlock = dir_start;
			return 2;
		}
	}
	return -1;
}

/*
 * This function finds the block index of the next free block.
 * ***IMPORTANT***: Calling this function assumed you are ready to allocate
 * immediately to this block, so the free space tracking bitmap WILL MARK
 * THIS BLOCK AS FULL once it finds a free block. You must allocate to this
 * block or set the bit to zero in the bitmap if you do not.
 *
 * RETURN:
 * long: the block index of the free block to use, -1 if storage is full
 */
long find_free_block() {
	FILE *fp = fopen(".disk", "rb+");
	csc452_bitmap bitmap;
	fseek(fp, -(NUM_BLOCKS / 8), SEEK_END);
	fread(&bitmap, NUM_BLOCKS / 8, 1, fp);
	if (!(bitmap.bitmap[0] & 0x1)) { // if the first bit is zero, initialize
		bitmap.bitmap[0] |= 0x1; // set the first block as in use
		for (long i = ((NUM_BLOCKS / 8) - 1) - (int) ceil((double) BITMAP_SIZE / 8.0);
				i < NUM_BLOCKS / 8; i++) { // set the bitmap as in use
			bitmap.bitmap[i] |= (char) 0xFF;
		}
	}
	for (long i = 0; i < NUM_BLOCKS / 8; i++) {
		int mask = 0x1;
		for (int j = 0; j < 8; j++) {
			if (!((bitmap.bitmap[i] & mask) >> j)) {
				bitmap.bitmap[i] |= mask;
				fseek(fp, -(NUM_BLOCKS / 8), SEEK_END);
				fwrite(&bitmap, NUM_BLOCKS / 8, 1, fp);
				fclose(fp);
				return (i * 8) + j;
			}
			mask <<= 1;	
		}
	}
	fseek(fp, -(NUM_BLOCKS / 8), SEEK_END);
	fwrite(&bitmap, NUM_BLOCKS / 8, 1, fp);
	fclose(fp);
	return -1;
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf) {
	
	int res = 0;

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		csc452_directory_entry dir;
		csc452_file_directory file;
		long dirBlock;
		int retval = search(path, &dir, &file, &dirBlock);
		if (retval == 1) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		} else if (retval == 2) {
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = file.fsize;
		} else {
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
			 off_t offset, struct fuse_file_info *fi) {
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		FILE *fp = fopen(".disk", "rb");
		csc452_root_directory root;
		fread(&root, BLOCK_SIZE, 1, fp);
		fclose(fp);
		for (int i = 0; i < root.nDirectories; i++) {
			csc452_directory cur = root.directories[i];
			filler(buf, cur.dname, NULL, 0);
		}
	}
	else {
		csc452_directory_entry dir;
		csc452_file_directory file;
		long dirBlock;
		int retval = search(path, &dir, &file, &dirBlock);
		if (retval == -1 || retval == 2) { // invalid directory
			return -ENOENT;
		} else {
			filler(buf, ".", NULL, 0);
			filler(buf, "..", NULL, 0);
			for (int i = 0; i < dir.nFiles; i++) {
				csc452_file_directory cur = dir.files[i];
				char filename[MAX_FILENAME+MAX_EXTENSION+2] = "\0";
				strcat(filename, cur.fname);
				strcat(filename, ".");
				strcat(filename, cur.fext);
				filler(buf, filename, NULL, 0);
			}
		}
	}
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode) {
	char directory[65];
	char extra[65];
	int retval = sscanf(path, "/%64[^/]/%64s", directory, extra);
	if (strlen(directory) > MAX_FILENAME) {
		return -ENAMETOOLONG;
	} else if (retval != 1) {
		return -EPERM;
	}
	FILE *fp = fopen(".disk", "rb+");
	csc452_root_directory root;
	fread(&root, BLOCK_SIZE, 1, fp);
	for (int i = 0; i < root.nDirectories; i++) {
		csc452_directory cur = root.directories[i];
		if (strcmp(cur.dname, directory) == 0) {
			fclose(fp);
			return -EEXIST;
		}
	}
	fclose(fp);
	long nStartBlock = find_free_block();
	if (nStartBlock == -1 || root.nDirectories == MAX_DIRS_IN_ROOT) {
		return -ENOSPC;
	}
	fp = fopen(".disk", "rb+");
	csc452_directory new_dir;
	strcpy(new_dir.dname, directory);
	new_dir.nStartBlock = nStartBlock;
	root.directories[root.nDirectories] = new_dir;
	root.nDirectories++;
	fseek(fp, 0, SEEK_SET);
	fwrite(&root, BLOCK_SIZE, 1, fp);
	
	csc452_directory_entry dir_entry;
	dir_entry.nFiles = 0;	
	fseek(fp, nStartBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&dir_entry, BLOCK_SIZE, 1, fp);
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
	if (strcmp(path, "/") == 0) {
		return -EPERM;
	}
	char directory[MAX_FILENAME+1];
	int num_read;
	if ((num_read = sscanf(path, "/%" str(MAX_FILENAME) "[^/]/%*s", directory)) == 0) {
		return -ENOENT;
	}
	char dir_path[MAX_FILENAME+2] = "/";
	strcat(dir_path, directory);
	csc452_directory_entry dir;
	csc452_file_directory file;
	long dirBlock;
	int retval = search(dir_path, &dir, &file, &dirBlock);
	if (retval == -1) {
		return -ENOENT;
	}
	
	char filename[65];
	char extension[33];
	if ((num_read = sscanf(path, "/%*[^/]/%64[^.].%32s", filename, extension)) != 2) {
		return -ENOENT;
	}
	if (strlen(filename) > MAX_FILENAME || strlen(extension) > MAX_EXTENSION) {
		return -ENAMETOOLONG;
	}
	csc452_directory_entry dir2;
	long dirBlock2;
	if (search(path, &dir2, &file, &dirBlock2) != -1) {
		return -EEXIST;
	}
	long nStartBlock = find_free_block();
	if (nStartBlock == -1 || dir.nFiles == MAX_FILES_IN_DIR) {
		return -ENOSPC;
	}
	csc452_file_directory new_file;
	strcpy(new_file.fname, filename);
	strcpy(new_file.fext, extension);
	new_file.fsize = 0;
	new_file.nStartBlock = nStartBlock;
	dir.files[dir.nFiles] = new_file;
	dir.nFiles++;

	FILE *fp = fopen(".disk", "rb+");
	csc452_root_directory root;
	fread(&root, BLOCK_SIZE, 1, fp);
	fseek(fp, dirBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&dir, BLOCK_SIZE, 1, fp);
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

	csc452_directory_entry dir;
	csc452_file_directory file;
	long dirBlock;
	int retval = search(path, &dir, &file, &dirBlock);
	if (retval == -1) {
		return -ENOENT;
	} else if (retval == 1) {
		return -EISDIR;
	}
	if (file.fsize == 0) {
		return 0;
	} else if (offset > file.fsize) {
		return 0;
	}
	off_t cur_offset = 0;
	FILE *fp = fopen(".disk", "rb");
	csc452_disk_block cur_block;
	fseek(fp, file.nStartBlock * BLOCK_SIZE, SEEK_SET);
	fread(&cur_block, BLOCK_SIZE, 1, fp);
	while (offset > cur_offset + MAX_DATA_IN_BLOCK) {
		fseek(fp, cur_block.nNextBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur_block, BLOCK_SIZE, 1, fp);
		cur_offset += MAX_DATA_IN_BLOCK;
	}
	off_t inside_offset = offset - cur_offset;
	size_t size_in_block = MAX_DATA_IN_BLOCK - inside_offset;
	if (size_in_block > size) {
		size_in_block = size;
	}
	memcpy(buf, &(cur_block.data[inside_offset]), size_in_block);
	long size_read = size_in_block;
	while (size_read < size) {
		fseek(fp, cur_block.nNextBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur_block, BLOCK_SIZE, 1, fp);
		size_in_block = MAX_DATA_IN_BLOCK;
		if (size_in_block + size_read > size) {
			size_in_block = size - size_read;
		}
		memcpy(&(buf[size_read]), &cur_block.data, size_in_block);
		size_read += size_in_block;
	}
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

	csc452_directory_entry dir;
	csc452_file_directory file;
	long dirBlock;
	int retval;
	if ((retval = search(path, &dir, &file, &dirBlock)) != 2) {
		return -ENOENT;
	}
	if (offset > file.fsize) {
		return -EFBIG;
	}

	FILE *fp = fopen(".disk", "rb+");
	if (file.fsize == 0 && size != 0) { // edge case if empty file
		long nStartBlock = find_free_block();
		if (nStartBlock == -1) {
			fclose(fp);
			return -ENOSPC;
		}
		file.nStartBlock = nStartBlock;
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];
		retval = sscanf(path, "/%*[^/]/%" str(MAX_FILENAME) "[^.].%"
				str(MAX_EXTENSION) "s", filename, extension);
		for (int i = 0; i < dir.nFiles; i++) {
			csc452_file_directory cur = dir.files[i];
			if (strcmp(cur.fname, filename) == 0 &&
					strcmp(cur.fext, extension) == 0) {
				dir.files[i] = file;
			}
		}
		fseek(fp, dirBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&dir, BLOCK_SIZE, 1, fp); // write dir with new file dir	
	}
	
	off_t cur_offset = 0;
	csc452_disk_block cur_block;
	fseek(fp, file.nStartBlock * BLOCK_SIZE, SEEK_SET);
	fread(&cur_block, BLOCK_SIZE, 1, fp);
	while (offset > cur_offset + MAX_DATA_IN_BLOCK) {
		fseek(fp, cur_block.nNextBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur_block, BLOCK_SIZE, 1, fp);
		cur_offset += MAX_DATA_IN_BLOCK;
	}
	off_t inside_offset = offset - cur_offset;
	size_t size_in_block = MAX_DATA_IN_BLOCK - inside_offset;
	if (size_in_block > size) {
		size_in_block = size;
	}
	memcpy(&(cur_block.data[inside_offset]), buf, size_in_block);
	fseek(fp, -BLOCK_SIZE, SEEK_CUR);
	fwrite(&cur_block, BLOCK_SIZE, 1, fp);
	long size_wrote = size_in_block;
	cur_offset += MAX_DATA_IN_BLOCK;
	while (size_wrote < size && cur_offset < file.fsize) {
		fseek(fp, cur_block.nNextBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur_block, BLOCK_SIZE, 1, fp);
		size_in_block = MAX_DATA_IN_BLOCK;
		if (size_in_block + size_wrote > size) {
			size_in_block = size - size_wrote;
		}
		memcpy(&cur_block.data, &(buf[size_wrote]), size_in_block);
		fseek(fp, -BLOCK_SIZE, SEEK_CUR);
		fwrite(&cur_block, BLOCK_SIZE, 1, fp);
		size_wrote += size_in_block;
		cur_offset += MAX_DATA_IN_BLOCK;
	}
	while (size_wrote < size) { // Now appending blocks
		long nNewBlock = find_free_block();
		if (nNewBlock == -1) {
			fclose(fp);
			return -ENOSPC;
		}
		cur_block.nNextBlock = nNewBlock;
		fseek(fp, -BLOCK_SIZE, SEEK_CUR);
		fwrite(&cur_block, BLOCK_SIZE, 1, fp); // write next block pointer
		fseek(fp, cur_block.nNextBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur_block, BLOCK_SIZE, 1, fp);
		size_in_block = MAX_DATA_IN_BLOCK;
		if (size_in_block + size_wrote > size) {
			size_in_block = size - size_wrote;
		}
		memcpy(&cur_block.data, &(buf[size_wrote]), size_in_block);
		fseek(fp, -BLOCK_SIZE, SEEK_CUR);
		fwrite(&cur_block, BLOCK_SIZE, 1, fp); // write new data
		size_wrote += size_in_block;
	}

	if (offset + size > file.fsize) { // Update size if append
		file.fsize = offset + size;
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];
		retval = sscanf(path, "/%*[^/]/%" str(MAX_FILENAME) "[^.].%"
				str(MAX_EXTENSION) "s", filename, extension);
		for (int i = 0; i < dir.nFiles; i++) {
			csc452_file_directory cur = dir.files[i];
			if (strcmp(cur.fname, filename) == 0 &&
					strcmp(cur.fext, extension) == 0) {
				dir.files[i] = file;
			}
		}
		fseek(fp, dirBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&dir, BLOCK_SIZE, 1, fp);
	}
	fclose(fp);

	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path) {
	csc452_directory_entry dir;
	csc452_file_directory file;
	long dirBlock;
	int retval = search(path, &dir, &file, &dirBlock);
	if (retval == 0) {
		return -ENOENT;
	} else if (retval == 2) {
		return -ENOTDIR;
	} else if (dir.nFiles != 0) {
		return -ENOTEMPTY;
	}
	char directory[MAX_FILENAME + 1];
	sscanf(path, "/%" str(MAX_FILENAME) "[^/]", directory);
	FILE *fp = fopen(".disk", "rb+");
	csc452_root_directory root;
	fread(&root, BLOCK_SIZE, 1, fp);
	for (int i = 0; i < root.nDirectories; i++) {
		csc452_directory cur = root.directories[i];
		if (strcmp(cur.dname, directory) == 0) {
			root.nDirectories--;
			for (int j = i; j < root.nDirectories; j++) {
				root.directories[j] = root.directories[j+1];
			}
			break;
		}
	}
	fseek(fp, 0, SEEK_SET);
	fwrite(&root, BLOCK_SIZE, 1, fp);

	fseek(fp, -(NUM_BLOCKS / 8), SEEK_END);
	csc452_bitmap bitmap;
	fread(&bitmap, NUM_BLOCKS / 8, 1, fp);
	bitmap.bitmap[dirBlock / 8] &= ~(0x1 << (dirBlock % 8));
	fseek(fp, -(NUM_BLOCKS / 8), SEEK_END);
	fwrite(&bitmap, NUM_BLOCKS / 8, 1, fp);
	fclose(fp);
 
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path) {
	csc452_directory_entry dir;
	csc452_file_directory file;
	long dirBlock;
	int retval = search(path, &dir, &file, &dirBlock);
	if (retval == -1) {
		return -ENOENT;
	} else if (retval == 1) {
		return -EISDIR;
	}
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	sscanf(path, "/%" str(MAX_FILENAME) "[^/]/%" str(MAX_FILENAME) 
			"[^.].%" str(MAX_EXTENSION) "s",
			directory, filename, extension);
	long nNextBlock;
	long size = 0;
	for (int i = 0; i < dir.nFiles; i++) {
		csc452_file_directory cur = dir.files[i];
		if (strcmp(cur.fname, filename) == 0 &&
				strcmp(cur.fext, extension) == 0) {
			nNextBlock = cur.nStartBlock;
			size = cur.fsize;
			dir.nFiles--;
			for (int j = i; j < dir.nFiles; j++) {
				dir.files[j] = dir.files[j+1];
			}
			break;
		}
	}
	FILE *fp = fopen(".disk", "rb+");
	fseek(fp, dirBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&dir, BLOCK_SIZE, 1, fp);
	
	csc452_bitmap bitmap;
	fseek(fp, -(NUM_BLOCKS / 8), SEEK_END);
	fread(&bitmap, NUM_BLOCKS / 8, 1, fp);
	csc452_disk_block cur_block;
	while (size > 0) { // Set associated blocks as free
		bitmap.bitmap[nNextBlock / 8] &= ~(0x1 << (nNextBlock % 8));
		fseek(fp, nNextBlock * BLOCK_SIZE, SEEK_SET);
		fread(&cur_block, BLOCK_SIZE, 1, fp);
		nNextBlock = cur_block.nNextBlock;
		size -= MAX_DATA_IN_BLOCK;
	}
	fseek(fp, -(NUM_BLOCKS / 8), SEEK_END);
	fwrite(&bitmap, NUM_BLOCKS / 8, 1, fp);
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
