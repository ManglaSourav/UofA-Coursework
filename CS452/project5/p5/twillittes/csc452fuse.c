/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452

	Taylor Willittes
	Project 5
	How to run: gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452
	Purpose: creates file system in user space. can create directories and files and can also write to files using echo or nano.
	This is done with the syscalls outlined in the spec.
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
#include <sys/stat.h>
#include <unistd.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

int freevar = 1;
#define DISK_SIZE 5242880
#define MAX_BLOCKS_IN_DISK (DISK_SIZE / BLOCK_SIZE)

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

//given
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
csc452_root_directory r; //root dir
	memset(stbuf, 0, sizeof(struct stat));
	//if statement is given
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		//printf("HERE");
		char directory[MAX_FILENAME + 1] = {0};
		char filename[MAX_FILENAME + 1] = {0};
		char extension[MAX_EXTENSION + 1] = {0};
		int dir2 = 0;
		//int one =
		int fi = 0;
		int isdir = 0;
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); //read absolute path
		
		FILE *disk = fopen(".disk", "rb+"); //open file
		fread(&r, BLOCK_SIZE, 1, disk); //read disk
	    
		for (int i  = 0; i < r.nDirectories; i++) { //goes through root dir
			struct csc452_directory cur = r.directories[i];
			//printf("cur %s\n", cur.dname);
			if(strcmp(directory, cur.dname) == 0) { //enter if dir exists
				dir2 = 1; //set var to 1
				if(strlen(filename) == 0) { //path has filename
					isdir = 1;
					break;
				}
				struct csc452_directory_entry dirE;
				//fseek(disk, )
				fseek(disk, cur.nStartBlock * BLOCK_SIZE, SEEK_SET);
				fread(&dirE, BLOCK_SIZE, 1, disk); //read the entry
				/*
				for(int j = 0; j < dirE.nFiles; j++) { //iterates
					struct csc452_file_directory fileN = dirE.files[j];
					if(strcmp(filename, fileN.fname) {
						if (strcmp(extension, fileN.fext) == 0) {
				*/
				for(int j = 0; j < dirE.nFiles; j++) { //iterates
	struct csc452_file_directory fileN = (struct csc452_file_directory) dirE.files[j];
					//printf("here %s\n", fileN.fname);
if(strcmp(filename, fileN.fname) == 0 && strcmp(extension, fileN.fext) == 0) { //check name & extension of correct file
					//printf("HERE\n");
						fi = 1;
						stbuf->st_size = fileN.fsize;
						break; //extis
					}
				}
			}
		}	
		
		//If the path does exist and is a directory:
		//stbuf->st_mode = S_IFDIR | 0755;
		//stbuf->st_nlink = 2;

		//If the path does exist and is a file:
		//stbuf->st_mode = S_IFREG | 0666;
		//stbuf->st_nlink = 2;
		//stbuf->st_size = file size
		
		//Else return that path doesn't exist
		//res = -ENOENT;

		if(dir2 && isdir) { ///If the path does exist and is a directory:
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		} else if (dir2 && !isdir && fi) { //If the path does exist and is a file:
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			//stbuf->st_size = file size
		} else { //Else return that path doesn't exist
			res = -ENOENT; //file not found
		}
		fclose(disk); //close
		

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

struct csc452_root_directory r; //root
	csc452_directory_entry entry; //entry of dir
	struct csc452_directory dir;

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0 || *path == '/') {
		//filler is given
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		char directory[MAX_FILENAME + 1] = {0}; //gets dir
		char filename[MAX_FILENAME + 1] = {0};
		char extension[MAX_EXTENSION + 1] = {0}; //get extention
		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); //from spec yuh
		FILE *disk = fopen(".disk", "rb");
		//struct csc452_root_directory root; //root
		fread(&r, BLOCK_SIZE, 1, disk);
		int pres = 0;
		//struct csc452_directory dir;
		int i = 0;
		for(i = 0; i < r.nDirectories; i++) { //loops through dir in root
			dir = r.directories[i];
			//printf("HERE\n");
			//printf("dir %s\n", dir);
			//printf("dir %s\n", dir.dname);
			if(strcmp(dir.dname, directory) == 0 || strlen(directory) == 0) {
				if(strlen(directory) == 0) {
					//printf("HERE2");
					filler(buf, dir.dname, NULL, 0); //lists contents
				} else {
					//printf("HERE3");
					pres = 1; //dir exists
					break;
				}
			}
		}
		if(pres) { //if dir exists then we need to loop through files
		//fseek(disk, r.directories[r.nDirectories].nStartBlock * BLOCK_SIZE, SEEK_SET);
			fseek(disk, dir.nStartBlock * BLOCK_SIZE, SEEK_SET);
			fread(&entry, BLOCK_SIZE, 1, disk); //reads it
			for(i = 0; i < entry.nFiles; i++) {
				//printf("ore");
			char full[MAX_FILENAME + MAX_EXTENSION + 2] = { 0 }; //full file name
				strcpy(full, entry.files[i].fname); //puts name into buf
				if(strlen(entry.files[i].fext) > 0) {
					//printf("EF");
					strcat(full, "."); //file extention needs a .
				}
				strcat(full, entry.files[i].fext);
				filler(buf, full, NULL, 0); //list contents
			}
		}
		fclose(disk);

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
	//voids are given to us
	(void) path;
	(void) mode;

