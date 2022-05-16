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

// define constants that will be used in the bitmap
#define DISK_SIZE 5242880 // 5 MB disk
#define MAP_CHUNK 8 // 8 bits in a byte

#define NUM_BITMAP_BYTES (DISK_SIZE / BLOCK_SIZE) / MAP_CHUNK

/*
 * define a struct for the bitmap, which holds one bit per disk block
 * number of blocks is disk size / block size = 10240
 * 10240 bits / 8 = 1280 bytes for the map, stored as unsigned chars
 * this is 2.5 disk blocks, so pad the rest
 */
struct csc452_bitmap
{
	unsigned char map[NUM_BITMAP_BYTES];
	// padding to align the bitmap with the disk blocks
	char padding[BLOCK_SIZE - (NUM_BITMAP_BYTES % BLOCK_SIZE)];
};

typedef struct csc452_bitmap csc452_bitmap;


/*
 * This function determines if a path string is a valid path to
 * a subdirectory. Doesn't check that the directory exists; only
 * checks that the dir name is of valid length and the permissions
 * are correct. That is, the directory must be a single level under
 * the root (and not the root itself)
 * 
 * -----------Examples-----------
 * valid: /abcdefgh, /abc, /a
 * too long: /morethaneight is too long
 * wrong permissions: /, abc
 */
static int isValidSubdir(const char *path) {
	if (path[0] != '/' || strlen(path) <= 1)
		return -EPERM;
	for (int i = 1; i <= 9; i++) {
		if (path[i] == '\0')
			return 1;
		else if (path[i] == '/')
			return -EPERM;
	}
	return -ENAMETOOLONG;
}

/*
 * This function determines if a path string is a valid file under
 * our system. Doesn't check that the file exists; only checks that
 * the filename and file extension are of valid length and the 
 * permissions are correct. That is, the file must be under a sub-
 * directory that is under the root
 * 
 * -----------Examples-----------
 * valid: /subdir/file.ext
 * too long: /subdir/abracadra.ent, /subdir/abra.cadabra
 * wrong permissions: /file, file
 */
static int isValidFile(const char *path) {
	if (path[0] != '/' || strlen(path) <= 3)
		return -EPERM;
	// everything is > 1 to check for names that are too long
	char dname[MAX_FILENAME + 2];
	char fname[MAX_FILENAME + 2];
	char fext[MAX_EXTENSION + 2];
	sscanf(path, "/%9[^/]/%9[^.].%4s", dname, fname, fext);
	if (strlen(fname) == 0)
		return -EPERM;
	if (strlen(fname) > 8 || strlen(fext) > 3)
		return -ENAMETOOLONG;
	return 1;
}

/*
 * Allocates a block from the disk using the bitmap. Always ensures
 * that the first block (root) and last three blocks (bitmap) are taken.
 * Then linearly searches for the first free block (represented by a 
 * zero bit). Updates the value to 1 and returns the block number of the
 * allocated block, or -1 if the disk is full.
 */
static long allocateBlock(){
	csc452_bitmap bitmap;
	FILE *disk = fopen(".disk", "r+");
	fseek(disk, -sizeof(bitmap), SEEK_END);
	fread(&bitmap, sizeof(bitmap), 1, disk);
	// make sure to set the first and last 3 blocks to be used
	bitmap.map[0] = bitmap.map[0] | 0x80;
	bitmap.map[NUM_BITMAP_BYTES - 1] = bitmap.map[NUM_BITMAP_BYTES - 1] | 0x7;
	for (int i = 0; i < NUM_BITMAP_BYTES; i++) {
		unsigned char curByte = bitmap.map[i];
		if (curByte < 0xff) { // there is a free space somewhere
			// find index of leftmost zero bit in the byte
			int freeBit = 0;
			while (curByte > 0x7F) {
				curByte = curByte << 1;
				freeBit++;
			}
			long freeBlock = (MAP_CHUNK * i) + freeBit;
			// allocate this disk block by writing out the changed byte
			unsigned char updated = bitmap.map[i] | (0x80 >> freeBit);
			fseek(disk, -sizeof(bitmap) + (sizeof(unsigned char) * i), SEEK_END);
			fwrite(&updated, sizeof(unsigned char), 1, disk);
			// clear the block in case it holds junk
			char block[BLOCK_SIZE];
			memset(block, 0, BLOCK_SIZE);
			fseek(disk, freeBlock * BLOCK_SIZE, SEEK_SET);
			fwrite(block, sizeof(BLOCK_SIZE), 1, disk);
			fclose(disk);
			return freeBlock;
		}
	}
	fclose(disk);
	return -1; // disk is completely full
}

