/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/

/* File: csc452fuse.c
 * Author: Michael Tuohy
 * Class: CSc 452
 * Project: Project 
 * Description: This file is our syscall implementations for a FUSE system. FUSE,
 * or Filespace in User Environment, allow us to create mount points in our normal
 * linux filesystems in order to implement our own file system without breaking the
 * existing linux filesystem.  
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

// This will be set by init_disk, and will be used as the offset from SEEK_END
#define BITMAP_OFFSET (BLOCK_SIZE * -3)
#define BITMAP_SIZE 1280

/*
 * This method will check to see if the disk has been written to or not. It does this by
 * opening the .disk file, seeking to the bitmap location, then checking the very first
 * bit of the bitmap. If that bit is set, it means that the .disk file was initalized previously
 * and we can return 0 for success. Else, we return 1 and the calling method will call init_disk.
 * Return values: 0 == Disk already initialized
 *				  1 == Disk needs to be initialized
 * 				 -1 == Disk read error
 */
int is_disk_initialized() {
	FILE *disk;
	unsigned char bitmap[1280];

	disk = fopen(".disk", "rb");
	if(disk == NULL) {
		printf("Opening of .disk file failed, return -2\n");
		return -1;
	}
	
	if(fseek(disk, BITMAP_OFFSET, SEEK_END) != 0) {
		printf("Seeking to bitmap location failed, terminating\n");
		return -1;
	}

	if(fread(bitmap, sizeof(unsigned char), 1280, disk) != 1280) {
		printf("Reading of bitmap off of disk failed, terminating\n");
		return -1;
	}
	
	// At this point, we have successfully read the bitmap off of disk and are now
	// able to check if the bitmap has been set successfully

	// Close the FILE* because we no longer need it

	fclose(disk);

	// This should be the bit that represents our root directory. If this is set, 
	// return 0, else 1
	
	if((*bitmap & 0x80) != 0x80) {
		
		return 1;
	}

	

	return 0;
}


/*
 * This method initializes the .disk file with a new root directory and bitmap for
 * tracking the free space on disk. This should only be called if the .disk file is fresh 
  *and has no root directory on it, i.e. it was corrputed and needs to be recreated
 */
int init_disk (){
	
	unsigned char *bitmap;
	csc452_root_directory *root;
	FILE *disk;
	unsigned char *bitmap_offset_loc;
	int error_value;
	// We start by initializing the root


	// Check to see if the bitmap has been updated on disk. If error_value == 0,
	// the disk was set previously and we can return safely. If error_value == -1,
	// something went wrong. If error_value == 1, then we need to perform the initialization

	error_value = is_disk_initialized();
	
	if(error_value == 0) {
		return 0;
	} else if(error_value == -1) {
		return -1;
	}

	root = (csc452_root_directory *) malloc(sizeof(csc452_root_directory));
	if(root == NULL) {
		printf("Allocation of root directory failed, terminating\n");
		return -1;
	}

	// Note: this line below is used for debug purposes
	//root->nDirectories = 37;

	disk = fopen(".disk", "rb+");
	if(disk == NULL) {
		printf(".disk file not found, terminating\n");
		return -1;
	}

	// Now we assign the root to the very first block of the .disk
	rewind(disk);
	if(fwrite(root, sizeof(root), 1, disk) != 1) {
		printf("Writing of root directory to .disk failed, terminating\n");
		return -1;
	}

	// At this point we can free the root data structure as we no longer need it

	free(root);

	// Now we need to create the bitmap and assign it to a location on disk. Since
	// we know our disk to be of 5MB in size total and our block size is 512 bytes,
	// we can calculate the number of bits in our bitmap to be 5MB / 512B = 10240 bits
	// to store our bitmap. 10240 bits / 8 bits = 1280 Bytes of storage. Since our minimum
	// block size is 512, this means we need 3 blocks total to store our bitmap

	bitmap = (unsigned char *) malloc(sizeof(unsigned char) * 1280);

	// Now we have to update our mappings to show what blocks have been taken thus far. We
	// will have the bitmap take up the last three blocks of the disk, so we will mark those as
	// taken as well

	*bitmap = 0x80;
	
	bitmap_offset_loc = bitmap + 1279;
	*bitmap_offset_loc = 0x03;

	// Now we can write the bitmap to disk. As stated above, we are using the very last 3 blocks
	// of our disk in order to store the bitmap. To do so, we will fseek from the beginning of the 
	// disk to that location on the bitmap * 512B

	if(fseek(disk, BITMAP_OFFSET, SEEK_END) != 0) {
		printf("Seeking to bitmap locaiton failed, terminating\n");
		return -1;
	}

	if(fwrite(bitmap, sizeof(unsigned char), BITMAP_SIZE, disk) != 1280) {
		printf("Writing of bitmap to disk failed, terminating\n");
		return -1;
	}
	

	// With the bitmap successfully written to disk, we can now free the bitmap safely

	free(bitmap);

	// If at this point, the disk should be initialized, and should be good to go

	fclose(disk);

	return 0;
	
}	

