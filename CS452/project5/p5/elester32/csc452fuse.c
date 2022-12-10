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
};

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

int blocks[512];

int next_block() {
	int i = 0;

	while (blocks[i] != 0) {
		if (i > 511) {
			return -1;
		}
		i++;
	}
	return i;
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
	char dir[11] = "";
	char file[11] = "";
	char ext[5] = "";

	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		FILE *disk;
		disk = fopen(".disk", "r");
		struct csc452_root_directory root;
		fseek(disk, 0, SEEK_SET);
		memset(&root, 0, sizeof(root));
		fread(&root, 1, sizeof(root), disk);

		int size = root.nDirectories;
		for (int i = 0; i < size; i++) {
			struct csc452_directory temp = root.directories[i];
			blocks[temp.nStartBlock] = 1;
			if (strcmp(dir, temp.dname) == 0 && strcmp(file,"") == 0) {
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				fclose(disk);
				return res;
			} else {
				if (strcmp(dir, temp.dname) == 0 && strcmp(file,"") != 0) {
					int blockNum = temp.nStartBlock;
					int blockOffset = blockNum * 512;
					fseek(disk, blockOffset, SEEK_SET);
					struct csc452_directory_entry entry;
					memset(&entry, 0, sizeof(entry));
					fread(&entry, 1, sizeof(entry), disk);
					
					int dirSize = entry.nFiles;
					if (dirSize == 0) {
						fclose(disk);
						return -ENOENT;
					}
					printf("%d\n", dirSize);
					for (int j = 0; j < dirSize; j++) {	
						struct csc452_file_directory dirFile = entry.files[j];	
						if (strcmp(dirFile.fname, file) == 0 && strcmp(dirFile.fext, ext)) {
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 2;
							stbuf->st_size = dirFile.fsize;
							fclose(disk);
							return res;
						}
					}
				}
			}
		}
		fclose(disk);
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

	char dir[11] = "";
	char file[11] = "";
	char ext[5] = "";
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	
	FILE *disk;
	disk = fopen(".disk", "r");
	struct csc452_root_directory root;
	fseek(disk, 0, SEEK_SET);
	memset(&root, 0, sizeof(root));
	fread(&root, 1, sizeof(root), disk);
	
	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		int size = root.nDirectories;
		for (int i = 0; i < size; i++) {
			filler(buf, root.directories[i].dname, NULL, 0);
		}	
	} else {
		int size = root.nDirectories;
		for (int i = 0; i < size; i++) { 
			struct csc452_directory temp = root.directories[i];
			if (strcmp(temp.dname, dir) == 0 && strcmp(file, "") == 0) {
				filler(buf, ".", NULL, 0);
				filler(buf, "..", NULL, 0);
				int blockNum = temp.nStartBlock;
				int blockOffset = blockNum * 512;
				fseek(disk, blockOffset, SEEK_SET);
				struct csc452_directory_entry entry;
				memset(&entry, 0, sizeof(entry));
				fseek(disk, blockOffset, SEEK_SET);
				fread(&entry, 1, sizeof(entry), disk);
				int dirSize = entry.nFiles;
				for (int j = 0; j < dirSize; j++) {
					struct csc452_file_directory files = entry.files[j];
					char fFile[11];
					strncat(fFile, files.fname, 8);
					strncat(fFile, ".", 2);
					strncat(fFile, files.fext, 3);
					filler(buf, fFile, NULL, 0);
				}
				fclose(disk);
				return 0;
			}
		}
		fclose(disk);
		return -ENOENT;
	}
	
	fclose(disk);
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
	
	blocks[0] = 1;
	
	char dir[11] = "";
	char file[11] = "";
	char ext[5] = "";
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
		
	if (strlen(dir) > 8) {
		return -ENAMETOOLONG;
	}

	if (strcmp(file, "") != 0) {
		return -EPERM;
	}

	int block = next_block();
	int block_offset = block * 512;
	blocks[block] = 1;
	
	FILE *disk;
	disk = fopen(".disk", "r+");
	struct csc452_root_directory root;
	memset(&root, 0, sizeof(root));
	fseek(disk, 0, SEEK_SET);
	fread(&root, 1, sizeof(root), disk);
	
	int size = root.nDirectories;
	for (int i = 0; i < size; i++) {
		struct csc452_directory temp = root.directories[i];
		if (strcmp(temp.dname, dir) == 0) {
			return -EEXIST;
		}
	}

	struct csc452_directory dirir;
	strcpy(dirir.dname, dir);
	dirir.nStartBlock = block;
	root.directories[root.nDirectories] = dirir;	
	root.nDirectories++;

	fseek(disk, 0, SEEK_SET);
	fwrite(&root, 1, sizeof(struct csc452_root_directory), disk);

	struct csc452_directory_entry newDir;
	memset(&newDir, 0, sizeof(newDir));
	fseek(disk, block_offset, SEEK_SET);
	fwrite(&newDir, 1, sizeof(csc452_directory_entry), disk); 		
	
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
 */