/*
 * Frees a block from the disk using the bitmap. Receives block number 
 * as parameter and opens the bitmap to mark the block as free (changes
 * bit from 1 to 0).
 */
static void freeBlock(long block){
	csc452_bitmap bitmap;
	FILE *disk = fopen(".disk", "r+");
	fseek(disk, -sizeof(bitmap), SEEK_END);
	fread(&bitmap, sizeof(bitmap), 1, disk);
	// mark disk block as free
	int mapIndex = block / MAP_CHUNK;
	int bitOffset = block % MAP_CHUNK;
	unsigned char mask = bitmap.map[mapIndex] - (1 << (MAP_CHUNK - 1 - bitOffset));
	unsigned char updated = bitmap.map[mapIndex] & mask;
	fseek(disk, -sizeof(bitmap) + (sizeof(unsigned char) * mapIndex), SEEK_END);
	fwrite(&updated, sizeof(unsigned char), 1, disk);
	fclose(disk);
}

/* 
 * Gets the start block of the subdirectory entry with the given path.
 * Assumes that the path is a valid subdirectory. Opens up the root 
 * and linearly searches through its list of directories to find the 
 * matching subdirectory. Returns the start block of this subdir, or 
 * -ENOENT if no matching subdir is found.
 */
static int getSubdirStartBlock(const char *path){
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);
	csc452_root_directory root;
	FILE *disk = fopen(".disk", "r+");
	fseek(disk, 0, SEEK_SET);
	fread(&root, sizeof(csc452_root_directory), 1, disk);
	// linearly search through root's directories array
	for (int i = 0; i < root.nDirectories; i++) {
		if (strcmp(root.directories[i].dname, dname) == 0) {
			fclose(disk);
			return root.directories[i].nStartBlock;
		}
	}
	fclose(disk);
	return -ENOENT;
}

/*
 * Gets the start block of the file with the given path. Assumes the
 * path is a valid file. First calls getSubdirStartBlock to get the 
 * subdirectory block containing the file. Opens the subdir entry and
 * lienarly searches through its list of files to find the matching
 * file. Returns the start block of the file, or -ENOENT if no matching
 * file is found.
 */
