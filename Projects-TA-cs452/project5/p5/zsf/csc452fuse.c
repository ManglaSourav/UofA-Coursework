/*
	Author: Part Zachary Florez 
	Course: CSC 452
	File: csc452fuse.c
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

struct csc452_alloc_table_block {
	short table[MAX_ENTRY]; 
}

typedef struct csc452_directory_entry csc452_directory_entry;
typedef struct csc452_alloc_table_block csc452_table_block; 

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE- sizeof(long))
#define MAX_ENTRY (BLOCK_SIZE/sizeof(short))
#define ALLOC_BLOCK 2 


static csc452_table_block read_block(void);
static csc452_root_directory read_root(void); 

/**
 * @brief Helper functions called in this file. 
 * 
 * @param root 
 */
static void get_root(csc452_root_directory *root) {

	// First get the frame pointer 
	FILE *pointer = fopen(".disk", "rb");

	// Now do the checks 
	if (pointer == NULL) {
		return;
	} else {
		fread(root, sizeof(csc452_root_directory), 1, pointer);	
	}

	// Close the file pointer always for best practice. 
	fclose(pointer);

}


/**
 * @brief Helper function called in this file to see if the directorty 
 * 			we're looking at actually exists or not.  
 * 
 * @param directory 
 * @return int 
 */
static int check_directory_exists(char *directory) {

	// Set the root then get the root. 
	csc452_root_directory root;
	get_root(&root);

	// Now iterate through until we actually find that a directory exists. 
	int i = 0;
	for (i = 0; i < root.nDirectories; i++) {
		if (strcmp(root.directories[i].dname, directory) == 0) {
			return i; 
		}
	}

	// If we get here then directory does not exist :(  
	return -1;
}

/**
 * @brief Helper functions called in this file. 
 * 
 * @param entry 
 * @param directory 
 * @return int 
 */
static int return_directory(csc452_directory_entry *entry, char *directory) {

	// First initialize the root. 
	csc452_root_directory root;
	get_root(&root);

	// Now make sure that said directory does in fact exist. 
	// If it doesn't then get out of here. 
	int order = check_directory_exists(directory); 
	if (order == -1) {
		return -1; 
	}

	// Otherwise open up your pointer
	FILE *pointer = fopen(".disk", "rb");
	if (pointer == NULL) { 
		return -1;
	}

	// Seek, Read, Close then return the order
	fseek(pointer, root.directories[order].nStartBlock, SEEK_SET);
	fread(entry, sizeof(csc452_directory_entry), 1, pointer);
	fclose(pointer);
	return order; 
}


/**
 * @brief Here we will write the root data to disk. 
 * 
 * @param root 
 */
static csc452_write_to_root(csc452_root_directory* root) {

	// Create FILE pointer, write then close. 
	FILE* disk = fopen(".disk", "r+b"); 
	fwrite(root, BLOCK_SIZE, 1, disk); 
	fclose(disk); 
}


/**
 * @brief Here we will write the block data to disk. 
 * 
 * @param root 
 */
static csc452_write_to_block(csc452_table_block* block) {

	// Create FILE pointer, seek, write, then close. 
	FILE* disk = fopen(".disk", "r+b");
	fseek(disk, BLOCK_SIZE, SEEK_SET);  
	fwrite(block, BLOCK_SIZE, 1, disk); 
	fclose(disk); 
}


/**
 * @brief Here will we read the file allocation table 
 * from disk. 
 * 
 * @return csc452_table_block 
 */
static csc452_table_block read_block() {

	// Create our file pointer 
	FILE* disk = fopen(".disk", "r+b"); 

	// Seek and read into struct 
	fseek(disk, BLOCK_SIZE, SEEK_SET); 
	csc452_table_block block; 
	fread(&block. BLOCK_SIZE, 1, disk); 

	// Return the struct
	return block; 

}

/**
 * @brief Here we will read directly from disk. 
 * 
 * @return csc452_root_directory 
 */