/*
 * Here we will have some helper methods for doing the file I/O
 */

/*
 * Given an input path, this method parses out the directory, file name,
 * and extension and fills those pointers with their values. If those
 * values are missing from the path, then those pointers remain
 * unfilled. Returns the number of arguments that were filled.
 * Return values: 0 == path given is "/", dir, file_name and ext not found
 *				  1 == directory found, file_name and ext not found
 *				  2 == directory and file_name found, ext not found
 *				  3 == directory, file_name and ext found
 *				 -1 == path given contains more items than is allowed by our file system
 */
int parse_path(const char *path, char *dir, char *file_name, char *ext) {
	char *token, path_copy[30];
	int i;

	if(strcmp(path, "/") == 0) {
		strcpy(dir, "");
		strcpy(file_name, "");
		strcpy(ext, "");
		return 0;
	}

	strcpy(path_copy, path);

	i = 0;
	token = strtok(path_copy, "/.");
	while(token != NULL) {
		if(i == 0) {
			strcpy(dir, token);
		} else if(i == 1) {
			strcpy(file_name, token);
		} else if(i == 2) {
			strcpy(ext, token);
		} else {
			printf("File given is too long, return -1\n");
			return -1;
		}
		i++;
		token = strtok(NULL, "/.");
	}
	return i;
}

/*
 * This method takes a string representing a potential directory name
 * determines if it is a directory, and returns a negative number if
 * the directory name passed is not a directory, else it returns
 * the directory location in the root 
 * Return Values: Any positive number == potential_dir is a directory 
 *				 -1 == Error Value
 *				 -2 == Directory name not found
 */
int is_directory(const char *potential_dir) {
	FILE *disk;
	csc452_root_directory root;
	int i;
	//char temp_file_name[9];

	disk = fopen(".disk", "rb");
	if(disk == NULL) {
		printf("Error reading disk image\n");
		return -1;
	}
	
	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Error reading root directory\n");
		fclose(disk);
		return -1;
	}

	/*
	// For now, just see if root is storing the number of directories safely
	if(root.nDirectories != 37) {
		printf("Root numDirectories failed\n");
		return -1;
	}
	*/


	// Check every single directory name in the root, return 0 if directory is
	// found
	for(i = 0; i < root.nDirectories; i++) {
		/*
		printf("Now comparing potential_dir with the directories inside of root\n");
		printf("Potential Directory: %s\n");
		*/

		if(strcmp(root.directories[i].dname, potential_dir) == 0) {
			fclose(disk);	
			return i;
		}
	}


	fclose(disk);
	return -2;
}

/*
 * This method takes a directory, file, and extension as input, and determines
 * if the file exists in the given directory with the given extension.
 * Return values: Any positive integer == file location in csc452_directory_entry
 * 				 -1 == Error Value
 * 				 -2 == File at path location does not exist
 *				 -3 == Path is a directory
 */
int is_file(const char *path) {
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir_entry;
	char dir_name[25], file_name[25], file_ext[25];
	int dir_index, file_index, i;

	// Check things in order

	i = parse_path(path, dir_name, file_name, file_ext);

	if(i == 1) {
		return -3;	
	}
	if(i < 1) {
		return -2;
	}

	dir_index = is_directory(dir_name);
	if(dir_index < 0) {
		return -2;
	}

	// At this point, we know that the directory exists at this location, so we should now
	// open up the root and navigate to the directory location on disk

	disk = fopen(".disk", "rb");
	if(disk == NULL) { return -1; }

	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) { return -1;}
	
	if(fseek(disk, (root.directories[dir_index].nStartBlock * BLOCK_SIZE), SEEK_SET) != 0) {return -1;}
	
	// We can now read the csc452_directory_entry
	if(fread(&dir_entry, sizeof(csc452_directory_entry), 1, disk) != 1) {return -1;}
	
	// Now we need to grab the file name and the extension, and determine if a file exists in the
	// directory that matches BOTH of those values

	for(file_index = 0; file_index < dir_entry.nFiles; file_index++) {
		if((strcmp(file_name, dir_entry.files[file_index].fname) == 0) &&
		   (strcmp(file_ext, dir_entry.files[file_index].fext) == 0)) {
				// We have found a file that matches the file name and extension,
				// so now we can do all of the cleanup that we need and exit safely

				fclose(disk);
				return file_index;
		}
	}

	// If at this point, we've looked at every file in the directory and can now
	// safely return -2
	
	fclose(disk);
	return -2;
}

/*
 * This method looks at the bitmap stored on disk, determines (if any) blocks are free, then marks
 * that bit as taken and returns the block location on disk.
 * Return values: Positive integer representing the block number that is "free" for the 
 * 				  data we want to store
 *				  -1 - Error in reading values
 *				  -2 - No more space on disk
 */
