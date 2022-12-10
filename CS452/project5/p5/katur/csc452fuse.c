/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452

  @author Carter Boyd
  CS452 spring 22
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

typedef struct storage {
	int nextBlock;
	char padding[BLOCK_SIZE - sizeof(int)];
} storagePad;

/*
 * reads the file to the specific offset with the seek set to the start
 * root will then take the information that is stored there grabbing the size
 * that it needs to get. after that the seek is reset back to 0 incase there
 * are bugs in the code that will mess up the seeking
 */
void readStart(void *root, FILE *file, long offset, size_t sizeOf) {
	fseek(file, offset, SEEK_SET);
	fread(root, sizeOf, 1, file);
	fseek(file, 0, SEEK_SET);
}

/*
 * reads from the end of the file with the negated offset so counteract the
 * seek-end, the root will then grab the information there by the specific size
 * then the seek will be set to 0 incase of any errors
 */
void readEnd(void *root, FILE *file, long offset, size_t sizeOf) {
	fseek(file, -offset, SEEK_END);
	fread(root, sizeOf, 1, file);
	fseek(file, 0, SEEK_SET);
}

/*
 * writes from the beginning of the file using the offset, after writing to
 * the specific location it will then set the seek back to 0 and flush
 */
void writeStart(void *root, FILE *file, long offset, size_t sizeOf) {
	fseek(file, offset, SEEK_SET);
	fwrite(root, sizeOf, 1, file);
	fseek(file, 0, SEEK_SET);
	fflush(file);
}

/*
 * writes to the end of the file, using the negated offset to counteract the
 * seek_end, after seeking to the desired location it will write into that
 * location then seek back to 0, afterwhich it will flush
 */
void writeEnd(void *root, FILE *file, long offset, size_t sizeOf) {
	fseek(file, -offset, SEEK_END);
	fwrite(root, sizeOf, 1, file);
	fseek(file, 0, SEEK_SET);
	fflush(file);
}

/*
 * loops thorugh the root until it finds the specified directory, if
 * the directory is inside the disk then it will return 1, 0 elsewise
 */
int getDirectoryBlock(csc452_root_directory root, char *directory) {
	int i;
	for (i = 0; i < root.nDirectories; ++i) {
		if (strcmp(directory, root.directories[i].dname) == 0) {
			return root.directories[i].nStartBlock;
		}
	}
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
	int res = 0, size = strlen(path) + 1, i, found, directoryBlock;
	FILE *disk = fopen(".disk", "rb+");
	csc452_root_directory root;
	csc452_directory_entry entry;
	char directory[size], filename[size], extension[size];
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	readStart(&root, disk, 0, sizeof(csc452_root_directory));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (*filename == '\0') {
		
		//If the path does exist and is a directory:
		//stbuf->st_mode = S_IFDIR | 0755;
		//stbuf->st_nlink = 2;

		//If the path does exist and is a file:
		//stbuf->st_mode = S_IFREG | 0666;
		//stbuf->st_nlink = 2;
		//stbuf->st_size = file size
		
		//Else return that path doesn't exist
		found = 0;
		for (i = 0; i < root.nDirectories; ++i) {
			if (strcmp(directory, root.directories[i].dname) == 0) {
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				found = 1;
			}
		}
		if (!found) {
			fclose(disk);
			fprintf(stderr, "path does not exist\n");
			res = -ENOENT;
		}

	} else {
		directoryBlock = getDirectoryBlock(root, directory);
		if (directoryBlock == -1) {
			fclose(disk);
			fprintf(stderr, "could not find directory block\n");
			return -ENOENT;
		}

		readStart(&entry, disk, directoryBlock * sizeof(csc452_directory_entry), sizeof(csc452_directory_entry));
		found = 0;
		for (i = 0; i < entry.nFiles; ++i) {
			if (strcmp(entry.files[i].fname, filename) == 0) {
				if (strcmp(entry.files[i].fext, extension) == 0) {
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 2;
					stbuf->st_size = entry.files[i].fsize;
					found = 1;
					break;
				}
			}
		}
		if (!found) {
			fclose(disk);
			fprintf(stderr, "file not found\n");
			res = -ENOENT;
		}
	}
	fclose(disk);
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

	int size = strlen(path + 1), i, directoryBlock;
	csc452_root_directory root;
	csc452_directory_entry entry;
	FILE *disk = fopen(".disk", "rb+");
	size_t entrySize = sizeof(csc452_directory_entry);
	char directory[size], filename[size], extension[size];
	readStart(&root, disk, 0, sizeof(csc452_root_directory));
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		for (i = 0; i < root.nDirectories; ++i) {
			filler(buf, root.directories[i].dname, NULL, 0);
		}
	} else if (*filename == '\0') {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		directoryBlock = getDirectoryBlock(root, directory);
		if (directoryBlock == -1) {
			fclose(disk);
			fprintf(stderr, "could not find directory block\n");
			return -ENOENT;
		}
		readStart(&entry, disk, directoryBlock * entrySize, entrySize);
		for (i = 0; i < entry.nFiles; ++i) {
			char copy[strlen(entry.files[i].fname) + 1 + strlen(entry.files[i].fext) + 1];
			strcpy(copy, entry.files[i].fname);
			strcat(copy, ".");
			strcat(copy, entry.files[i].fext);
			filler(buf, copy, NULL, 0);
		}
	} else {
		// All we have _right now_ is root (/), so any other path must
		// not exist. 
		fclose(disk);
		return -ENOENT;
	}
	fclose(disk);
	return 0;
}

/*
 * counts how many slashes there are in the path
 */
