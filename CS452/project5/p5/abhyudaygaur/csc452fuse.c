/*
	FUSE: Filesystem in Userspace
	OG Author: Misurda
	Edited by Flynn
	Project 5: File System, Spring 2022 
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
#define NODE_POINTERS (BLOCK_SIZE - sizeof(int) - sizeof(long)) / sizeof(long)
#define END_OF_BITMAP (5 * BLOCK_SIZE - 1)


struct csc452_bitmap {
	unsigned char bitmap[5 * BLOCK_SIZE];
}

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

struct csc452_node {
	int nextNode;
	long value;
	long nodePointers[NODE_POINTERS];
}

static long adjust_dirOffset(FILE *file, char *dir) {
	csc452_root_directory root;
	if (fseek(file, 0, SEEK_SET) == 0) {
		if (fread(&root,1, BLOCK_SIZE, file) == BLOCK_SIZE) {
			int i;
			int directories = root.nDirectories;
			for (i = 0; i<directories; i++) {
				if(strcmp(dir, root.directories[i].dname) == 0 ) {
					return root.directories[i].nStartBlock;
				}
			}
		}
	}
	return 0;
}

static int getBlock(FILE *file) {
	struct csc452_bitmap bitMap;
	if (fread(&bitMap, 1, BLOCK_SIZE * 5, file) == (END_OF_BITMAP + 1)) {
		int i;
		int x;
		for (i = 1; i < END_OF_BITMAP; i++) {
			if (bitMap.bitmap[i] < 255) {
				unsigned char bits = 128;
				unsigned char mark = bitMap.bitmap[i] + 1;
				for (x = 7; (bits ^ mark) != 0; bits >>= 1, x--);
				x += (i * 8);
				bitMap.bitmap[i] = bitMap.bitmap[i] | mark;
				break;
			}
		}
		if (fwrite(bitMap.bitmap, 1, BLOCK_SIZE * 5, file) == END_OF_BITMAP + 1) {
			return x;
		}
	}
	return -1;
}

int write(FILE *file, csc452_directory_entry entry, long location, char filename[MAX_FILENAME + 1], char extension[MAX_EXTENSION + 1], size_t size, long nStartBlock) {
	int curr;
	if (entry.nFiles == MAX_FILES_IN_DIR) {
		return -1;
	}
	for (curr = 0; curr < MAX_FILES_IN_DIR; curr++) {
		if (entry.files[curr].fsize == -1) {
			strcpy(entry.files[curr].fname, filename);
			strcpy(entry.files[curr].fext, extension);
			entry.files[curr].fsize = size;
			entry.files[curr].nStartBlock = nStartBlock;
			entry.nFiles++;
			fseek(file, BLOCK_SIZE * location, SEEK_SET);
			fwrite(&entry, 1, BLOCK_SIZE, file);
			return 1;
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
	int res = 0;
	int i=0;
	int directoryFiles;
	int fileLocation; 
	memset(stbuf, 0, sizeof(struct stat));
	int directoryLoc = -1;
	int fileStart = -1;
	FILE *file;

	csc452_root_directory root;
	csc452_directory_entry entry;

	char extension[MAX_EXTENSION + 1];
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	memset(extension, 0, MAX_EXTENSION + 1);
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// check if path root
	if (strcmp(path, "/") == 0) {
		//If the path does exist and is a directory:
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
	else  {
		if (directory[MAX_FILENAME] && directory[0] == '\0' && filename[MAX_FILENAME] == '\0' && extension[MAX_EXTENSION] == '\0') {
			file = fopen(".disk", "rb");
			if (file) {
				directoryLoc = adjust_dirOffset(file, directory);
				if (directoryLoc) {
					if (filename[0] == '\0') {
						stbuf -> st_mode = S_IFDIR | 0755;
						stbuf -> st_nlink = 2;
						res = 0; 
					}
					else {
						if (fseek(file, BLOCK_SIZE, *directoryLoc; SEEK_SET) == 0) {
							if (fread(&entry, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
								directoryFiles = entry.nFiles;
								// iterate through directory files
								for (fileLocation = 0; fileLocation<directoryFiles; fileLocation++) {
									if ((strcmp(extension, entry.files[fileLoc].fext) = 0 ) && (strcmp(filename, entry.files[fileLocation].fname) == 0)) {
										fileStart = root.directories[i].nStartBlock;
									}
								}
								if(fileStart == -1) {
									fclose(file);
									return -ENOENT;
								}
								//If the path does exist and is a file:
								else {
									stbuf->st_mode = S_IFREG | 0666;
									stbuf->st_nlink = 2; 
									//update size
									stbuf->st_size = entry.files[file_start].fsize; 
									res = 0; 
								} 
							}
						}
					}
				}
			}
		else {
			res = -1;
		}
		fclose(file)
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

	int directoryFiles, i;
	long directoryLoc = -1
	int res = -ENOENT;

	csc452_root_directory root;
	csc452_directory_entry entry;
	FILE *file;

	char extension[MAX_EXTENSION + 1];
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];

	memset(extension, 0, MAX_EXTENSION + 1);
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	char temporary[MAX_EXTENSION + MAX_FILENAME + 2];


	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
	}
	if (strcmp(path, "/") != 0) {
		if (directory[0] && directory[MAX_FILENAME] == '\0' && extension[MAX_EXTENSION] == '\0' && filename[MAX_FILENAME] == '\0') {
			file = fopen(".disk", "rb");
			directoryLoc = adjust_dirOffset(file, directory);
			if (directoryLoc) {
				if (fseek(file, directoryLoc * BLOCK_SIZE, SEEK_SET) == 0) {
					if (fread(&file, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
						directoryFiles = entry.nFiles;
						for (i = 0; i < directoryFiles; i++) {
							strcpy(temporary, entry.files[i].fname);
							if (entry.files[i].fext[0]) {
								strcat(temporary, ".");
								strcat(temporary, entry.files[i].fext);
							}
							filler(buf, temporary, NULL, 0);
						}
						res = 0;
					}
				}
			}
			fclose(file);
		else {
			// All we have _right now_ is root (/), so any other path must
			// not exist. 
			return -ENOENT;
		}
	}
	else {
		file = fopen(".disk", "rb");
		if (file && (fread(&root, 1, BLOCK_SIZE, file) == BLOCK_SIZE)) {
			int directories = root.nDirectories;
			for (i = 0; i < directories; i++) {
				filler(buf, root.directories[i].dname, NULL, 0);
			}
			res = 0;
		}
		fclose(file);
	}

	return res;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;
	int i;
	int directories;
	long start;
	struct csc452_bitmap bitMap;
	csc452_directory_entry entry;
	csc452_directory root;
	char extension[MAX_EXTENSION + 1];
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	unsigned char x = 128;
	unsigned char map; 

	memset(extension, 0, MAX_EXTENSION + 1);
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	FILE *file = fopen(".disk", "r+b");
	size_t bytes = 0;
	bytes = fread(&root, sizeof(csc452_root_directory), 1, file);
	if (filename[0] == '/') {
		return -EPERM;
	}
	//exceeeds filename limit
	if (directory[MAX_FILENAME]) {
		return -ENAMETOOLONG;
	}

	if (directory[0] && filename[0] == '\0' && directory[MAX_FILENAME] == '\0') {
		directories = root.nDirectories;
		for (i = 0; i < directories; i++) {
			if (strcmp(directory, root.directories[i].dname) == 0) {
				return -EEXIST;
			}
		}
		if (!fseek(file, -BLOCK_SIZE*5; SEEK_END)) {
			fread(&bitMap, 1, BLOCK_SIZE*5, file);
			start = 0;
			for (i = 1; i < END_OF_BITMAP; i++ ) {
				if (bitMap.bitmap[i] < 255) {
					map = bitMap[i].bitmap[i] + 1;
					for (start = 7; (map^x)!=0; x>>=1, start--);
					start += (i*8);
					bitMap.bitmap[index] = bit_map.bitmap[i] | map;
					fwrite(&bit_map, 1, BLOCK_SIZE*5; file);
					break;
				}
			}
			if(start!=0) {
				fseek(file, 0, SEEK_SET);
				fread(&root, 1, BLOCK_SIZE, )
				root.directories[root.nDirectories].mStartBlock = start;
				strcpy(root.directories[root.nDirectories].dname, directory)
				root.nDirectories++;
				fseek(file, 0, SEEK_SET);
				fwrite(file,start *, BLOCK_SIZE, SEEK_SET);
				fwrite(&entry, 1, BLOCK_SIZE, file);
			}
		}
	}
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
static int csc452_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) path;
	(void) mode;
    (void) dev;
	
	int count, flag, nStartBlock;
	int res = -1;
	long loc;
	csc452_directory_entry entry;
	char extension[MAX_EXTENSION + 1];
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	memset(extension, 0, MAX_EXTENSION + 1);
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	FILE *file = fopen(".disk", "rb+");
	if (strcmp(path, "/") != 0) {
		if (directory[MAX_FILENAME]) {
			res = -ENAMETOOLONG;
		}
		else if (directory[0] && (directory[MAX_FILENAME] == '\0') && (filename[MAX_FILENAME] == '\0')) {
			if (file) {
				if (fseek(file, BLOCK_SIZE*-5, SEEK_END) == 0) {
					nStartBlock = getBlock(file);
					loc = adjust_dirOffset(file, directory);
					printf("Directory: %s, Directory Location: %d\n", directory, loc);
					if (loc) {
						if (fseek(file, BLOCK_SIZE*loc, SEEK_SET) == 0) {
							if (fread(&entry, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
								int i;
								printf("Directory Located");
								count = entry.nFiles;
								for (i = 0; i < count; i++) {
									if (strcmp(entry.files[i].fext, extension) == 0 && strcmp(entry.files[i].fname, filename) == 0) {
										fclose(file);
										return -EEXIST;
									}
								}
								flag = write(file, entry, location, filename, extension, 0, nStartBlock);
								if (flag < 0) {
									return -1;
								}
								csc452_node newNode;
								newNode.next_node = 0;
								newNode.value = 0;

								if (fseek(file, nStartBlock*BLOCK_SIZE, SEEK_SET) == 0) {
									if (fwrite(&newNode, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
										res = 0;
									}
								}
							}
						}
					}
				}
			}

			fclose(file);
		}
	}
	else {
		res = -EPERM;
	}
	return res;
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

	int directoryLoc, fileLoc, i, tempSize;
	int res = 0;
	long fileOffset;
	csc452_directory_entry entry;
	csc452_node node;
	char extension[MAX_EXTENSION + 1];
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	memset(extension, 0, MAX_EXTENSION + 1);
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	FILE *file;

	//check to make sure path exists
	if (strcmp(path, "/")!=0) {
		memset(&entry, 0, sizeof(csc452_directory_entry));
		file = fopen(".disk", "rb+");
		if (file) {
			directoryLoc = adjust_dirOffset(file, directory);
			if (directoryLoc) {
				if (fseek(file, BLOCK_SIZE * directoryLoc, SEEK_SET) == 0) {
					if (fread(&entry, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
						for (fileLoc = 0; fileLoc < entry.nFiles; fileLoc++) {
							if (strcmp(entry.files[fileLoc].fname, filename) == 0 && strcmp(entry.files[fileLoc].fext, extension) == 0) {
								fileOffset = entry.files[fileLoc].nStartBlock;
								break;
							}
						}
						if (fileLoc == entry.nFiles) 
							return 0;
						//check that offset is <= to the file size
						if (offset > size)
							return -EISDIR;
					
						if (fseek(file, fileOffset * BLOCK_SIZE, SEEK_SET) == 0) {
								//read in data
							if (fread(&node, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
								tempSize = size;
								while(tempSize > 0) {
									csc452_disk_block block;
									int index;
									int v;
									fseek(file, node.node_pointers[i] * BLOCK_SIZE, SEEK_SET);
									fread(&block, 1, BLOCK_SIZE, file);
									if (tempSize <= BLOCK_SIZE) 
										v = size;
									else 
										v = BLOCK_SIZE;
									for (index = 0; index < v; index++) {
										*buf = block.data[index];
										buf++;
									}
									i++;
									res += strlen(block.data);
									tempSize -= BLOCK_SIZE;
								}
							}
						}
					}
				}
			}
			fclose(file);
		}
		else {
			printf("Error Reading File");
		}
	}
	//check that size is > 0
	if (size <= 0) 
		return -1;

	//return success, or error
	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi) {
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	int fileLoc, dataLoc, directoryLoc;
	int res = 0;
	long fileOffset, tempSize;
	csc452_directory_entry entry;
	csc452_node node; 
	char extension[MAX_EXTENSION + 1];
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	memset(extension, 0, MAX_EXTENSION + 1);
	memset(directory, 0, MAX_FILENAME + 1);
	memset(filename, 0, MAX_FILENAME + 1);
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	FILE *file;

	//check to make sure path exists
	if (strcmp(path, "/") != 0) {
		memset(&entry, 0, sizeof(csc452_directory_entry));
		file = fopen(".disk", "rb+");
		if (file) {
			directoryLoc = adjust_dirOffset(file, directory);
			if (directoryLoc) {
				if (fseek(file, BLOCK_SIZE * directoryLoc, SEEK_SET) == 0) {
					if (fread(&entry, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
						for (fileLoc = 0; fileLoc < entry.nFiles; fileLoc++) {
							if (strcmp(entry.files[fileLoc].fname, filename) == 0 && strcmp(entry.files[fileLoc].fext, extension) == 0) {
								fileOffset = entry.files[fileLoc].nStartBlock;
								break;
							}
						}
						if (fileLoc == entry.nFiles) {
							return 0;
						}
						//check that offset is <= to the file size
						if (offset > entry.files[fileLoc].fsize) {
							fclose(file);
							return -EFBIG;
						}
						if (fseek(file, fileOffset * BLOCK_SIZE, SEEK_SET) == 0) {
							if (fread(&node, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
								dataLoc = offset;
								tempSize = size;
								while (tempSize > 0) {
									int v, location = getBlock(file);
									csc452_disk_block block;
									block.nNextBlock = 0;
									if (tempSize <= BLOCK_SIZE) 
										v = tempSize;
									else 
										v = BLOCK_SIZE;

									for (dataLoc = 0; dataLoc < v; dataLoc++) {
										block.data[dataLoc] = buf[0];
										buf++;
									}
									if (node.value == 0) {
										node.node_pointers[node.next_node] = next_node_location;
										node.next_node++;

										if (fseek(file, this_node_location * BLOCK_SIZE, SEEK_SET) == 0) {
											if (fwrite(&node, 1, BLOCK_SIZE, file) == BLOCK_SIZE) {
												
											}
										}
									}
							
									//write data
									if (fseek(file, location * BLOCK_SIZE, SEEK_SET) == 0) 
										fwrite(&block, 1, BLOCK_SIZE, file);
									
									dataLoc = 0;
									tempSize -= MAX_DATA_IN_BLOCK;
								}
								entry.files[fileLoc].fsize = size;
								if (fseek(file, directoryLoc * BLOCK_SIZE, SEEK_SET) == 0) {
									fwrite(&entry, 1, BLOCK_SIZE, file);
								}
								res = size;
							}
						}
					}
				}
			}
			fclose(file);
		}
		else {
			printf("Error writing to file! Try again");
		}
	}
	//check that size is > 0
	if (size <= 0) 
		return -1;
	//return success, or error
	return res;
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
	
		char extension[MAX_EXTENSION + 1];
		char directory[MAX_FILENAME + 1];
		char filename[MAX_FILENAME + 1];
		memset(extension, 0, MAX_EXTENSION + 1);
		memset(directory, 0, MAX_FILENAME + 1);
		memset(filename, 0, MAX_FILENAME + 1);
        printf("Unlink Path: %s\n", path);
        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);    

		// check if exists
		if (filename[0] == 0){
	  		printf("Invalid, path is a directory\n");
	  		return -EISDIR;
		}
		int dirLoc = adjust_dirOffset(directory);
		if(dirLoc < 0){
			printf("Directory %s Not Found! Enter a valid directory\n", directory);
			return -ENOENT;
		}
		int fileLoc = adjust_dirOffset(dirLoc, filename, NULL);
		if (fileLoc < 0){
			printf("File %s Not Found! Enter a valid file!\n", filename);
			return -ENOENT;
		}
		FILE *file;
		file = fopen(".disk", "rb+");
		if (file == NULL){
			printf("Error opening disk!\n");
			return -1;
		}
		// reading root
		csc452_root_directory root;
		int readRet = fread((void*)&root, sizeof(csc452_directory_entry), 1, file);
		if ( readRet < 0) {
			printf("Error reading root!\n");
			return -1;
		}
		// Reading directory
		// grab block, free and delete 
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
