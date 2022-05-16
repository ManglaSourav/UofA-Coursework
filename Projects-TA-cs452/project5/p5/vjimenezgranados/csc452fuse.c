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
};

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

//Next Empty Directory
int emptyDir = 0;
//Start of Space for Directory
long emptyDirStart = 512;
int emptyFile = 0;
long startOfFiles = 20480;
long emptyFileStart = 20480;
long startOfExtraSpace = 479744;
long emptyExtraSpace = 479744;

//Array of Indexes of Free Space
int freeSpace[9303];

//bitman: There are 10240 blocks in 5MB of space, and in order to hold 10240 bits, I will mask them into ints which hold 32 bits each, and thus 10240/32 is a 320 int array.
long bitMapPointer = 5241344;
int bitmap[320];
#define BITMAP_LENGTH 320
#define THIRTY_TWO_BLOCK_LENGTH 16384

static void clearBit(long nStartBlock) {
	//Get the index of bitmap[]
	int bitmapIndex = nStartBlock / THIRTY_TWO_BLOCK_LENGTH;
	//Which bit to change
	int bitIndex = 32 - ((nStartBlock - (32 * BLOCK_SIZE * bitmapIndex)) / BLOCK_SIZE);
	bitmap[bitmapIndex] &= ~(1 << bitIndex);
}

