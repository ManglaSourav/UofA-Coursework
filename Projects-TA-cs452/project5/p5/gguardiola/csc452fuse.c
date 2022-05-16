/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/
/* Filename: csc452fuse.c
 * Author: Gerry Guardiola and Dr. Misurda
 * Purpose: Creates a two level file system using structs and FUSE
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
	char dname[MAX_FILENAME + 1]; // added to simplify directory management
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
	int dirIdx;  // index of directory 
	csc452_directory_entry curr;  // not using malloc 
	// create char arrays to hold strings
	char dir[MAX_FILENAME+1]; 
	char file[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];
	
	// fill with 0 
	memset(stbuf, 0, sizeof(struct stat));
	// if path is root
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		//If the path does exist and is a directory:
		//stbuf->st_mode = S_IFDIR | 0755;
		//stbuf->st_nlink = 2;

		//If the path does exist and is a file:
		//stbuf->st_mode = S_IFREG | 0666;
		//stbuf->st_nlink = 2;
		//stbuf->st_size = file size
		
		//Else return that path doesn't exist

		// set with 0 
		memset(dir, 0, MAX_FILENAME+1);
		memset(file, 0, MAX_FILENAME+1);
		memset(ext, 0, MAX_EXTENSION+1);
		sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
		dirIdx = -1;  // -1 if not found
		csc452_directory_entry temp;  // temp directory to iterate through
		int index = 0;
		int seek;  // return for fseek 
		int read;  // return for fread
		int val;  // status integer ( 0s if error)
		// find directory
		FILE *f = fopen(".directories", "rb");
		if (f == NULL) {
			val = 0;
		}
		else {
			seek = fseek(f,sizeof(csc452_directory_entry)*dirIdx,SEEK_SET);
			if(seek == -1) { 
				val = 0;
			}
			read = fread(&temp,sizeof(csc452_directory_entry),1,f);
			if(read == 1) {
				val = 1;
			}
		}
		fclose(f);
		// directory must exist
		while (dirIdx == -1 && val!=0) {
			// if match directory name
			if (strcmp(temp.dname, dir) == 0) {
				dirIdx = index;
			}
			index+=1;
			FILE *f = fopen(".directories", "rb");
			if (f == NULL) {
				val = 0;
			}	
			else {
				seek = fseek(f,sizeof(csc452_directory_entry)*index,SEEK_SET);
				if(seek == -1) { 
					val = 0;
				}
				read = fread(&temp,sizeof(csc452_directory_entry),1,f);
				if(read == 1) {
					val = 1;
				}
			}
			fclose(f);
		}
		//if directory is actually valid 
		if(dirIdx != -1 && dir != NULL) {
			// write to curr
			FILE *f = fopen(".directories", "rb");
			if(f != NULL) {
				seek = fseek(f,sizeof(csc452_disk_block)*dirIdx, SEEK_SET);
				if(seek != -1) {
					fwrite(&curr,sizeof(csc452_disk_block),1, f);
				}
			}
			fclose(f);

			if(file[0] == '\0') {  // not a file
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
			}
			else {
				int i = 0;
				while (curr.nFiles > i) {
					// find of file name matches and ext matches
					if(strcmp(curr.files[i].fname, file) == 0 && strcmp(curr.files[i].fext, ext) == 0) {  
						stbuf->st_mode = S_IFREG | 0666; 
						stbuf->st_nlink = 1;
						stbuf->st_size = curr.files[i].fsize;
					}
					i+=1;
				}
			}
		}
		// no directory found
		else {
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
	
	// set variables to be used
	int dirIdx;
	csc452_directory_entry curr;
	char dir[MAX_FILENAME+1];
	char file[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];
	char arr[50];

	// set values to 0
	memset(dir, 0, MAX_FILENAME+1);
	memset(file, 0, MAX_FILENAME+1);
	memset(ext, 0, MAX_EXTENSION+1);
	
	// fill array with name inputs
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	dirIdx = -1;
	csc452_directory_entry temp;
	int index = 0;
	int seek;
	int read;
	int val;
	// read directory at index to find correct index
	FILE *f = fopen(".directories", "rb");
	if (f == NULL) {
		val = 0;
	}
	else {
		seek = fseek(f,sizeof(csc452_directory_entry)*dirIdx, SEEK_SET);
		if(seek == -1) { 
			val = 0;
		}
		read = fread(&temp,sizeof(csc452_directory_entry),1,f);
		if(read == 1) {
			val = 1;
		}
	}
	fclose(f);
	// iterate until index name is found
	while (dirIdx == -1 && val != 0) {
		if (strcmp(temp.dname, dir) == 0) {
			dirIdx = index;
		}
		index+=1;
		FILE *f = fopen(".directories", "rb");
		if (f == NULL) {
			val = 0;
		}
		else {
			seek = fseek(f,sizeof(csc452_directory_entry)*index,SEEK_SET);
			if(seek == -1) { 
				val = 0;
			}
			read = fread(&temp,sizeof(csc452_directory_entry),1,f);
			if(read == 1) {
				val = 1;
			}
		}
		fclose(f);
	}	
	// if not root 
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		int i = 0;
		while(val != 0) {
			FILE *f = fopen(".directories", "rb");
			if (f == NULL) {
				val = 0;
			}
			else {
				seek = fseek(f,sizeof(csc452_directory_entry)*dirIdx,SEEK_SET);
				if(seek == -1) { 
					val = 0;
				}
				read = fread(&curr,sizeof(csc452_directory_entry),1,f);
				if(read == 1) {
					val = 1;
				}
			}
			fclose(f);
			filler(buf, curr.dname, NULL, 0);
			i+=1;
		}
	}
	else {
		// All we have _right now_ is root (/), so any other path must
		// not exist. 
		if(dir != NULL && dirIdx != -1) {
			// write to directory
			FILE *f = fopen(".directories", "rb");
			if(f != NULL) {
				seek = fseek(f, sizeof(csc452_disk_block)*dirIdx, SEEK_SET);
				if(seek != -1) {
					fwrite(&curr, sizeof(csc452_disk_block), 1, f);
				}
			}
			fclose(f);

			filler(buf, ".", NULL, 0);
			filler(buf, "..", NULL, 0);
		
			for(int i=0; i < curr.nFiles; i++) {
				if(strlen(curr.files[i].fext) > 0) {
					sprintf(arr,"%s.%s",curr.files[i].fname,curr.files[i].fext);
				}
				else { 
					sprintf(arr,"%s",curr.files[i].fname);
				}

				filler(buf,arr,NULL,0);
			}
		}
		else {
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
	(void) mode;
	int dirIdx = -1;
	csc452_directory_entry temp;  // temp directory to find if dir exists
	// set char arrays 
	char dir[MAX_FILENAME+1];
	char file[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];
	
	// input strings into char arrays 
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	// no directory input
	if(dir == NULL) { 
		return -EPERM;
	}
	if (dir[0] == '\0') {
		return -EPERM;
	}
	else {
		int index = 0;
		int seek;
		int read;
		int val;
		// search for directory at index
		FILE *f = fopen(".directories", "rb");
		if (f == NULL) {
			val = 0;
		}
		else {
			seek = fseek(f, sizeof(csc452_directory_entry) * dirIdx, SEEK_SET);
			if(seek == -1) { 
				val = 0;
			}
			read = fread(&temp, sizeof(csc452_directory_entry), 1, f);
			if(read == 1) {
				val = 1;
			}
		}
		fclose(f);
		// iterate until directory name matches
		while (val != 0 && (dirIdx == -1)) {
			if (strcmp(dir, temp.dname) == 0) {
				dirIdx = index;
			}
			index+=1;
			FILE *f = fopen(".directories", "rb");
			if (f == NULL) {
				val = 0;
			}
			else {
				seek = fseek(f,sizeof(csc452_directory_entry)*index,SEEK_SET);
				if(seek == -1) { 
					val = 0;
				}
				read = fread(&temp,sizeof(csc452_directory_entry),1,f);
				if(read == 1) {
					val = 1;
				}
			}
			fclose(f);
		}
		// dir not found
		if(dirIdx == -1) {
			// name meets length requirement
			if(MAX_FILENAME < strlen(dir)) {
				return -ENAMETOOLONG;
			}
			else {
				memset(&temp,0,sizeof(struct csc452_directory_entry));
				strcpy(temp.dname, dir);
				temp.nFiles = 0;
				// write to directory
				FILE *f = fopen(".directories", "ab");
				fwrite(&temp,sizeof(csc452_directory_entry),1,f);
				fclose(f);
			}
		}
		// directory found elsewhere
		else {
			return -EEXIST; 
		}
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
	(void) mode;
    (void) dev;
	int dirIdx;
	// create char arrays to read in inputs
	char dir[MAX_FILENAME+1];
	char file[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];
	// fill with 0s
	memset(dir, 0, MAX_FILENAME+1);
	memset(file, 0, MAX_FILENAME+1);
	memset(ext, 0, MAX_EXTENSION+1);
	// insert input to char arrays 
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);

	// dir name given 
	if(dir != NULL) {
		dirIdx = -1;
		csc452_directory_entry temp;  // temp entry to find if dir exists
		int index = 0;
		int seek;
		int read;
		int val;
		FILE *f = fopen(".directories", "rb");
		if (f == NULL) {
			val = 0;
		}
		else {
			seek = fseek(f,sizeof(csc452_directory_entry)*dirIdx,SEEK_SET);
			if(seek == -1) { 
				val = 0;
			}
			read = fread(&temp, sizeof(csc452_directory_entry), 1, f);
			if(read == 1) {
				val = 1;
			}
		}
		fclose(f);
		// iterate until dir name matches given name
		while (val != 0 && (dirIdx == -1)) {
			if (strcmp(dir, temp.dname) == 0) {
				dirIdx = index;
			}
			index++;
			FILE *f = fopen(".directories", "rb");
			if (f == NULL) {
				val = 0;
			}
			else {
				seek = fseek(f, sizeof(csc452_directory_entry) * index, SEEK_SET);
				if(seek == -1) { 
					val = 0;
				}
				read = fread(&temp, sizeof(csc452_directory_entry), 1, f);
				if(read == 1) {
					val = 1;
				}
			}
			fclose(f);
		}
		// name does not meet length requirement 
		if(strlen(file) > MAX_FILENAME || strlen(ext) > MAX_EXTENSION) {
			return -ENAMETOOLONG;
		}
		else {
			csc452_directory_entry entry;
			FILE *f = fopen(".directories", "rb");
			// write to entry at disk index 
			if(f != NULL) {
				seek = fseek(f, sizeof(csc452_disk_block)*dirIdx,SEEK_SET);
				if(seek != -1) {
					fwrite(&entry,sizeof(csc452_disk_block),1,f);
				}
			}
			fclose(f);
			int i;
			int ret = -1;
			// iterate through files 
			for (i = 0; i < entry.nFiles; i++) {
				// check for filename match
				if (strcmp(entry.files[i].fname, file) == 0) {
					// NULL extension 
					if (ext[0] == '\0' && entry.files[i].fext[0] == '\0') { 
						ret = i;
					}
					// ext not NULL and it matches the one file
					else if (ext != NULL && strcmp(ext, entry.files[i].fext) == 0) { 
						ret = i;
					}
				}
			}
			// file DNE
			if(ret == -1) { 
				strcpy(entry.files[entry.nFiles].fname , file);
				if(strlen(ext) > 0) {
					strcpy(entry.files[entry.nFiles].fext , ext);
				}
				else {
					strcpy(entry.files[entry.nFiles].fext, "\0");
				}
				int blockIdx;
				int newBlockIdx;
				FILE *fi = fopen(".disk", "rb+");
				// .disk not found
				if(fi == NULL) {
					entry.files[entry.nFiles].nStartBlock = -1;
				}
				int seekReturn = fseek(fi, -sizeof(csc452_disk_block), SEEK_END);
				if(seekReturn == -1) {
					entry.files[entry.nFiles].nStartBlock = -1;
				}
				int readReturn = fread(&blockIdx, sizeof(int), 1, fi);
				if(readReturn == -1) {
					entry.files[entry.nFiles].nStartBlock = -1;
				}
				seekReturn = fseek(fi, -sizeof(csc452_disk_block), SEEK_END);
				if(seekReturn == -1) {
					entry.files[entry.nFiles].nStartBlock = -1;
				}
				newBlockIdx = blockIdx + 1;
				int writeReturn = fwrite(&newBlockIdx, 1, sizeof(int), fi);
				if(writeReturn == -1) {
					entry.files[entry.nFiles].nStartBlock = -1;
				}
				fclose(fi);

				entry.files[entry.nFiles].nStartBlock = blockIdx;
				entry.files[entry.nFiles].fsize = 0;
				entry.nFiles = entry.nFiles+1;
				FILE *f = fopen(".directories", "rb+");
				fseek(f,sizeof(csc452_directory_entry)*dirIdx,SEEK_SET);
				fwrite(&entry,sizeof(csc452_directory_entry),1,f);
				fclose(f);
			}
			else {
				return -EEXIST;
			}
		}
	}
	else {
		return-EPERM;
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

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//return success, or error
	// create objects to search directory and disk space
	csc452_directory_entry tempD;
	csc452_disk_block tempB;
	// initiate char arrays 
	char dir[MAX_FILENAME+1];
	char file[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];
	// copy of original offset to modify
	int sec_offset = offset; 
	int dirIdx;
	int fileIdx;
	
	// set values to 0
	memset(dir, 0, MAX_FILENAME+1);
	memset(file, 0, MAX_FILENAME+1);
	memset(ext, 0, MAX_EXTENSION+1);
	
	// insert input strings to char arrays
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	// outside of bounds
	if(size < offset) {
		return -1;
	}
	if (size <= 0) {
		return -1;
	}
	// directory was input	
	if(dir != NULL) {
		if(file == NULL) {
			return -EISDIR;
		}	
		if( MAX_FILENAME > strlen(file)) {
			// extension must be NULL otherwise must meet length requirements
			if(ext == NULL || ext[0] == '\0' || ((strlen(ext) <= MAX_EXTENSION)&&(ext != NULL && ext[0] != '\0'))) { 
				dirIdx = -1;
				csc452_directory_entry temp;  // temp dir to read 
				int index = 0;
				int seek;
				int read;
				int val;

				// search for index of directory
				FILE *f = fopen(".directories", "rb");
				if (f == NULL) {
					val = 0;
				}
				else {
					seek = fseek(f,sizeof(csc452_directory_entry)*dirIdx,SEEK_SET);
					if(seek == -1) { 
						val = 0;
					}
					read = fread(&temp,sizeof(csc452_directory_entry),1,f);
					if(read == 1) {
						val = 1;
					}
				}
				fclose(f);
				// iterate until name of directory was found 
				while (val != 0 && dirIdx == -1) {
					if (strcmp(temp.dname, dir) == 0) {
						dirIdx = index;
					}
					index+=1;
					FILE *f = fopen(".directories", "rb");
					if (f == NULL) {
						val = 0;
					}
					else {
						seek = fseek(f,sizeof(csc452_directory_entry)*index,SEEK_SET);
						if(seek == -1) { 
							val = 0;
						}
						read = fread(&temp,sizeof(csc452_directory_entry),1,f);
						if(read == 1) {
							val = 1;
						}
					}
					fclose(f);
				}
				// directory not found
				if(dirIdx == -1) {
					return -1;
				}
		
				FILE *fi = fopen(".directories", "rb");
				if (fi == NULL) {
					val = 0;
				}
				else {
					// use found index to read at address
					seek = fseek(fi,sizeof(csc452_directory_entry)*dirIdx,SEEK_SET);
					if(seek == -1) { 
						val = 0;
					}
					read = fread(&tempD,sizeof(csc452_directory_entry),1,fi);
					if(read == 1) {
						val = 1;
					}
				}
				fclose(fi);
				// if nothing found at index
				if(val == 0) {
					return -1;
				}
				fileIdx = -1;
				int i;
				// iterate through all files in directory
				for (i = 0; i < tempD.nFiles; i++) {
					if (strcmp(tempD.files[i].fname,file) == 0) {  // file match
						// if matches extension and not NULL
						if (ext != NULL && strcmp(tempD.files[i].fext, ext) == 0) {
							fileIdx = i;
						}
						// ext is NULL
						else if (ext[0] == '\0' && tempD.files[i].fext[0] == '\0') {
							fileIdx = i;
						}

					}
				}
				// file found
				if(fileIdx != -1) { 
					// is empty		
					if(tempD.files[fileIdx].fsize == 0) {
						return 0;
					}
					int blockIdx = tempD.files[fileIdx].nStartBlock;
					// while offset is not out of bounds
					while(size > offset){	
					
						FILE *f = fopen(".disk", "rb");
						if(f != NULL) {
							seek = fseek(f,sizeof(csc452_disk_block)*blockIdx, SEEK_SET);
							if(seek != -1) {
								fwrite(&tempB,sizeof(csc452_disk_block),1,f);
							}
						}
						fclose(f);
						
						if (MAX_DATA_IN_BLOCK < sec_offset) {
							blockIdx = tempB.nNextBlock;
							sec_offset -= MAX_DATA_IN_BLOCK;
							continue;
						}
						else {
							int bufferRet = 0;
							int rem = size-offset;
							while(rem > 0) {
								if(sec_offset > MAX_DATA_IN_BLOCK) {
									bufferRet = rem;
								}
								else {
									*buf = tempB.data[sec_offset];
									buf+=1;
									rem-=1;
									sec_offset+=1;
								}
							}
							bufferRet = rem;

							sec_offset = 0;
							if (bufferRet == 0) {
								break;
							}
							else {
								blockIdx = tempB.nNextBlock;
								offset += MAX_DATA_IN_BLOCK;
								buf += MAX_DATA_IN_BLOCK;
							}
						}
					}
					
					return size;                            
				}
				else {
					return -1; 
				}
			}
			else {
				return -1; 
			}
		}
		else {
			return -1; 
		}
	}
	else {
		return -1;
	}

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
	

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//return success, or error

	// temporary disk and block objs to iterate through 
	csc452_directory_entry tempD;
	csc452_disk_block tempB;
	// create char arrays 
	char dir[MAX_FILENAME+1];
	char file[MAX_FILENAME+1];
	char ext[MAX_EXTENSION+1];
	int sec_offset = offset; // Make a copy
	int dirIdx;
	int fileIdx;
	// set values to 0
	memset(dir, 0, MAX_FILENAME+1);
	memset(file, 0, MAX_FILENAME+1);
	memset(ext, 0, MAX_EXTENSION+1);

	// insert inputs into arrays 
	sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);
	memset(&tempB, 0, sizeof(csc452_disk_block)); 
	// out of bounds
	if(offset > size || size <= 0) { 
		return -1;
	}
	// directory was input 
	if(dir != NULL) {
		// file was input and valid 
		if(file != NULL && file[0] != '\0' && strlen(file) < MAX_FILENAME) {
			// extension was valid
			if(ext == NULL || ext[0] == '\0' || ((strlen(ext) <= MAX_EXTENSION) && (ext != NULL && ext[0] != '\0'))) {
				dirIdx = -1;
				csc452_directory_entry temp;  // temp directory to read
				int index = 0;
				int seek;
				int read;
				int val;
				FILE *f = fopen(".directories", "rb");
				if (f == NULL) {
					val = 0;
				}
				else {
					seek = fseek(f, sizeof(csc452_directory_entry) * dirIdx, SEEK_SET);
					if(seek == -1) { 
						val = 0;
					}
					read = fread(&temp, sizeof(csc452_directory_entry), 1, f);
					if(read == 1) {
						val = 1;
					}
				}
				fclose(f);
				// iterate to find directory index by matching names 
				while (dirIdx == -1 && val != 0) {
					if (strcmp(temp.dname, dir) == 0) {
						dirIdx = index;
					}		
					index+=1;
					FILE *f = fopen(".directories", "rb");
					if (f == NULL) {
						val = 0;
					}
					else {
						seek = fseek(f,sizeof(csc452_directory_entry)*index,SEEK_SET);
						if(seek == -1) { 
							val = 0;
						}
						read = fread(&temp,sizeof(csc452_directory_entry),1,f);
						if(read == 1) {
							val = 1;
						}
					}
					fclose(f);
				}
				// no index found
				if(dirIdx == -1) {
					return -1; 
				}
				// find if directory exists 
				FILE *fi = fopen(".directories", "rb");
				if (fi == NULL) {
					val = 0;
				}
				else {
					seek = fseek(fi,sizeof(csc452_directory_entry)*dirIdx,SEEK_SET);
					if(seek == -1) { 
						val = 0;
					}
					read = fread(&tempD,sizeof(csc452_directory_entry),1,fi);
					if(read == 1) {
						val = 1;
					}
				}
				fclose(fi);
				// could not read/find directory 
				if (val == 0) {
					return -1; 
				}
				fileIdx = -1;
				int i;
				// name matches 
				for (i = 0; i < tempD.nFiles; i++) {
					if (strcmp(tempD.files[i].fname, file) == 0) {
						if (ext != NULL && strcmp(tempD.files[i].fext, ext) == 0) {
							fileIdx = i;
						}
						else if (ext[0] == '\0' && tempD.files[i].fext[0] == '\0') { 
							fileIdx = i;
						}
			
					}
				}
				
				if(fileIdx != -1) 
				{
					int blockIdx = tempD.files[fileIdx].nStartBlock;
					tempD.files[fileIdx].fsize = size;
					FILE *f = fopen(".directories", "rb+");
					fseek(f,sizeof(csc452_directory_entry)*dirIdx,SEEK_SET);
					fwrite(&tempD,sizeof(csc452_directory_entry),1,f);
					fclose(f);
					// iterate until offset is maximum data size
					while(MAX_DATA_IN_BLOCK<=sec_offset) {
						blockIdx = tempB.nNextBlock;
						sec_offset -= MAX_DATA_IN_BLOCK;
						FILE *f = fopen(".disk", "rb");
						// write into block address
						if(f != NULL) {
							seek = fseek(f,sizeof(csc452_disk_block)*blockIdx,SEEK_SET);
							if(seek != -1) {
								fwrite(&tempB,sizeof(csc452_disk_block),1,f);
							}
						}
						fclose(f);
					}
					// iterate until offset is larger than size allotted
					while( size > offset) {
						// at max size 
						if (MAX_DATA_IN_BLOCK <sec_offset) {
							blockIdx = tempB.nNextBlock;
							sec_offset -= MAX_DATA_IN_BLOCK;
							continue;
						}
						else {
							// find buffer space needed until full block
							int bufferRet = 0;
							int rem = size-offset;
							while(rem > 0) {
								if(MAX_DATA_IN_BLOCK < sec_offset) {
									bufferRet = rem; // Return the amount of data left to write
								}
								else {
									tempB.data[sec_offset] = *buf;
									buf+=1; 
									rem-=1;
									sec_offset+=1;
								}
							}
							bufferRet = rem;
							// block not full
							if (bufferRet != 0 && tempB.nNextBlock <= 0) {
								int blockLoc;
								int newBlockIdx;
								
								FILE *fil = fopen(".disk", "rb+");
								// .disk not found 
								if(fi == NULL) {
									tempB.nNextBlock = -1;
								}
								int seekReturn = fseek(fil,-sizeof(csc452_disk_block),SEEK_END);
								if(seekReturn == -1) {
									tempB.nNextBlock = -1;
								}
								int readReturn = fread(&blockLoc,sizeof(int),1,fil);
								if(readReturn == -1) {
									tempB.nNextBlock = -1;
								}
								seekReturn = fseek(fil, -sizeof(csc452_disk_block), SEEK_END);
								if(seekReturn == -1) {
									tempB.nNextBlock = -1;
								}
								newBlockIdx = blockLoc + 1;
								int writeReturn = fwrite(&newBlockIdx, 1, sizeof(int), fil);
								if(writeReturn == -1) {
									tempB.nNextBlock = -1;
								}
								fclose(fil);
								tempB.nNextBlock = blockLoc;
							}									
							
							FILE *f = fopen(".disk", "rb+");
							// write to temp block 
							if(f != NULL) {
								seek = fseek(f,sizeof(csc452_disk_block)*blockIdx,SEEK_SET);
								if(seek != -1) {
									fwrite(&tempB,sizeof(csc452_disk_block),1,f);
								}
							}
							fclose(f);
							sec_offset = 0;
							// no space in block 
							if (bufferRet == 0) {
								break;
							}
							else {
								blockIdx = tempB.nNextBlock;
								offset = offset + MAX_DATA_IN_BLOCK;
								FILE *f = fopen(".disk", "rb");
								// .disk was found
								if (f != NULL) {
									seek = fseek(f,sizeof(csc452_disk_block)*blockIdx,SEEK_SET);
								
										fread(&tempB,sizeof(csc452_disk_block),1,f);
								}
								fclose(f);

								buf = buf + MAX_DATA_IN_BLOCK;
							}
						}
					}
					return size;                            
				}
				else {
					return -1;
				}
			}
			else {
				return -1;
			}
		}
		else {
			return -EISDIR; 
		}
	}
	else {
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

