/*
	FUSE: Filesystem in Userspace
	By Danny Ryngler
	CSC 452 Spring 2022


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

#include <stdbool.h>

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
	int nFiles;		//How many files are in this directory.
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
	long nsb;
};

typedef struct csc452_disk_block csc452_disk_block;

// free space tracker
struct storage_avail
{
	int block;
	char padding[BLOCK_SIZE - sizeof(int)];
};

typedef struct storage_avail storage_avail;

/* Helper function to write to disk */
void write_file(void* buf, FILE* file_desc, long int offset, size_t size, int end){
	if (end == 1) {
		fseek(file_desc, -offset, SEEK_END);
		fwrite(buf, size, 1, file_desc);
		fseek(file_desc, 0, SEEK_SET);
		fflush(file_desc);
	} else {
		fseek(file_desc, offset, SEEK_SET);
		fwrite(buf, size, 1, file_desc);
		fseek(file_desc, 0, SEEK_SET);
		fflush(file_desc);
	}
}

/* Helper function to read from disk*/
void read_file(void* buf, FILE* file_desc, long int offset, size_t size, int end){
	if (end == 1) {
		fseek(file_desc, -offset, SEEK_END);
		fread(buf, size, 1, file_desc);
		fseek(file_desc, 0, SEEK_SET);
	} else {
		fseek(file_desc, offset, SEEK_SET);
		fread(buf, size, 1, file_desc);
		fseek(file_desc, 0, SEEK_SET);
	}
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	csc452_root_directory root;
	FILE *disk = fopen(".disk","rb+");

	read_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);
	int path_len = strlen(path) + 1;
	char dir[path_len];
	char fn[path_len];
	char ext[path_len];
	fn[0] = '\0';
	int res = 0;
	
	sscanf(path, "/%[^/]/%[^.].%s", dir, fn, ext);
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if (fn[0] == '\0') {
		int i = 0;
		while (i < root.nDirectories && strcmp(root.directories[i].dname, dir) != 0) {
			i++;
		}
		if (i < root.nDirectories && strcmp(root.directories[i].dname, dir) == 0){
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
		} else {
			res = -ENOENT;
		}
	}
	else {
		int db_idx = -1;
		int i = 0;
		while (i < root.nDirectories && strcmp(root.directories[i].dname, dir) != 0){
			i++;
		}
		if (i < root.nDirectories && strcmp(root.directories[i].dname, dir) == 0){
			db_idx = root.directories[i].nStartBlock;
		} else {
			fclose(disk);
			return -ENOENT;
		}

		csc452_directory_entry dir;
		read_file((void*) &dir, disk, sizeof(csc452_directory_entry) * db_idx, sizeof(csc452_directory_entry), 0);
		i = 0;
		while (i < dir.nFiles && !((strcmp(dir.files[i].fname, fn) == 0 && strcmp(dir.files[i].fext, ext) == 0))) {
			i++;
		}
		if (i < dir.nFiles && (strcmp(dir.files[i].fname, fn) == 0 && strcmp(dir.files[i].fext, ext) == 0)) {
			stbuf->st_mode = S_IFREG | 0666;
			stbuf->st_nlink = 2;
			stbuf->st_size = dir.files[i].fsize;
		} else {
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
	
	int res = 0;
	csc452_root_directory root;
	FILE *disk = fopen(".disk","rb+");

	read_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);
	int path_len = strlen(path) + 1;
	char dir[path_len];
	char fn[path_len];
	char ext[path_len];
	fn[0] = '\0';
	ext[0] = '\0';
	sscanf(path, "/%[^/]/%[^.].%s", dir, fn, ext);
	//A dir holds two entries, one that represents itself (.)
	//and one that represents the dir above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		int i;
		for (i=0; i < root.nDirectories; i++){
			filler(buf, root.directories[i].dname, NULL, 0);
		}
	}
	else if(fn[0] == '\0') {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		int dirBlock = 0;
		int i = 0;
		while (i < root.nDirectories && strcmp(root.directories[i].dname, dir) != 0) {
			i++;
		}
		if (i < root.nDirectories && strcmp(root.directories[i].dname, dir) == 0){
			dirBlock = root.directories[i].nStartBlock;
		} else {
			fclose(disk);
			return -ENOENT;
		}

		csc452_directory_entry dir;
		read_file((void*) &dir, disk, sizeof(csc452_disk_block) * dirBlock, sizeof(csc452_directory_entry), 0);
		for (i = 0; i < dir.nFiles; i++){
			char fn[2 + strlen(dir.files[i].fname) + strlen(dir.files[i].fext)];
			strcpy(fn, dir.files[i].fname);
			strcat(fn, ".");
			strcat(fn, dir.files[i].fext);
			filler(buf, fn, NULL, 0);
		}
	}
	else {
		res = -ENOENT;
	}

	fclose(disk);
	return res;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	csc452_root_directory root;
	char *dirname = path + 1;
	FILE *disk = fopen(".disk","rb+");

	read_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);

	storage_avail storage;
	read_file((void*) &storage, disk, sizeof(storage_avail), sizeof(storage_avail), 1);

	// Check for errors
	if (strlen(dirname) > 8) {
		fclose(disk);
		return -ENAMETOOLONG;
	}

	int s_cnt = 0;
	int idx = 0;
	while(path[idx] != '\0'){
		if (path[idx] == '/')
			s_cnt++;
		idx++;
	}
	if (s_cnt < 1 || s_cnt > 1) {
		fclose(disk);
		return -EPERM;
	}
	if (root.nDirectories == MAX_FILES_IN_DIR){
		fclose(disk);
		return -ENOSPC;
	}
	int i = 0;
	while (i < root.nDirectories) {
		if (strcmp(root.directories[i].dname, dirname) == 0){
			fclose(disk);
			return -EEXIST;
		}
		i++;
	}
	if (storage.block == 0) {
		storage.block++;
	}
	int sb = storage.block; 
 	storage.block++;
	strcpy(root.directories[root.nDirectories].dname, dirname);
	root.directories[root.nDirectories].nStartBlock = sb;
	root.nDirectories++;
	write_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);
	write_file((void*)  &storage, disk, sizeof(storage_avail), sizeof(storage_avail), 1);

	
	// set number of files to a new empty dir to be 0
	csc452_directory_entry dir;	
	i = 0;
	int dirBlock = 0;
	while (i < root.nDirectories && strcmp(root.directories[i].dname, dirname) != 0) {
		i++;
	}
	if (i < root.nDirectories && strcmp(root.directories[i].dname, dirname) == 0) {
		dirBlock = root.directories[i].nStartBlock;
	}
	read_file((void*) &dir, disk, sizeof(csc452_directory_entry)*dirBlock, sizeof(csc452_directory_entry), 0);
	dir.nFiles = 0;
	write_file((void*) &dir, disk, sizeof(csc452_directory_entry)*dirBlock, sizeof(csc452_directory_entry), 0);

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
	
	csc452_root_directory root;
	FILE *disk = fopen(".disk","rb+");
	int path_len = strlen(path) + 1;
	char cdir[path_len];
	char fn[path_len];
	char ext[path_len];
	fn[0] = '\0';
	ext[0] = '\0';
	sscanf(path, "/%[^/]/%[^.].%s", cdir, fn, ext);
	read_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);

	// check for errors
	if(strlen(fn) >= MAX_FILENAME) {
		fclose(disk);
		return -ENAMETOOLONG;
	}

	int s_cnt = 0;
	int idx = 0;
	while(path[idx] != '\0'){
		if (path[idx] == '/')
			s_cnt++;
		idx++;
	}
	if (s_cnt == 1){
		fclose(disk);
		return -EPERM; 
	}

	int i = 0;
	int dirBlock = 0;
	while (i < root.nDirectories && strcmp(root.directories[i].dname, cdir) != 0) {
		i++;
	}
	if (i < root.nDirectories && strcmp(root.directories[i].dname, cdir) == 0) {
		dirBlock = root.directories[i].nStartBlock;
	} else {
		fclose(disk);
		return -ENOENT;
	}

	csc452_directory_entry dir;
	read_file((void*) &dir, disk, sizeof(csc452_directory_entry)*dirBlock, sizeof(csc452_directory_entry), 0);

	if (dir.nFiles == MAX_FILES_IN_DIR) {
		fclose(disk);
		return -ENOSPC;
	}

	i = 0;
	while (i < dir.nFiles && !(strcmp(dir.files[i].fname, fn) == 0 && strcmp(dir.files[i].fext, ext) != 0)) {
		i++;
	}
	if (i < dir.nFiles && strcmp(dir.files[i].fname, fn) == 0 && strcmp(dir.files[i].fext, ext) == 0) {
		fclose(disk);
		return -EEXIST;
	}

	storage_avail storage;
	read_file((void*)  &storage, disk, sizeof(storage_avail), sizeof(storage_avail), 1);
	strcpy(dir.files[dir.nFiles].fname, fn);
	strcpy(dir.files[dir.nFiles].fext, ext);
	dir.files[dir.nFiles].nStartBlock = storage.block;
	dir.nFiles++;
 	storage.block++;
	write_file((void*) &dir, disk, sizeof(csc452_directory_entry)*dirBlock, sizeof(csc452_directory_entry), 0);
	write_file((void*)  &storage, disk, sizeof(storage_avail), sizeof(storage_avail), 1);
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

	FILE *disk = fopen(".disk", "rb+");

	csc452_root_directory root;
	read_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);

	int path_len = strlen(path) + 1;
	char dir[path_len];
    	char fn[path_len];
    	char ext[path_len];
	fn[0] = '\0';
	ext[0] = '\0';
    	sscanf(path, "/%[^/]/%[^.].%s", dir, fn, ext);


	// check for errors
	if (size < 0){
		fclose(disk);
		return -EINVAL;
	}
	if (fn[0] == '\0') {
		fclose(disk);
		return -EISDIR;
	}

	int db = -1;
	int i = 0;
	while (i < root.nDirectories && strcmp(dir, root.directories[i].dname) != 0) {
		i++;
	}
	if (i < root.nDirectories && strcmp(dir, root.directories[i].dname) == 0) {
		db = root.directories[i].nStartBlock;
	}

	
	if(db < 0) {	
		fclose(disk);
		return -ENOENT;
	}

	csc452_directory_entry curr_dir;
	read_file((void*) &curr_dir, disk, sizeof(csc452_directory_entry)*db, sizeof(csc452_directory_entry), 0);
	
	i = 0;
	int idx = -1;
	int fb = -1;
	while (i < curr_dir.nFiles && !(strcmp(fn,curr_dir.files[i].fname) == 0 && strcmp(ext,curr_dir.files[i].fext) == 0)) {
		i++;
	}
	if (i < curr_dir.nFiles && strcmp(fn,curr_dir.files[i].fname) == 0 && strcmp(ext,curr_dir.files[i].fext) == 0) {
		idx = i;
		fb = curr_dir.files[i].nStartBlock;
	}
	if (offset > curr_dir.files[idx].fsize) {
		fclose(disk);
		return -EINVAL;
	}
	if (idx < 0) {
		fclose(disk);
		return -ENOENT;
	}

	if (size + offset > curr_dir.files[idx].fsize) {
		size = curr_dir.files[idx].fsize - offset;
	}
	
	// read data into buffer
	csc452_disk_block file_block;
	size_t size_left = size;
	off_t coff = offset;
	while (size_left > 0){
		read_file((void*) &file_block, disk, sizeof(csc452_disk_block)*fb, sizeof(csc452_disk_block), 0);
		fb = file_block.nsb;
	       	if (coff >= MAX_DATA_IN_BLOCK) {
			coff = coff - MAX_DATA_IN_BLOCK;

		} else {
			int cpy_size = 0;
			if (MAX_DATA_IN_BLOCK < size_left) {
				cpy_size = MAX_DATA_IN_BLOCK - coff;
			} else {
				cpy_size = size_left - coff;
			}
			memcpy(buf, &file_block.data[coff], cpy_size);
			buf += cpy_size;
			coff = 0;
			size_left -= cpy_size;
		}
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

	csc452_root_directory root;
	FILE *disk = fopen(".disk","rb+");

	read_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);

	int path_len = strlen(path) + 1;
	char dir[path_len];
    	char fn[path_len];
    	char ext[path_len];
	fn[0] = '\0';
	ext[0] = '\0';
    	sscanf(path, "/%[^/]/%[^.].%s", dir, fn, ext);

	// check for errors
	int db = -1;
	int i = 0;
	while (i < root.nDirectories && strcmp(dir, root.directories[i].dname) != 0) {
		i++;
	}
	if (i < root.nDirectories && strcmp(dir, root.directories[i].dname) == 0) {
		db = root.directories[i].nStartBlock;
	}
	
	if(db < 0) {
		fclose(disk);
		return -ENOENT;
	}
	if (size < 0) {
		fclose(disk);
		return -EINVAL;
	}

	csc452_directory_entry curr_dir;
	read_file((void*) &curr_dir, disk, sizeof(csc452_directory_entry)*db, sizeof(csc452_directory_entry), 0);

	i = 0;
	int idx = -1;
	int nfb = 0;
	while (i < curr_dir.nFiles && !(strcmp(fn,curr_dir.files[i].fname) == 0 && strcmp(ext,curr_dir.files[i].fext) == 0)) {
		i++;
	}
	if (i < curr_dir.nFiles && strcmp(fn,curr_dir.files[i].fname) == 0 && strcmp(ext,curr_dir.files[i].fext) == 0) {
		if (offset > curr_dir.files[i].fsize) {
			fclose(disk);
			return -EFBIG;
		}
		idx = i;
		nfb = curr_dir.files[i].nStartBlock;
	}
	if (idx < 0) {
		fclose(disk);
		return -ENOENT;
	}

	// write to a block
	storage_avail storage;
	read_file((void*)  &storage, disk, sizeof(storage_avail), sizeof(storage_avail), 1);

	csc452_disk_block file;
	size_t size_left = size;
	off_t coff = offset;
	int cfb = 0;
	while (size_left > 0){
		cfb = nfb;
		read_file((void*) &file, disk, sizeof(csc452_disk_block)*nfb, sizeof(csc452_disk_block), 0);
		nfb = file.nsb; 
		if (coff >= MAX_DATA_IN_BLOCK) {
			coff = coff - MAX_DATA_IN_BLOCK;
			continue;
		}
		if(size_left > 0 && nfb == 0) {
			file.nsb = storage.block;
		 	storage.block++;
			nfb = file.nsb;
		}

		int cpy_size;
		if (MAX_DATA_IN_BLOCK < size_left) {
			cpy_size = MAX_DATA_IN_BLOCK;
		} else {
			cpy_size = size_left;
		}
		if (MAX_DATA_IN_BLOCK < coff + cpy_size) {
			cpy_size = MAX_DATA_IN_BLOCK - coff;
		}
		memcpy(&file.data[coff], buf, cpy_size);
		buf += cpy_size;
		coff = 0;
		size_left -= cpy_size;
		write_file((void*) &file, disk, sizeof(csc452_disk_block)*cfb, sizeof(csc452_disk_block), 0);
	}

	if (curr_dir.files[idx].fsize <= offset + size) {
		curr_dir.files[idx].fsize = offset + size;
	} 

	write_file((void*)  &storage, disk, sizeof(storage_avail), sizeof(storage_avail), 1);
	write_file((void*) &curr_dir, disk, sizeof(curr_dir)*db, sizeof(csc452_directory_entry), 0);

	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;

	csc452_root_directory root;
	char *dirname = path + 1;
	FILE *disk = fopen(".disk","rb+");

	read_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);

	int dir_found = 0;
	int i = 0;
	while (i < root.nDirectories) {
		if (strcmp(root.directories[i].dname, dirname) == 0){
			// read dir from block to see if empty
			int start_block = root.directories[i].nStartBlock;
			csc452_directory_entry dir;
			read_file((void*) &dir, disk, sizeof(csc452_directory_entry)*start_block, sizeof(csc452_directory_entry), 0);
			if (dir.nFiles > 0) {
				fclose(disk);
				return -ENOTEMPTY;
			}		

			int j;
			for (j = i; j < root.nDirectories - 1; j++) {
				root.directories[j] = root.directories[j + 1];
			}
			root.nDirectories--;
			dir_found = 1;
			break;
		}
		i++;
	}
	if (dir_found == 0) {
		fclose(disk);
		return -ENOENT;
	}

	write_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);
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
	csc452_root_directory root;
	FILE *disk = fopen(".disk","rb+");
	
	char *chunks = strtok(path, "/");
	char token_list[MAX_FILES_IN_DIR][15];
	int count = 0;
	while (chunks != NULL){
		strcpy(token_list[count], chunks);
		count++;
		chunks = strtok(NULL, " ");
	}
	if (count == 1) {
		fclose(disk);
		return -EISDIR;
	}
    	
	for (int i=0; i < count; i++) {
       		printf("%s\n", token_list[i]);
    	}

	char *dirname = token_list[count - 2];
	char *filename = token_list[count -1];

	read_file((void*) &root, disk, 0, sizeof(csc452_root_directory), 0);

	int dir_found = 0;
	int file_found = 0;
	int i = 0;
	while (i < root.nDirectories) {
		if (strcmp(root.directories[i].dname, dirname) == 0){
			// find file
			int block_start = root.directories[i].nStartBlock;
			csc452_directory_entry dir;
			read_file((void*) &dir, disk, sizeof(csc452_directory_entry)*block_start, sizeof(csc452_directory_entry), 0);
			int j;
			for (j = 0; j < dir.nFiles; j ++) {
				char combined[strlen(dir.files[j].fname) + 1];
				strcpy(combined, dir.files[j].fname);
				strcat(combined, ".");
				strcat(combined, dir.files[j].fext);
				if (strcmp(combined, filename) == 0){
					//FOUND NOW JUST NEED TO DELETE PROPERLY
					int k;
					for (k = j; k < dir.nFiles - 1; k++) {
						dir.files[k] = dir.files[k + 1];
					}
					dir.nFiles--;
					write_file((void*) &dir, disk, sizeof(dir)*block_start, sizeof(csc452_directory_entry), 0);
					file_found = 1;
					break;
				}
			}
			dir_found = 1;
			break;
		}
		i++;
	}
	if (dir_found == 0 || file_found == 0) {
		fclose(disk);
		return -ENOENT;
	}

	fclose(disk);
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
