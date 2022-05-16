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

static int parsePath(const char *path, char parsed[3][22]){
	int i;
 	for(i = 0; i<3; i++){
		parsed[i][0] = '\0';
	}

	if(!path){
		return 0;
	}
	if(strlen(path) > 22){
		return -1;
	}
	if(strlen(path) == 1){
		return 1;
	}

	sscanf(path, "/%[^/]/%[^.].%s", parsed[0], parsed[1], parsed[2]);

	if(strlen( ((char*)parsed[0]) ) > 8){
		return -8;
	}
}

static int getDirs(csc452_root_directory *root)
{
	FILE* disk = fopen(".disk","r+b");
	if(disk != 0){
		fseek(disk, 0, SEEK_SET);
		fread(root,sizeof(csc452_root_directory), 1, disk);
		fclose(disk);
		return 0;
	}
	fclose(disk);
	return -1;	
}

static int dirExist(const char* path){
	
	csc452_root_directory dirs;
	int ret = getDirs(&dirs); 
	int index = 0;	
	while(index < MAX_DIRS_IN_ROOT){
		if(strcmp(path, dirs.directories[index].dname) == 0){
			return index;
		}
		index++;
	}
	return -1;
}

static int writeRoot(csc452_root_directory* root){
	FILE * disk = fopen(".disk","r+b");
	fseek(disk, 0, SEEK_SET);
	fwrite(root, sizeof(csc452_root_directory), 1,disk);
	fclose(disk);
	
	return 0;
}

static int writeDir(long bNumeber){
	csc452_directory_entry* entry;
	entry->nFiles = 0;
	FILE * disk = fopen(".disk", "r+b");
	fseek(disk, bNumber, SEEK_SET);
	fwrite(entry, sizeof(csc452_directory_entry), 1, disk);
	fclose(disk);
	
	return 0;
}

static int upDir(long sBlkID, csc452_directory_entry * e){
	FILE * disk = fopen(".disk", "r+b");
	fseek(disk, sBlkID, SEEK_SET);
	fwrite(e, sizeof(csc452_directory_entry, 1, disk));
	fclose(disk);

	return 0;
}

static int writeFile(long blkID, csc452_disk_block* data){
	FILE * disk = open(".disk","r+b");
	fseek(disk, blkID, SEEK_SET);
	fwrite(data, sizeof(csc452_directory_entry), 1, disk);
	fclose(disk);

	return 0;
	
}

static int getBlkId(char *dir){
	int index;
	csc452_root_directory root;
	getDirs(&root);
	for(index = 0; index < root.nDirectories; index++){
		char* dirName = root.directories[index].dname;
		if(strcmp(dir, dirName)==0){
			return root.directories[index].nStartBlock;
		}
	}
	return -1;
}

static int getDir(long blkId, csc452_directory_entry * ret){
	FILE * disk = fopen(".disk","r+b");
	fseek(disk, blkId, SEEK_SET);
	fread(ret, sizeof(csc452_directory_entry), 1, disk);
	fclose;
	return 0;
}

static int check(char * path){
	int i, count = 0;
	for(i=0; i<strlen(path);i++){
		char x = path[i];
		if(x == '/'){
			count++;
			if(count > 1){
				return 0;
			}
		}
	}
	return 1;
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
	
	memset(stbuff, 0 , sizeof(struct stat));
	csc452_root_directory root;
	int index = 0, ret;
	char paresed[3][22];
	ret = parsePath(path, paresed);
	
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} 
	else  {
		
		if(dirExist(parsed[0] != -1){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
		}
	else  {
	res = -ENOENT;
	for(index = 0; index < MAX_DIRS_IN_ROOT; index++){
		char * dname = root.directories[index].dname;
		if(strcmp(parsed[0], dname)==0){
			int strBlk = root.directories[index].nStartBlock;
			csc452_directory_entry e;
			getDir(strBlk, &e);
			if(check(path)==0){ 
				if( (e.nFiles) ==0 ) return -ENOENT;
				if((e.nFiles) > 0){
					int j;
					for(j=0; j<MAX_FILES_IN_DIR; j++){
						char* fName = e.files[j].fname;
						if(strcmp(parsed[1], fName)==0){
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 2;
							stbuf->st_size = e.files[j].fsize;
							return 0;		
						}
					}
					return -ENOENT;
				}
				else{
					stbuf->st_mode = S_IFREG | 0755;
					stbuf->st_nlink = 2;
					return 0;
				}	
			}	
		}

	}		
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
	csc452_root_directory root;
	getDirs(&root);
	int i=0;
	char parsed[3][22];
	int ret = parsePath(path, parsed);
	
	
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
		if(root.nDirectories>0){
			for(i=0;i<root.nDirectories; i++){
				filler(buf, root->directories[i].dname, NULL,0);
			}
		}
		return 0;
	}
	else {
		for(i=0; i<MAX_DIRS_IN_ROOT;i++){
			char* dName = root.directories[i].dname;
			if(strcmp(parsed[0], dName)==0){
				int start = root.directories[i].nStartBlock;
				csc452_directory_entry dir;
				getDir(start, &dir);
				if(dir->nFiles>0){
					char name[12];
					strcpy(name, dir.files[i].fname);
					if(strcmp(dir.files[i].fext, "") !=0){
						strcat(name, ".");
						strcat(name, dir.files[i].fext);
					}
					filler(buf, name, NULL,0);
				} 
			}
		} 
		return 0;
	}

	return -ENOENT;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	//(void) path;
	//(void) mode;
	
	int index;
	
	char parsed[3][22];
	int ret = parsePath(path, parsed);

	
	if(ret == -8) {
		return -ENAMETOOLONG;
	}	
	if(ret != 2){
		return -EPERM;
	}
	int ex;
	ex = dirExist(parsed[0]);
	if(ex != -1){
		return -EEXIST;
	} 
	
	csc452_root_directory dirs;
	ret = getDirs(&dirs);
	if(ret != 0){
		return -1; 
	}
	index = dirs.nDirectories;
	long sBlock = BLOCK_SIZE+(index*BLOCK_SIZE);
	strcpy(dirs.directories[index].dname, pardes[0]);
	dirs.nDirectories ++;
	dirs.directories[index].nStartBlock = sBlock;
	
	writeRoot(&dir);	
	writeDir(sBlock);
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
	char parsed[3][22];
	parsePath(path, parsed);
	if(parsed[1] != 0) return -ENOTDIR;
	if(dirExist(parsed[0]) == -1) return -ENOENT;
	csc452_root_directory root;

	getDirs(&root);
	int i;	
	for(i=0; i<root.nDirectories; i++)[
		if(strcmp(parsed[0], root.directories[i].dname) == 0){
			csc452_directory_entry e;
			long blkId = root.directories[i].nStartBlock;
			getDir(blkId, &e);
			if(e.nFiles > 0) return -ENOTEMPTY;
			if(e.nFiles ==0) return -ENOENT;
			e.nFiles = 0;
			FILE * disk = fopen(".disk", "r+b");
			fseek(disk, blkId, SEEK_SET);
			fwrite(e, sizeof(csc452_directory_entry), 1, disk);
			fclose(disk);
			return 0;
		}
	}
	(void) path;
	
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
