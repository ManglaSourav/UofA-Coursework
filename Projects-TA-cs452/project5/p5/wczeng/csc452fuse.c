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

#define MAX_BLOCKS 10240
#define BITMAP_SIZE 1280
#define BLOCK_BOUNDARY 10237
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
	struct csc452_root_directory *rootDir = {0};

	FILE *fd = fopen(".disk", "rb");
	if(fd != NULL) {
		fseek(fd, BLOCK_SIZE * 0, SEEK_SET);
		fread(rootDir, BLOCK_SIZE, 1, fd);
		fclose(fd);
	}

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		// parse the path for directory name, file name and file extension
		char dirName[20], filename[10], extension[4];
		sscanf(path, "/%[^/]/%[^.].%s", dirName, filename, extension);
		//If the path does exist and is a directory:
		int index = rootDir->nDirectories;
		int legalDir = 0;
		long dirStart = 0;
		for(int i=0; i<index; i++) {
			struct csc452_directory temp = rootDir->directories[i];
			if(strcmp(temp.dname, dirName) == 0) {
				legalDir = 1;
				dirStart = temp.nStartBlock;
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				break;
			}
		}
		struct csc452_directory_entry *direc = {0};
		if(legalDir) {
			fd = fopen(".disk", "rb");
			fseek(fd, BLOCK_SIZE*dirStart, SEEK_SET);
			fread(direc, BLOCK_SIZE, 1, fd);
			fclose(fd);
		
			if(strlen(filename) > 0 && strlen(filename) <= 8) {	
				//If the path does exist and is a file:
				int files = direc->nFiles;
				for(int j=0; j<files; j++) {
					struct csc452_file_directory temp = direc->files[j];
					if(strcmp(temp.fname, filename) == 0) {
						stbuf->st_mode = S_IFREG | 0666;
						stbuf->st_nlink = 2;
						stbuf->st_size = temp.fsize;
					}
				}
			}
		} else {
			//Else return that path doesn't exist
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
	
	struct csc452_root_directory *rootDir = {0};
	FILE *fd = fopen(".disk", "rb");
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	if(fd != NULL) {
		fseek(fd, 0, SEEK_SET);
		fread(rootDir, BLOCK_SIZE, 1, fd);
		// parse the path for directory name, file name and file extension
		char dirName[10], filename[10], extension[4];
		sscanf(path, "/%[^/]/%[^.].%s", dirName, filename, extension);
		//If the path does exist and is a directory:
		int index = rootDir->nDirectories;
		int legalDir = 0;
		long dirStart = 0;
		for(int i=0; i<index; i++) {
			struct csc452_directory temp = rootDir->directories[i];
			if(strcmp(temp.dname, dirName) == 0) {
				legalDir = 1;
				dirStart = temp.nStartBlock;
				break;
			}
		}
		struct csc452_directory_entry *temp = {0};
		if(legalDir) {
			fseek(fd, BLOCK_SIZE * dirStart, SEEK_SET);
			fread(temp, BLOCK_SIZE, 1, fd);
			int index2 = temp->nFiles;
			for(int i=0; i<index2; i++) {
				char fullname[13];
				strcpy(fullname, temp->files[i].fname);
				strcpy(fullname, ".");
				strcpy(fullname, temp->files[i].fext);
				filler(buf, fullname, NULL, 0);
			}
		} else {
			return -ENOENT;
		}
		fclose(fd);
	}

	return 0;
}

/*
 * Helper function to find the index of the next free block,
 * represented as bits in a bit array.
 *
 * @param *arr: char array, each char is 8 bits, each bit
 * represents a single block
 */