static int getFileStartBlock(const char *path) {
	int subdirBlock = getSubdirStartBlock(path);
	if (subdirBlock < 0)
		return subdirBlock;
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);

	FILE *disk = fopen(".disk", "r+");
	csc452_directory_entry subdir;
	fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
	fread(&subdir, sizeof(csc452_directory_entry), 1, disk);
	// linearly search through subdirectory's files array
	for (int j = 0; j < subdir.nFiles; j++) {
		if (strcmp(subdir.files[j].fname, fname) == 0 && strcmp(subdir.files[j].fext, fext) == 0) {
			fclose(disk);
			return subdir.files[j].nStartBlock;
		}
	}
	fclose(disk);
	return -ENOENT;
}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 * 
 * Checks if the path is rot and sets corresponding attributes. Then
 * checks if the path is a subdirectory and exists; sets directory
 * attributes if yes. If the path continues, opens up the subdir and 
 * searches for a matching file. Sets file attributes if yes. On success,
 * returns 0; returns -ENOENT if path does not exist.
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	//fprintf(stderr, "----- in getattr -----\n");
	//fprintf(stderr, "\t---path: %s\n", path);
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		if (isValidFile(path) < 0 && isValidSubdir(path) < 0)
			return -ENOENT;
		char dname[MAX_FILENAME + 1];
		char fname[MAX_FILENAME + 1];
		char fext[MAX_EXTENSION + 1];
		sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);
		// get root
		csc452_root_directory root;
		FILE *disk = fopen(".disk", "r+");
		fseek(disk, 0, SEEK_SET);
		fread(&root, sizeof(csc452_root_directory), 1, disk);
		for (int i = 0; i < root.nDirectories; i++) {
			if (strcmp(root.directories[i].dname, dname) == 0) {
				// found the subdirectory
				if (isValidSubdir(path) > 0) {
					//If the path does exist and is a directory:
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					fclose(disk);
					return res;
				} else {
					// get subdirectory
					csc452_directory_entry subdir;
					fseek(disk, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
					fread(&subdir, sizeof(csc452_directory_entry), 1, disk);
					// search through all the files in the subdirectory
					for (int i = 0; i < subdir.nFiles; i++) {
						if (strcmp(subdir.files[i].fname, fname) == 0 && strcmp(subdir.files[i].fext, fext) == 0) {
							//If the path does exist and is a file:
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 2;
							stbuf->st_size = subdir.files[i].fsize;
							fclose(disk);
							return res;
						}
					}
					break; // finish for loop: file not found
				}
			}		
		}	// finish for loop: subdir not found		
		// Else return that path doesn't exist
		fclose(disk);
		res = -ENOENT;
	}
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 * 
 * Adds itself and above directory to the buf. If the path is the root dir,
 * fills the buf with all subdirectories inside root. If the path is a subdir,
 * opens up the subdir and adds all the files to the buf. Returns -ENOENT
 * if the given path does not exist or is invalid.
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{

	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	//fprintf(stderr, "----- in readdir -----\n");

	int ret = 0;
	// get root
	csc452_root_directory root;
	FILE *disk = fopen(".disk", "r+");
	fseek(disk, 0, SEEK_SET);
	fread(&root, sizeof(csc452_root_directory), 1, disk);

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		for (int i = 0; i < root.nDirectories; i++)
			filler(buf, root.directories[i].dname, NULL, 0);
	}
	else { // subdirectory case
		if (isValidSubdir(path) < 0) {
			fclose(disk);
			return -ENOENT; // invalid subdir path
		}
		fclose(disk);
		int subdirBlock = getSubdirStartBlock(path);
		if (subdirBlock < 0)
			return subdirBlock;
		// found the subdirectory, get subdirectory
		disk = fopen(".disk", "r+");
		csc452_directory_entry subdir;
		fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
		fread(&subdir, sizeof(csc452_directory_entry), 1, disk);
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		// iterate through all the files in the subdirectory and add them
		for (int i = 0; i < subdir.nFiles; i++) {
			char filename[MAX_FILENAME + MAX_EXTENSION + 2] = "";
			strcat(filename, subdir.files[i].fname);
			strcat(filename, ".");
			strcat(filename, subdir.files[i].fext);
			filler(buf, filename, NULL, 0);
		}
	}
	fclose(disk);
	return ret;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 * 
 * Checks that the given path is a valid subdirectory string. Then 
 * reads in the root and searches it to check if subdir exists already;
 * returns -EEXIST if this is the case. Otherwise, creates a new subdir 
 * by calling allocateBlock() and updating the root entry.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	//fprintf(stderr, "----- in mkdir -----\n");

	// check that the path is of the form "/subdir"
	int ret = isValidSubdir(path);
	if (ret < 0)
		return ret;
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);
	csc452_root_directory root;
	FILE *disk = fopen(".disk", "r+");
	fseek(disk, 0, SEEK_SET);
	fread(&root, sizeof(csc452_root_directory), 1, disk);
	// linearly search through root's directories array
	for (int i = 0; i < root.nDirectories; i++) {
		if (strcmp(root.directories[i].dname, dname) == 0)
			return -EEXIST;
	}
	// error checking done, here we actually make a subdirectory in root
	fclose(disk); // must close since nextFree opens disk, oof
	long nextFree = allocateBlock(); // allocate disk block for subdir
	//fprintf(stderr, "\t---dir %s allocated to block %d\n", dname, (int)nextFree);
	disk = fopen(".disk", "r+");
	strcpy(root.directories[root.nDirectories].dname, dname);
	root.directories[root.nDirectories].nStartBlock = nextFree;
	root.nDirectories++;
	// write out updated root
	fseek(disk, 0, SEEK_SET);
	fwrite(&root, sizeof(csc452_root_directory), 1, disk);
	fclose(disk);
	return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 * Note that the mknod shell command is not the one to test this.
 * mknod at the shell is used to create "special" files and we are
 * only supporting regular files.
 * 
 * Checks that the given path is a valid file path string. Then gets
 * the subdir from the path, returning -ENOENT if it is invalid. Searches
 * the subdir to check if the file exists already; returns -EEXIST if this
 * is the case. Otherwise, creates a new file by calling allocateBlock()
 * and updating the subdir entry.
 *
 */