long find_free_block() {
	FILE *disk;
	unsigned char bitmap[BITMAP_SIZE], c, *bitmap_curr;
	int i, j;
	unsigned long output;
	disk = fopen(".disk", "rb+");
	if(disk == NULL) {
		printf("Error reading disk\n");
		return -1;
	}
	if(fseek(disk, BITMAP_OFFSET, SEEK_END) != 0) {
		printf("Error seeking to bitmap location\n");
		return -1;
	}
	if(fread(bitmap, sizeof(unsigned char), BITMAP_SIZE, disk) != 1280) {
		printf("Error reading bitmap\n");
		return -1;
	}

	// If at this point, we should be safe to seek a free block on disk 

	for(i = 0; i < BITMAP_SIZE; i++) {

		bitmap_curr = bitmap + i;
		// Here, we search every character in the bitmap for a character 
		// that is not "full". We do this asking if the character is equal to 0xf
		if(*bitmap_curr != 0xff) {
	 		// character with a free block found, now we need to determine
			// which block is actually free
			for(j = 0; j < 8; j++) {

				c = *bitmap_curr;
				c = c << j;
				if((c & 0x80) == 0x00) {
					// This means that we found a block that is free. We now know 
					// it's location on disk, and can stop searching for another
					// block. 

					output = (i*8) + j;

					// TODO: Mark that bit as taken
					c = 0x80;
					c = c >> j;
					*bitmap_curr = *bitmap_curr | c;


					// Bit is marked in bitmap, now time to write out the bitmap to disk

					if(fseek(disk, BITMAP_OFFSET, SEEK_END) != 0) {
						printf("Error seeking to bitmap location for updating bit\n");
						return -1;
					}
					
					// Rewrite bitmap to disk 
					if(fwrite(bitmap, sizeof(unsigned char), 1280, disk) != 1280) {
						printf("Rewriting of bitmap to disk failed\n");
						return -1;
					} 
					

					// If at this point, we are now ready to do our clean up and return 
					fclose(disk);

					printf("Block location found for new block: %lu\n", output);
					return output;
				}
			}
		}
	}
	

	// If we reached this point, then there is no longer any space on disk
	fclose(disk);
	return -2;
}

/*
 * This method takes a block location and will update the bitmap to mark that
 * block as free
 */
long free_block(long block_loc) {
	FILE *disk;
	unsigned char bitmap[BITMAP_SIZE], *bitmap_curr, c;
	int bitmap_index, bitmap_bit_loc;


	disk = fopen(".disk", "rb+");
	if(disk == NULL) {
		printf("Reading of disk failed\n");
		return -1;
	}

	if(fseek(disk, BITMAP_OFFSET, SEEK_END) != 0) {
		printf("Seeking to bitmap failed\n");
		return -1;
	}

	if(fread(bitmap, sizeof(unsigned char), BITMAP_SIZE, disk) != 1280) {
		printf("Reading of bitmap failed\n");
		return -1;
	}

	// To actually get to the index of the bitmap we want, we need to
	// translate the block location to a bitmap index
	bitmap_index = block_loc / 8;
	bitmap_bit_loc = block_loc % 8;


	bitmap_curr = bitmap + bitmap_index;
	c = *bitmap_curr;
	

	c = c << bitmap_bit_loc;


	if((c & 0x80) != 0x80) {
		printf("Bitmap index not set correctly with find_free_block\n");
		return -1;
	}
	c = 0x80;
	c = c >> bitmap_bit_loc;
	c = ~c;
	*bitmap_curr = *bitmap_curr & c;

	// Bitmap should be updated, so now we have to write it to disk
	if(fseek(disk, BITMAP_OFFSET, SEEK_END) != 0) {
		printf("Seeking to bitmap on rewrite failed\n");
		return -1;
	}
	if(fwrite(bitmap, sizeof(unsigned char), BITMAP_SIZE, disk) != 1280) {
		printf("Rewriting of bitmap failed\n");
		return -1;
	}

	// Rewriting to disk is complete, we can now cleanup and return
	fclose(disk);
	return 0;
	
}

/*
 * This method is to be used with csc452_getattr for determining
 * the file size of the file given an index into the directories in root
 * and the index of the file in that directory. The return value is 
 * the size of the size_t in the directory entry or a negative number
 * in error.
 */