static long getNextEmptyBlock() {
	int i;
	//Iterate through the int array which masks my bitmap.
	for (i = 0; i < BITMAP_LENGTH; i++) {
		//Index of actual bit.
		int bitIndex = 0;
		int j;
		//If it is the first index, need to shift by 31 since first bit is root.
		if (i == 0) {
			//Iterate through the bits.
			for (j = 31; j > 0; j--) {
				//Save a tempVal of the int in bitmap[].
				int tempVal = bitmap[i];
				//Shift by j bits, and check if it is 0.
				if (((tempVal >> j) & 1) == 0) {
					//Set bit to 1, meaning it is taken.
					bitmap[i] |= 1 << j;
					bitIndex = 32 - j;
					return (BLOCK_SIZE * bitIndex) + (32 * BLOCK_SIZE * i);
				}
			}
		}//It is not the first index, so check all 32 bits.
		else {
			//Iterate through the bits.
			for (j = 31; j > 0; j--) {
				//Save a tempVal of the int in bitmap[].
				int tempVal = bitmap[i];
				//Shift by j bits, and check if it is 0.
				if (((tempVal >> j) & 1) == 0) {
					//Set bit to 1, meaning it is taken.
					bitmap[i] |= 1 << j;
					bitIndex = 32 - j;
					return (BLOCK_SIZE * bitIndex) + (32 * BLOCK_SIZE * i);
				}
			}
		}
	}
	//No more empty blocks.
	return -1;
}


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char* path, struct stat* stbuf)
{
	int res = 0;
	//Parsing the path.
	char dir[MAX_FILENAME + 2] = "";
	char fileName[MAX_FILENAME + 2] = "";
	char ext[MAX_EXTENSION + 2] = "";
	sscanf(path, "/%9[^/]/%9[^.].%4s", dir, fileName, ext);

	//Opening the .disk file to read/write
	FILE* file;
	file = fopen(".disk", "rb");
	if (file == NULL)
	{
		exit(1);
	}

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else {//If the path does exist and is a directory:
		csc452_root_directory root;
		fseek(file, 0L, SEEK_SET);
		fread(&root, sizeof(csc452_root_directory), 1, file);
		if (root.nDirectories == 0) {
			//Else return that path doesn't exist
			res = -ENOENT;
		}
		else {
			//Tests to see if I am searching for a directory.
			if (strlen(fileName) == 0 && strlen(ext) == 0) {
				int i;
				for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
					if (strcmp(root.directories[i].dname, dir) == 0) {
						//If the path does exist and is a directory:
						stbuf->st_mode = S_IFDIR | 0755;
						stbuf->st_nlink = 2;
						fclose(file);
						return res;
					}
				}
			}
			else {
				long directoryStart = 0;
				int foundDir = 0;

				//Check to see if the directory exists.
				int i;
				for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
					if (strcmp(root.directories[i].dname, dir) == 0) {
						foundDir = 1;
						directoryStart = root.directories[i].nStartBlock;
					}
				}
				//Directory Found
				if (foundDir) {
					//Reading existing directory.
					csc452_directory_entry directory;
					fseek(file, directoryStart, SEEK_SET);
					fread(&directory, sizeof(csc452_directory_entry), 1, file);

					if (directory.nFiles < MAX_FILES_IN_DIR) {
						//Check to see if the file already exists.
						int i;
						for (i = 0; i < MAX_FILES_IN_DIR; i++) {
							if (strlen(ext) == 0) {
								if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
									//The path does exist and is a file:
									stbuf->st_mode = S_IFREG | 0666;
									stbuf->st_nlink = 2;
									stbuf->st_size = directory.files[i].fsize;
									fclose(file);
									return 0;
								}
							}
							else if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
								//The path does exist and is a file:
								stbuf->st_mode = S_IFREG | 0666;
								stbuf->st_nlink = 2;
								stbuf->st_size = directory.files[i].fsize;
								fclose(file);
								return 0;
							}
						}
					}
				}
			}
			//Else return that path doesn't exist
			res = -ENOENT;
		}
	}
	fclose(file);
	return res;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info* fi)
{

	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void)offset;
	(void)fi;

	//Parsing the path.
	char dir[MAX_FILENAME + 2] = "";
	char fileName[MAX_FILENAME + 2] = "";
	char ext[MAX_EXTENSION + 2] = "";
	sscanf(path, "/%9[^/]/%9[^.].%4s", dir, fileName, ext);

	//Opening the .disk file to read/write
	FILE* file;
	file = fopen(".disk", "rb");
	if (file == NULL)
	{
		exit(1);
	}

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		csc452_root_directory root;
		fseek(file, 0L, SEEK_SET);
		fread(&root, sizeof(csc452_root_directory), 1, file);
		int i;
		for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
			if (strlen(root.directories[i].dname) != 0) {
				filler(buf, root.directories[i].dname, NULL, 0);
			}
		}
	}
	else {
		//Tests to see if I am reading only a directory.
		if (strlen(fileName) == 0 && strlen(ext) == 0) {
			csc452_root_directory root;
			fseek(file, 0L, SEEK_SET);
			fread(&root, sizeof(csc452_root_directory), 1, file);
			int found = 0;
			long startOfBlock = 0;
			//Check to see if the file already exists.
			int i;
			for (i = 0; i < root.nDirectories; i++) {
				if (strcmp(root.directories[i].dname, dir) == 0) {
					found = 1;
					startOfBlock = root.directories[i].nStartBlock;
					break;
				}
			}
			if (found) {
				filler(buf, ".", NULL, 0);
				filler(buf, "..", NULL, 0);
				csc452_directory_entry directory;
				fseek(file, startOfBlock, SEEK_SET);
				fread(&directory, sizeof(csc452_directory_entry), 1, file);
				//List all files available in the directory.
				int j;
				for (j = 0; j < MAX_FILES_IN_DIR; j++) {
					if (strlen(directory.files[j].fname) != 0 && strlen(directory.files[j].fext) != 0) {
						char nameOfFile[MAX_FILENAME + MAX_EXTENSION + 2] = "";
						strncat(nameOfFile, directory.files[j].fname, MAX_FILENAME + 1);
						strcat(nameOfFile, ".");
						strncat(nameOfFile, directory.files[j].fext, MAX_EXTENSION + 1);
						filler(buf, nameOfFile, NULL, 0);
					}
					else if (strlen(directory.files[j].fname) != 0) {
						char nameOfFile[MAX_FILENAME + MAX_EXTENSION + 2] = "";
						strncat(nameOfFile, directory.files[j].fname, MAX_FILENAME + 1);
						filler(buf, nameOfFile, NULL, 0);
					}
				}
			}
			else {
				fclose(file);
				return -ENOENT;
			}
		}
		else {
			fclose(file);
			return -ENOENT;
		}
	}
	fclose(file);
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char* path, mode_t mode)
{
	(void)mode;

	//Parsing the path.
	char dir[MAX_FILENAME + 2] = "";
	char fileName[MAX_FILENAME + 2] = "";
	char ext[MAX_EXTENSION + 2] = "";
	sscanf(path, "/%9[^/]/%9[^.].%4s", dir, fileName, ext);

	//Opening the .disk file to read/write
	FILE* file;
	file = fopen(".disk", "rb+");
	if (file == NULL)
	{
		exit(1);
	}

	//Tests to see if I am adding only a directory.
	if (strlen(fileName) == 0 && strlen(ext) == 0) {
		//Check the length of the name of dir.
		if (strlen(dir) == 9) {
			fclose(file);
			return -ENAMETOOLONG;
		}
		//Read the root.
		csc452_root_directory root;
		fseek(file, 0L, SEEK_SET);
		fread(&root, sizeof(csc452_root_directory), 1, file);

		if (root.nDirectories < MAX_DIRS_IN_ROOT) {
			//Check to see if the directory already exists.
			int i;
			for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
				if (strcmp(root.directories[i].dname, dir) == 0) {
					return -EEXIST;
				}
			}
			//Reading bitmap.
			fseek(file, bitMapPointer, SEEK_SET);
			fread(&bitmap, sizeof(bitmap), 1, file);
			//Checking to see the next empty Block()
			long nextOpenFile = getNextEmptyBlock();
			//Writing bitmap
			fseek(file, bitMapPointer, SEEK_SET);
			fwrite(&bitmap, sizeof(bitmap), 1, file);

			//Update fields.

			strncpy(root.directories[emptyDir].dname, dir, MAX_FILENAME + 1);
			//root.directories[emptyDir].nStartBlock = sizeof(csc452_root_directory) + (root.nDirectories * BLOCK_SIZE);
			root.directories[emptyDir].nStartBlock = nextOpenFile;
			root.nDirectories++;

			fseek(file, 0L, SEEK_SET);
			fwrite(&root, sizeof(csc452_root_directory), 1, file);

			csc452_directory_entry newEntry;
			fseek(file, root.directories[emptyDir].nStartBlock, SEEK_SET);
			newEntry.nFiles = 0;
			//Create empty directory_entry:
			int j;
			for (j = 0; j < MAX_FILES_IN_DIR; j++) {
				strcpy(newEntry.files[j].fname, "");
				strcpy(newEntry.files[j].fext, "");
				newEntry.files[j].nStartBlock = 0L;
				newEntry.files[j].fsize = 0L;
			}
			fwrite(&newEntry, sizeof(csc452_directory_entry), 1, file);

			for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
				if (root.directories[i].nStartBlock == 0) {
					emptyDir = i;
					fclose(file);
					return 0;
				}
			}
			emptyDir = -1;
		}
		else {
			fclose(file);
			return -ENOSPC;
		}
	}
	else {
		fclose(file);
		return -EPERM;
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
static int csc452_mknod(const char* path, mode_t mode, dev_t dev)
{
	(void)mode;
	(void)dev;
	//Parsing the path.
	char dir[MAX_FILENAME + 2] = "";
	char fileName[MAX_FILENAME + 2] = "";
	char ext[MAX_EXTENSION + 2] = "";
	sscanf(path, "/%9[^/]/%9[^.].%4s", dir, fileName, ext);

	//Opening the .disk file to read/write
	FILE* file;
	file = fopen(".disk", "rb+");
	if (file == NULL)
	{
		exit(1);
	}
	//Tests to see if I am creating a directory in root.
	if (strlen(fileName) == 0 && strlen(ext) == 0) {
		fclose(file);
		return -EPERM;
	}
	else {
		if (strlen(fileName) == 9 || strlen(ext) == 4) {
			fclose(file);
			return -ENAMETOOLONG;
		}
		//Read the root.
		csc452_root_directory root;
		fseek(file, 0L, SEEK_SET);
		fread(&root, sizeof(csc452_root_directory), 1, file);
		long directoryStart = 0;
		int foundDir = 0;

		//Check to see if the directory exists.
		int i;
		for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
			if (strcmp(root.directories[i].dname, dir) == 0) {
				foundDir = 1;
				directoryStart = root.directories[i].nStartBlock;
				break; //Added This
			}
		}
		//Directory Found
		if (foundDir) {
			//Reading existing directory.
			csc452_directory_entry directory;
			fseek(file, directoryStart, SEEK_SET);
			fread(&directory, sizeof(csc452_directory_entry), 1, file);

			if (directory.nFiles < MAX_FILES_IN_DIR) {
				//Check to see if the file already exists.
				int i;
				for (i = 0; i < MAX_FILES_IN_DIR; i++) {
					if (strlen(ext) == 0) {
						if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
							//File exists.
							fclose(file);
							return -EEXIST;
						}
					}
					else if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
						//File exists.
						fclose(file);
						return -EEXIST;
					}
				}
				//Update fields. File Does Not Exist.

				//Reading bitmap.
				fseek(file, bitMapPointer, SEEK_SET);
				fread(&bitmap, sizeof(bitmap), 1, file);
				//Checking to see the next empty Block()
				long nextOpenFile = getNextEmptyBlock();
				//Writing bitmap
				fseek(file, bitMapPointer, SEEK_SET);
				fwrite(&bitmap, sizeof(bitmap), 1, file);

				int fileIndex = 0;
				int x;
				/*New Implementation - No Test*/
				for (x = 0; x < MAX_FILES_IN_DIR; x++) {
					if (strlen(directory.files[x].fname) == 0 && strlen(directory.files[x].fext) == 0) {
						fileIndex = x;
						break;
					}
				}
				strncpy(directory.files[fileIndex].fname, fileName, MAX_FILENAME + 1);
				strncpy(directory.files[fileIndex].fext, ext, MAX_EXTENSION + 1);

				directory.files[fileIndex].nStartBlock = nextOpenFile;
				directory.files[fileIndex].fsize = 0;

				directory.nFiles++;

				fseek(file, directoryStart, SEEK_SET);
				fwrite(&directory, sizeof(csc452_directory_entry), 1, file);

				csc452_disk_block newFile;
				fseek(file, nextOpenFile, SEEK_SET);
				newFile.nNextBlock = 0L;
				fwrite(&newFile, sizeof(csc452_disk_block), 1, file);
			}
			else {
				//Num of files reached.
				fclose(file);
				return -ENOSPC;
			}

		}
		else {
			//Directory not found
			fclose(file);
			return -ENOENT;
		}
	}
	fclose(file);
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char* path, char* buf, size_t size, off_t offset,
	struct fuse_file_info* fi)
{
	(void)fi;