static int csc452_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) path;
	(void) mode;
    (void) dev;

	//fprintf(stderr, "----- in mknod -----\n");

	// check that path is a valid file
	int ret = isValidFile(path);
	if (ret < 0)
		return ret;
	int subdirBlock = getSubdirStartBlock(path);
	if (subdirBlock < 0)
		return -ENOENT; // subdirectory is invalid

	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);
	FILE *disk = fopen(".disk", "r+");
	csc452_directory_entry subdir;
	fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
	fread(&subdir, sizeof(csc452_directory_entry), 1, disk);
	for (int j = 0; j < subdir.nFiles; j++) {
		if (strcmp(subdir.files[j].fname, fname) == 0 &&
			strcmp(subdir.files[j].fext, fext) == 0) {
			fclose(disk);
			return -EEXIST;
		}
	}
	fclose(disk);
	long nextFree = allocateBlock(); // allocate disk block for file
	//fprintf(stderr, "\t---file %s.%s allocated to block %d\n", fname, fext, (int)nextFree);
	strcpy(subdir.files[subdir.nFiles].fname, fname);
	strcpy(subdir.files[subdir.nFiles].fext, fext);
	subdir.files[subdir.nFiles].fsize = 0;
	subdir.files[subdir.nFiles].nStartBlock = nextFree;
	subdir.nFiles++;
	// write out updated subdir entry
	disk = fopen(".disk", "r+");
	fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&subdir, sizeof(csc452_directory_entry), 1, disk);
	fclose(disk);
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 * Checks that the path is a valid file, returning -EISDIR if it is a
 * directory path. Gets the subdirectory block and file start block, 
 * returning -ENOENT if the file cannot be found. Then jumps to the 
 * correct file block based on the given offset and starts reading,
 * returning -EFBIG if the offset is beyond the file size. Reads 
 * blocks until the size to be read has been read, filling in the
 * buffer for each block. Returns the size read on success.
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//fprintf(stderr, "----- in read -----\n");
	//fprintf(stderr, "\t---size=%d, offset=%d\n", size, (int)offset);

	if (isValidSubdir(path) > 0 || strcmp(path, "/") == 0)
		return -EISDIR;

	int ret = isValidFile(path);
	if (ret < 0)
		return ret;
	if (size <= 0)
		return -1;
	
	int subdirBlock = getSubdirStartBlock(path);
	int fileStartBlock = getFileStartBlock(path);
	if (fileStartBlock < 0)
		return fileStartBlock; // no entry
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);
	// need to follow file blocks to get to the offset locale
	int fileBlockNum = offset / MAX_DATA_IN_BLOCK;	// how many file blocks to jump
	int blockOffset = offset % MAX_DATA_IN_BLOCK;	// where to start reading w/in block
	FILE *disk = fopen(".disk", "r+");
	// get subdirectory
	csc452_directory_entry subdir;
	fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
	fread(&subdir, sizeof(csc452_directory_entry), 1, disk);
	int fileIndex = -1;
	for (int j = 0; j < subdir.nFiles; j++) {
		if (strcmp(subdir.files[j].fname, fname) == 0 &&
			strcmp(subdir.files[j].fext, fext) == 0) {
			// must exist, since fileStartBlock worked above
			fileIndex = j;
			break;
		}
	}
	if (offset > subdir.files[fileIndex].fsize) {
		fclose(disk);
		return -EFBIG;
	}
	//fprintf(stderr, "\t---file %s.%s starts at block %d\n", fname, fext, fileStartBlock);
	csc452_disk_block fileBlock;
	fseek(disk, fileStartBlock * BLOCK_SIZE, SEEK_SET);
	fread(&fileBlock, sizeof(csc452_disk_block), 1, disk);
	int curBlock = fileStartBlock;
	for (int i = 0; i < fileBlockNum; i++) {
		//fprintf(stderr, "\t---jumping to a middle point\n");
		fseek(disk, fileBlock.nNextBlock * BLOCK_SIZE, SEEK_SET);
		curBlock = fileBlock.nNextBlock;
		fread(&fileBlock, sizeof(csc452_disk_block), 1, disk);
	}
	// now should have all the info needed
	int leftToRead = size;
	int lenRead = 0;
	char *bufPtr = buf;
	while (leftToRead > 0) {
		// amount that can be read in this block
		int spaceOnBlock = MAX_DATA_IN_BLOCK - blockOffset;
		if (spaceOnBlock < leftToRead) {
			//fprintf(stderr, "\t---read part of file at block %d\n", curBlock);
			// read the amount that fits
			strncpy(bufPtr, fileBlock.data + blockOffset, spaceOnBlock);
			lenRead += strlen(fileBlock.data + blockOffset);
			bufPtr += spaceOnBlock;
			leftToRead -= spaceOnBlock;
			int nextBlock = fileBlock.nNextBlock;
			//fprintf(stderr, "\t---nextblock: %d\n", nextBlock);
			if (nextBlock == 0) { // we are done reading, actually
				break;
			}
			fseek(disk, nextBlock * BLOCK_SIZE, SEEK_SET);
			fread(&fileBlock, sizeof(csc452_disk_block), 1, disk);
			blockOffset = 0;
			curBlock = nextBlock;
		}
		else {
			//fprintf(stderr, "\t---finish reading of file at block %d\n", curBlock);
			// what's left to read is on this block, we're done
			strncpy(bufPtr, fileBlock.data + blockOffset, leftToRead);
			lenRead += strlen(fileBlock.data + blockOffset);
			leftToRead = 0;
		}
	}
	fclose(disk);
	//fprintf(stderr, "\t---read %d bytes\n", lenRead);
	return lenRead;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 * Checks that the path is a valid file and size is > 0, returning -1
 * if not. Gets the subdirectory block and file start block, returning
 * -ENOENT if the file cannot be found. Then jumps to the correct file
 * block based on the given offset and starts writing, returning -EFBIG
 * if the offset is beyond the file size. Writes from the buffer into 
 * data blocks until the size to be read has been read, allocating blocks
 * as needed. Returns the size written on success.
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//fprintf(stderr, "----- in write -----\n");
	//fprintf(stderr, "\t---size=%d, offset=%d\n", size, (int)offset);

	int ret = isValidFile(path);
	if (ret < 0 || size <= 0)
		return -1;
	
	int subdirBlock = getSubdirStartBlock(path);
	int fileStartBlock = getFileStartBlock(path);
	if (fileStartBlock < 0)
		return fileStartBlock; // no entry
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);
	// need to follow file blocks to get to the offset locale
	int fileBlockNum = offset / MAX_DATA_IN_BLOCK;	// how many file blocks to jump
	int blockOffset = offset % MAX_DATA_IN_BLOCK;	// where to start writing w/in block
	FILE *disk = fopen(".disk", "r+");
	// get subdirectory
	csc452_directory_entry subdir;
	fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
	fread(&subdir, sizeof(csc452_directory_entry), 1, disk);
	int fileIndex = -1;
	for (int j = 0; j < subdir.nFiles; j++) {
		if (strcmp(subdir.files[j].fname, fname) == 0 &&
			strcmp(subdir.files[j].fext, fext) == 0) {
			// must exist, since fileStartBlock worked above
			fileIndex = j;
			break;
		}
	}
	if (offset > subdir.files[fileIndex].fsize) {
		fclose(disk);
		return -EFBIG;
	}
	//fprintf(stderr, "\t---file %s.%s starts at block %d\n", fname, fext, fileStartBlock);
	int curBlock = fileStartBlock;
	csc452_disk_block fileBlock;
	fseek(disk, fileStartBlock * BLOCK_SIZE, SEEK_SET);
	fread(&fileBlock, sizeof(csc452_disk_block), 1, disk);
	for (int i = 0; i < fileBlockNum; i++) {
		//fprintf(stderr, "\t---jumping to a middle point\n");
		fseek(disk, fileBlock.nNextBlock * BLOCK_SIZE, SEEK_SET);
		curBlock = fileBlock.nNextBlock;
		fread(&fileBlock, sizeof(csc452_disk_block), 1, disk);
	}
	// now should have all the info needed
	int leftToWrite = size;
	const char *bufPtr = buf;
	while (leftToWrite > 0) {
		// what can fit in this file block
		int spaceOnBlock = MAX_DATA_IN_BLOCK - blockOffset;
		if (spaceOnBlock < leftToWrite) {
			//fprintf(stderr, "\t---write part of file to block %d\n", curBlock);
			// write the amount that fits
			strncpy(fileBlock.data + blockOffset, bufPtr, spaceOnBlock);
			fseek(disk, curBlock * BLOCK_SIZE, SEEK_SET);
			fwrite(&fileBlock, sizeof(csc452_disk_block), 1, disk);
			bufPtr += spaceOnBlock;
			leftToWrite -= spaceOnBlock;
			int nextBlock = fileBlock.nNextBlock;
			if (nextBlock == 0) {
				fclose(disk);
				nextBlock = allocateBlock();
				if (nextBlock < 0)
					return -ENOSPC; // BAD NEED TO FIX THIS
				fopen(".disk", "r+");
				// update the file block with linking, need to write it out
				fileBlock.nNextBlock = nextBlock;
				fseek(disk, curBlock * BLOCK_SIZE, SEEK_SET);
				fwrite(&fileBlock, sizeof(csc452_disk_block), 1, disk);
			}
			fseek(disk, nextBlock * BLOCK_SIZE, SEEK_SET);
			fread(&fileBlock, sizeof(csc452_disk_block), 1, disk);
			blockOffset = 0;
			curBlock = nextBlock;
		}
		else {
			//fprintf(stderr, "\t---finish writing file to block %d\n", curBlock);
			// what's left fits on this block, we're done
			strncpy(fileBlock.data + blockOffset, bufPtr, leftToWrite);
			fseek(disk, curBlock * BLOCK_SIZE, SEEK_SET);
			fwrite(&fileBlock, sizeof(csc452_disk_block), 1, disk);
			leftToWrite = 0;
		}
	}
	int endpoint = offset + size;
	if (endpoint > subdir.files[fileIndex].fsize)
		subdir.files[fileIndex].fsize = endpoint;
	fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&subdir, sizeof(csc452_directory_entry), 1, disk);
	fclose(disk);
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 * Checks that the path is a valid subdirectory, returning -ENOTDIR if 
 * not. Opens the root and searches for the matching subdir, returning
 * -ENOENT if it doesn't exist. Then checks that the subdir is empty,
 * returning -ENOTEMPTY if not. If empty, frees the block that contains
 * the subdirectory entry and updates the root, then returns 0.
 */