static csc452_root_directory read_root() {
	// Create a file pointer 
	FILE* disk = fopen(".disk", "r+b"); 

	// Seek and read into struct. 
	fseek(disk, 0, SEEK_SET); 
	csc452_root_directory root; 
	fread(&root, BLOCK_SIZE, 1, disk); 

	// Return that struct. 
	return root; 

} 

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

	// First thing that we want to do is create our path data 
	// that way we don't have to do as much work later on! 
	char directory[MAX_FILENAME+1]; 
	strcpy(directory, ""); 

	char filename[MAX_FILENAME+1]; 
	strcpy(filename, ""); 

	char extension[MAX_FILENAME]; 
	strcpy(extension, "");


	// Next we can clear the data inside the stbuf for safe measures. 
	memset(stbuf, 0, sizeof(struct stat)); 

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return res; 
	} else  {

		if (strcmp(directory, "") == 0) {
 
			struct csc452_file_directory directory; 
			strcpy(directory.fname, ""); 
			directory.nStartBlock = 1; 
			csc452_root_directory root = read_root();

			// Now we can loop through to find the subdirectory inside of the
			// array of directories. 
			int i = 0; 
			for (int i = 0; i < root.nDirectories; i++) {
				struct csc452_file_directory pointer = root.directories[i];

				// Check to see if they're equal
				if (strcmp(pointer.fname, directory) == 0) {
					directory = pointer; 
					break; 
				}
			} // END FOR LOOP 

			// Now outside here we can just check to see if the directory is still found. 
			if (strcmp("", directory.fname) == 0) {
				res = -ENONET; 
				return res; 
			}

			// Create disk and seek 
			FILE* disk = fopen(".disk", "r+b"); 
			int start = BLOCK_SIZE * directory.nStartBlock; 
			fseek(disk, start, SEEK_SET);

			// Once we're here we know the user was just looking to get the attributes of a 
			// specific directory. 
			if (strcmp("", filename) == 0) {
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;	
				res = 0; 
				return res; 
			}

			// Now we can memset the files 
			struct csc452_directory_entry entry;
			entry.nFiles = 0; 
			memset(entry.files, 0, MAX_FILES_IN_DIR * sizeof(struct csc452_file_directory)); 
			int read = fread(&entry, BLOCK_SIZE, 1, disk); 
			fclose(disk); 


			// Now that we are here what we want to do is check if read is 1, then 
			// we want to proceed. 
			if (read == 1) {
				struct csc452_file_directory file; 

				// Initialize sizes 
				file.nStartBlock = -1; 
				file.fsize = 0; 
				
				// Initialize the name and text 
				strcpy(file.fname, ""); 
				strcpy(file.fext, ""); 

				// Inside here we can now try and find the matching pair. 
				int i = 0; 
				for (i = 0; i < MAX_FILES_IN_DIR; i++) {
					struct csc452_file_directory pointer = entry.files[i]; 

					// compare to see if we get a match 
					if (strcmp(filename, pointer.fname) == 0) {
						if (strcmp(extension, pointer.fext) == 0) {
							file = pointer; 
							break; 
						}
					}
				} // END FOR 


				// Now that we're here we can check if there was no file found. 
				if ((file.nStartBlock) != -1) {
					res = 0; 
					stbuf->st_mode = S_IFREG | 0666; 
					stbuf->st_nlink = 1; 
					stbuf->st_size = file.fsize; 
					return res; 
				} else {

					// Otherwise we have our error 
					res = -ENOENT; 
					return res; 
				}

			} // END IF READ == 1

		}
		
		//Else return that path doesn't exist
		else if (strcmp(directory, "") == 0) { 
			res = -ENOENT;
			return res; 
		}

	} // END ELSE STATEMENT

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
	char* ext; 
	char* fName; 
	char* dest; 

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);

		// After that we can parse the path and throw them into our vars. 
		int length = strlen(path); 
		char path_copy[length]; 
		strcpy(path_copy, path); 
		ext = strtok(NULL, "."); 
		fName = strtok(NULL, "."); 
		dest = strtok(path_copy, "/"); 

		// Now we can iterate through
		csc452_root_directory rootPointer = read_root(); 
		int i = 0; 
		for(i = 0; i < MAX_DIRS_IN_ROOT; i++) {
			char* directory = rootPointer.directories[i].dname; 

			// Compare the two to see if we need to fill. 
			if (strcmp(directory, "") == 0) {
				filler(buf, directory, NULL, 0); 
			}
		}

		// Return successfully :) 
		return 0; 

	}
	else {
		// All we have _right now_ is root (/), so any other path must
		// not exist. 
		return -ENOENT;
	}

	return 0;
}