int getSlashCount(const char *path) {
	int count = 0;
	char *ptr;
	for (ptr = (char *) path; *ptr; ++ptr)
		if (*ptr == '/')
			++count;
	return count;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;
	FILE *disk;
	csc452_root_directory root;
	storagePad storage;
	char *name;
	int i, nextBlock;
	disk = fopen(".disk", "rb+");
	readStart(&root, disk, 0, sizeof(csc452_root_directory));
	readEnd(&storage, disk, sizeof(storagePad), sizeof(storagePad));

	if (getSlashCount(path) != 1) {
		fclose(disk);
		fprintf(stderr, "The directory is not under the root directory\n");
		return -EPERM;
	}

	name = ((char *) path) + 1;
	if (strlen(name) > 8) {
		fclose(disk);
		fprintf(stderr, "%s is too long\n", name);
		return -ENAMETOOLONG;
	}

	for (i = 0; i < root.nDirectories; ++i) {
		if (strcmp(root.directories[i].dname, name) == 0) {
			fclose(disk);
			fprintf(stderr, "the director already exists\n");
			return -EEXIST;
		}
	}
	
	if (storage.nextBlock == 0)
		++storage.nextBlock;
	nextBlock = storage.nextBlock++;
	strcpy(root.directories[root.nDirectories].dname, name);
	root.directories[root.nDirectories++].nStartBlock = nextBlock;
	if (root.nDirectories >= MAX_DIRS_IN_ROOT) {
		fclose(disk);
		fprintf(stderr, "nDirectories has reached the max directories in root\n");
		return -ENOSPC;
	}
	writeStart(&root, disk, 0, sizeof(csc452_root_directory));
	writeEnd(&storage, disk, sizeof(storagePad), sizeof(storagePad));
	fclose(disk);
	return 0;
}

/*
 * basic boolean operation that will loop through the files of an entry and see
 * if the file matches teh extension
 */
int doesFileExist(csc452_directory_entry entry, char *filename, char *extension) {
	int i;
	for (i = 0; i < entry.nFiles; ++i) {
		if (strcmp(entry.files[i].fname, filename) == 0)
			if (strcmp(entry.files[i].fext, extension) == 0)
				return 1;
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
	FILE *disk = fopen(".disk", "rb+");
	csc452_root_directory root;
	csc452_directory_entry entry;
	storagePad storage;
	int size = strlen(path) + 1, directoryBlock;
	char directory[size], filename[size], extension[size];
	size_t directEntrySize = sizeof(csc452_directory_entry);
	readStart(&root, disk, 0, sizeof(csc452_root_directory));
	readStart(&storage, disk, sizeof(storagePad), sizeof(storagePad));
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (getSlashCount(path) == 1) {
		fclose(disk);
		fprintf(stderr, "cannot write into the root\n");
		return -EPERM;
	}
	if (strlen(filename) > MAX_FILENAME) {
		fclose(disk);
		fprintf(stderr, "file name is too long\n");
		return -ENAMETOOLONG;
	}

	directoryBlock = getDirectoryBlock(root, directory);

	if (directoryBlock == -1) {
		fclose(disk);
		fprintf(stderr, "could not find the directory name\n");
		return -ENOENT;
	}

	readStart(&entry, disk, directoryBlock * directEntrySize, directEntrySize);
	if (doesFileExist(entry, filename, extension)) {
		fclose(disk);
		fprintf(stderr, "file already exists\n");
		return -EEXIST;
	}
	
	strcpy(entry.files[entry.nFiles].fname, filename);
	strcpy(entry.files[entry.nFiles].fext, extension);
	entry.files[entry.nFiles++].nStartBlock = storage.nextBlock;
	writeStart(&entry, disk, directoryBlock * directEntrySize, directEntrySize);
	writeEnd(&storage, disk, sizeof(storagePad), sizeof(storagePad));
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
	FILE *disk = fopen(".disk", "rb+");
	csc452_root_directory root;
	int pathSize = strlen(path) + 1, directoryBlock;
	char directory[pathSize], filename[pathSize], extension[pathSize];
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	if (*filename == '\0') {
		fclose(disk);
		fprintf(stderr, "path does not exist\n");
		return -EISDIR;
	}
	if (size <= 0) {
		fclose(disk);
		fprintf(stderr, "size not greater than 0\n");
		return -EINVAL;
	}
	
	directoryBlock = getDirectoryBlock(root, directory);
	if (directoryBlock == -1) {
		fclose(disk);
		fprintf(stderr, "could not find directoryBlock\n");
		return -ENOENT;
	}
	fclose(disk);
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
	FILE *disk = fopen(".disk", "rb+");
	csc452_root_directory root;
	csc452_directory_entry directoryEntry;
	storagePad storage;
	int directoryBlock;
	readStart(&root, disk, 0, sizeof(csc452_root_directory));
	readStart(&storage, disk, sizeof(storagePad), sizeof(storagePad));
	if (getSlashCount(path) != 1) {
		fclose(disk);
		fprintf(stderr, "directory is not in the root\n");
		return -EPERM;
	}

	directoryBlock = getDirectoryBlock(root, (char *) (path + 1));
	if (directoryBlock == -1) {
		fclose(disk);
		fprintf(stderr, "the directory was not found\n");
		return -ENOENT;
	}
	readStart(&directoryEntry, disk, directoryBlock * sizeof(csc452_directory_entry), sizeof(csc452_directory_entry));
	if (directoryEntry.nFiles > 0) {
		fclose(disk);
		fprintf(stderr, "directory is not empty\n");
		return -ENOTEMPTY;
	}
	// with the verification that the directory is empty then the directory can be removed
	int i;
	for (i = 0; i < root.nDirectories; ++i) {
		if (strcmp(root.directories[i].dname, (char *) (path + 1)) == 0) {
			free(root.directories[i].dname);
		}

		}

	fclose(disk);
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