int read_file_size(int dir_index, int file_index) {
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir;
	long next_loc;

	disk = fopen(".disk", "rb");
	if(disk == NULL) {
		return -1;
	}

	// Now, we read in the root

	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {	return -1; }
	
	// Next, extract the location of the directory on disk into next_loc

	next_loc = BLOCK_SIZE * root.directories[dir_index].nStartBlock;
	fseek(disk, next_loc, SEEK_SET);

	// Now we're ready to read in the directory entry

	if(fread(&dir, sizeof(csc452_directory_entry), 1, disk) != 1) { 
		printf("Reading of directory entry faile\n");
		return -1;
	}

	// We have successfully read in the directory at this point, so we
 	// are now allowed to close the disk file and return the size safely

	fclose(disk);
	return dir.files[file_index].fsize;
	
			
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	
	if(init_disk() != 0) {
		printf("Disk initialization failed, terminating\n");
	}
	int res = 0;
	char directory[25], filename[25], extension[25];
	int dir_index, file_index;

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		
		parse_path(path, directory, filename, extension);
		
		/*
		printf("\n\nPath: %s\n", path);
		printf("Directory: %s\n", directory);
		printf("Filename: %s\n", filename);
		printf("Extension: %s\n\n", extension);
		*/
		dir_index = is_directory(directory);
		// printf("Getting here\n");

		if(dir_index >= 0) {
			// Now we know that the directory is valid, but we need to determine
			// if the file also exists within the directory
			file_index = is_file(path);

			if(file_index == -3) {
				// Return is_directory stuff

				//If the path does exist and is a directory
				//stbuf->st_mode = S_IFDIR | 0755;
				//stbu->st_nlink = 2

				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				return 0;			

			} else if(file_index >= 0) {
				//If the path does exist and is a file:
				//stbuf->st_mode = S_IFREG | 0666;
				//stbuf->st_nlink = 2;
				//stbuf->st_size = file size

				// Now that we know that given the directory and file_name that this
				// is in fact a file, we can now safely access the directory entry
				// to get the file size		

				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 2;
				stbuf->st_size = read_file_size(dir_index, file_index);
				return 0;
			} 
	
		} 


		//If the path does exist and is a file:
		//stbuf->st_mode = S_IFREG | 0666;
		//stbuf->st_nlink = 2;
		//stbuf->st_size = file size
		
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
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir_entry;
	char dir_name[25];
	char file_name[25];
	char file_ext[25];
	int i;	

	if(init_disk() != 0) {
		printf("Disk initialization failed, terminating\n");
		return -1;
	}
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		
		// If there are any directories in the root (which there should be)
		// we need to add those to the buffer as well
		disk = fopen(".disk", "rb");
		if(disk == NULL) {
			printf("Error opening disk\n");
			return -1;
		}
		rewind(disk);
		if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
			printf("Error opening root dir\n");
			return -1;
		}

		// We can now safely copy the directories into the buffer, and close the disk file
		fclose(disk);
		printf("I'm assuming that we are getting here\n");
		printf("Number of directories in root: %d\n", root.nDirectories);
		for(i = 0; i < root.nDirectories; i++) {
			printf("How many times is this running\n");
			filler(buf, root.directories[i].dname, NULL, 0);
		}
	}
	else {
		
		// TODO: Add all files to the buffer
		
		// First we have to determine if the directory that we were given on the
		// path is a directory or not

		i = parse_path(path, dir_name, file_name, file_ext);
		if(i != 1) {
			printf("Error in parsing path %d\n", i);
			return -1;
		}

		// First, determine if the directory that we were given was good or not

		int dir_index = is_directory(dir_name);
		if(dir_index < 0) {
			// Directory given on the path was bad, we can return our error value
			return -ENOENT;
		}	

		// Now that we know that the directory is good, we need to go through the disk and
		// add every single file that is inside of the directory to it

		disk = fopen(".disk", "rb");
		if(disk == NULL) { return -1; }

		rewind(disk);
		if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
			printf("Error reading root dir\n");
			return -1;
		}

		long dir_loc = BLOCK_SIZE * root.directories[dir_index].nStartBlock;

		if(fseek(disk, dir_loc, SEEK_SET) != 0) { return -1; }
		if(fread(&dir_entry, sizeof(csc452_directory_entry), 1, disk) != 1) {
			printf("Reading of directory failed\n");
			return -1;
		}

		// Now we have successfully read in the target directory, and may start filling it

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		char full_file_name[30];

		for(i = 0; i < dir_entry.nFiles; i++) {
			strcpy(full_file_name, dir_entry.files[i].fname);
			strcat(full_file_name, ".");
			strcat(full_file_name, dir_entry.files[i].fext);
			filler(buf, full_file_name, NULL, 0);
		}

		fclose(disk);	

		// All we have _right now_ is root (/), so any other path must
		// not exist. 
	}
	printf("Getting here?\n");
	return 0;
}

/*
 * Helper method for creating a directory and assigning it to the root.
 * All error checking is done inside of the actual syscall implementation,
 * all this does is try to create the directory and store it on disk.
 * Return values: 0 - Successfully created directory
 * 				 -1 - Error value
 */
