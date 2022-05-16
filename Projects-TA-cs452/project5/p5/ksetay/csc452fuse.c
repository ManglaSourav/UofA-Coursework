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

struct csc452_bitmap
{
	long cur;
};
typedef struct csc452_bitmap csc452_bitmap;

#define LAST_BLOCK MAX_FILES_IN_DIR * MAX_DIRS_IN_ROOT - 1
static int write_to_block(int blockNum, void* fileSystemStruct) {

	FILE *fp;
	fp = fopen(".disk", "r+");

	if (fp == NULL) {
		printf("Failed file open\n");
		return -1;
	}
	fseek(fp, BLOCK_SIZE * blockNum, SEEK_SET);

	if (fwrite(fileSystemStruct, BLOCK_SIZE, 1, fp) != 0) {
	//	printf("Successfully wrote to file\n");
		fclose(fp);
		return 0;
	}
	else {
		printf("Failed write to file\n");
		fclose(fp);
		return -1;
	}
}

static int read_from_block(int blockNum, void* fileSystemStruct) {	
	FILE *fp;
	fp = fopen(".disk", "r");

	if (fp == NULL) {
		printf("Failed file open\n");
		return -1;
	}

	fseek(fp, BLOCK_SIZE * blockNum, SEEK_SET);

	if (fread(fileSystemStruct, BLOCK_SIZE, 1, fp) != 0) {
		fclose(fp);
		return 0;
	}
	else {
		fclose(fp);
		return -1;
	}
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
	
	char splitDir[MAX_FILENAME + 1] = "";
	char splitFile[MAX_FILENAME + 1] = "";
	char splitExt[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", splitDir, splitFile, splitExt);
	
	printf("GETATTR: path: %s\n", path);
	printf("GETATTR: sD: '%s', sF: '%s', sE: '%s'\n", splitDir, splitFile, splitExt);	
	printf("Blocks: %d\n", MAX_FILES_IN_DIR * MAX_DIRS_IN_ROOT);
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
       	else {
		// Set root_dir to value at block 0
		csc452_root_directory *root_dir;
		root_dir = malloc(sizeof(char) * BLOCK_SIZE);
		read_from_block(0, root_dir);
		
		// Iterate through the directories in the root's array
		printf("GETATTR: Searching through dirs for %s\n", splitDir);
		printf("GETATTR: nDirs: %d\n", root_dir->nDirectories); 
		int i;
		for (i = 0; i < root_dir->nDirectories; i++) {
		//	printf("GETATTR: dirs[i] = %s\n", root_dir->directories[i].dname);
			// If the split directory matches the array value
			if (strcmp(root_dir->directories[i].dname, splitDir) == 0) {				
				printf("GETATTR: Found existing directory\n");
				printf("GETATTR: splitFile: '%s', strlen: %d\n", splitFile, strcmp(splitFile, "")); 
				if (strcmp(splitFile, "") == 0 || strcmp(splitFile,"\0") == 0) { // We are looking for a directory
					printf("GETATTR: Stopped search because looking for dir\n");
					//If the path does exist and is a directory:
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					return res;
				}
				else { // We are looking for a file in a directory
					// Loads the directory from .disk
					csc452_directory_entry *dir_entry;
					dir_entry = malloc(sizeof(char) * BLOCK_SIZE);
					read_from_block(root_dir->directories[i].nStartBlock, dir_entry);

					// Iterate through the files in the directory
					printf("GETATTR: Searching through files in %s\n", splitDir);
					int j;
					for (j = 0; j < dir_entry->nFiles; j++) {
						// Check if the fname matches our search
						printf("GETATTR: Looking at file '%s'\n", dir_entry->files[j].fname);
						if (strcmp(dir_entry->files[j].fname, splitFile) == 0) {
							//If the path does exist and is a file:
							printf("GETATTR: Found fname\n");
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 2;
							stbuf->st_size = dir_entry->files[j].fsize;
							return res;
						}
					}	
				}
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

	int i, j;

	char splitDir[MAX_FILENAME + 1] = "";
	char splitFile[MAX_FILENAME + 1] = "";
	char splitExt[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", splitDir, splitFile, splitExt);
	// Set root_dir to value at block 0
	csc452_root_directory *root_dir;
	root_dir = malloc(sizeof(char) * BLOCK_SIZE);
	read_from_block(0, root_dir);
	
	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		
		for (i = 0; i < root_dir->nDirectories; i++) {
			filler(buf, root_dir->directories[i].dname, NULL, 0);
		}
	}
	else {
		// Iterate through the directories in the root's array
		printf("READDIR: Searching through dirs for %s\n", splitDir);
		printf("READDIR: nDirs: %d\n", root_dir->nDirectories); 
		
		for (i = 0; i < root_dir->nDirectories; i++) {
			printf("READDIR: dirs[i] = %s\n", root_dir->directories[i].dname);
			// If the split directory matches the array value
			if (strcmp(root_dir->directories[i].dname, splitDir) == 0) {				
				printf("READDIR: Found matching directory\n");
				printf("READDIR: splitFile: '%s', strlen: %d\n", splitFile, strcmp(splitFile, "")); 
				// Loads the directory from .disk
				csc452_directory_entry *dir_entry;
				dir_entry = malloc(sizeof(char) * BLOCK_SIZE);
				read_from_block(root_dir->directories[i].nStartBlock, dir_entry);

				// Iterate through the files in the directory

				filler(buf, ".", NULL,0);
				filler(buf, "..", NULL, 0);
				for (j = 0; j < dir_entry->nFiles; j++) {
					filler(buf, dir_entry->files[j].fname, NULL, 0);	
				}
				return 0;
			}
		}
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
	
	char splitDir[MAX_FILENAME + 1];

	sscanf(path, "/%s", splitDir);
	
	printf("MKDIR: splitDir: '%s'\n", splitDir);
	
	// Set root_dir to value at block 0
	csc452_root_directory *root_dir;
	root_dir = malloc(sizeof(char) * BLOCK_SIZE);
	read_from_block(0, root_dir);
	printf("MKDIR: nDirs after initial read: %d\n", root_dir->nDirectories);

	csc452_bitmap *map;
	map = malloc(sizeof(char) * BLOCK_SIZE);
	read_from_block(LAST_BLOCK, map);
	map->cur += 1;
	
	// Fill the array entry in the root_dir for a new dir
	int cur_dir = root_dir->nDirectories;
	strcpy(root_dir->directories[cur_dir].dname, splitDir);
	root_dir->directories[cur_dir].nStartBlock = map->cur;
	root_dir->nDirectories = root_dir->nDirectories + 1;
	printf("MKDIR: New nDirs: %d\n", root_dir->nDirectories); 
	printf("MKDIR: Array val: %s\n", root_dir->directories[cur_dir].dname);
	write_to_block(0, root_dir); 

	// Create new directory entry and write it to .disk
	csc452_directory_entry *newDir;
	newDir = malloc(sizeof(csc452_directory_entry));
	newDir->nFiles = 0;
	printf("MKDIR: Wrote the new directory to block %d\n", 1);


	write_to_block(map->cur, newDir);

	write_to_block(LAST_BLOCK, map);

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
	(void) mode;
    	(void) dev;
	
	int i, j;

	char splitDir[MAX_FILENAME + 1] = "";
	char splitFile[MAX_FILENAME + 1] = "";
	char splitExt[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", splitDir, splitFile, splitExt);
	// Set root_dir to value at block 0
	
	printf("MKNOD: sD: '%s', sF: '%s', sE: '%s'\n", splitDir, splitFile, splitExt);	
	if (strlen(splitFile) <= 1) {
		printf("MKNOD: File being created in root\n"); 
		return -EPERM;
	}

	csc452_root_directory *root_dir;
	root_dir = malloc(sizeof(char) * BLOCK_SIZE);
	read_from_block(0, root_dir);
	
	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	
	// Iterate through the directories in the root's array
	printf("MKNOD: Searching through dirs for %s\n", splitDir);
	printf("MKNOD: nDirs: %d\n", root_dir->nDirectories); 
	
	for (i = 0; i < root_dir->nDirectories; i++) {
		printf("MKNOD: dirs[i] = %s\n", root_dir->directories[i].dname);
		// If the split directory matches the array value
		if (strcmp(root_dir->directories[i].dname, splitDir) == 0) {				
			printf("MKNOD: Found matching directory\n");
			
			// Loads the directory from .disk
			csc452_directory_entry *dir_entry;
			dir_entry = malloc(sizeof(char) * BLOCK_SIZE);
			read_from_block(root_dir->directories[i].nStartBlock, dir_entry);

			// Checks if the files exists already.
			for (j = 0; j < dir_entry->nFiles; j++) {
				if (strcmp(splitFile, dir_entry->files[j].fname) == 0 && strcmp(splitExt, dir_entry->files[j].fext) == 0) {
					return -EEXIST;
				}		
			}

			// Reads from the bitmap
			csc452_bitmap *map;
			map = malloc(sizeof(char) * BLOCK_SIZE);
			read_from_block(LAST_BLOCK, map);
			map->cur += 1;
			printf("MKNOD: Read %ld from the bitmap\n", map->cur); 

			// Fill up dir_entry array index
			strcpy(dir_entry->files[dir_entry->nFiles].fname, splitFile);		
			strcpy(dir_entry->files[dir_entry->nFiles].fext, splitExt);
			dir_entry->files[dir_entry->nFiles].fsize = 512;		
			dir_entry->files[dir_entry->nFiles].nStartBlock = map->cur;
			
			// Increment dir_entry nFiles
			dir_entry->nFiles += 1;

			// Make new file entry and write it to .disk
			csc452_disk_block *new_block;
			new_block = malloc(sizeof(char) * BLOCK_SIZE);
			new_block->nNextBlock = 0;
			write_to_block(map->cur, new_block);

			// Updates dir_entry to disk
			write_to_block(root_dir->directories[i].nStartBlock, dir_entry);
			write_to_block(LAST_BLOCK, map);
			return 0;
		}
	}
	return -ENOENT;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) fi;

	int i, j;

	char splitDir[MAX_FILENAME + 1] = "";
	char splitFile[MAX_FILENAME + 1] = "";
	char splitExt[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", splitDir, splitFile, splitExt);
	// Set root_dir to value at block 0
	
	printf("READ: sD: '%s', sF: '%s', sE: '%s'\n", splitDir, splitFile, splitExt);	
	if (strlen(splitFile) <= 1) {
		printf("READ: Target is not a file\n"); 
		return -EPERM;
	}

	csc452_root_directory *root_dir;
	root_dir = malloc(sizeof(char) * BLOCK_SIZE);
	read_from_block(0, root_dir);
	
	// Iterate through the directories in the root's array
	printf("READ: Searching through dirs for %s\n", splitDir);
	printf("READ: nDirs: %d\n", root_dir->nDirectories); 
	
	for (i = 0; i < root_dir->nDirectories; i++) {
		printf("READ: dirs[i] = %s\n", root_dir->directories[i].dname);
		// If the split directory matches the array value
		if (strcmp(root_dir->directories[i].dname, splitDir) == 0) {				
			printf("READ: Found matching directory\n");
			
			// Loads the directory from .disk
			csc452_directory_entry *dir_entry;
			dir_entry = malloc(sizeof(char) * BLOCK_SIZE);
			read_from_block(root_dir->directories[i].nStartBlock, dir_entry);

			// Checks if the files exists already.
			for (j = 0; j < dir_entry->nFiles; j++) {
				if (strcmp(splitFile, dir_entry->files[j].fname) == 0 && strcmp(splitExt, dir_entry->files[j].fext) == 0) {
					csc452_disk_block *diskBlock;
					diskBlock = malloc(sizeof(char) * BLOCK_SIZE);
					read_from_block(dir_entry->files[j].nStartBlock, diskBlock);

					int sizeI;
					for (sizeI = offset; sizeI < size; sizeI++ ) {
						buf[sizeI - offset] = diskBlock->data[sizeI];
					}

					return size;
				}		
			}
			return -EISDIR;
		}
	}
	return -EISDIR;
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

	int i, j;

	char splitDir[MAX_FILENAME + 1] = "";
	char splitFile[MAX_FILENAME + 1] = "";
	char splitExt[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", splitDir, splitFile, splitExt);
	// Set root_dir to value at block 0
	
	printf("WRITE: sD: '%s', sF: '%s', sE: '%s'\n", splitDir, splitFile, splitExt);	
	if (strlen(splitFile) <= 1) {
		printf("WRITE: Target is not a file\n"); 
		return -EPERM;
	}

	csc452_root_directory *root_dir;
	root_dir = malloc(sizeof(char) * BLOCK_SIZE);
	read_from_block(0, root_dir);
	
	// Iterate through the directories in the root's array
	printf("WRITE: Searching through dirs for %s\n", splitDir);
	printf("WRITE: nDirs: %d\n", root_dir->nDirectories); 
	
	for (i = 0; i < root_dir->nDirectories; i++) {
		printf("WRITE: dirs[i] = %s\n", root_dir->directories[i].dname);
		// If the split directory matches the array value
		if (strcmp(root_dir->directories[i].dname, splitDir) == 0) {				
			printf("WRITE: Found matching directory\n");
			
			// Loads the directory from .disk
			csc452_directory_entry *dir_entry;
			dir_entry = malloc(sizeof(char) * BLOCK_SIZE);
			read_from_block(root_dir->directories[i].nStartBlock, dir_entry);

			// Checks if the files exists already.
			for (j = 0; j < dir_entry->nFiles; j++) {
				if (strcmp(splitFile, dir_entry->files[j].fname) == 0 && strcmp(splitExt, dir_entry->files[j].fext) == 0) {
					csc452_disk_block *diskBlock;
					diskBlock = malloc(sizeof(char) * BLOCK_SIZE);
					read_from_block(dir_entry->files[j].nStartBlock, diskBlock);

					int sizeI;
					for (sizeI = offset; sizeI < size; sizeI++ ) {
						diskBlock->data[sizeI] = buf[sizeI - offset];
					}

					write_to_block(dir_entry->files[j].nStartBlock, diskBlock);
					return size;
				}		
			}
			return -EFBIG;
		}
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

	int i, j;

	char splitDir[MAX_FILENAME + 1] = "";
	char splitFile[MAX_FILENAME + 1] = "";
	char splitExt[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", splitDir, splitFile, splitExt);

	printf("READDIR: sD: '%s', sF: '%s', sE: '%s'\n", splitDir, splitFile, splitExt);	
	printf("READDIR: splitFile: '%s', strlen: %d\n", splitFile, strcmp(splitFile, "")); 
	if (strlen(splitFile) > 1 || strlen(splitExt) > 1) {
		printf("RMDIR: strlen dont work\n");
		return -ENOTDIR; 
	}

	// Set root_dir to value at block 0
	csc452_root_directory *root_dir;
	root_dir = malloc(sizeof(char) * BLOCK_SIZE);
	read_from_block(0, root_dir);
	
	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		return -ENOTEMPTY;
	}
	else {
		// Iterate through the directories in the root's array
		printf("RMDIR: Searching through dirs for %s\n", splitDir);
		printf("RMDIR: nDirs: %d\n", root_dir->nDirectories); 
		
		for (i = 0; i < root_dir->nDirectories; i++) {
			printf("RMDIR: dirs[i] = %s\n", root_dir->directories[i].dname);
			// If the split directory matches the array value
			if (strcmp(root_dir->directories[i].dname, splitDir) == 0) {				
				printf("READDIR: Found matching directory\n");
				// Loads the directory from .disk
				csc452_directory_entry *dir_entry;
				dir_entry = malloc(sizeof(char) * BLOCK_SIZE);
				read_from_block(root_dir->directories[i].nStartBlock, dir_entry);

				if (dir_entry->nFiles == 0) {
					for (j = i; j < root_dir->nDirectories; j++) {
						root_dir->directories[j] = root_dir->directories[j+1];

					}
					root_dir->nDirectories -= 1;
					write_to_block(0, root_dir);			
					return 0;
				}
				else {
					return -ENOTEMPTY;
				}
			}
		}
		return -ENOENT;
	}

	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
	int i, j;

	char splitDir[MAX_FILENAME + 1] = "";
	char splitFile[MAX_FILENAME + 1] = "";
	char splitExt[MAX_EXTENSION + 1] = "";

	sscanf(path, "/%[^/]/%[^.].%s", splitDir, splitFile, splitExt);
	// Set root_dir to value at block 0
	
	printf("WRITE: sD: '%s', sF: '%s', sE: '%s'\n", splitDir, splitFile, splitExt);	
	if (strlen(splitFile) <= 1) {
		printf("WRITE: Target is not a file\n"); 
		return -EPERM;
	}

	csc452_root_directory *root_dir;
	root_dir = malloc(sizeof(char) * BLOCK_SIZE);
	read_from_block(0, root_dir);
	
	// Iterate through the directories in the root's array
	printf("UNLINK: Searching through dirs for %s\n", splitDir);
	printf("UNLINK: nDirs: %d\n", root_dir->nDirectories); 
	
	for (i = 0; i < root_dir->nDirectories; i++) {
		printf("UNLINK: dirs[i] = %s\n", root_dir->directories[i].dname);
		// If the split directory matches the array value
		if (strcmp(root_dir->directories[i].dname, splitDir) == 0) {				
			printf("UNLINK: Found matching directory\n");
			
			// Loads the directory from .disk
			csc452_directory_entry *dir_entry;
			dir_entry = malloc(sizeof(char) * BLOCK_SIZE);
			read_from_block(root_dir->directories[i].nStartBlock, dir_entry);

			// Checks if the files exists already.
			for (j = 0; j < dir_entry->nFiles; j++) {
				if (strcmp(splitFile, dir_entry->files[j].fname) == 0 && strcmp(splitExt, dir_entry->files[j].fext) == 0) {

					int k;
					for (k = j; k < dir_entry->nFiles - 1; k++) {
						dir_entry->files[k] = dir_entry->files[k+1];

					}
					dir_entry->nFiles -= 1;
					write_to_block(root_dir->directories[i].nStartBlock, dir_entry);			
					return 0;
				}		
			}
			return -ENOENT;
		}
	}
	return -ENOENT;
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