struct csc452_root_directory r;
	struct csc452_directory_entry entry;


	char directory[MAX_FILENAME + 2] = {0}; //fill dir
	char filename[MAX_FILENAME + 2] = {0}; //fill filename
	char extension[MAX_EXTENSION + 2] = {0}; //fill extention
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	if(strlen(directory) > MAX_FILENAME) {
		return -ENAMETOOLONG; //return
	}
	if(strlen(filename) > 0) {
		return -EPERM; //return
	}
	struct stat s;
	int ret = csc452_getattr(path, &s); //gets input path

	if(ret == -ENOENT && s.st_mode == (S_IFDIR | 0755)){
		return -EEXIST;
	} else {
		FILE *disk = fopen(".disk", "rb+"); //open new directory
		fread(&r, BLOCK_SIZE, 1, disk); //read root


		if(r.nDirectories < MAX_DIRS_IN_ROOT && freevar < MAX_BLOCKS_IN_DISK) {
			strcpy(r.directories[r.nDirectories].dname, directory); //creates dir

			r.directories[r.nDirectories].nStartBlock = freevar;
			freevar++; //increment glob
			//fseek(&entry, 0, sizeof(struct csc452_directory_entry));
		memset(&entry, 0, sizeof(struct csc452_directory_entry)); //create new dir
	fseek(disk, r.directories[r.nDirectories].nStartBlock * BLOCK_SIZE, SEEK_SET); //find start
			fwrite(&entry, BLOCK_SIZE, 1, disk); //writes
			r.nDirectories++; //increments
		} else {
			return -ENOSPC;
		}
		//r.nDirectories++;
		fseek(disk, 0, SEEK_SET);
		fwrite(&r, BLOCK_SIZE, 1, disk);
		fclose(disk); //close
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
	//voids given
	(void) path;
	(void) mode;
    (void) dev;
	
	char directory[MAX_FILENAME + 1] = {0}; //get dir
	char filename[MAX_FILENAME + 2] = {0};
	char ext[MAX_EXTENSION + 2] = {0}; //get extention

	struct csc452_root_directory r;
		struct csc452_directory dir;
		struct csc452_directory_entry entry; //dile dir
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, ext); //scan path
	struct stat s;
	int ret = csc452_getattr(path, &s); //uses func we made to check if path is valid

	if (ret == 0 && strlen(filename) > 0) {
		return -EEXIST; //file existo
	} else if (strcmp(path, "/") == 0 || strlen(filename) == 0) {
		return -EPERM; //if file is created in root
	} else if (strlen(filename) > MAX_FILENAME || strlen(ext) > MAX_EXTENSION) {
		return -ENAMETOOLONG; //name beyond 8.3 chars
	} else {
		FILE *disk = fopen(".disk", "rb+"); //open sesame

		fread(&r, BLOCK_SIZE, 1, disk); //reads
		for (int i = 0; i < r.nDirectories; i++) { //programatically loops
			dir = r.directories[i];
			//printf("entr %s\n", dir);
			if (strcmp(dir.dname, directory) == 0) { //name and dir are samesies
				break;
			}
		}
		
		fseek(disk, BLOCK_SIZE * dir.nStartBlock, SEEK_SET); //seek
		//fseek(disk, BLOCK_SIZE * dir.nStartBlock, SEEK_SET);
		fread(&entry, BLOCK_SIZE, 1, disk); //read

	if (entry.nFiles >= MAX_FILES_IN_DIR || freevar >= MAX_BLOCKS_IN_DISK) { //too many files
			return -ENOSPC;
		} else {
			strcpy(entry.files[entry.nFiles].fname, filename); //set filename
		//programatically copies
		strcpy(entry.files[entry.nFiles].fext, ext); //set extension
		entry.files[entry.nFiles].fsize = 0; //empty

			entry.files[entry.nFiles].nStartBlock = freevar; //index set
			freevar++; //increment
			entry.nFiles++;

			fseek(disk, BLOCK_SIZE * dir.nStartBlock, SEEK_SET);
			fwrite(&entry, BLOCK_SIZE, 1, disk); //writes new file
		}
		fclose(disk);
		return 0;
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
	//voids given
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;


//same code as the other mehtods above
	char directory[MAX_FILENAME + 1] = { 0 };
	char filename[MAX_FILENAME + 1] = { 0 };
	char ext[MAX_EXTENSION + 1] = { 0 };

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, ext); //scan

	struct csc452_directory dir;
				struct csc452_directory_entry entry;
				struct csc452_disk_block block;