int mkdir_helper(char *dir_name) {
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry *new_dir;
	long dir_loc;

	dir_loc = find_free_block();
	if(dir_loc == -2) {
		printf("Disk full\n");
		return -1;
	} else if(dir_loc == -1) {
		printf("Error finding block on disk\n");
		return -1;
	}

	// We want to find a block on disk to save to first as we need
	// to open the FILE* in find_free_block first

	disk = fopen(".disk", "rb+");
	if(disk == NULL) {
		printf("Reading of disk failed\n");
		return -1;
	}
	
	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Error reading root directory\n");
		return -1;
	}
	
	// Now, we are going to create a directory entry in our root

	

	strcpy(root.directories[root.nDirectories].dname, dir_name);
	root.directories[root.nDirectories].nStartBlock = dir_loc;


	root.nDirectories = root.nDirectories + 1;

	// Now we can save the root to disk
	if(fseek(disk, 0, SEEK_SET) != 0) {
		printf("Error seeking to beginning of disk\n");
	}
	if(fwrite(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Error of rewriting root to disk\n");
		return -1;
	}
	
	// Now we can create the directory entry that we're actually going to save to disk

	new_dir = (csc452_directory_entry *) malloc(sizeof(csc452_directory_entry));
	if(new_dir == NULL) {
		printf("Error in allocation of new directory\n");
		return -1;
	}
	
	// This is only for debug purposes
	//new_dir->nFiles = 34;

	if(fseek(disk, dir_loc * BLOCK_SIZE, SEEK_SET) != 0) {
		printf("Seeking to new directory location on disk failed\n");
		return -1;
	}/*
	if(fwrite(new_dir, sizeof(csc452_directory_entry), 1, disk) != 1) {
		printf("Error in writing new directory to disk\n");
		return -1;
	}*/

	// If at this point, everything has been written to disk, and we are safe to free new_dir and
	// close the disk file
	free(new_dir);
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
	int err_val;
	char directory[25], filename[25];

	if(init_disk() != 0) {
		printf("Disk initialization failed, terminating\n");
		return -1;
	}

	sscanf(path, "/%[^/]/", directory);

	// First we check to see if the directory name is valid or not
	if(strlen(directory) > 8) {
		return -ENAMETOOLONG;
	}


	err_val = is_directory(directory);
	

	if(err_val == -1) {
		printf("Reading of directories failed, terminating\n");
		return -1;
	} else if(err_val >= 0) {
		// This means that the directory already exists under root, so
		// we need to determine if the user is attempting to create a new directory
		// w/ the same name or if they are attempting to make a subdirectory
		// We can do this by checking to see if the filename is specified on the path
		sscanf(path, "/%[^/]/%[^.]", directory, filename);
		
		// If strlen == 0, then the user is trying to create a subdirectory to a
		// directory, which is not what we are implementing in this project
		if(strlen(filename) == 0) {
			return -EPERM;
		} else {
			// This means that the user is trying to make a directory that already exists
			// in our root, so we return an error value
			return -EEXIST;
		}
	
	}
	
	// If at this point, then we are good to create a directory and assign it to our root

	// TODO: Add code to create a directory, then assign it to a location on disk
	
	err_val = mkdir_helper(directory);

	return 0;
}

int mknod_helper(const char *path, int dir_index) {
	char dir[25], file_name[25], ext[25];
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir_entry;
	csc452_disk_block *entry_disk_block;
	long dir_loc, file_loc;

	parse_path(path, dir, file_name, ext);
	
	disk = fopen(".disk", "rb+");
	if(disk == NULL) { return -1; }
	
	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) { 
		printf("Error reading root in mknod\n");
		return -1; 
	}
	dir_loc = BLOCK_SIZE * root.directories[dir_index].nStartBlock;
	if(fseek(disk, dir_loc, SEEK_SET) != 0) { 
		printf("Error seeking to directory location on disk\n");
		return -1; 
	}
	if(fread(&dir_entry, sizeof(csc452_directory_entry), 1, disk) != 1) { 
		printf("Error reading directory entry off disk\n");
		return -1; 	
	}
	
	// We need to find a block on disk that is available
	file_loc = find_free_block();
	if(file_loc < 0) {
		printf("Location on disk not found\n");
		return -1;
	}
	
	if((dir_entry.nFiles + 1) == MAX_FILES_IN_DIR) { 
		printf("Disk is full\n");
		return -2; 
	}
	
	// If at this point, we are now ready to fill out the file
	// information in the directory entry
	strcpy(dir_entry.files[dir_entry.nFiles].fname, file_name);
	strcpy(dir_entry.files[dir_entry.nFiles].fext, ext);
	dir_entry.files[dir_entry.nFiles].fsize = 0; // This needs to be 0 right now
	dir_entry.files[dir_entry.nFiles].nStartBlock = file_loc;
	dir_entry.nFiles = dir_entry.nFiles + 1;

	// Write dir_entry out to file
	if(fseek(disk, dir_loc, SEEK_SET) != 0) { 
		printf("Error reseeking to directory location\n");
		return -1; 
	}
	if(fwrite(&dir_entry, sizeof(csc452_directory_entry), 1, disk) != 1) { 
		printf("Error writing directory entry to disk\n");
		return -1; 
	}
	
	// Lastly, we will create the actual entry block and save it to disk
	entry_disk_block = (csc452_disk_block *) malloc(sizeof(csc452_disk_block));
	if(entry_disk_block == NULL) { 
		printf("Error in creation of entry disk block\n");
		return -1; 
	}
	entry_disk_block->nNextBlock = 0; // Make sure to change this so we don't forget later
	

	// Now we write the disk block out to disk
	if(fseek(disk, file_loc * BLOCK_SIZE, SEEK_SET) != 0) { 
		printf("Error in seeking to beginnging of disk block\n");
		return -1; 
	}
	if(fwrite(entry_disk_block, sizeof(csc452_disk_block), 1, disk) != 1) { 
		printf("Error in writing entry disk block to disk\n");
		return -1; 
	}

	// At this point, we can now cleanup and return
	free(entry_disk_block);
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
	char dir[25], file_name[25], ext[25];
	int dir_index, file_index;

	if(strcmp(path, "/") == 0) {
		return -EPERM;
	}
	parse_path(path, dir, file_name, ext);
	
	if((strlen(file_name) > 8) || strlen(ext) > 3) {
		return -ENAMETOOLONG;
	}

	dir_index = is_directory(dir);
	if(is_directory < 0) {
		return -EPERM;
	}
	
	file_index = is_file(path);
	if(file_index >= 0) {
		return -EEXIST;
	}

	// At this point, we should be able to safely create a file, which 
	// we will do inside of a helper method
	return mknod_helper(path, dir_index);
}

