/*
 *	File: csc452fuse.c
 *	Author: Jacob Edwards
 *	Description: Contains an implementation of the FUSE interface.
 *
 *	Compilation: gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452
 *	Usage: ./csc452 -d [foldername]
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
#include <unistd.h>

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


// ========================================================================== //
/*
 * Parsing a path for its folder name (or NULL), file name, and extension.
 * Populates a struct Path: char * folder, char * file, char * ext;
 */
struct Path {
	char folder[200];
	char file[200];
	char ext[20];
	int level;
};
static void parse(const char * path, struct Path * out) {
	char folder[200] = "";
	char file[200] = "";
	char ext[20] = "";

	int count = 0;
	for (char * ptr = path; *ptr != '\0'; ptr++) {
		if (*ptr == '/') {
			count++;
		}
	}
	if (count == 1) {
		sscanf(path, "/%[^/]", folder);
		strcpy(out->folder, folder);
		strcpy(out->file, file);
		strcpy(out->ext, ext);
	} else if (count == 2) {
		sscanf(path, "/%[^/]/%[^.].%s", folder, file, ext);
		strcpy(out->folder, folder);
		strcpy(out->file, file);
		strcpy(out->ext, ext);
	}

	out->level = count;
	printf("parsing done\n");
};

// helper functions for finding structs with certain names
static struct csc452_directory * findDirectory(struct csc452_root_directory * root, char * dname) {
	printf("Looking through %d dirs\n", root->nDirectories);
	for (int i = 0; i < root->nDirectories; i++) {
		struct csc452_directory * dir = &(root->directories[i]);
		if (strcmp(dir->dname, dname) == 0) {
			return dir;
		}
	}
	return NULL;
}
static struct csc452_file_directory * findFile(struct csc452_directory_entry * entry, char * fname, char * fext) {
	for (int i = 0; i < entry->nFiles; i++) {
		struct csc452_file_directory * dir = &(entry->files[i]);
		if (strcmp(dir->fname, fname) == 0 && strcmp(dir->fext, fext) == 0) {
			return dir;
		}
	}
	return NULL;
}