static int csc452_rmdir(const char *path)
{
	(void) path;

	//fprintf(stderr, "----- in rmdir -----\n");

	if (isValidSubdir(path) < 0)
	  	return -ENOTDIR;
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);
	// get root
	csc452_root_directory root;
	FILE *disk = fopen(".disk", "r+");
	fseek(disk, 0, SEEK_SET);
	fread(&root, sizeof(csc452_root_directory), 1, disk);
	for (int i = 0; i < root.nDirectories; i++) {
		if (strcmp(root.directories[i].dname, dname) == 0) {
			// found the subdirectory, get subdirectory
			csc452_directory_entry subdir;
			fseek(disk, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			fread(&subdir, sizeof(csc452_directory_entry), 1, disk);
			if (subdir.nFiles != 0) {
				fclose(disk);
				return -ENOTEMPTY;
			} else {	// found an empty directory, remove it
				int toFree = root.directories[i].nStartBlock;
				fclose(disk);	// close disk because freeBlock opens disk
				freeBlock(toFree);		// update bitmap
				disk = fopen(".disk", "r+");
				// update root; shift directories array
				for (int cur = i; cur < root.nDirectories - 1; cur++) {
					root.directories[cur] = root.directories[cur + 1];
				}
				root.nDirectories--;
				// write out updated root
				fseek(disk, 0, SEEK_SET);
				fwrite(&root, sizeof(csc452_root_directory), 1, disk);
				fclose(disk);
				return 0;
			}
		}
	}
	fclose(disk);
	return -ENOENT;
}