/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{

	char* directory; 
	char* sub_directory; 


	(void) path;
	(void) mode;
	
	// Here we are going to parse the input strings (directory and sub_directory) 
	int length = strlen(path);
	char copy[length]; 
	strcpy(copy, path); 
	sub_directory = strtok(NULL, "/"); 
	directory = strtok(copy, "/"); 


	// Here we check to make sure 2nd level is a file only
	if (sub_directory && sub_directory[0]) {
		return -EPERM;
	} 

	// Directory is too long. 
	else if (strlen(directory) > MAX_FILENAME) { 
		return -ENAMETOOLONG; 
	}

	csc452_root_directory root = read_root(); 
	csc452_disk_block block = read_block(); 

	// Now we can check if we have a max number of directories before 
	// we try to add another one. 
	if (MAX_DIRS_IN_ROOT <= root.nDirectories) {
		return -EEXIST; 
	}


	// Iterate through all the directories to check if user is trying to create 
	// a directory that is already there. 
	int i = 0; 
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {

		if (strcmp(root.directories[i].dname, directory) == 0) {
			return -EEXIST; 
		} 
	}

	// Now we can iterate through the directories and 
	// Create that new directory
	for(i = 0; i < MAX_DIRS_IN_ROOT; i++) { 

		// Check if folder has a name 
		if(strcmp(root.directories[i].dname, "") == 0) { 

			struct csc452_directory new_dir_in_root;
			strcpy(new_dir_in_root.dname, directory); 
			
			// Here we can iterate over the table because what we want to do 
			// is find a new block to create and store the directory inside. 
			int j = 0;
			for(j = 2; j < MAX_ENTRY; j++){ 
				if(block.table[j] == 0){ 
					block.table[j] = EOF; 
					new_dir_in_root.nStartBlock = j;
					break;
				}
			}

			// Now we create our new file pointer and seek. 
			// Initialize the directory to have zero files 
			// inside of the new directory. 
			FILE* disk = fopen(".disk", "r+b");
			int location = new_dir_in_root.nStartBlock * BLOCK_SIZE ; 
			fseek(disk, location, SEEK_SET);
			csc452_directory_entry directory;
			directory.nFiles = 0; 
			int items = fread(&directory, BLOCK_SIZE, 1, disk);
			
			// If items is one that means that we sucessfully were able to return 
			// with one item! 
			if(items == 1){ 
				// If that's the case then we clear the data and then write the new data in :) 
				memset(&directory, 0, sizeof(struct cs1550_directory_entry)); 
				fwrite(&directory, BLOCK_SIZE, 1, disk); 
				fclose(disk);

				// Here we can update root
				root.nDirectories++;
				root.directories[i] = new_dir_in_root;				

				// Write to the root and then the table
				csc452_write_to_root(&root);
				csc452_write_to_block(&block);
			} else{ 

				// Close if we get an error at all. 
				fclose(disk);
			}
			return 0;
		} // END (IF STRCMP)

	} // END (FOR LOOP)

	return 0;
}