// finds the first block after directory entries
static int findNextAvailableBlock(int disk) {
	int offset = BLOCK_SIZE * (MAX_DIRS_IN_ROOT);
	struct csc452_disk_block block;
	do {
		offset += sizeof(struct csc452_disk_block);
		printf("reading block at %d\n", offset);
		pread(disk, &block, sizeof(struct csc452_disk_block), offset);
	} while (block.data[0] != 0);
	printf("Found new block at %d\n", offset);
	return offset;
}
// ========================================================================== //

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *strPath, struct stat *stbuf)
{
	printf("Hello from get attribute!\n");
	printf("Getting attrs for %s\n", strPath);
	int res = -ENOENT;

	if (strcmp(strPath, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0;
	} else  {
		struct Path path;
		struct csc452_root_directory root;

		parse(strPath, &path);
		int disk = open(".disk", O_RDWR);
		pread(disk, &root, sizeof(struct csc452_root_directory), 0);
		for (int i = 0; i < root.nDirectories; i++) {
			struct csc452_directory dir = root.directories[i];

			// if the path to the folder exists
			if (strcmp(dir.dname, path.folder) == 0) {
				// if the path is a folder
				if (path.level == 1) {
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					res = 0;
				} else {
					struct csc452_directory_entry entry;
					pread(disk, &entry, sizeof(struct csc452_directory_entry), dir.nStartBlock);
					for (int j = 0; j < entry.nFiles; j++) {
						struct csc452_file_directory diskFile = entry.files[j];

						// if the path to the file exists
						if (strcmp(diskFile.fname, path.file) == 0
						    && strcmp(diskFile.fext, path.ext) == 0) {
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 2;
							stbuf->st_size = diskFile.fsize;
							res = 0;
						}
					}
				}
			}
		}
		close(disk);

		//IF the path exists and is a folder:
		//stbuf->st_mode = S_IFDIR | 0755;
		//stbuf->st_nlink = 2;

		//If the path does exist and is a file:
		//stbuf->st_mode = S_IFREG | 0666;
		//stbuf->st_nlink = 2;
		//stbuf->st_size = file size;

		//Else return that path doesn't exist
	}

	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *strPath, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	printf("Hello from readdir!\n");
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	int res = -ENOENT;

	// opening the disk
	int disk = open(".disk", O_RDWR);

	// getting the root
	struct csc452_root_directory root;
	pread(disk, &root, sizeof(struct csc452_root_directory), 0);

	//A directory holds two entries, one that represents itself (.)
	//and one that represents the directory above us (..)
	if (strcmp(strPath, "/") == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		for (int i = 0; i < root.nDirectories; i++) {
			if (strlen(root.directories[i].dname) != 0) { // no need to list unused dirs
				filler(buf, root.directories[i].dname, NULL, 0);
			}
		}
		res = 0;
	}
	else {
		struct Path path;
		parse(strPath, &path);

		for (int i = 0; i < root.nDirectories; i++) {
			struct csc452_directory dir = root.directories[i];

			// if we find the referenced folder
			if (strcmp(dir.dname, path.folder) == 0) {
				struct csc452_directory_entry entry;
				pread(disk, &entry, sizeof(struct csc452_directory_entry), dir.nStartBlock);

				filler(buf, ".", NULL, 0);
				filler(buf, "..", NULL, 0);

				for (int j = 0; j < entry.nFiles; j++) {
					char full[20] = "";
					strcat(full, entry.files[j].fname);
					if (strlen(entry.files[j].fext) > 0) {
						strcat(full, ".");
						strcat(full, entry.files[j].fext);
					}
					filler(buf, full, NULL, 0);
				}
				res = 0;
			}
		}
	}

	// closing the disk
	close(disk);

	return res;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *strPath, mode_t mode)
{
	printf("Hello from make directory!\n");

	// parsing the path
	struct Path path;
	parse(strPath, &path);

	// opening the disk
	int disk = open(".disk", O_RDWR);

	// getting the root
	struct csc452_root_directory root;
	pread(disk, &root, sizeof(struct csc452_root_directory), 0);

	// checking path is of the correct form
	if (path.level == 2) {
		return -EPERM;
	}

	// checking directory name is not too long
	if (strlen(path.folder) > MAX_FILENAME) {
		return -ENAMETOOLONG;
	}

	// making sure there is room to add new directory
	if (root.nDirectories == MAX_DIRS_IN_ROOT - 1) {
		return -EPERM;
	}

	// checking to make sure dir doesn't already exist
	int i;
	for (i = 0; i < root.nDirectories; i++) {
		struct csc452_directory dir = root.directories[i];
		if (strcmp(dir.dname, path.folder) == 0) {
			return -EEXIST;
		}
	}

	// making the new directory in root
	struct csc452_directory * dir = &(root.directories[root.nDirectories]);

	strcpy(dir->dname, path.folder);
	dir->nStartBlock = BLOCK_SIZE * (root.nDirectories + 1);
	printf("New directory at: %ld\n", dir->nStartBlock);
	root.nDirectories++;

	// writing the new root to disk
	pwrite(disk, &root, sizeof(struct csc452_root_directory), 0);

	// we don't need to make a new directory entry at this time

	// closing the disk
	close(disk);

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
static int csc452_mknod(const char *strPath, mode_t mode, dev_t dev)
{
	printf("Hello from make file\n");

	(void) mode;
    	(void) dev;

	struct Path path;
	parse(strPath, &path);

	// checking we aren't trying to create in root directory
	if (path.level == 1) {
		return -EPERM;
	}

	// checking file name and ext length
	if (strlen(path.file) > MAX_FILENAME || strlen(path.ext) > MAX_EXTENSION) {
		printf("f: %s e: %s\n", path.file, path.ext);
		return -ENAMETOOLONG;
	}

	// opening the disk
	int disk = open(".disk", O_RDWR);
	if (disk == -1) {
		fprintf(stderr, "\nCould not open the disk\n");
		exit(1);
	}

	// getting the root dir
	struct csc452_root_directory root;
	pread(disk, &root, sizeof (struct csc452_root_directory), 0);

	// finding the folder
	for (int i = 0; i < root.nDirectories; i++) {
		struct csc452_directory * dir = &(root.directories[i]);

		if (strcmp(dir->dname, path.folder) == 0) {
			printf("Found directory: %s at %ld\n", dir->dname, dir->nStartBlock);
			struct csc452_directory_entry entry;
			pread(disk, &entry, sizeof(struct csc452_directory_entry), dir->nStartBlock);

			for (int j = 0; j < entry.nFiles; j++) {
				if (strcmp(entry.files[j].fname, path.file) == 0 && strcmp(entry.files[j].fext, path.ext) == 0)  {
					return -EEXIST;
				}
			}

			// if we get here, we know the file does not already exist
			struct csc452_file_directory * file = &(entry.files[entry.nFiles]);
			strcpy(file->fname, path.file);
			strcpy(file->fext, path.ext);
			file->fsize = 0;
			file->nStartBlock = 0;
			entry.nFiles++;

			// writing the modified directory entry back to disk
			pwrite(disk, &entry, sizeof(struct csc452_directory_entry), dir->nStartBlock);
		}
	}

	// closing the disk
	close(disk);

	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *strPath, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) fi;

	printf("Hello from read!\n");

	struct Path path;
	parse(strPath, &path);

	if (path.level == 1) {
		return -EISDIR;
	}

	if (size <= 0) {
		return -EFBIG;
	}

	// opening the disk
	int disk = open(".disk", O_RDWR);

	// getting the root
	struct csc452_root_directory root;
	pread(disk, &root, sizeof(struct csc452_root_directory), 0);

	// getting the directory
	struct csc452_directory * dir = findDirectory(&root, path.folder);
	if (dir == NULL) {
		fprintf(stderr, "\nCould not find directory: %s\n", path.folder);
		exit(1);
	}

	struct csc452_directory_entry entry;
	pread(disk, &entry, sizeof(struct csc452_directory_entry), dir->nStartBlock);

	// getting the file entry
	struct csc452_file_directory * file = findFile(&entry, path.file, path.ext);
	if (file == NULL) {
		fprintf(stderr, "\nCould not find file: %s.%s\n", path.file, path.ext);
		exit(1);
	}

	if (offset > file->fsize) {
		close(disk);
		return -EFBIG;
	}

	int localOffset = offset % MAX_DATA_IN_BLOCK;
	int numBlocks = offset / MAX_DATA_IN_BLOCK;

	struct csc452_disk_block block;
	pread(disk, &block, sizeof(struct csc452_disk_block), file->nStartBlock);

	int currentBlock = file->nStartBlock;

	// getting to the current block
	for (int i = 0; i < numBlocks; i++) {
		currentBlock = block.nNextBlock;
		pread(disk, &block, sizeof(struct csc452_disk_block), block.nNextBlock);
	}

	printf("Current block: %d\n", currentBlock);

	// reading the block(s) data
	int i;
	for (i = 0; i < size; i++) {
		buf[i] = block.data[localOffset];
		printf("Read %c from %d\n", buf[i], currentBlock + localOffset);

		if (buf[i] == '\0') {
			i++;
			break;
		}

		localOffset++;

		if (localOffset == MAX_DATA_IN_BLOCK) { // we have filled up this block
			if (block.nNextBlock == 0) {
				i++;
				break;
			} else {
				currentBlock = block.nNextBlock;
				pread(disk, &block, sizeof(struct csc452_disk_block), currentBlock);
			}
			localOffset = 0;
		}
	}

	close(disk);

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *strPath, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	struct Path path;
	parse(strPath, &path);

	if (size <= 0) {
		return -EFBIG;
	}

	// opening the disk
	int disk = open(".disk", O_RDWR);

	// getting the root
	struct csc452_root_directory root;
	pread(disk, &root, sizeof(struct csc452_root_directory), 0);

	// getting the directory
	struct csc452_directory * dir = findDirectory(&root, path.folder);
	if (dir == NULL) {
		fprintf(stderr, "\nCould not find directory: %s\n", path.folder);
		exit(1);
	}
	struct csc452_directory_entry entry;
	pread(disk, &entry, sizeof(struct csc452_directory_entry), dir->nStartBlock);

	// getting the file entry
	struct csc452_file_directory * file = findFile(&entry, path.file, path.ext);
	if (file == NULL) {
		fprintf(stderr, "\nCould not find file: %s.%s\n", path.file, path.ext);
		exit(1);
	}

	if (offset > file->fsize) {
		close(disk);
		return -EFBIG;
	}
	// if this file does not have a block on disk yet, give it one
	if (file->nStartBlock == 0) {
		int block = findNextAvailableBlock(disk);
		if (block == -1) {
			return -ENOSPC;
		} else {
			file->nStartBlock = block;
		}
	}

	// getting the disk block where the part of the file we care about lives
	int numBlocks = offset / MAX_DATA_IN_BLOCK;
	int localOffset = offset % MAX_DATA_IN_BLOCK;
	int currentBlock = file->nStartBlock;

	struct csc452_disk_block block;
	pread(disk, &block, sizeof(struct csc452_disk_block), file->nStartBlock);

	// getting to the current block
	for (int i = 0; i < numBlocks; i++) {
		currentBlock = block.nNextBlock;
		pread(disk, &block, sizeof(struct csc452_disk_block), block.nNextBlock);
	}

	printf("Current block: %d\n", currentBlock);

	// writing to the new block(s)
	for (int i = 0; i < size; i++) {
		block.data[localOffset] = buf[i];
		printf("Wrote %c to %d\n", buf[i], currentBlock + localOffset);

		localOffset++;

		if (localOffset == MAX_DATA_IN_BLOCK) { // we have filled up this block
			if (block.nNextBlock > 1) {
				pwrite(disk, &block, sizeof(struct csc452_disk_block), currentBlock);
				currentBlock = block.nNextBlock;
			} else {
				int nextBlock = findNextAvailableBlock(disk);
				block.nNextBlock = nextBlock;
				pwrite(disk, &block, sizeof(struct csc452_disk_block), currentBlock);
				currentBlock = nextBlock;
			}

			pread(disk, &block, sizeof(struct csc452_disk_block), currentBlock);
			localOffset = 0;
		}
	}

	// at this point, just the current block needs to be saved back to disk
	block.nNextBlock = 1;
	pwrite(disk, &block, sizeof(struct csc452_disk_block), currentBlock);

	// updating the file directory and writing the new directory entry
	file->fsize += size;
	pwrite(disk, &entry, sizeof(struct csc452_directory_entry), dir->nStartBlock);

	close(disk);
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *strPath)
{
	struct Path path;
	parse(strPath, &path);

	int res = ENOENT;

	// checking the path is actually a directory
	if (path.level != 1) {
		return -ENOTDIR;
	}

	// opening the disk
	int disk = open(".disk", O_RDWR);

	// getting the root
	struct csc452_root_directory root;
	pread(disk, &root, sizeof(struct csc452_root_directory), 0);

	// removing the specified directory from the disk
	for (int i = 0; i < root.nDirectories; i++) {
		struct csc452_directory * dir = &(root.directories[i]);
		if (strcmp(dir->dname, path.folder) == 0) {
			// getting the directory entry
			struct csc452_directory_entry entry;
			pread(disk, &entry, sizeof(struct csc452_directory_entry), dir->nStartBlock);

			if (entry.nFiles == 0) {
				strcpy(dir->dname, "");
				res = 0;
			} else {
				res = -ENOTEMPTY;
			}
		}
	}

	// writing the root to disk
	pwrite(disk, &root, sizeof(struct csc452_root_directory), 0);

	// closing the disk
	close(disk);

	return res;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *strPath)
{
        struct Path path;
	parse(strPath, &path);

	if (path.level == 1) {
		return -EISDIR;
	}

	// opening the disk
	int disk = open(".disk", O_RDWR);

	// getting the root
	struct csc452_root_directory root;
	pread(disk, &root, sizeof(struct csc452_root_directory), 0);

	// getting the directory
	struct csc452_directory * dir = findDirectory(&root, path.folder);
	if (dir == NULL) {
		fprintf(stderr, "\nCould not get directory: %s.\n", path.folder);
		exit(1);
	}

	// getting the directory entry
	struct csc452_directory_entry entry;
	pread(disk, &entry, sizeof(struct csc452_directory_entry), dir->nStartBlock);

	// getting the file directory
	struct csc452_file_directory * file = findFile(&entry, path.file, path.ext);
	if (file == NULL) {
		close(disk);
		return -ENOENT;
	}

	// removing the file information from the file directory
	strcpy(file->fname, "");
	strcpy(file->fext, "");
	file->fsize = 0;
	file->nStartBlock = 0;

	// decrementing the number of files in the directory
	entry.nFiles--;

	// writing the new file information to the disk
	pwrite(disk, &entry, sizeof(struct csc452_directory_entry), dir->nStartBlock);

	// close the disk
	close(disk);

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
