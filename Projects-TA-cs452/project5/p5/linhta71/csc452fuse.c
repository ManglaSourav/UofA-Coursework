/*
	FUSE: Filesystem in Userspace

	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452

    Author: My Linh Ta
    Project 5
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


static void getRoot(csc452_root_directory *root); 
static int getDirectory(csc452_directory_entry *entry, char *directory);
static int fileExist(char *directory, char *fileName, char *extension, int pathCount); 
static int directoryExist(char *directory);
static long next_free_block(); 

/*
    this function will get the root from the mount via the parameter
*/
static void getRoot(csc452_root_directory *root) {
	FILE * filePointer = fopen(".disk", "rb");
	if (filePointer != NULL) {
		fread(root, sizeof(csc452_root_directory), 1, filePointer);
	}
	fclose(filePointer);
}

/*
    this function will get the directory with the specified name
    and return the index of that directory under root
*/
static int getDirectory(csc452_directory_entry *entry, char *directory) {
	csc452_root_directory root;
	getRoot(&root);
	int dirIndex = directoryExist(directory);
	if (dirIndex == -1) return -1;
	FILE *filePointer = fopen(".disk", "rb");
	if (filePointer == NULL) {
		fprintf(stderr, "Cannot open root!");
		return -1;
	}
	fseek(filePointer, root.directories[dirIndex].nStartBlock, SEEK_SET);
	fread(entry, sizeof(csc452_directory_entry), 1, filePointer);
	fclose(filePointer);

	return dirIndex;
}

/*
    this function will check if the file exist in the path, return the file size if found,
    -1 if not found
*/
static int fileExist(char * directory, char * fileName, char * extension, int pathCount) {
	csc452_directory_entry entry;
	int dirIndex = getDirectory(&entry, directory);
	if (dirIndex == -1) return -1;
	int i;
	for (i = 0; i < entry.nFiles; i++) {
		if (strcmp(fileName, entry.files[i].fname) == 0) {
			if (pathCount == 3) {
				if (strcmp(extension, entry.files[i].fext) == 0) {
					return entry.files[i].fsize;
				}
			} else {
				return entry.files[i].fsize;
			}
		}
	}
	return -1;
}

/*
    this function will check if a direcotyr exist under root and return its index
*/
static int directoryExist(char *directory) {
	csc452_root_directory root;
	getRoot(&root);
	for (int i = 0; i < root.nDirectories; i++) {
		if (strcmp(directory, root.directories[i].dname) == 0) {
			return i; 
		}
	}
	return -1;
}