int nextFree(char *arr) {
	arr[0] = '1';
	int i, j = 0;
	while((i<512 && j<3) || (i<256 && j==2)) {
		if((j != 2) || (j==2 && i<255)) {
			char t = arr[512*j+i];
			if(t == 0xFF) {
				continue;
			} else {
				for(int k=0; k<8; k++) {
					if(t << k == 0) {
						return 512*8*j+8*i+k;
					}
				}
			}
		} else if(j == 2 && i == 255) {
			char t = arr[512*j+i];
			t = t >> 3;
			for(int k=0; k<5; k++) {
				if(t << k == 0) {
					return 512*8*j+8*i+k;
				}
			}
		}
		i++;
		if(i == 512) {
			i = 0;
			j++;
		}
	}
	return -1;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;
	struct csc452_root_directory *rootDir = {0};

	FILE *fd = fopen(".disk", "rb+");
	if(fd != NULL) {
		fseek(fd, BLOCK_SIZE * 0, SEEK_SET);
		fread(rootDir, BLOCK_SIZE, 1, fd);
	}
	// parse the path for directory name, file name and file extension
	char dirName[10], filename[10], extension[4];
	sscanf(path, "/%[^/]/%[^.].%s", dirName, filename, extension);
	if(strlen(dirName) > 8) {
		return -ENAMETOOLONG;
	}
	if(filename != NULL) {
		return -EPERM;
	} else {
		int index = rootDir->nDirectories;
		for(int i=0; i<index; i++) {
			if(strcmp(rootDir->directories[i].dname, dirName) == 0) {
				return -EEXIST;
			}
		}
	}
	
	if(rootDir->nDirectories != MAX_DIRS_IN_ROOT) {
		struct csc452_directory newDir = {0};
	
		// open the bitmap to update
		char *arr;
		fseek(fd, BLOCK_SIZE * BLOCK_BOUNDARY, SEEK_SET);
		fread(arr, BLOCK_SIZE * 3, 1, fd);
		newDir.nStartBlock = nextFree(arr);
		rootDir->directories[rootDir->nDirectories] = newDir;
		rootDir->nDirectories++;
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
	
	struct csc452_root_directory *rootDir = {0};
	FILE *fd = fopen(".disk", "rb");
	if(fd != NULL) {
		fseek(fd, 0, SEEK_SET);
		fread(rootDir, BLOCK_SIZE, 1, fd);
		// parse the path for directory name, file name and file extension
		char dirName[10], filename[10], extension[4];
		sscanf(path, "/%[^/]/%[^.].%s", dirName, filename, extension);
		if(dirName == NULL || filename == NULL || extension == NULL) {
			return -EPERM;
		} else if(strlen(filename) > 8 || strlen(extension) > 3) {
			return -ENAMETOOLONG;
		}
		int index = rootDir->nDirectories;
		int legalDir = 0;
		long dirStart = 0;
		for(int i=0; i<index; i++) {
			struct csc452_directory temp = rootDir->directories[i];
			if(strcmp(temp.dname, dirName) == 0) {
				legalDir = 1;
				dirStart = temp.nStartBlock;
				break;
			}
		}
		struct csc452_directory_entry *temp = {0};
		if(legalDir) {
			fseek(fd, BLOCK_SIZE * dirStart, SEEK_SET);
			fread(temp, BLOCK_SIZE, 1, fd);
			int index2 = temp->nFiles;
			for(int i=0; i<index2; i++) {
				if(strcmp(temp->files[i].fname, filename) == 0) {
					return -EEXIST;
				}
			}
			struct csc452_file_directory newFile = {0};
			char *arr = "";
			fseek(fd, BLOCK_SIZE * BLOCK_BOUNDARY, SEEK_SET);
			fread(arr, BLOCK_SIZE * 3, 1, fd);
			newFile.nStartBlock = nextFree(arr);
			temp->files[temp->nFiles] = newFile;
			temp->nFiles++;
		}
	}
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{	
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;
	struct csc452_root_directory *rootDir = {0};
	FILE *fd = fopen(".disk", "rb");
	if(fd != NULL) {
		fseek(fd, 0, SEEK_SET);
		fread(rootDir, BLOCK_SIZE, 1, fd);
		// parse the path for directory name, file name and file extension
		char dirName[10], filename[10], extension[4];
		sscanf(path, "/%[^/]/%[^.].%s", dirName, filename, extension);
		if(dirName == NULL || extension == NULL) {
			return -EPERM;
		} else if(strlen(filename) > 8 || strlen(extension) > 3) {
			return -ENAMETOOLONG;
		} else if(filename == NULL) {
			return -EISDIR;
		}
		int index = rootDir->nDirectories;
		int legalDir = 0;
		long dirStart = 0;
		for(int i=0; i<index; i++) {
			struct csc452_directory temp = rootDir->directories[i];
			if(strcmp(temp.dname, dirName) == 0) {
				legalDir = 1;
				dirStart = temp.nStartBlock;
				break;
			}
		}
		struct csc452_directory_entry *temp = {0};
		if(legalDir) {
			fseek(fd, BLOCK_SIZE * dirStart, SEEK_SET);
			fread(temp, BLOCK_SIZE, 1, fd);
			int index2 = temp->nFiles;
			for(int i=0; i<index2; i++) {
				if(strcmp(temp->files[i].fname, filename) == 0) {
					//check to make sure path exists
					index2 = i;
					break;
				}
			}
			if(offset > temp->files[index2].fsize) {
				return -EFBIG;
			} else if(temp->nFiles == MAX_FILES_IN_DIR) {
				return -ENOSPC;
			} else {
				struct csc452_file_directory *fil = {0};
				fseek(fd, BLOCK_SIZE * temp->files[index2].nStartBlock, SEEK_SET);
				fread(fil, BLOCK_SIZE, 1, fd);
				int index3 = fil->nStartBlock;
				fseek(fd, BLOCK_SIZE * index3, SEEK_SET);
				struct cs1550_disk_block *actualFile = {0};
				fread(actualFile, BLOCK_SIZE, 1, fd);
				//fwrite(actualFile->data, 1, size, buf);
			}
		}
	}
	fclose(fd);
	return size;
}

static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//return success, or error
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;
	struct csc452_root_directory *rootDir = {0};
	FILE *fd = fopen(".disk", "rb");
	if(fd != NULL) {
		fseek(fd, 0, SEEK_SET);
		fread(rootDir, BLOCK_SIZE, 1, fd);
		// parse the path for directory name, file name and file extension
		char dirName[10], filename[10], extension[4];
		sscanf(path, "/%[^/]/%[^.].%s", dirName, filename, extension);
		if(dirName == NULL || filename == NULL || extension == NULL) {
			return -EPERM;
		} else if(strlen(filename) > 8 || strlen(extension) > 3) {
			return -ENAMETOOLONG;
		}
		int index = rootDir->nDirectories;
		int legalDir = 0;
		long dirStart = 0;
		for(int i=0; i<index; i++) {
			struct csc452_directory temp = rootDir->directories[i];
			if(strcmp(temp.dname, dirName) == 0) {
				legalDir = 1;
				dirStart = temp.nStartBlock;
				break;
			}
		}
		struct csc452_directory_entry *temp = {0};
		if(legalDir) {
			fseek(fd, BLOCK_SIZE * dirStart, SEEK_SET);
			fread(temp, BLOCK_SIZE, 1, fd);
			int index2 = temp->nFiles;
			for(int i=0; i<index2; i++) {
				if(strcmp(temp->files[i].fname, filename) == 0) {
					//check to make sure path exists
					index2 = i;
					break;
				}
			}
			if(offset > temp->files[index2].fsize) {
				return -EFBIG;
			} else if(temp->nFiles == MAX_FILES_IN_DIR) {
				return -ENOSPC;
			} else {
				struct csc452_disk_block *fil = {0};
				fseek(fd, BLOCK_SIZE * temp->files[index2].nStartBlock, SEEK_SET);
				fread(fil, BLOCK_SIZE, 1, fd);
				//fwrite(buf, 1, offset, fil->data);
			}
		}
	}
	fclose(fd);
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;
	struct csc452_root_directory *rootDir = {0};
	FILE *fd = fopen(".disk", "rb+");
	if(fd != NULL) {
		fseek(fd, 0, SEEK_SET);
		fread(rootDir, BLOCK_SIZE, 1, fd);
		// parse the path for directory name, file name and file extension
		char dirName[10], filename[10], extension[4];
		sscanf(path, "/%[^/]/%[^.].%s", dirName, filename, extension);
		if(filename != NULL) {
			return -ENOTDIR;
		}
		int index = rootDir->nDirectories;
		int found = 0;
		long dirStart = 0;
		for(int i=0; i<index; i++) {
			struct csc452_directory temp = rootDir->directories[i];
			if(strcmp(temp.dname, dirName) == 0) {
				found = 1;
				dirStart = temp.nStartBlock;
			}
		}
		if(found == 0) {
			return -ENOENT;
		}
		struct csc452_directory_entry *direct;
		fseek(fd, BLOCK_SIZE * dirStart, SEEK_SET);
		fread(direct, BLOCK_SIZE, 1, fd);
		if(direct->nFiles != 0) {
			return -ENOTEMPTY;
		} else {
			char ZEROARRAY[BLOCK_SIZE] = {0};
			fwrite(ZEROARRAY, 1, BLOCK_SIZE, fd);
		}
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
	struct csc452_root_directory *rootDir = {0};
	FILE *fd = fopen(".disk", "rb");
	if(fd != NULL) {
		fseek(fd, 0, SEEK_SET);
		fread(rootDir, BLOCK_SIZE, 1, fd);
		// parse the path for directory name, file name and file extension
		char dirName[10], filename[10], extension[4];
		sscanf(path, "/%[^/]/%[^.].%s", dirName, filename, extension);
		if(dirName == NULL || filename == NULL || extension == NULL) {
			return -EPERM;
		} else if(strlen(filename) > 8 || strlen(extension) > 3) {
			return -ENAMETOOLONG;
		}
		int index = rootDir->nDirectories;
		int legalDir = 0;
		long dirStart = 0;
		for(int i=0; i<index; i++) {
			struct csc452_directory temp = rootDir->directories[i];
			if(strcmp(temp.dname, dirName) == 0) {
				legalDir = 1;
				dirStart = temp.nStartBlock;
				break;
			}
		}
		struct csc452_directory_entry *temp = {0};
		if(legalDir) {
			fseek(fd, BLOCK_SIZE * dirStart, SEEK_SET);
			fread(temp, BLOCK_SIZE, 1, fd);
			int index2 = temp->nFiles;
			for(int i=0; i<index2; i++) {
				if(strcmp(temp->files[i].fname, filename) == 0) {
					//check to make sure path exists
					index2 = i;
					break;
				}
			}
			fseek(fd, BLOCK_SIZE * temp->files[index2].nStartBlock, SEEK_SET);
			char ZEROARRAY[BLOCK_SIZE] = {0};
			fwrite(ZEROARRAY, 1, BLOCK_SIZE, fd);
		}
	}
	fclose(fd);
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
/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