int read_block(csc452_disk_block *block, char *buf, off_t offset, size_t size) {
	size_t read_size = 0;
	char *start;
	start = block->data + offset;
	if((offset + size) >= MAX_DATA_IN_BLOCK) {
		read_size = MAX_DATA_IN_BLOCK - size;
	} else {
		read_size = size;
	}
	memcpy(buf, start, read_size);
	return read_size;
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
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir;
	csc452_disk_block curr_block;
	int dir_index, file_index, file_size, err_val;
	char directory[25], file_name[25], ext[25];
	//check to make sure path exists
	parse_path(path, directory, file_name, ext);
	
	dir_index = is_directory(directory);
	if(dir_index < 0) {
		printf("Directory not found\n");
		return -1;
	}
	file_index = is_file(path);
	if(file_index == -3) {
		return -EISDIR;
	}
	if(file_index < 0) {
		printf("File with given name and extension not found\n");
		return -1;
	}
	//check that size is > 0
	file_size = read_file_size(dir_index, file_index);
	if(file_size < 0) { return -1; }

	//check that offset is <= to the file size
	if(offset > file_size) {
		printf("Offset too large for size of file\n");
		return -1;
	}
	//read in data
	disk = fopen(".disk", "rb+");
	if(disk == NULL) {
		printf("Reading of disk failed\n");
		return -1;
	}
	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Reading of root failed\n");
		return -1;
	}
	
	long dir_location = root.directories[dir_index].nStartBlock * BLOCK_SIZE;
	if(fseek(disk, dir_location, SEEK_SET) != 0) { return -1; }
	if(fread(&dir, sizeof(csc452_directory_entry), 1, disk) != 1) {
		printf("Reading of directory entry failed\n");
		return -1;
	}
	
	long curr_block_loc;
	// off_t effective_offset = offset;
	// size_t remaining_bytes = size;

	curr_block_loc = dir.files[file_index].nStartBlock * BLOCK_SIZE;

	if(fseek(disk, curr_block_loc, SEEK_SET) != 0) { return -1; }
	if(fread(&curr_block, sizeof(csc452_disk_block), 1, disk) != 1) {
		printf("Reading of disk block failed\n");
		return -1;
	}

	// Like with write, we're just going to read what's on the first block for now,
	// then attempt to read the other blocks in the linked list
	
	err_val = read_block(&curr_block, buf, offset, file_size);
	if(err_val < 0) {
		printf("Reading of block failed\n");
		return -1;
	}
	//return success, or error
	fclose(disk);
	return file_size;
}

/*
 * This method takes a disk block and copies the contents of buffer to it, starting
 * at offset. At the end of the method, block is ready to be written to disk
 * Returns the number of bytes written to this block
 */