/*
    this function will get the next free block in disk
*/
static long next_free_block() {
	FILE *filePointer = fopen(".disk", "rb+");
	fseek(filePointer, 0, SEEK_END); 
	long file_max = ftell(filePointer) - BLOCK_SIZE;  
	fseek(filePointer, (-1)*BLOCK_SIZE, SEEK_END); 
	// tmp is most recenly occupied
	long tmp;
	fread(&tmp, sizeof(long), 1, filePointer); 
	// so, next is next one
	long next = tmp + BLOCK_SIZE; 
	if (next >= file_max) {
		fclose(filePointer); 
		return -1; 
	}
	fseek(filePointer, (-1)*BLOCK_SIZE, SEEK_END); 
	// write back the recently occupied block
	fwrite(&next, sizeof(long), 1, filePointer); 
	fclose(filePointer); 
	return next; 

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
		char fileName[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];
		int pathCount = sscanf(path, "/%[^/]/%[^.].%s", directory, fileName, extension); 
		//If the path does exist and is a directory:
		if (pathCount == 1 && directoryExist(directory)!= -1) {
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return res;
		} else if (pathCount == 2 || pathCount == 3) {
			int fileSize = fileExist(directory, fileName, extension, pathCount);
			if (fileSize != -1) {
				//If the path does exist and is a file:
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 2;
				stbuf->st_size = (size_t) fileSize;
				return res;
			}
		}
		//Else return that path doesn't exist
		fprintf(stderr, "Invalid path/Path does not exists\n");
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
		int pathCount = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
		if (pathCount == 1) {
			csc452_directory_entry entry;  
			int directoryLocation = getDirectory(&entry, directory); 
			// dir not exist
            if (directoryLocation == -1) return -ENOENT;
			for (int i = 0; i < entry.nFiles; i++) {
				filler(buf, entry.files[i].fname, NULL, 0);
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
	// (void) path;
	(void) mode;

	char directory[MAX_FILENAME+1];
	char fileName[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	int pathCount = sscanf(path, "/%[^/]/%[^.].%s", directory, fileName, extension);
	if (pathCount != 1) return -EPERM;
	if (strlen(directory) > MAX_FILENAME) return -ENAMETOOLONG;
	if (directoryExist(directory) != -1) return -EEXIST;
	csc452_root_directory root;
	getRoot(&root);

	if (root.nDirectories >= MAX_DIRS_IN_ROOT) {
		fprintf(stderr, "Cannot create directory, max # of directory reached.");
		return -ENOSPC;
	}

	long address = next_free_block();
	if (address == -1) {
		fprintf(stderr, "Out of space");
		return -EDQUOT;
	}

    root.directories[root.nDirectories].nStartBlock = address;
	strcpy(root.directories[root.nDirectories].dname, directory);
	root.nDirectories++;
	FILE *filePointer = fopen(".disk", "rb+");
	fwrite(&root, sizeof(csc452_root_directory), 1, filePointer);
	fclose(filePointer);

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
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int pathCount = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);	
    if (pathCount != 3) { 
        return -EPERM; 
    } 
	if (strlen(filename) > 8 || strlen(extension) > 3) {
		return -ENAMETOOLONG; 
	}
	if (fileExist(directory, filename, extension, 3) != -1) {
		return -EEXIST;
	}
	csc452_directory_entry entry;  
	int directoryLocation = getDirectory(&entry, directory); 
	csc452_root_directory root; 
	getRoot(&root); 
	if (entry.nFiles >= MAX_FILES_IN_DIR) {
		fprintf(stderr, "Cannot create directory, max # of directory reached.\n");
		return -ENOSPC; 
	} else {
		long address = next_free_block(); 
		if (address == -1) { 
			fprintf(stderr, "Out of space");
			return -EDQUOT; 
		}
		strcpy(entry.files[entry.nFiles].fname, filename);
		strcpy(entry.files[entry.nFiles].fext, extension);
		entry.files[entry.nFiles].fsize = 0; 
		entry.files[entry.nFiles].nStartBlock = address; 
		entry.nFiles += 1; 
		// write the updated directory into the disk. 
		FILE *filePointer = fopen(".disk", "rb+");
		fseek(filePointer, root.directories[directoryLocation].nStartBlock, SEEK_SET); 
		fwrite(&entry, sizeof(csc452_directory_entry), 1, filePointer);
		fclose(filePointer); 
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
	(void) fi;
	char directory[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	int pathCount = sscanf(path, "/%[^/]/%[^.].%s", directory, fileName, extension);
	//check to make sure path exists
	if (pathCount == 1 && directoryExist(directory) != 1) return -EISDIR;
	if (pathCount != 3) return -ENOENT;
	size_t fileSize = fileExist(directory, fileName, extension, 3);
	if (fileSize == -1) return -ENOENT;
	//check that size is > 0
	if (size == 0) return size;
	csc452_directory_entry entry;
	getDirectory(&entry, directory);
	int i;
	for (i = 0; i < entry.nFiles; i++) {
		if (strcmp(fileName, entry.files[i].fname) == 0 &&
			strcmp(extension, entry.files[i].fext) == 0) break;
	}
	//check that offset is <= to the file size
	if (offset > fileSize) return -EINVAL;
	//read in data
	FILE *filePointer = fopen(".disk", "rb");
	csc452_disk_block blockFile;
	fseek(filePointer, entry.files[i].nStartBlock, SEEK_SET);
	fread(&blockFile, sizeof(csc452_disk_block), 1, filePointer);
	int begin = offset;
	while (begin > MAX_DATA_IN_BLOCK) {
		fseek(filePointer, blockFile.nNextBlock, SEEK_SET);
		fread(&blockFile, sizeof(csc452_disk_block), 1, filePointer);
		begin = begin - MAX_DATA_IN_BLOCK;
	}
	if (size > fileSize) size = fileSize;
	size_t leftRead = size;
	int buf_ind = 0;
	int blockFile_left = MAX_DATA_IN_BLOCK - begin;
	while (leftRead > 0) {
		if (blockFile_left == 0) {
			fseek(filePointer, blockFile.nNextBlock, SEEK_SET);
			fread(&blockFile, sizeof(csc452_disk_block), 1, filePointer);
			blockFile_left = MAX_DATA_IN_BLOCK;
			begin = 0;
		}
		buf[buf_ind] = blockFile.data[begin];
		leftRead--;
		blockFile_left--;
		begin++;
		buf_ind++;
	}
	//return success, or error
	fclose(filePointer);

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
	(void) fi;
	char directory[MAX_FILENAME + 1];
	char fileName[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	int pathCount = sscanf(path, "/%[^/]/%[^.].%s", directory, fileName, extension); 
	// check path
	if (pathCount == 1 && directoryExist(directory) != -1) return -EISDIR;
	if (pathCount != 3) return -ENOENT;
	size_t fileSize = fileExist(directory, fileName, extension, 3);
	// size has to be bigger
	if (size == 0) return 0;
	csc452_directory_entry entry;
	int dirInd = getDirectory(&entry, directory);
	int i;
	for (i = 0; i < entry.nFiles; i++) {
		if (strcmp(fileName, entry.files[i].fname) == 0 && strcmp(extension, entry.files[i].fext) == 0) break;
	}
	//check that offset is <= to the file size
	if (offset > fileSize) return -EFBIG;
	FILE *filePointer = fopen(".disk", "rb+");
	csc452_disk_block blockFile;
	long addressBlock = entry.files[i].nStartBlock;
	fseek(filePointer, addressBlock, SEEK_SET);
	fread(&blockFile, sizeof(csc452_disk_block), 1, filePointer);
	size_t sizeIncrease = offset - fileSize + size;
	if (sizeIncrease > 0) {
		entry.files[i].fsize += sizeIncrease;
		csc452_root_directory root;
		getRoot(&root);
		fseek(filePointer, root.directories[dirInd].nStartBlock, SEEK_SET);
		fwrite(&entry, sizeof(csc452_directory_entry), 1, filePointer);
	}
	int begin = offset; 
	while (begin > MAX_DATA_IN_BLOCK) {
		addressBlock =  blockFile.nNextBlock; 
		fseek(filePointer, addressBlock, SEEK_SET); 
		fread(&blockFile, sizeof(csc452_disk_block), 1, filePointer); 
		begin = begin - MAX_DATA_IN_BLOCK; 
	}

	size_t leftWrite = size;
	int bufInd = 0;
	int sizeLeft = MAX_DATA_IN_BLOCK - begin;
	while (leftWrite > 0) {
		if (sizeLeft == 0) {
			if (blockFile.nNextBlock == 0){
				long address = next_free_block();
				if (address == -1) {
					fprintf(stderr, "Out of space");
					return -EFBIG;
				}
				blockFile.nNextBlock = address;
			}
			fseek(filePointer, addressBlock, SEEK_SET);
			fwrite(&blockFile, sizeof(csc452_disk_block), 1, filePointer);
			addressBlock = blockFile.nNextBlock;
			fread(&blockFile, sizeof(csc452_disk_block), 1, filePointer);
			sizeLeft = MAX_DATA_IN_BLOCK;
			begin = 0;
		}
		blockFile.data[begin] = buf[bufInd];
		leftWrite--; sizeLeft--; begin++; bufInd++;
	}
	//write data
	fseek(filePointer, addressBlock, SEEK_SET);
	fwrite(&blockFile, sizeof(csc452_disk_block), 1, filePointer);
	fclose(filePointer);
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

	  return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
        (void) path;
    //Parse path
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	
	int pathCount = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
    if (pathCount != 3) { 
        return -EISDIR; 
    } 
	// check if file exists

    //Find directory
	csc452_directory_entry entry;
	int directoryLocation = getDirectory(&entry, directory);
	//Find file
	int i, fileIndex = -1;
	for(i=0; i<entry.nFiles; i++)
	{
		if(strcmp(entry.files[i].fname, filename) == 0 && strcmp(entry.files[i].fext, extension) == 0)
		{
			fileIndex = i;
			break;
		}
	}
	if(fileIndex == -1)
	{
		return -ENOENT; //File not found
	}
	
	//Remove from bitmap
	FILE* file = fopen(".disk", "r+b");
	if (!file)
	{
		return -ENOENT; //Could not open .disk
	}
	
	//Calculate the location on disk to write && mask used to flip the correct bit
	int byteToWrite = (entry.files[fileIndex].nStartBlock-3) / 8;
	int mask = 0b10000000 >> ((entry.files[fileIndex].nStartBlock-3) % 8);
	
	fseek(file, byteToWrite, SEEK_SET); //Move to the appropriate character
	unsigned char cur = fgetc(file);
	
	cur ^= mask;
	
	fseek(file, byteToWrite, SEEK_SET); //Move back
	fputc(cur, file);

    //Delete from directory entry
	entry.files[fileIndex] = entry.files[entry.nFiles - 1];
	entry.nFiles--;
    csc452_root_directory root; 
	getRoot(&root); 
    fseek(file, root.directories[directoryLocation].nStartBlock, SEEK_SET); 
    fwrite(&entry, sizeof(csc452_directory_entry), 1, file);
	
	fclose(file);	

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