/*
 * Removes a file.
 *
 * Checks that the the path is not a directory, returning -EISDIR if
 * it is. Then gets the subdirectory and file start blocks, returning 
 * -ENOENT if the file is not found. Opens up the subdirectory to find
 * the index of the file in its list of files. Then iterates through
 * the links of file blocks to deallocates each one, overwriting file
 * blocks to 0's. Updates the subdirectory entry and returns 0.
 */
static int csc452_unlink(const char *path)
{
    (void) path;

	//fprintf(stderr, "----- in unlink -----\n"); 

	if (isValidSubdir(path) > 0 || strcmp(path, "/") == 0)
		return -EISDIR;
	int subdirBlock = getSubdirStartBlock(path);
	int fileStartBlock = getFileStartBlock(path);
	if (fileStartBlock < 0)
		return fileStartBlock; // file not found
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_EXTENSION + 1];
	sscanf(path, "/%8[^/]/%8[^.].%3s", dname, fname, fext);
	// need to open the subdirectory and find the file in files list
	FILE *disk = fopen(".disk", "r+");
	// get subdirectory
	csc452_directory_entry subdir;
	fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
	fread(&subdir, sizeof(csc452_directory_entry), 1, disk);
	int fileIndex = -1;
	for (int j = 0; j < subdir.nFiles; j++) {
		if (strcmp(subdir.files[j].fname, fname) == 0 &&
			strcmp(subdir.files[j].fext, fext) == 0) {
			// must exist, since fileStartBlock worked above
			fileIndex = j;
			break;
		}
	}
	csc452_disk_block fileBlock;
	long curBlock = fileStartBlock;
	long nextBlock;
	// deallocate blocks
	// set all of their links to 0 and write them out
	while (curBlock != 0) {
		fseek(disk, curBlock * BLOCK_SIZE, SEEK_SET);
		fread(&fileBlock, sizeof(csc452_disk_block), 1, disk);
		nextBlock = fileBlock.nNextBlock;
		memset(fileBlock.data, 0, sizeof(csc452_disk_block)); // rewrite it all to 0
		fileBlock.nNextBlock = 0;
		fseek(disk, curBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&fileBlock, sizeof(csc452_disk_block), 1, disk); // write it out
		// deallocate in bitmap
		fclose(disk);
		freeBlock(curBlock);
		disk = fopen(".disk", "r+");
		curBlock = nextBlock;
	}
	// update the subdirectory and write it out
	for (int cur = fileIndex; cur < subdir.nFiles - 1; cur++) {
		subdir.files[cur] = subdir.files[cur + 1];
	}
	subdir.nFiles--;
	// write out updated subdir
	fseek(disk, subdirBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&subdir, sizeof(csc452_directory_entry), 1, disk);
	fclose(disk);
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