struct stat s;
	int ret = csc452_getattr(path, &s); //check to make sure path exists
	if(ret != -ENOENT) { //path is breathing & existing
		
		if(s.st_mode == (S_IFDIR | 0755)) { //if path is dir
			return -EISDIR;
		}

		if(size > 0) { //check that size is > 0
			if(offset <= s.st_size) { //check that offset is <= to the file size
			struct csc452_root_directory root;
				FILE *disk = fopen(".disk", "rb");
				
				fread(&root, BLOCK_SIZE, 1, disk); //read in data
				//fre
				for(int i = 0; i < root.nDirectories; i++) { //loops through root
					dir = root.directories[i];
					//printf("entry %s\n", dir);
				if(strcmp(dir.dname, directory) == 0) {
				fseek(disk, dir.nStartBlock * BLOCK_SIZE, SEEK_SET); //find file
				//fseek(disk, entry.files[j].nStartBlock * BLOCK_SIZE, SEEK_SET);
					fread(&entry, BLOCK_SIZE, 1, disk);
					for(int j = 0; j < entry.nFiles; j++) { //loops
					//printf("BER");
	if(strcmp(entry.files[j].fname, filename) == 0 && strcmp(entry.files[j].fext, ext) == 0) {
	fseek(disk, entry.files[j].nStartBlock * BLOCK_SIZE, SEEK_SET); //find disk block & load
				fread(&block, BLOCK_SIZE, 1, disk);
				break;
				}
					}
					}
				}
				memset(buf, 0, size);//buff goes to 0
				int bufIdx = 0;
				int fIdx = offset;
//memcpy
				while(fIdx < s.st_size && bufIdx < size) { //loop
					buf[bufIdx] = block.data[fIdx]; 
					bufIdx++;
					fIdx++;
				}

				fclose(disk);

				return bufIdx;	
			}
		}
	} else {
		//printf("patho no existo. read func\n");	
		return -EBADF;
	}
	
	
	
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
	//given
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

struct csc452_root_directory r; //root
	char directory[MAX_FILENAME + 1] = { 0 }; //dir
	char filename[MAX_FILENAME + 1] = { 0 };