static int csc452_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) path;
	(void) mode;
   	(void) dev;
	
	char dir[11] = "";
	char file[11] = "";
	char ext[5] = "";
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	
       	if (strcmp(file, "") == 0) {
		return -EPERM;
	}
	
	if (strlen(file) > 8) {
		return -ENAMETOOLONG;
	}

	if (strlen(ext) > 3) {
		return -ENAMETOOLONG;
	}

	int block = next_block();
       	int block_offset = block * 512;
	blocks[block] = 1;

	FILE *disk;
	disk = fopen(".disk", "r+");
	struct csc452_root_directory root;
	memset(&root, 0, sizeof(root));
	fseek(disk, 0, SEEK_SET);
	fread(&root, 1, sizeof(root), disk);

	int size = root.nDirectories;
	for (int i = 0; i < size; i++) {
		struct csc452_directory temp = root.directories[i];
		if (strcmp(temp.dname, dir) == 0) {
			int blockNum = temp.nStartBlock;
			int blockOffset = blockNum * 512;
			struct csc452_directory_entry entry;
			memset(&entry, 0, sizeof(entry));
			fseek(disk, blockOffset, SEEK_SET);
			fread(&entry, 1, sizeof(entry), disk);
			int dirSize = entry.nFiles;
			for (int j = 0; j < dirSize; j++) {
				struct csc452_file_directory dirFile = entry.files[j];
				if (strcmp(dirFile.fname, file) == 0 && strcmp(dirFile.fext, ext)) {
					fclose(disk);
					return -EEXIST;
				}
			}
			struct csc452_file_directory fi;
			strcpy(fi.fname, file);
			strcpy(fi.fext, ext);
			fi.nStartBlock = block;
			entry.files[entry.nFiles] = fi;
			entry.nFiles++;

			fseek(disk, blockOffset, SEEK_SET);
			fwrite(&entry, 1, sizeof(entry), disk);

			struct csc452_disk_block newBlock;
			memset(&newBlock, 0, sizeof(newBlock));
			fseek(disk, block_offset, SEEK_SET);
			fwrite(&newBlock, 1, sizeof(newBlock), disk);
			fclose(disk);
			return 0;
		}
	}
	fclose(disk);
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
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;

	char dir[11] = "";
	
	char file[11] = "";
	char ext[5] = "";
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	FILE *disk;
	disk = fopen(".disk", "r+");
	struct csc452_root_directory root;
	memset(&root, 0, sizeof(root));
	fread(&root, 1, sizeof(root), disk);
	fseek(disk, 0, SEEK_SET);
	
	if (strcmp(file, "") != 0) {
		return -ENOTDIR;
	}

	int size = root.nDirectories;
	for (int i = 0; i < size; i++) {
		struct csc452_directory temp = root.directories[i];
		if (strcmp(temp.dname, dir) == 0) {
			int blockNum = temp.nStartBlock;
			int blockOffset = blockNum * 512;
			fseek(disk, blockOffset, SEEK_SET);
			struct csc452_directory_entry entry;
			memset(&entry, 0, sizeof(entry));
			fread(&entry, 1, sizeof(entry), disk);
			
			if (entry.nFiles == 0) {
				blocks[blockNum] = 0;
				memset(&entry, 0, sizeof(entry));
				fseek(disk, blockOffset, SEEK_SET);
				fwrite(&entry, 1, sizeof(entry), disk);
				for (int j = 0; j < size; j++) {
					if (j >= i) {
						root.directories[j] = root.directories[j+1];
					}
				}
				root.nDirectories--;
				fseek(disk, 0, SEEK_SET);
				fwrite(&root, 1, sizeof(root), disk);
				fclose(disk);
				return 0;
			}
			return -ENOTEMPTY;
		}		
	}
	return -ENOENT;
	
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