int write_block(csc452_disk_block *block, const char *buf, off_t offset, size_t size) {
	size_t written_size = 0;
	char *start;
	start = block->data + offset;
	if((offset + size) >= MAX_DATA_IN_BLOCK) {
		written_size = MAX_DATA_IN_BLOCK - size;
	} else {
		written_size = size;
	}
	memcpy(start, buf, written_size);
	return written_size;
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
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir;
	csc452_disk_block current_block;
	int dir_index, file_index, file_size, err_val;
	char directory[25], file_name[25], ext[25];
	//check to make sure path exists
	parse_path(path, directory, file_name, ext);

	dir_index = is_directory(directory);
	if(dir_index < 0) { 
		printf("Directory not found\n");
		return -1;
	}
	file_index = is_file(path);
	if(file_index < 0) {
		printf("File with given name and extension not found\n");
		return -1;
	}
	//check that size is > 0
	if(size <= 0) {
		return size;
	}
	//check that offset is <= to the file size
	file_size = read_file_size(dir_index, file_index);
	if(file_size < 0) { return -1; }
	if(offset > file_size) {
		return -EFBIG;
	}

	//write data
	disk = fopen(".disk", "rb+");
	if(disk == NULL) {
		printf("Reading of disk failed\n");
		return -1;
	}
	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Reading of root failed\n");
		return -1;
	}
	
	long dir_location = root.directories[dir_index].nStartBlock * BLOCK_SIZE;
	if(fseek(disk, dir_location, SEEK_SET) != 0) { return -1; }
	if(fread(&dir, sizeof(csc452_directory_entry), 1, disk) != 1) {
		printf("Reading of directory entry failed\n");
		return -1;
	}

	// If at this point, we can calculate the size of the modified file
	if((offset + size) > file_size) {
		dir.files[file_index].fsize = offset + size;
	} // Otherwise, the write is not modifying beyond the size of the file, so we don't care
	
	long curr_block_loc;
	
	//off_t effective_offset = offset;
	//size_t remaining_bytes = size;

	curr_block_loc = dir.files[file_index].nStartBlock * BLOCK_SIZE;
	
	if(fseek(disk, curr_block_loc, SEEK_SET) != 0) { return -1; }
	if(fread(&current_block, sizeof(csc452_disk_block), 1, disk) != 1) {
		printf("Reading of disk block failed\n");
		return -1;
	}

	// For now, we're just going to copy in to the very first block for testing purposes
	err_val = write_block(&current_block, buf, offset, size);
	if(err_val < 0) {
		printf("Writing to block failed\n");
		return -1;
	}


	// We also need to update the file size on disk, so we'll do that now

	if(fseek(disk, dir_location, SEEK_SET) != 0) { return -1; }
	if(fwrite(&dir, sizeof(csc452_directory_entry), 1, disk) != 1) {
		printf("Updating file size failed\n");
		return -1;
	}

	// Now we can write the current block to disk and exit safely

	if(fseek(disk, curr_block_loc, SEEK_SET) != 0) { return -1; }
	if(fwrite(&current_block, sizeof(csc452_disk_block), 1, disk) != 1) {
		printf("Writing of block to disk failed\n");
		return -1;
	}

	// Now the hard part. We need to go to the offset by navigating through a linked list to the actual
	// location in the file
/*
	while(effective_offset > MAX_DATA_IN_BLOCK) {
		curr_block_loc = current_block.nNextBlock * BLOCK_SIZE;
		if(fseek(disk, curr_block_loc, SEEK_SET) != 0) { return -1 }
		if(fread(&current_block, sizeof(csc452_disk_block), 1, disk) != 1) {
			printf("Reading of disk block failed\n");
		}
		effective_offset = effective_offset - MAX_DATA_IN_BLOCK;
	}
*/	 

	//return success, or error
	fclose(disk);
	return size;
}

/*
 * This method is a helper method for csc452_rmdir. It should only ever
 * be called by csc452_rmdir, and actually performs a removal of the directory
 * at the given index, as well as update the bitmap to show that the block
 * the directory was in is no longer available 
 */
int rmdir_helper(int dir_index) {
	FILE *disk;
	csc452_root_directory root;
	int dir_loc, i;	

	disk = fopen(".disk", "rb+");
	if(disk == NULL) { printf("Error opening disk\n"); return -1; }

	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Error reading root\n");
		return -1;
	}
	
	// Store this for use later on
	dir_loc = root.directories[dir_index].nStartBlock;

	for(i = dir_index; i < root.nDirectories - 1; i++) {
		strcpy(root.directories[i].dname, root.directories[i+1].dname);
		root.directories[i].nStartBlock = root.directories[i+1].nStartBlock;
	}
	
	// Clear out the directory information left over in the last entry

	strcpy(root.directories[root.nDirectories - 1].dname, "");
	root.directories[root.nDirectories - 1].nStartBlock = 0;
	
	root.nDirectories = root.nDirectories - 1;

	// Now we can write the root to disk

	rewind(disk);
	if(fwrite(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Rewriting of root to disk failed\n");
		return -1;
	}

	// Cleanup
	fclose(disk);

	// Now we call free_block on the block location that we stored earlier

	return free_block(dir_loc);

	// The way that our root works, we will have to move every directory
	// entry that follows the directory that we are moving over one
	//
	return 0;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir_entry;
	int err_val, dir_index;
	long dir_loc;
	char dir_name[25], file_name[25], ext[25];
	// To begin with, let's parse the path to see if the path we were given is correct
	err_val = parse_path(path, dir_name, file_name, ext);
	if(err_val == 0) {
		return -1;
	} else if(err_val > 1) {
		return -ENOTDIR;
	}

	dir_index = is_directory(dir_name);
	if(dir_index < 0) {
		// Directory was not found, safe to return -ENOTDIR
		return -ENOENT;
	}

	// At this point, we now know the directory exists, but we need to check if it is empty

	disk = fopen(".disk", "rb");
	if(disk == NULL) {
		return -1;
	}
	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) { return -1; }
	dir_loc = BLOCK_SIZE * root.directories[dir_index].nStartBlock;
	if(fseek(disk, dir_loc, SEEK_SET) != 0) { return -1; }
	if(fread(&dir_entry, sizeof(csc452_directory_entry), 1, disk) != 1) { return -1; }
	
	if(dir_entry.nFiles != 0) {
		return -ENOTEMPTY;
	}

	// At this point, we are safe to delete the directory from the system.  

	fclose(disk);
	rmdir_helper(dir_index);

	return 0;	
}