struct csc452_directory dir;
				struct csc452_directory_entry entry; //entry of fir
				struct csc452_disk_block block;
	char ext[MAX_EXTENSION + 1] = { 0 }; //extention
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, ext);
	struct stat s;
	int ret = csc452_getattr(path, &s); //check to make sure path exists
	
	//if (ret != -ENOENT && size > 0 && offset <= s.st_size) {
	if(ret != -ENOENT) {
		//if (size > 0 && offset <= s.st_size) {
		if(size > 0) {//check that size is > 0
			if(offset <= s.st_size) { //check that offset is <= to the file size
				FILE *disk = fopen(".disk", "rb+"); //open .disk
				
				fread(&r, BLOCK_SIZE, 1, disk); //read
		for(int i = 0; i < r.nDirectories; i++) { //loops throuh dir in roooot
					
					dir = r.directories[i];
					//printf("entry %s\n", dir);
					if(strcmp(dir.dname, directory) == 0) {
						//printf("HERE \n");
					fseek(disk, dir.nStartBlock * BLOCK_SIZE, SEEK_SET);
					
					fread(&entry, BLOCK_SIZE, 1, disk);
					int j = 0;
				for(j = 0; j < entry.nFiles; j++) { //loops throuuugh enities
							//printf("HERE2 \n");
	if(strcmp(entry.files[j].fname, filename) == 0 && strcmp(entry.files[j].fext, ext) == 0) {
			fseek(disk, entry.files[j].nStartBlock * BLOCK_SIZE, SEEK_SET); //seek
			fread(&block, BLOCK_SIZE, 1, disk); //read
					break;
					}
					}
					strcpy(&block.data[offset], buf);
					//memset(
					entry.files[j].fsize = size - offset; //size change

					fseek(disk, dir.nStartBlock * BLOCK_SIZE, SEEK_SET);
					fwrite(&entry, BLOCK_SIZE, 1, disk); //write data
					//fwrite(&block, BLOCK_SIZE, 1, disk);
				fseek(disk, entry.files[j].nStartBlock * BLOCK_SIZE, SEEK_SET);
					fwrite(&block, BLOCK_SIZE, 1, disk); //write to disk

						fclose(disk);
					}
				}
			} else {
				return -EFBIG;
			}
		}
	} else {
		//printf("path no existo. write func\n"); //path no existo
	return -EBADF;
	}
	
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
	  
	  
	char directory[MAX_FILENAME + 1] = {0};
	char extention[MAX_FILENAME + 1] = {0};
	char filename[MAX_FILENAME + 1] ={0};
	if (strcmp(path, "/") == 0) {
		return -EPERM;
	} else {
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extention); //given
	struct stat s;
	int ret = csc452_getattr(path, &s);
	if (ret == -ENOENT) {
		return -ENOENT;
	}
	if (S_ISDIR(s.st_mode)) { //is a dir yuh
		//delete dir
		//printf("HERE\n");
		csc452_directory_entry entry;
		csc452_root_directory r;
		FILE *disk = fopen(".disk", "rb+");
		fread(&r, BLOCK_SIZE, 1, disk);

		for (int i = 0; i < r.nDirectories; i++){ //loops
			
			struct csc452_directory cur = r.directories[i];
			//printf("HERE2 %s\n", cur.dname);

			if (strcmp(directory, cur.dname) == 0) {
				//delete
				fseek(disk, cur.nStartBlock * BLOCK_SIZE, SEEK_SET);
				fread(&entry, BLOCK_SIZE, 1, disk);

				if (!(entry.nFiles > 0)) {
					//int one = i;
					//int two = ;
				memset(&r.directories[i], 0, sizeof(struct csc452_directory));

					//printf("R\n");
					for (int j = i; j < r.nDirectories - 1; j++) {
					r.directories[j] = r.directories[j+1]; //move them down

					}

					r.nDirectories --;
					fseek(disk, 0, SEEK_SET);
					fwrite(&r, BLOCK_SIZE, 1, disk); //write to disj
						fclose(disk);
					//break;
					return 0;
				} else {

				return -ENOTEMPTY;
				}

			}

		}
	} else {
		return -ENOTDIR;
	}
	}
	//fclose(disk);
	  return 0;
}

/*
 * Removes a file.
 *
 */
/*static int csc452_unlink(const char *path)
{
	(void) path;
	return 0;
}*/
static int csc452_unlink(const char *path)
{
        (void) path;

	char directory[MAX_FILENAME + 1] ={0};
	char filename[MAX_FILENAME + 1] = {0};
	char extension[MAX_FILENAME + 1] = {0};
	
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	struct stat s;
	int ret = csc452_getattr(path, &s);
	if (ret == -ENOENT) {
		return -ENOENT;
	}

	if (S_ISREG(s.st_mode)) {
		printf("HERE\n");
		struct csc452_root_directory r;
		struct csc452_directory d;
		struct csc452_directory_entry entry;
		FILE *disk = fopen(".disk", "rb+");
		fread(&r, BLOCK_SIZE, 1, disk);

		for (int i = 0; i < r.nDirectories; i++) { //loops through dir
			d = r.directories[i];
			//printf("d %s\n", d.dname);

			if (strcmp(d.dname, directory) == 0) { //compares directory
				fseek(disk, d.nStartBlock * BLOCK_SIZE, SEEK_SET);
				fread(&entry, BLOCK_SIZE, 1, disk);
			for (int k = 0; k < entry.nFiles; k++) {
				//printf("entry %s\n", entry.files[k].fname);
if (strcmp(entry.files[k].fname, filename) == 0 && strcmp(entry.files[k].fext, extension) == 0) {

				//fseek(disk, BLOCK_SIZE * entry.files[k].nStartBlock , SEEK_SET);
	entry.nFiles --;
	memset(&entry.files[k], 0, sizeof(struct csc452_file_directory));
	fseek(disk, BLOCK_SIZE * d.nStartBlock, SEEK_SET);
				fwrite(&entry.files[k], BLOCK_SIZE, 1, disk);
	
	fclose(disk);				//fseek(disk
	return 0;				
}
				//return 0;
			}
			
			}

		}
		//fclose(disk);

	} else {
	//	return 
	return -EISDIR;
	}
	//sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	
	

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