/*
 * TODO: Implement 
 *
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

	// First things first is we are going to create our main three 
	// variables (directory, filename and then the extension for that filename)
	char directory[MAX_FILENAME+1]; 
	strcpy(directory, ""); 

	char filename[MAX_FILENAME+1]; 
	strcpy(filename, ""); 

	char extension[MAX_FILENAME]; 
	strcpy(extension, "");

	// Use sscanf to get the actual values into place. 
	int breakdown = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// Check to make sure file isn't trying to be created in the root directory 
	if (breakdown != 3) {
		return -EPERM; 
	}

	// Too long name 
	if (strlen(extension) > 3 || strlen(filename) > 8) {
		return -ENAMETOOLONG; 
	}

	// Now we can try to add a new file to a subdirectory. 
	csc452_root_directory root = read_root();
	csc452_table_block = read_block();
	struct csc452_directory direct;

	// iterate through to find the next right dir. 
	int i = 0; 
	for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
		struct csc452_directory curr = root.directories[i]; 

		// Compare the current and the directory 
		if (strcmp(curr.dname, direct) == 0) {
			direct = curr; 
			break; 
		}
	} // END FOR I = 0

	// Now that we're here we can check if we found a good directory. 
	if (strcmp(direct.dname, "") == 0) {

		// First things first we can read the directory right from disk. 
		long directory_address = BLOCK_SIZE * direct.nStartBlock; 
		FILE* disk = fopen(".disk", "r+b"); 
		fseek(disk, directory_address, SEEK_SET); 
		csc452_directory_entry direct_entry;
		int status = fread(&direct_entry, BLOCK_SIZE, 1, disk); 

		// Check to see if there are too many files to create already. 
		if (MAX_FILES_IN_DIR <= direct_entry.nFiles) {
			return -EPERM; 
		}

		// If we get here we can successfully add another file onto this directory. 
		if (status == 1) {
			int free_index = -1; 
			int exists = 0; 

			for (i = 0; i < MAX_FILES_IN_DIR; i++) {
				struct csc452_file_directory pointer = direct_entry.files[i]; 

				// Set free index if these conditions are met.  
				if (strcmp(pointer.fext, "") == 0 && free_index == -1 && strcmp(pointer.fname, "") == 0 ) {
					free_index = i; 
				}

				// Set exists to 1 if these conditions are met
				if (strcmp(pointer.fext, extension) == 0 && (pointer.fname, filename) == 0) {
					exists = 1; 
				}
			}
		} else { 
			fclose(disk); 
			return -EPERM; 
		}



	} else {
		if (strcmp(filename, "") == 0) { 
			return -EPERM; 
		} else if (strcmp(direct, "") == 0) {
			return 0; 
		}
	}

	
	return 0;
}


/*
 *
 * Read size bytes from file into buf starting from offset
 * 
 * Return: size on success
 * -EFBIG if the offset is beyond the file size 
 * -ENOSPC if the disk is full 
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	// Create our first three always. And set those apart. 
	char directory[MAX_FILENAME + 1]; 
	char fileName[MAX_FILENAME + 1]; 
	char extension[MAX_FILENAME + 1]; 
	int setter = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 

	// First we check to see that the path exists. 
	if (setter == 1) {
		if (!directory) {
			return -EISDIR; 
		}
	}

	// Now we check to see that size is > 0 
	if (size <= 0) {
		return size; 
	}

	csc452_directory_entry entry; 
	return_directory(&entry, directory); 

	// Now that we're here we can iterate through until we
	// find the right filename and extension 
	int i = 0; 
	for (i = 0; i < entry.nFiles; i++) {
		if (strcmp(extension, entry.files[i].ftext) == 0) {
			if (strcmp(fileName, entry.files[i].fname) == 0) {
				break; 
			}
		}
	}

	//Now we are able to read in data 
	csc452_disk_block block; 
	FILE* pointer = fopen(".disk", "r+b"); 
	fseek(pointer, entry.files[i].nStartBlock, SEEK_SET); 
	fread(&block, sizeof(csc452_disk_block), 1, pointer); 

	int buffer_pointer = 0; 
	size_t leftovers = size;
	int file_block_leftover = MAX_DATA_IN_BLOCK - offset; 

	// Iterate through after reading every block. 
	while (leftovers > 0) {
		if (file_block_leftover == 0) {
			offset = 0; 
			feesk(pointer, block.nNextBlock, SEEK_SET); 
			fread(&block, sizeof(csc452_disk_block), 1, pointer); 
			file_block_leftover = MAX_DATA_IN_BLOCK; 
		}

		// Subtract and Add to ur values 
		buffer_pointer ++; 
		offset ++; 
		file_block_leftover --; 
		leftovers --; 
		buf[buffer_pointer] = block.data[offset]; 
	} 


	//return success, or error
	fclose(pointer); 
	return size;
}


/*
 *
 * Write size bytes from buf into file starting from offset
 * 
 * Return: size on success. 
 * 			-EFBIG if the offset is beyond the file size; 
 * 			- ENOSPC if the disk is full. 
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	// Create three variables like always. 
	char fileName[MAX_FILENAME + 1]; 
	char directory[MAX_FILENAME + 1]; 
	char extension[MAX_FILENAME + 1]; 

	//check to make sure path exists
	int setter = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
	if (setter == 1) {
		if (!directory) {
			return -EISDIR; 
		}
	}

	// Now we check to see that size is > 0 
	if (size <= 0) {
		return size; 
	}

	// Now we can get started with writing our data. 
	csc452_directory_entry entry; 
	int order = return_directory(&entry, directory); 

	// iterate through to get to the loction we want to be at. 
	int i = 0; 
	for (i = 0; i < entry.nFlies; i ++) {
		if (strcmp(extension, entry.files[i].ftext) == 0) {
			if (strcmp(fileName, entry.files[i].fname) == 0 {
				break; 
			}
		}
	}

	// Create our file pointer for the disk and our block. 
	csc452_disk_block block; 
	FILE* pointer = fopen(".disk", "r+b"); 
	long curr_address = entry.files[i].nStartBlock; 

	// Seek and Read where we want to be.
	fseek(pointer, curr_address, SEEK, SET); 
	fread(&block, sizeof(csc452_disk_block), 1, pointer); 

	// When we get here we need to find where we want to start writing at. 
	int startPointer = offset; 
	while (MAX_DATA_IN_BLOCK < startPointer) {
		curr_address = block.nNextBlock; 
		
		// Seek and Read
		fseek(pointer, curr_address, SEEK_SET); 
		fread(&block, sizeof(csc452_disk_block), 1, pointer); 
		startPointer = startPointer - MAX_DATA_IN_BLOCK;
	}

	//
	// Almost there, we just now need to see what we have left to write 
	//
	int block_leftovers = MAX_DATA_IN_BLOCK - startPointer; 
	size_t leftovers = size; 
	int buffer_index = 0; 

	while (0 < leftovers ) {

		// First check if we have any block leftovers 
		if (block_leftovers == 0) {

			// Here we have reached the end of the file and have to 
			// Just create new blocks. 
			if (block.nNextBlock == 0) {
				long temp;
				fread(&temp, sizeof(long), 1, pointer); 
				long address = temp + BLOCK_SIZE; 

				// Out of disk space :(
				if (address == -1) {
					return -EFBIG; 
				}

				block.nNextBlock = address; 
			} // END IF BLOCK.NNEXTBLOCK  

			// Now we can seek, write, and read
			fseek(pointer, curr_address, SEEK_SET); 
			fwrite(&block, sizeof(csc452_disk_block), 1, pointer); 
			fseek(pointer, block.nNextBlock, SEEK_SET); 
			fread(&block, sizeof(csc452_disk_block), 1, pointer); 

			// Update values for next iteration 
			curr_address = block.nNextBlock; 
			startPointer = 0; 
			block_leftovers = MAX_DATA_IN_BLOCK; 
		} 

		// Update all of our variables
		block_leftovers --; 
		startPointer ++; 
		block.data[startPointer] = buf[buffer_index]; 
		buffer_index ++; 
		leftovers --; 
	}


	// return success, or error
	fseek(pointer, curr_address, SEEK_SET); 
	fwrite(&block, sizeof(csc452_disk_block), 1, fp); 
	fclose(pointer); 
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
 * 
 * Removes a file. 
 *
 */
static int csc452_unlink(const char *path)
{
        (void) path;
        return 0;
}



//
//
//
//
//
/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/
//
//
//
//

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