int delete_file_from_bitmap(long block_start) {
	FILE *disk;
	csc452_disk_block current_block;
	long block_to_free_address = block_start * BLOCK_SIZE;
	long next_block_to_free = block_start;
	

	while(next_block_to_free != 0) {
		free_block(next_block_to_free);
		disk = fopen(".disk", "rb+");
		if(disk == NULL) { 
			return -1; 
		}
		if(fseek(disk, block_to_free_address, SEEK_SET) != 0) { 
			return -1; 
		}
		if(fread(&current_block, sizeof(csc452_disk_block), 1, disk) != 1) {
			printf("Reading of block failed\n");
			return -1;
		}
		next_block_to_free = current_block.nNextBlock;
		block_to_free_address = next_block_to_free * BLOCK_SIZE;
		current_block.nNextBlock = 0;

		if(fseek(disk, block_to_free_address, SEEK_SET) != 0) { 
			return -1; 
		}
		if(fwrite(&current_block, sizeof(csc452_disk_block), 1, disk) != 1) {
			printf("Writing of updated block failed\n");
			return -1;
		}
		fclose(disk);
	}

	// If at this point, the file should be successfully removed from the disk

	return 0;
}

/*
 * This method updates the directory entry to remove the file at the
 * given dir_index and file_index
 */
int unlink_helper(int dir_index, int file_index) {
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir;
	int i;
	long dir_loc;

	disk = fopen(".disk", "rb+");
	if(disk == NULL) { 
		return -1; 
	}
	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Error reading root\n");
		return -1;
	}

	dir_loc = root.directories[dir_index].nStartBlock * BLOCK_SIZE;
	if(fseek(disk, dir_loc, SEEK_SET) != 0) { 
		return -1; 
	}
	if(fread(&dir, sizeof(csc452_directory_entry), 1, disk) != 1) {
		printf("Reading of directory entry failed\n");
		return -1;
	}

	long file_start_block = dir.files[file_index].nStartBlock;

	for(i = file_index; i < dir.nFiles - 1; i++) {
		strcpy(dir.files[i].fname, dir.files[i+1].fname);
		strcpy(dir.files[i].fext, dir.files[i+1].fext);
		dir.files[i].fsize = dir.files[i+1].fsize;
		dir.files[i].nStartBlock = dir.files[i+1].nStartBlock;
	}
	
	// Clear out the info left over in the last entry
	strcpy(dir.files[dir.nFiles - 1].fname, "");
	strcpy(dir.files[dir.nFiles - 1].fext, "");
	dir.files[dir.nFiles - 1].fsize = 0;
	dir.files[dir.nFiles - 1].nStartBlock = 0;

	dir.nFiles = dir.nFiles - 1;

	// Now we can write out the directory entry to disk
	if(fseek(disk, dir_loc, SEEK_SET) != 0) { return -1; }
	if(fwrite(&dir, sizeof(csc452_directory_entry), 1, disk) != 1) {
		printf("Reading of directory entry failed\n");
		return -1;
	}
	fclose(disk);
	return delete_file_from_bitmap(file_start_block);
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path) {
	(void) path;
	FILE *disk;
	csc452_root_directory root;
	csc452_directory_entry dir_entry;
	int dir_index, file_index;
	char dir_name[30], file_name[30], file_ext[30];
	
	parse_path(path, dir_name, file_name, file_ext);
	dir_index = is_directory(dir_name);
	if(dir_index < 0) {
		printf("Directory not found\n");
		return -1;
	}
	file_index = is_file(path);
	if(file_index == -3) {
		return -EISDIR;
	} else if(file_index < 0) {
		return -ENOENT;
	}

	// If at this point, we are now ready to unlink a file from the system

	disk = fopen(".disk", "rb+");
	if(disk == NULL) { return -1; }
	if(fread(&root, sizeof(csc452_root_directory), 1, disk) != 1) {
		printf("Error reading root\n");
		return -1;
	}
	long dir_loc = root.directories[dir_index].nStartBlock * BLOCK_SIZE;

	if(fseek(disk, dir_loc, SEEK_SET) != 0) { return -1; }
	if(fread(&dir_entry, sizeof(csc452_directory_entry), 1, disk) != 1) {
		printf("Error reading directory entry\n");
		return -1;
	}

	// At this point, we are safe to remove the file from the disk
	fclose(disk);
	unlink_helper(dir_index, file_index);
	

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
