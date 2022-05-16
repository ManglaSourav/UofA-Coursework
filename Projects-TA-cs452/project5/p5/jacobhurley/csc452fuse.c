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
		csc452_root_directory* rootDir = malloc(sizeof(csc452_root_directory));
		FILE* file;
		file = fopen(".disk", "rb+"); 
		fread(rootDir, sizeof(csc452_root_directory), 1, file); // get root from disk
		int numRead = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		//If the path does exist and is a directory:
		//stbuf->st_mode = S_IFDIR | 0755;
		//stbuf->st_nlink = 2;
		if (numRead == 1) { // directory
			for(int i=0; i < rootDir->nDirectories; i++){ // loop through all directories
				if (strcmp(directory, rootDir->directories[i].dname) == 0) { // if dir found
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					return 0;
				}
			}
			//return -ENOENT; returned at end 
		}
		//If the path does exist and is a file:
		//stbuf->st_mode = S_IFREG | 0666;
		//stbuf->st_nlink = 2;
		//stbuf->st_size = file size
		else if (numRead >= 2) { // if file
			int direcBlock = -1;
			for (int i = 0; i < rootDir->nDirectories; i++) {
				if (strcmp(directory, rootDir->directories[i].dname) == 0) { // if dir found
					direcBlock = rootDir->directories[i].nStartBlock;
				}
			}
			csc452_directory_entry* directoryEntry = malloc(sizeof(csc452_directory_entry));
			long offset = (sizeof(csc452_directory_entry) * direcBlock);
			fseek(file, offset, SEEK_SET);
			fread(directoryEntry, sizeof(csc452_directory_entry), 1, file); // get directory
			for (int i = 0; i < directoryEntry->nFiles; i++) { // loop through files
				if (strcmp(directoryEntry->files[i].fname, filename) == 0 && strcmp(directoryEntry->files[i].fext, extension) == 0) { // if file found
					fclose(file);
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 2;
					stbuf->st_size = directoryEntry->files[i].fsize;
					return 0;
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
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	csc452_root_directory* rootDir = malloc(sizeof(csc452_root_directory));
	FILE* file;
	file = fopen(".disk", "rb+");
	fread(rootDir, sizeof(csc452_root_directory), 1, file); // get root from disk
	int numRead = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

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
		// list all directories
		for (int i = 0; i < rootDir->nDirectories; i++) {
			filler(buf, rootDir->directories[i].dname, NULL, 0);
		}
	}
	else {
		// check if directory exists
		int found = -1;
		if (numRead == 1) { // only directory read in
			for (int i = 0; i < rootDir->nDirectories; i++) { // search for directory
				if (strcmp(rootDir->directories[i].dname, directory) == 0) {
					found = i;
					break;
				}
			}
			if (found == -1) { // if directory doesnt exist, exit
				fclose(file);
				return -ENOENT;
			}
			else {
				filler(buf, ".", NULL, 0);
				filler(buf, "..", NULL, 0);
				csc452_directory_entry* directoryEntry = malloc(sizeof(csc452_directory_entry));
				int block = rootDir->directories[found].nStartBlock;
				long offset = (sizeof(csc452_directory_entry) * block);
				fseek(file, offset, SEEK_SET);
				fread(directoryEntry, sizeof(csc452_directory_entry), 1, file);
				for (int i = 0; i < directoryEntry->nFiles; i++) { // list all files
					char tempStr[(MAX_FILENAME + 1) * 2];
					strcat(tempStr, directoryEntry->files[i].fname);
					strcat(tempStr, directoryEntry->files[i].fext);
					filler(buf, tempStr, NULL, 0);
				}
			}
		}
		
	}
	fclose(file);
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	// create variables for attributes
	char directory[MAX_FILENAME+1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION + 1];
	csc452_root_directory* rootDir = malloc(sizeof(csc452_root_directory));
	// open disk to read and write data
	FILE* file;
	file = fopen(".disk", "rb+");
	fread(rootDir, sizeof(csc452_root_directory), 1, file); // get root from disk
	//(void) path;
	(void)mode;
	// get attributes from path
	int readIn = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	if (readIn == 0) { // directory was not read in
		fclose(file);
		return -1;
	}
	if (readIn > 1) { // not under root directory
		fclose(file);
		return -EPERM;
	}
	if (strlen(directory) > MAX_FILENAME) { // check if length of directory is too long
		fclose(file);
		return -ENAMETOOLONG;
	}
	if (rootDir == NULL) { // read nothing from disk
		rootDir->nDirectories = 0;
	}
	else if (rootDir->nDirectories > MAX_DIRS_IN_ROOT) {
		fclose(file);
		return -1; // dont know error code for not adding dir because too many exist
	}
	else { // check if directory exists
		int i = 0;
		while (i < rootDir->nDirectories) { // loop through all directories
			if (strcmp(directory, rootDir->directories[i].dname) == 0) { // if dir found
				return -EEXIST;
			}
			i++;
		}
	}
	int emptySlot = rootDir->nDirectories;
	// directory memory starts after root (1 block) + previous directories (ndirectories blocks)
	//rootDir->directories[rootDir->nDirectories].dname = directory;
	strcpy(rootDir->directories[emptySlot].dname, directory);
	// long offset = sizeof(csc452_root_directory) + (sizeof(csc452_directory_entry) * rootDir->nDirectories);  // used for offset when creating the actual directory
	rootDir->directories[emptySlot].nStartBlock = 1 + emptySlot;
	rootDir->nDirectories += 1;
	fseek(file, 0, SEEK_SET);
	fwrite(rootDir, sizeof(csc452_root_directory), 1, file);
	fclose(file);
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
static int csc452_mknod(const char* path, mode_t mode, dev_t dev)
{
	(void)mode;
	(void)dev;
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	csc452_root_directory* rootDir = malloc(sizeof(csc452_root_directory));
	FILE* file;
	file = fopen(".disk", "rb+");
	fread(rootDir, sizeof(csc452_root_directory), 1, file); // get root from disk
	int numRead = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (numRead < 2) {
		fclose(file);
		return -EPERM;
	}
	else {
		if (strlen(filename) > MAX_FILENAME) {
			fclose(file);
			return -ENAMETOOLONG;
		}
		else {
			// find directory
			int blockNum = -1;
			for (int i = 0; i < rootDir->nDirectories; i++) {
				if (strcmp(rootDir->directories[i].dname, directory) == 0) {
					blockNum = rootDir->directories[i].nStartBlock;
					break;
				}
			}
			if (blockNum == -1) {
				// directory not found
				fclose(file);
				return -1;
			}
			csc452_directory_entry* directoryEntry = malloc(sizeof(csc452_directory_entry));
			long offset = (sizeof(csc452_directory_entry) * blockNum);
			fseek(file, offset, SEEK_SET);
			fread(directoryEntry, sizeof(csc452_directory_entry), 1, file);
			if (directoryEntry->nFiles == MAX_FILES_IN_DIR) { // max files reached
				fclose(file);
				return -1;
			}
			for (int i = 0; i < directoryEntry->nFiles; i++) {
				if (strcmp(directoryEntry->files[i].fname, filename) == 0 && strcmp(directoryEntry->files[i].fext, extension) == 0) {
					fclose(file);
					return -EEXIST;
				}
			}
			strcpy(directoryEntry->files[directoryEntry->nFiles].fname, filename);
			strcpy(directoryEntry->files[directoryEntry->nFiles].fext, extension);
			directoryEntry->files[directoryEntry->nFiles].fsize = 0;
			int startBlock = 1;
			csc452_directory_entry* temp = malloc(sizeof(csc452_directory_entry));
			for (int i = 0; i < rootDir->nDirectories; i++) {
				long offset = (sizeof(csc452_directory_entry) * rootDir->directories[i].nStartBlock);
				fseek(file, offset, SEEK_SET);
				fread(temp, sizeof(csc452_directory_entry), 1, file);
				startBlock += temp->nFiles;
			}
			directoryEntry->files[directoryEntry->nFiles].nStartBlock = startBlock;
			directoryEntry->nFiles += 1;
			fwrite(directoryEntry, sizeof(csc452_directory_entry), 1, file);
			fclose(file);
		}
	}
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char* path, char* buf, size_t size, off_t offset,
	struct fuse_file_info* fi)
{
	(void)buf;
	(void)offset;
	(void)fi;
	(void)path;

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
static int csc452_write(const char* path, const char* buf, size_t size,
	off_t offset, struct fuse_file_info* fi)
{
	(void)fi;
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	csc452_root_directory* rootDir = malloc(sizeof(csc452_root_directory));
	FILE* file;
	file = fopen(".disk", "rb+");
	fread(rootDir, sizeof(csc452_root_directory), 1, file); // get root from disk
	int numRead = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (numRead < 2) {
		fclose(file);
		return -EPERM;
	}
	else {
		if (strlen(filename) > MAX_FILENAME) {
			fclose(file);
			return -ENAMETOOLONG;
		}
		else {
			// check that directory exists
			int blockNum = -1;
			for (int i = 0; i < rootDir->nDirectories; i++) {
				if (strcmp(rootDir->directories[i].dname, directory) == 0) {
					blockNum = rootDir->directories[i].nStartBlock;
					break;
				}
			}
			if (blockNum == -1) { // directory not valid
				fclose(file);
				return -1;
			}
			else { // if directory valid
				csc452_directory_entry* directoryEntry = malloc(sizeof(csc452_directory_entry));
				long offset = (sizeof(csc452_directory_entry) * blockNum);
				fseek(file, offset, SEEK_SET);
				fread(directoryEntry, sizeof(csc452_directory_entry), 1, file);
				int fileBlock = -1;
				for (int i = 0; i < directoryEntry->nFiles; i++) {
					if(strcmp(directoryEntry->files[i].fname, filename) == 0 && strcmp(directoryEntry->files[i].fext, extension) == 0){
						fileBlock = directoryEntry->files[i].nStartBlock;
					}
				}
				if (fileBlock == -1) {
					fclose(file);
					return -1; // not valid
				}
				// check size
				if (size <= 0) {
					fclose(file);
					return -1;
				}
				offset = (sizeof(csc452_directory_entry) * fileBlock);
				fseek(file, offset, SEEK_SET);
				fwrite(buf, sizeof(buf), 1, file);
				fclose(file);
				return size;
			}
		}
	}
	


	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//return success, or error

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
	csc452_root_directory* rootDir = malloc(sizeof(csc452_root_directory));
	FILE* file;
	file = fopen(".disk", "rb+");
	fread(rootDir, sizeof(csc452_root_directory), 1, file); // get root from disk
	int numRead = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (numRead == 2) {
		fclose(file);
		return -ENOTDIR;
	}
	if (numRead == 1) { // directory
		//check if directory exists
		int i = 0;
		int exists = -1;
		while (i < rootDir->nDirectories) { // loop through all directories
			if (strcmp(directory, rootDir->directories[i].dname) == 0) { // if dir found
				exists = i;
				break;
			}
			i++;
		}
		if (exists == -1) {
			fclose(file);
			return -ENOENT;
		}
		
		csc452_directory_entry* directoryEntry = malloc(sizeof(csc452_directory_entry));
		int block = rootDir->directories[exists].nStartBlock;
		long offset = (sizeof(csc452_directory_entry) * block);
		fseek(file, offset, SEEK_SET);
		fread(directoryEntry, sizeof(csc452_directory_entry), 1, file);
		if (directoryEntry->nFiles > 0) {
			fclose(file);
			return -ENOTEMPTY;
		}
		for (int i = exists; i < rootDir->nDirectories; i++) {
			memcpy(&rootDir->directories[exists], &rootDir->directories[exists + 1], sizeof(struct csc452_directory));
			//rootDir->directories[exists] = rootDir->directories[exists + 1];
		}
		struct csc452_directory* temp = malloc(sizeof(struct csc452_directory));
		memcpy(&rootDir->directories[MAX_DIRS_IN_ROOT], &temp, sizeof(struct csc452_directory));
		//memset(rootDir->directories[exists].dname, 0, MAX_FILENAME);
		rootDir->nDirectories -= 1;
		//rootDir->directories[exists].nStartBlock = 0;
		fseek(file, 0, SEEK_SET);
		fwrite(rootDir, sizeof(csc452_root_directory), 1, file);
	}
	fclose(file);
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