	//Parsing the path.
	char dir[MAX_FILENAME + 2] = "";
	char fileName[MAX_FILENAME + 2] = "";
	char ext[MAX_EXTENSION + 2] = "";
	sscanf(path, "/%9[^/]/%9[^.].%4s", dir, fileName, ext);

	//Opening the .disk file to read/write
	FILE* file;
	file = fopen(".disk", "rb+");
	if (file == NULL)
	{
		exit(1);
	}
	if (size > 0) {
		//Tests to see if path is valid
		if (strlen(fileName) == 0 && strlen(ext) == 0) {
			fclose(file);
			return -EISDIR;
		}
		else {
			//Read the root.
			csc452_root_directory root;
			fseek(file, 0L, SEEK_SET);
			fread(&root, sizeof(csc452_root_directory), 1, file);
			long directoryStart = 0;
			int foundDir = 0;

			//Check to see if the directory exists.
			int i;
			for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
				if (strcmp(root.directories[i].dname, dir) == 0) {
					foundDir = 1;
					directoryStart = root.directories[i].nStartBlock;
					break; //Added This
				}
			}
			//Directory Found
			if (foundDir) {
				//Reading existing directory.
				csc452_directory_entry directory;
				fseek(file, directoryStart, SEEK_SET);
				fread(&directory, sizeof(csc452_directory_entry), 1, file);

				int i;
				long fileStart = 0;
				long sizeOfFile = 0L;
				int foundFile = 0;

				for (i = 0; i < MAX_FILES_IN_DIR; i++) {
					if (strlen(ext) == 0) {
						if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
							//The path does exist and is a file:
						//File exists.
							fileStart = directory.files[i].nStartBlock;
							foundFile = 1;
							sizeOfFile = directory.files[i].fsize;
							break;
						}
					}
					else if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
						//The path does exist and is a file:
						//File exists.
						fileStart = directory.files[i].nStartBlock;
						foundFile = 1;
						sizeOfFile = directory.files[i].fsize;
						break;
					}
				}
				if (foundFile) {
					long currentFilePos = fileStart;
					//Reading file
					csc452_disk_block fileHead;
					fseek(file, fileStart, SEEK_SET);
					fread(&fileHead, sizeof(csc452_disk_block), 1, file);

					if (offset <= sizeOfFile) {
						int numOfBlocks = offset / 507;
						int j;
						//Iterates to correct block which holds offset
						for (j = 0; j < numOfBlocks; j++) {
							currentFilePos = fileHead.nNextBlock;
							fseek(file, fileHead.nNextBlock, SEEK_SET);
							fread(&fileHead, sizeof(csc452_disk_block), 1, file);
						}
						//Writing to File
						int dataIndex = offset - (508 * numOfBlocks);
						int i;
						long currFileSize = offset - (508 * numOfBlocks);
						long sizeRead = 0;
						for (i = 0; i < sizeOfFile; i++) {
							buf[i] = fileHead.data[dataIndex];
							dataIndex++;
							currFileSize++;
							sizeRead++;

							if (currFileSize >= MAX_DATA_IN_BLOCK) {
								currentFilePos = fileHead.nNextBlock;
								//Reading new block
								fseek(file, currentFilePos, SEEK_SET);
								fread(&fileHead, sizeof(csc452_disk_block), 1, file);
								dataIndex = 0;
								currFileSize = 0;
							}
						}
						fclose(file);
						return sizeRead;
					}
					else {
						//Offset beyond the file size
						fclose(file);
						return -EFBIG;
					}
				}
				else {
					//File Does Not Exist
					fclose(file);
					return -ENOENT;
				}

			}
			else {
				//Directory not found
				fclose(file);
				return -ENOENT;
			}
		}
	}
	else {
		fclose(file);
		return size;
	}
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char* path, const char* buf, size_t size,
	off_t offset, struct fuse_file_info* fi)
{
	(void)fi;

	//Parsing the path.
	char dir[MAX_FILENAME + 2] = "";
	char fileName[MAX_FILENAME + 2] = "";
	char ext[MAX_EXTENSION + 2] = "";
	sscanf(path, "/%9[^/]/%9[^.].%4s", dir, fileName, ext);

	//Opening the .disk file to read/write
	FILE* file;
	file = fopen(".disk", "rb+");
	if (file == NULL)
	{
		exit(1);
	}
	if (size > 0) {
		//Tests to see if path is valid
		if (strlen(fileName) == 0 && strlen(ext) == 0) {
			fclose(file);
			return -ENOENT;
		}
		else {
			//Read the root.
			csc452_root_directory root;
			fseek(file, 0L, SEEK_SET);
			fread(&root, sizeof(csc452_root_directory), 1, file);
			long directoryStart = 0;
			int foundDir = 0;

			//Check to see if the directory exists.
			int i;
			for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
				if (strcmp(root.directories[i].dname, dir) == 0) {
					foundDir = 1;
					directoryStart = root.directories[i].nStartBlock;
					break; //Added This
				}
			}
			//Directory Found
			if (foundDir) {
				//Reading existing directory.
				csc452_directory_entry directory;
				fseek(file, directoryStart, SEEK_SET);
				fread(&directory, sizeof(csc452_directory_entry), 1, file);

				int i;
				long fileStart = 0;
				int fileIndex = 0;
				long sizeOfFile = 0L;
				int foundFile = 0;

				for (i = 0; i < MAX_FILES_IN_DIR; i++) {
					if (strlen(ext) == 0) {
						if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
							//The path does exist and is a file:
						//File exists.
							fileStart = directory.files[i].nStartBlock;
							fileIndex = i;
							foundFile = 1;
							sizeOfFile = directory.files[i].fsize;
							break;
						}
					}
					else if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
						//The path does exist and is a file:
						//File exists.
						fileStart = directory.files[i].nStartBlock;
						fileIndex = i;
						foundFile = 1;
						sizeOfFile = directory.files[i].fsize;
						break;
					}
				}
				if (foundFile) {
					long currentFilePos = fileStart;
					//Reading file
					csc452_disk_block fileHead;
					fseek(file, fileStart, SEEK_SET);
					fread(&fileHead, sizeof(csc452_disk_block), 1, file);

					if (offset <= sizeOfFile) {
						int numOfBlocks = offset / 507;
						int j;
						//Iterates to correct block which holds offset
						for (j = 0; j < numOfBlocks; j++) {
							currentFilePos = fileHead.nNextBlock;
							fseek(file, fileHead.nNextBlock, SEEK_SET);
							fread(&fileHead, sizeof(csc452_disk_block), 1, file);
						}
						//Writing to File
						int dataIndex = offset - (508 * numOfBlocks);
						int i;
						long currFileSize = offset - (508 * numOfBlocks);
						long sizeWritten = 0;
						for (i = 0; i < size; i++) {
							fileHead.data[dataIndex] = buf[i];
							dataIndex++;
							currFileSize++;
							sizeWritten++;
							if (currFileSize >= MAX_DATA_IN_BLOCK) {
								//Writting current fileHead
								//Writing back data to block.
								fseek(file, currentFilePos, SEEK_SET);
								fwrite(&fileHead, sizeof(csc452_disk_block), 1, file);

								//Reading bitmap.
								fseek(file, bitMapPointer, SEEK_SET);
								fread(&bitmap, sizeof(bitmap), 1, file);
								//Checking to see the next empty Block()
								long nextOpenFile = getNextEmptyBlock();
								if (nextOpenFile == -1) {
									//Bitmap is full which means no more blocks.
									fclose(file);
									return -ENOSPC;
								}
								//Writing bitmap
								fseek(file, bitMapPointer, SEEK_SET);
								fwrite(&bitmap, sizeof(bitmap), 1, file);

								//Linking previous block with new block.
								fileHead.nNextBlock = nextOpenFile;
								fseek(file, currentFilePos, SEEK_SET);
								fwrite(&fileHead, sizeof(csc452_disk_block), 1, file);

								//Creating new block
								csc452_disk_block newBlock;
								fseek(file, nextOpenFile, SEEK_SET);
								newBlock.nNextBlock = 0L;
								fwrite(&newBlock, sizeof(csc452_disk_block), 1, file);

								//Reading newly created block
								fseek(file, nextOpenFile, SEEK_SET);
								fread(&fileHead, sizeof(csc452_disk_block), 1, file);
								//Updating variables.
								dataIndex = 0;
								currFileSize = 0;
								currentFilePos = nextOpenFile;
							}
						}
						//Update the new File Size
						sizeOfFile = (sizeOfFile > (offset + sizeWritten)) ? sizeOfFile : (offset + sizeWritten);
						directory.files[fileIndex].fsize = sizeOfFile;
						//Writing back file size.
						fseek(file, directoryStart, SEEK_SET);
						fwrite(&directory, sizeof(csc452_directory_entry), 1, file);

						//Writing back data to block.
						fseek(file, currentFilePos, SEEK_SET);
						fwrite(&fileHead, sizeof(csc452_disk_block), 1, file);
						fclose(file);
						return sizeWritten;
					}
					else {
						//Offset beyond the file size
						fclose(file);
						return -EFBIG;
					}
				}
				else {
					//File Does Not Exist
					fclose(file);
					return -ENOENT;
				}

			}
			else {
				//Directory not found
				fclose(file);
				return -ENOENT;
			}
		}
	}
	else {
		fclose(file);
		return size;
	}
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char* path)
{
	//Parsing the path.
	char dir[MAX_FILENAME + 2] = "";
	char fileName[MAX_FILENAME + 2] = "";
	char ext[MAX_EXTENSION + 2] = "";
	sscanf(path, "/%9[^/]/%9[^.].%4s", dir, fileName, ext);

	//Opening the .disk file to read/write
	FILE* file;
	file = fopen(".disk", "rb+");
	if (file == NULL)
	{
		exit(1);
	}
	//Tests to see if I am removing a directory.
	if (strlen(fileName) == 0 && strlen(ext) == 0) {
		//Read the root.
		csc452_root_directory root;
		fseek(file, 0L, SEEK_SET);
		fread(&root, sizeof(csc452_root_directory), 1, file);
		long directoryStart = 0;
		int rootIndex = 0;
		int found = 0;

		//Check to see if the directory already exists.
		int i;
		for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
			if (strcmp(root.directories[i].dname, dir) == 0) {
				found = 1;
				directoryStart = root.directories[i].nStartBlock;
				rootIndex = i;
			}
		}
		if (found) {
			//Reading existing directory.
			csc452_directory_entry directory;
			fseek(file, directoryStart, SEEK_SET);
			fread(&directory, sizeof(csc452_directory_entry), 1, file);

			if (directory.nFiles > 0) {
				fclose(file);
				return -ENOTEMPTY;
			}
			else {
				//Replacing directory with empty directory.
				csc452_directory_entry emptyDirectory;
				fseek(file, directoryStart, SEEK_SET);
				fwrite(&emptyDirectory, sizeof(csc452_directory_entry), 1, file);

				//Reading bitmap.
				fseek(file, bitMapPointer, SEEK_SET);
				fread(&bitmap, sizeof(bitmap), 1, file);
				//Checking to see the next empty Block()
				clearBit(directoryStart);
				//Writing bitmap
				fseek(file, bitMapPointer, SEEK_SET);
				fwrite(&bitmap, sizeof(bitmap), 1, file);

				//Updating root.directories[]
				emptyDir = rootIndex;

				strcpy(root.directories[rootIndex].dname, "");
				root.directories[rootIndex].nStartBlock = 0L;
				root.nDirectories--;
				fseek(file, 0L, SEEK_SET);
				fwrite(&root, sizeof(csc452_root_directory), 1, file);
			}

		}
		else {
			fclose(file);
			return -ENOENT;
		}
	}
	else {
		fclose(file);
		return -ENOTDIR;
	}
	fclose(file);
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char* path)
{
	//Parsing the path.
	char dir[MAX_FILENAME + 2] = "";
	char fileName[MAX_FILENAME + 2] = "";
	char ext[MAX_EXTENSION + 2] = "";
	sscanf(path, "/%9[^/]/%9[^.].%4s", dir, fileName, ext);

	//Opening the .disk file to read/write
	FILE* file;
	file = fopen(".disk", "rb+");
	if (file == NULL)
	{
		exit(1);
	}
	//Tests to see if path is valid
	if (strlen(fileName) == 0 && strlen(ext) == 0) {
		fclose(file);
		return -EISDIR;
	}
	else {
		//Read the root.
		csc452_root_directory root;
		fseek(file, 0L, SEEK_SET);
		fread(&root, sizeof(csc452_root_directory), 1, file);
		long directoryStart = 0;
		int foundDir = 0;

		//Check to see if the directory exists.
		int i;
		for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
			if (strcmp(root.directories[i].dname, dir) == 0) {
				foundDir = 1;
				directoryStart = root.directories[i].nStartBlock;
				break; //Added This
			}
		}
		//Directory Found
		if (foundDir) {
			//Reading existing directory.
			csc452_directory_entry directory;
			fseek(file, directoryStart, SEEK_SET);
			fread(&directory, sizeof(csc452_directory_entry), 1, file);

			int i;
			long fileStart = 0;
			int foundFile = 0;
			int fileIndex = 0;

			for (i = 0; i < MAX_FILES_IN_DIR; i++) {
				if (strlen(ext) == 0) {
					if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
						//The path does exist and is a file:
					//File exists.
						fileStart = directory.files[i].nStartBlock;
						foundFile = 1;
						fileIndex = i;
						break;
					}
				}
				else if (strcmp(directory.files[i].fname, fileName) == 0 && strcmp(directory.files[i].fext, ext) == 0) {
					//The path does exist and is a file:
					//File exists.
					fileStart = directory.files[i].nStartBlock;
					foundFile = 1;
					fileIndex = i;
					break;
				}
			}
			if (foundFile) {
				//Update directory
				directory.files[fileIndex].nStartBlock = 0L;
				directory.files[fileIndex].fsize = 0L;
				strcpy(directory.files[fileIndex].fname, "");
				strcpy(directory.files[fileIndex].fext, "");
				directory.nFiles--;
				fseek(file, directoryStart, SEEK_SET);
				fwrite(&directory, sizeof(csc452_directory_entry), 1, file);


				//Reading file
				csc452_disk_block fileHead;
				fseek(file, fileStart, SEEK_SET);
				fread(&fileHead, sizeof(csc452_disk_block), 1, file);
				long currentFilePos = fileStart;

				if (fileHead.nNextBlock == 0) {
					//Reading bitmap.
					fseek(file, bitMapPointer, SEEK_SET);
					fread(&bitmap, sizeof(bitmap), 1, file);
					//Checking to see the next empty Block()
					clearBit(currentFilePos);
					//Writing bitmap
					fseek(file, bitMapPointer, SEEK_SET);
					fwrite(&bitmap, sizeof(bitmap), 1, file);

					//Creating empty block
					csc452_disk_block emptyBlock;
					emptyBlock.nNextBlock = 0L;
					fseek(file, currentFilePos, SEEK_SET);
					fwrite(&emptyBlock, sizeof(csc452_disk_block), 1, file);
				}
				else {
					//Iterate through all files and delete each block.
					while (fileHead.nNextBlock != 0) {
						//Reading bitmap.
						fseek(file, bitMapPointer, SEEK_SET);
						fread(&bitmap, sizeof(bitmap), 1, file);
						//Checking to see the next empty Block()
						clearBit(currentFilePos);
						//Writing bitmap
						fseek(file, bitMapPointer, SEEK_SET);
						fwrite(&bitmap, sizeof(bitmap), 1, file);

						//Creating empty block
						csc452_disk_block emptyBlock;
						emptyBlock.nNextBlock = 0L;
						fseek(file, currentFilePos, SEEK_SET);
						fwrite(&emptyBlock, sizeof(csc452_disk_block), 1, file);

						//Reading next file
						currentFilePos = fileHead.nNextBlock;
						fseek(file, currentFilePos, SEEK_SET);
						fread(&fileHead, sizeof(csc452_disk_block), 1, file);
					}
					//Crealring the last file.
					//Reading bitmap.
					fseek(file, bitMapPointer, SEEK_SET);
					fread(&bitmap, sizeof(bitmap), 1, file);
					//Checking to see the next empty Block()
					clearBit(currentFilePos);
					//Writing bitmap
					fseek(file, bitMapPointer, SEEK_SET);
					fwrite(&bitmap, sizeof(bitmap), 1, file);

					//Creating empty block
					csc452_disk_block emptyBlock;
					emptyBlock.nNextBlock = 0L;
					fseek(file, currentFilePos, SEEK_SET);
					fwrite(&emptyBlock, sizeof(csc452_disk_block), 1, file);
				}
				fclose(file);
				return 0;
			}
			else {
				//Offset beyond the file size
				fclose(file);
				return -EFBIG;
			}
		}
		else {
			//File Does Not Exist
			fclose(file);
			return -ENOENT;
		}

	}
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
static int csc452_truncate(const char* path, off_t size)
{
	(void)path;
	(void)size;

	return 0;
}

/*
 * Called when we open a file
 *
 */
static int csc452_open(const char* path, struct fuse_file_info* fi)
{
	(void)path;
	(void)fi;
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
static int csc452_flush(const char* path, struct fuse_file_info* fi)
{
	(void)path;
	(void)fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations csc452_oper = {
	.getattr = csc452_getattr,
	.readdir = csc452_readdir,
	.mkdir = csc452_mkdir,
	.read = csc452_read,
	.write = csc452_write,
	.mknod = csc452_mknod,
	.truncate = csc452_truncate,
	.flush = csc452_flush,
	.open = csc452_open,
	.unlink = csc452_unlink,
	.rmdir = csc452_rmdir
};

//Don't change this.
int main(int argc, char* argv[])
{
	return fuse_main(argc, argv, &csc452_oper, NULL);
}
