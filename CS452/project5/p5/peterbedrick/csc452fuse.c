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
	} else {
		// Read root from disk
		csc452_root_directory root;
		FILE *fptr;
		fptr = fopen(".disk", "rb");
		if(fptr == NULL) {
			printf("ERROR .disk not found\n");
			return -1;
		}
		fseek(fptr, 0, SEEK_SET);
		fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
		if(fclose(fptr) == EOF) {
			printf("ERROR .disk failed to close.\n");
			return -1;
		}
		if(root.nDirectories == 0) {
			// If root has no sub-directories, return not found
			res = -ENOENT;
		}
		char dirName[20];
		char fileName[20];
		char fileExt[20];
		int ret = sscanf(path, "/%[^/]/%[^.].%s", dirName, fileName, fileExt);
		int i = 0;
		for(i = 0; i < root.nDirectories; i++) {
			if(strcmp(path, root.directories[i].dname) == 0) {
				//If the path does exist and is a directory:
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				return 0;
			} else if(ret == 3 && strcmp(dirName, root.directories[i].dname + 1) == 0) {
				fptr = fopen(".disk", "rb");
				if(fptr == NULL) {
					printf("ERROR .disk not found.\n");
					return -1;
				}
				fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
				struct csc452_directory_entry dirEntry;
				fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
				int j = 0;
				for(j = 0; j < dirEntry.nFiles; j++) {
					if(strcmp(dirEntry.files[j].fname, fileName) == 0) {
						if(strcmp(dirEntry.files[j].fext, fileExt) == 0) {
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 2;
							stbuf->st_size = dirEntry.files[j].fsize;
							if(fclose(fptr) == EOF) {
								printf("ERROR .disk failed to close.\n");
								return -1;
							}
							return 0;
						}
					}
				}
				return -ENOENT;
			} else {
				//Else return that path doesn't exist
				res = -ENOENT;
			}
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

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		// "ls" call
		// Read in root from disk
		FILE *fptr;
		fptr = fopen(".disk", "rb");
		if(fptr == NULL) {
			printf("ERROR .disk not found.\n");
			return -1;
		}
		struct csc452_root_directory root;
		fseek(fptr, 0, SEEK_SET);
		fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
		if(fclose(fptr) == EOF) {
			printf("ERROR .disk failed to close.\n");
			return -1;
		}
		int i = 0;
		for(i = 0; i < root.nDirectories; i++) {
			// Print each sub-directory of root
			filler(buf, root.directories[i].dname + 1, NULL, 0);
		}
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
	} else {
		// "ls [directory]" call
		// Read in root from disk
		FILE *fptr;
		fptr = fopen(".disk", "rb+");
		if(fptr == NULL) {
			printf("ERROR .disk not found.\n");
			return -1;
		}
		struct csc452_root_directory root;
		fseek(fptr, 0, SEEK_SET);
		fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
		int i = 0;
		int found = 0;
		for(i = 0; i < root.nDirectories; i++) {
			// For each sub-directory in root
			if(strcmp(path, root.directories[i].dname) == 0) {
				// If path parameter is equal to the current sub-directory in root
				// Read in the directory entry from disk
				fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
				struct csc452_directory_entry dirEntry;
				fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
				int j = 0;
				for(j = 0; j < dirEntry.nFiles; j++) {
					// Print each file in the sub-directory
					char temp[MAX_FILENAME + MAX_EXTENSION + 2];
					strcpy(temp, dirEntry.files[j].fname);
					strcat(temp, ".");
					strcat(temp, dirEntry.files[j].fext);
					filler(buf, temp, NULL, 0);
				}
				found = 1;
				break;
			}
		}
		if(fclose(fptr) == EOF) {
			printf("ERROR .disk failed to close.\n");
			return -1;
		}
		if(found == 0) {
			printf("Directory not found.\n");
			return -ENOENT;
		}
		filler(buf, ".", NULL, 0);
	}
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	//(void) path;
	(void) mode;
	char dirName[20];
	char fileName[20];
	char fileExt[20];
	int ret = sscanf(path, "/%[^/]/%[^.].%s", dirName, fileName, fileExt);
	if(ret > 1) {
		printf("ERROR cannot make nested sub-directories.\n");
		return -EPERM;
	}
	if(strlen(dirName) > MAX_FILENAME) {
		printf("Directory name too long.\n");
		return -ENAMETOOLONG;
	}
	// Read root in from disk
	FILE *fptr;
	fptr = fopen(".disk", "rb+");
	if(fptr == NULL) {
		printf("ERROR .disk not found\n");
		return -1;
	}
	fseek(fptr, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
	if(fclose(fptr) == EOF) {
		printf("ERROR .disk failed to close.\n");
		return -1;
	}
	int i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname, path) == 0) {
			printf("Directory %s already exists\n", path);
			return -EEXIST;
		}
	}
	if(root.nDirectories == MAX_DIRS_IN_ROOT) {
		// If root has the maximum amount of directories already
		printf("Max subdirectories reached.\n");
		return 0;
	}
	// Create new directory with path parameter name
	struct csc452_directory newDir;
	strcpy(newDir.dname, path);
	newDir.nStartBlock = root.nDirectories + 1;
	root.directories[root.nDirectories] = newDir;
	root.nDirectories++;
	// Update disk information
	fptr = fopen(".disk", "rb+");
	if(fptr == NULL) {
		printf("ERROR .disk not found.\n");
		return -1;
	}
	// Update root in disk
	fseek(fptr, 0, SEEK_SET);
	fwrite(&root, sizeof root, 1, fptr);
	// Create new directory entry in disk
	struct csc452_directory_entry dirEntry;
	fseek(fptr, newDir.nStartBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&dirEntry, sizeof dirEntry, 1, fptr);
	if(fclose(fptr) == EOF) {
		printf("ERROR .disk failed to close.\n");
		return -1;
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
	//(void) path;
	(void) mode;
    	(void) dev;
	if(strcmp(path, "/") == 0) {
		return -1;
	}	
	char dirName[20];
	char fileName[20];
	char fileExt[20];
	int ret = sscanf(path, "/%[^/]/%[^.].%s", dirName, fileName, fileExt);
	if(ret == 1) {
		printf("ERROR path is directory.\n");
		return -EISDIR;
	}
	if(ret == 2) {
		printf("ERROR no file extension.\n");
		return -1;
	}
	if(strlen(fileName) > MAX_FILENAME) {
		printf("File name too long.\n");
		return -ENAMETOOLONG;
	}
	if(strlen(fileExt) > MAX_EXTENSION) {
		printf("File extension too long.\n");
		return -ENAMETOOLONG;
	}
	FILE *fptr;
	fptr = fopen(".disk", "rb+");
	if(fptr == NULL) {
		printf("ERROR .disk not found.\n");
		return -1;
	}
	fseek(fptr, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
	int i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, dirName) == 0) {		
			fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dirEntry;
			fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
			int j = 0;
			for(j = 0; j < dirEntry.nFiles; j++) {
				if(strcmp(fileName, dirEntry.files[j].fname) == 0) {
					if(strcmp(fileExt, dirEntry.files[j].fext) == 0) {
						printf("File already exists.\n");
						if(fclose(fptr) == EOF) {
							printf("ERROR .disk failed to close.\n");
							return -1;
						}
						return -EEXIST;
					}
				}
			}
			if(dirEntry.nFiles == MAX_FILES_IN_DIR) {
				printf("Directory is full.\n");
				if(fclose(fptr) == EOF) {
					printf("ERROR .disk failed to close.\n");
					return -1;
				}
				return 0;
			}
			struct csc452_file_directory fileEntry;
			strcpy(fileEntry.fname, fileName);
			strcpy(fileEntry.fext, fileExt);
			fileEntry.fsize = 0;
			fileEntry.nStartBlock = dirEntry.nFiles + 1;
			dirEntry.files[dirEntry.nFiles] = fileEntry;
			dirEntry.nFiles++;
			struct csc452_disk_block fileBlock;
			//	     all directories + root + all file space from directories before
			fseek(fptr, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) + 
						fileEntry.nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			// 		      + specific file spot
			fwrite(&fileBlock, sizeof(struct csc452_disk_block), 1, fptr);
			fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			fwrite(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
			break;
		}
	}
	if(fclose(fptr) == EOF) {
		printf("ERROR .disk failed to close.\n");
		return -1;
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
	if(strcmp(path, "/") == 0) {
		return -EISDIR;
	}
	if(size <= 0) {
		printf("SIZE ERROR: %d\n", size);
		return -1;
	}
	char dirName[20];
	char fileName[20];
	char fileExt[20];
	int ret = sscanf(path, "/%[^/]/%[^.].%s", dirName, fileName, fileExt);
	if(ret == 1) {
		printf("ERROR path is directory.\n");
		return -EISDIR;
	}
	if(ret == 2) {
		printf("ERROR no file extension.\n");
		return -1;
	}
	FILE *fptr;
	fptr = fopen(".disk", "rb");
	if(fptr == NULL) {
		printf("ERROR .disk not found.\n");
		return -1;
	}
	fseek(fptr, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
	int i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, dirName) == 0) {
			fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dirEntry;
			fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
			int j = 0;
			for(j = 0; j < dirEntry.nFiles; j++) {
				if(strcmp(dirEntry.files[j].fname, fileName) == 0) {
					if(strcmp(dirEntry.files[j].fext, fileExt) == 0) {
						fseek(fptr, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * 
							(root.directories[i].nStartBlock - 1)) + dirEntry.files[j].nStartBlock 
							- 1) * BLOCK_SIZE, SEEK_SET);
						struct csc452_disk_block block;
						fread(&block, sizeof(struct csc452_disk_block), 1, fptr);
						char *temp = "";
						temp = block.data;
						temp += offset;
						strncpy(buf, temp, dirEntry.files[j].fsize);
						buf[dirEntry.files[j].fsize] = '\0';
						break;
					}
				}
			}
			break;
		}
	}
	fclose(fptr);
	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	if(strcmp(path, "/") == 0) {
		return -1;
	}
	if(size <= 0) {
		printf("SIZE %d\n", size);
		return -1;
	}
	if(offset > MAX_DATA_IN_BLOCK) {
		printf("ERROR offset too big.\n");
		return -EFBIG;
	}
	char dirName[20];
	char fileName[20];
	char fileExt[20];
	int ret = sscanf(path, "/%[^/]/%[^.].%s", dirName, fileName, fileExt);
	if(ret == 1) {
		printf("ERROR path is directory.\n");
		return -EISDIR;
	}
	if(ret == 2) {
		printf("ERROR no file extension.\n");
		return -1;
	}
	if(strlen(fileName) > MAX_FILENAME) {
		printf("File name too long.\n");
		return -ENAMETOOLONG;
	}
	if(strlen(fileExt) > MAX_EXTENSION) {
		printf("File extension too long.\n");
		return -ENAMETOOLONG;
	}
	FILE *fptr;
	fptr = fopen(".disk", "rb+");
	if(fptr == NULL) {
		printf("ERROR .disk not found.\n");
		return -1;
	}
	fseek(fptr, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
	int i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, dirName) == 0) {
			fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dirEntry;
			fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
			int j = 0;
			for(j = 0; j < dirEntry.nFiles; j++) {
				if(strcmp(dirEntry.files[j].fname, fileName) == 0) {
					if(strcmp(dirEntry.files[j].fext, fileExt) == 0) {
						fseek(fptr, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * 
							(root.directories[i].nStartBlock - 1)) + dirEntry.files[j].nStartBlock 
							- 1) * BLOCK_SIZE, SEEK_SET);
						struct csc452_disk_block block;
						fread(&block, sizeof(struct csc452_disk_block), 1, fptr);
						block.data[offset] = '\0';
						char temp[size];
						temp[0] = '\0';
						strncpy(temp, buf, size);
						strcat(block.data, temp);
						block.data[dirEntry.files[j].fsize + size] = '\0';
						fseek(fptr, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * 
							(root.directories[i].nStartBlock - 1)) + dirEntry.files[j].nStartBlock 
							- 1) * BLOCK_SIZE, SEEK_SET);
						fwrite(&block, sizeof(struct csc452_disk_block), 1, fptr);
						dirEntry.files[j].fsize = strlen(block.data);
						fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
						fwrite(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
						break;
					}
				}
			}
			break;
		}
	}
	if(fclose(fptr) == EOF) {
		printf("ERROR .disk failed to close.\n");
		return -1;
	}
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	if(strcmp(path, "/") == 0) {
		printf("Path is not a directory\n");
		return -ENOTDIR;
	}
	char dirName[20];
	char fileName[20];
	char fileExt[20];
	int ret = sscanf(path, "/%[^/]/%[^.].%s", dirName, fileName, fileExt);
	if(ret > 1) {
		printf("Path is not a directory.\n");
		return -ENOTDIR;
	}
	FILE *fptr;
	fptr = fopen(".disk", "rb");
	if(fptr == NULL) {
		printf("ERROR .disk not found.\n");
		return -1;
	}
	fseek(fptr, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
	int i = 0;
	int found = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, dirName) == 0) {
			fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dirEntry;
			fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
			if(dirEntry.nFiles > 0) {
				if(fclose(fptr) == EOF) {
					printf("ERROR .disk failed to close.\n");
					return -1;
				}
				printf("Directory is not empty.\n");
				return -ENOTEMPTY;
			}
			found = 1;
			break;
		}
	}
	if(found == 0) {
		if(fclose(fptr) == EOF) {
			printf("ERROR .disk failed to close.\n");
			return -1;
		}
		printf("Directory not found.\n");
		return -ENOENT;
	}
	struct csc452_directory_entry allDirs[root.nDirectories];
	struct csc452_disk_block allBlocks[root.nDirectories][MAX_FILES_IN_DIR];
	i = 0;
	int after = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(dirName, root.directories[i].dname + 1) == 0) {
			after = 1;
			continue;
		}
		fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
		struct csc452_directory_entry dirEntry;
		fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
		int j = 0;
		for(j = 0; j < dirEntry.nFiles; j++) {
			fseek(fptr, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) 
						+ dirEntry.files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			struct csc452_disk_block block;
			fread(&block, sizeof(struct csc452_disk_block), 1, fptr);
			allBlocks[i - after][j] = block;
		}
		allDirs[i - after] = dirEntry;
	}
	if(fclose(fptr) == EOF) {
		printf("ERROR .disk failed to close.\n");
		return -1;
	}
	fptr = fopen(".disk", "wb");
	if(fptr == NULL) {
		printf("ERROR .disk not found.\n");
		return -1;
	}
	struct csc452_directory newArr[root.nDirectories - 1];
	i = 0;
	after = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(dirName, root.directories[i].dname + 1) == 0) {
			after = 1;
			continue;
		}
		newArr[i - after] = root.directories[i];
		newArr[i - after].nStartBlock -= after;
	}
	memcpy(root.directories, newArr, sizeof(root.directories));
	root.nDirectories--;
	fseek(fptr, 0, SEEK_SET);
	fwrite(&root, sizeof(struct csc452_root_directory), 1, fptr);
	i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&allDirs[i], sizeof(struct csc452_directory_entry), 1, fptr);
		int j = 0;
		for(j = 0; j < allDirs[i].nFiles; j++) {
			fseek(fptr, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) 
						+ allDirs[i].files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			fwrite(&allBlocks[i][j], sizeof(struct csc452_disk_block), 1, fptr);
		}
	}
	if(fclose(fptr) == EOF) {
		printf("ERROR .disk failed to close.\n");
		return -1;
	}
        return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
	if(strcmp(path, "/") == 0) {
		return -1;
	}
	char dirName[20];
	char fileName[20];
	char fileExt[20];
	int ret = sscanf(path, "/%[^/]/%[^.].%s", dirName, fileName, fileExt);
	if(ret == 1) {
		printf("Path is a directory.\n");
		return -EISDIR;
	}
	FILE *fptr;
	fptr = fopen(".disk", "rb");
	if(fptr == NULL) {
		printf("ERROR .disk not found.\n");
		return -1;
	}
	fseek(fptr, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, fptr);
	int i = 0;
	int found = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, dirName) == 0) {
			fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dirEntry;
			fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
			int j = 0;
			for(j = 0; j < dirEntry.nFiles; j++) {
				if(strcmp(dirEntry.files[j].fname, fileName) == 0) {
					if(strcmp(dirEntry.files[j].fext, fileExt) == 0) {
						found = 1;
						break;
					}
				}
			}
			break;
		}
	}
	if(found == 0) {
		if(fclose(fptr) == EOF) {
			printf("ERROR .disk failed to close.\n");
			return -1;
		}
		printf("File not found.\n");
		return -ENOENT;
	}
	struct csc452_directory_entry allDirs[root.nDirectories];
	struct csc452_disk_block allBlocks[root.nDirectories][MAX_FILES_IN_DIR];
	i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
		struct csc452_directory_entry dirEntry;
		fread(&dirEntry, sizeof(struct csc452_directory_entry), 1, fptr);
		int j = 0;
		int after = 0;
		for(j = 0; j < dirEntry.nFiles; j++) {
			if(strcmp(dirName, root.directories[i].dname + 1) == 0) {
				if(strcmp(fileName, dirEntry.files[j].fname) == 0) {
					if(strcmp(fileExt, dirEntry.files[j].fext) == 0) {
						after = 1;
						continue;
					}
				}
			}
			dirEntry.files[j].nStartBlock -= after;
			fseek(fptr, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) 
						+ dirEntry.files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			struct csc452_disk_block block;
			fread(&block, sizeof(struct csc452_disk_block), 1, fptr);
			allBlocks[i][j] = block;
		}
		if(strcmp(dirName, root.directories[i].dname + 1) == 0) {
			dirEntry.nFiles--;
		}
		allDirs[i] = dirEntry;
	}
	if(fclose(fptr) == EOF) {
		printf("ERROR .disk failed to close.\n");
		return -1;
	}
	fptr = fopen(".disk", "wb");
	if(fptr == NULL) {
		printf("ERROR .disk not found.\n");
		return -1;
	}
	fseek(fptr, 0, SEEK_SET);
	fwrite(&root, sizeof(struct csc452_root_directory), 1, fptr);
	i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		fseek(fptr, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&allDirs[i], sizeof(struct csc452_directory_entry), 1, fptr);
		int j = 0;
		for(j = 0; j < allDirs[i].nFiles; j++) {
			fseek(fptr, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) 
						+ allDirs[i].files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			fwrite(&allBlocks[i][j], sizeof(struct csc452_disk_block), 1, fptr);
		}
	}
	if(fclose(fptr) == EOF) {
		printf("ERROR .disk failed to close.\n");
		return -1;
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
