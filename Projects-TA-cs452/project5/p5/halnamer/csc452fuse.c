/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


*/
/**
 * Hassan Alnamer
 * 	In this project we are using a 5 MB = 5* 2**10 Bytes disk image
 * 		Alwasy rememeber to format it with `dd bs=1K count=5K if=/dev/zero of=.disk`
 * 
 * 
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

/**
 * @brief Marks the root block in memory (.disk)
 * ONLY contains other directories (struct csc452_directory_entry)
 * We use block 0 of .disk for this struct
 * **** Root is 1 block in size ****
 * 
 */
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
/**
 * @brief Represents files
 * Each file is 512 byte structure --> 2**9
 * 
 */
struct csc452_disk_block
{
	//Space in the block can be used for actual data storage.
	char data[MAX_DATA_IN_BLOCK];
	//Link to the next block in the chain
	long nNextBlock;
};

typedef struct csc452_disk_block csc452_disk_block;


/**
 * @brief per the email I need an available space tracking structure
 * The size of this structure = BLOCK_SIZE - 2
 * 							struct root			struct available_blocks
 * 
 */
#define MAX_AVAILABLE_SPACE (BLOCK_SIZE - 2)
struct available_blocks{
	int block_table[MAX_AVAILABLE_SPACE];
};

typedef struct available_blocks available_blocks;
/**
 * @brief The helper functions for the interface
 * 
 */
static struct csc452_root_directory get_root();
//static int get_file_size(const char * path);
static int find_dir(csc452_root_directory root, char *target_dir);
static int find_file(struct csc452_directory dir, char targetFile[], char targetExtension[], 
	csc452_root_directory root);

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
/**
static int get_file_size(const char * path){
	FILE *fp = fopen(path, "r");
	int size;
	if( fp == NULL )  {
		perror ("get_attr: Error opening file\n");
			return(-1);
   	}
	//get file size
	fseek(fp, 0, SEEK_END); // seek to end of file
	size = ftell(fp); // get current file pointer
	fseek(fp, 0, SEEK_SET); // seek back to beginning of file
	fclose(fp);
	return size;
}
*/

/**
 * @brief searches the root for a dir and returns its index
 * if doesn't exist return -EEXIST
 * 
 * @param root 
 * @param target_dir 
 * @return int 
 */
static int find_dir(csc452_root_directory root, char target_dir[]){
	printf("****** find_dir: args(%s) *******\n", target_dir);
	struct csc452_directory dir;
	strcpy(dir.dname, "");
	dir.nStartBlock =-1;
	printf("****** find_dir: is target dir empty? *******\n");
	if(strcmp(target_dir, "") == 0){
		printf("****** find_dir: dir string is empty (%s) *******\n", target_dir);
		return -1;
	}
	printf("****** find_dir: searching for dir *******\n");
	int i;
	if(root.nDirectories == 0) return -1;
	for(i = 0; i < MAX_DIRS_IN_ROOT; i++){
		struct csc452_directory curr_dir = root.directories[i];
		printf("****** find_dir: iter #%d: directory name (%s) == target (%s) *******\n",
		i, dir.dname, target_dir);

		if(strcmp(curr_dir.dname, target_dir) == 0){ 
			//found the directory!!
			printf("****** find_dir: found_dir(%s) *******\n", target_dir);
			dir = curr_dir;
			return i;
		}
	}
	
	printf("****** find_dir: dir doesn't exist(%s) *******\n", target_dir);
	return -1;
	
	
}
static int find_file(struct csc452_directory dir, char targetFile[], char targetExtension[], 
	csc452_root_directory root)
	{
	printf("****** find_file: targetFile: %s *******\n", targetFile);
	FILE* disk = fopen(".disk", "r+b");
	int location_on_disk = BLOCK_SIZE*dir.nStartBlock;
	printf("****** find_file: fetching dir that lives at (%d) *******\n", location_on_disk);
	fseek(disk, location_on_disk, SEEK_SET);
	
	csc452_directory_entry dir_entry;
	dir_entry.nFiles = 0;
	memset(dir_entry.files, 0, MAX_FILES_IN_DIR*sizeof(struct csc452_file_directory));
	
	int num_items_successfully_read = fread(&dir_entry, BLOCK_SIZE, 1, disk); //Read in the directory's data, such as its files contained within
	fclose(disk);

	//One block was successfully read, so proceed
	if(num_items_successfully_read == 1){ 
		if(dir_entry.nFiles == 0) return -1;

		for(int i = 0; i < MAX_FILES_IN_DIR; i++){ //Iterate over the files in the directory
			struct csc452_file_directory curr_file = dir_entry.files[i];
			//check that filename and extension matches target
			if(strcmp(curr_file.fname, targetFile) == 0 && 
						strcmp(curr_file.fext, targetExtension) == 0){ 
				printf("****** find_file: found file(%s.%s) *******\n", targetFile,
				 														targetExtension);
				
				return curr_file.fsize;
			}
		}

		
		// we didn't find any file
		printf("****** find_file: file doesn't exist(%s.%s) *******\n", targetFile,
																			targetExtension);
		return -1;
		
	}
	return -1;
}
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	
	//clean the struct
	printf("****** get_attr(path: %s) *******\n", path);
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		printf("****** get_attr: curr path is / *******\n");
		//if it is the root
		//then must return 
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2; //. ..
		printf("****** get_attr: return 0 / *******\n");
		return 0;
	} 
	else  {
		printf("****** curr path is (%s) *******\n", path);
		char directory[MAX_FILENAME+1], filename[MAX_FILENAME+1],
		extension[MAX_EXTENSION+1], path_c[strlen(path)+1];
		strcpy(directory, ""); strcpy(filename, ""); strcpy(extension, "");
		strcpy(path_c, path);
		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		//must search that dir exist?
		csc452_root_directory root = get_root();
		int index_root = find_dir(root, directory);
		if(index_root == -1){
			printf("****** get_attr: dir doesn't exist (%s) *******\n", path);
			printf("****** get_attr: return -ENOENT / *******\n");
			return -ENOENT;
		}
		struct csc452_directory dir = root.directories[index_root]; 
		
		//Doesn't deal with files so far!!!
		//does the file was given in args?
		
		if(strcmp(filename, "") == 0){ 
			printf("****** get_attr: success empty dir(%s) *******\n", path);
			printf("****** get_attr: return 0 / *******\n");
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			return 0; 
			
		}
		//look for file
		int size = find_file(dir, filename, extension, root);
		if( size < 0 ){
			printf("****** get_attr: path couldn't be found%s) *******\n", path);
			printf("****** get_attr: return -ENOENT / *******\n");
			return -ENOENT;
		}
		
		//The file we were looking for was found!
		printf("****** get_attr: returning correct stat(%s) *******\n", filename);
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1;
		
		stbuf->st_size = size;
		
		
	}
	printf("****** get_attr: return 0 / *******\n");
	return 0;
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
	
	//check path is dir and that it exist
	printf("****** csc452_readdir: curr path is (%s) *******\n", path);
	char path_c[strlen(path)+1], directory[MAX_FILENAME+1], filename[MAX_FILENAME+1], extension[MAX_EXTENSION+1];
	strcpy(path_c, path); strcpy(directory, ""); strcpy(filename, ""); strcpy(extension, "");
	sscanf(path_c, "/%[^/]/%[^.].%s", directory, filename, extension);
	if(strlen(directory) > MAX_FILENAME){
		printf("****** csc452_readdir: fail dir is too long *******\n");
		printf("****** csc452_readdir: return -ENOENT / *******\n");
		return -ENOENT;
	}
	//mot sure why I have this one?
	if(strlen(filename) > MAX_FILENAME){ 
		printf("****** csc452_readdir: fail filename is too long *******\n");
		printf("****** csc452_readdir: return -ENOENT / *******\n");
		return -ENOENT; 
	}
	if(strlen(extension) > MAX_EXTENSION){ 
		printf("****** csc452_readdir: fail extension is too long *******\n");
		printf("****** csc452_readdir: return -ENOENT / *******\n");
		return -ENOENT; 
	}
	if(filler(buf, ".", NULL,0) == 0){
		printf("****** csc452_readdir: Issue while filling the output *******\n");
	}
	if(filler(buf, "..", NULL, 0)){
		printf("****** csc452_readdir: Issue while filling the output *******\n");
	}

	printf("****** csc452_readdir: fetch root *******\n");
	csc452_root_directory root = get_root();
	printf("****** csc452_readdir: is this path the / ? *******\n");
	if(strcmp(path_c, "/") == 0){
		//ls directories
		for (int i = 0; i < root.nDirectories; i++){
			filler(buf, root.directories[i].dname, NULL, 0);

		}
		return 0;
	}

	//must search that dir exist?
	printf("****** csc452_readdir: this is second level -> list files *******\n");
	printf("****** csc452_readdir: try to find_dir *******\n");
	int index_root = find_dir(root, directory); 
	if(index_root == -1){
		printf("****** csc452_readdir: dir doesn't exist (%s) *******\n", path);
		printf("****** csc452_readdir: return -ENOENT / *******\n");
		return -ENOENT;
	}
	struct csc452_directory dir = root.directories[index_root];
	
	// found the dir
	// now I must fill the values and return
	printf("****** csc452_readdir: populating file list *******\n");
	printf("****** csc452_readdir: curr dir (%s) lives at block: %ld *******\n", dir.dname, dir.nStartBlock);
	FILE* disk = fopen(".disk", "rb+");
	int location_on_disk = dir.nStartBlock*BLOCK_SIZE;
	fseek(disk, location_on_disk, SEEK_SET);
	csc452_directory_entry dir_entry;
	dir_entry.nFiles = 0;
	memset(dir_entry.files, 0, MAX_FILES_IN_DIR*sizeof(struct csc452_file_directory));
	int items_read = fread(&dir_entry, BLOCK_SIZE, 1, disk); 
	fclose(disk);

	if(items_read != 1){
		printf("****** csc452_readdir: Couldn't find block for dir *******\n");
		return -ENOENT;
	}	

	printf("****** csc452_readdir: dir info: dir_name %s, nFiles: %d*******\n",
		dir.dname, dir_entry.nFiles);
	for(int j = 0; j < MAX_FILES_IN_DIR; j++){
		struct csc452_file_directory file_dir = dir_entry.files[j];
		printf("****** csc452_readdir: iter#%d %s/%s  *******\n",j, directory, file_dir.fname);
		char filename_copy[MAX_FILENAME+1];
		strcpy(filename_copy, "");
		strcpy(filename_copy, file_dir.fname);
		if(strcmp(file_dir.fext, "") != 0){ 
				strcat(filename_copy, ".");
				strcat(filename_copy, file_dir.fext); 
		}
		
		if(strcmp(file_dir.fname, "") != 0){ 
			printf("****** csc452_readdir: fill with %s/%s  *******\n",
			directory, filename_copy);
			filler(buf, filename_copy, NULL, 0);
		}
	}
	

	return 0;
}
static struct available_blocks get_block_directory(){
	/**
	 * @brief The root is a 5 MB of zeroes at the init of program.
	 * Thus when I first load, I just need to seek to the 
	 * right place and write whatever I want
	 * 
	 */
	printf("**** get_root: getting root from block 0***\n");
	FILE *disk = fopen(".disk", "r+b");
	int location_on_disk = BLOCK_SIZE*1;
	fseek (disk, location_on_disk, SEEK_SET);
	available_blocks blocks;
	fread(&blocks, BLOCK_SIZE, 1, disk);
	return blocks;
}
/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */

static struct csc452_root_directory get_root(){
	/**
	 * @brief The root is a 5 MB of zeroes at the init of program.
	 * Thus when I first load, I just need to seek to the 
	 * right place and write whatever I want
	 * 
	 */
	printf("**** get_root: getting root from block 0***\n");
	FILE *disk = fopen(".disk", "r+b");
	fseek (disk, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, BLOCK_SIZE, 1, disk);
	return root;
}

/**
 * @brief Pushes the available block structure to the disk
 * 
 * @param available_blocks 
 */

static void push_to_disk(void * block, int nStartBlock){
	printf("**** push_to_disk: pushing a block to disk location (%d) to disk***\n", nStartBlock);
	FILE* disk = fopen(".disk", "r+b");
	int location_on_disk = nStartBlock * BLOCK_SIZE;
	fseek(disk, location_on_disk, SEEK_SET);
	fwrite(block, BLOCK_SIZE, 1, disk);
	fclose(disk);
}
static int find_available_block(available_blocks block_directory){
	
	printf("**** push_dir: fetched available_blocks***\n");
	block_directory.block_table[0] = EOF; block_directory.block_table[1] = EOF; //reserved for root & block_dir
	for(int i = 0; i < MAX_AVAILABLE_SPACE; i++){
		if(block_directory.block_table[i] == 0){
			printf("**** find_available_block: Found an available block (%d) ***\n", i);
			return i;
		}
	}
	return -1;
}
static int push_dir(csc452_root_directory root, char targetDirectory[]){
	/**
	 * @brief 
	 * push_dir(root, dir)
	 * open disk
	 * proceed to the next available block
	 * write this new block on disk
	 * close
	 * 
	 */
	for(int i = 0; i < MAX_DIRS_IN_ROOT; i++){ 
		printf("**** push_dir: root.directory[%d].dname = (%s) &&  root.directory[%d].nStartBlock = %ld ***\n",
		i, root.directories[i].dname, i, root.directories[i].nStartBlock);
		//iterate over dir in root and find an empty one
		if(strcmp(root.directories[i].dname, "") == 0){ 
			printf("**** push_dir: Found an empty dir ***\n");
			struct csc452_directory new_dir;
			new_dir.nStartBlock = -1;
			strcpy(new_dir.dname, targetDirectory); 
			printf("**** push_dir: copied targetDir: %s ***\n", new_dir.dname);
			
			//What is you nStartBlock??
			available_blocks block_directory = get_block_directory(block_directory);
			printf("**** push_dir: fetched available_blocks***\n");
			int available_block = find_available_block(block_directory);
			if(available_block == -1){
				printf("**** push_dir: Couldn't find an available block ***\n");
				return -EPERM;
			}
			new_dir.nStartBlock = available_block;
			
			printf("**** push_dir: found available block %ld***\n", new_dir.nStartBlock);

			csc452_directory_entry dir;
			
			memset(dir.files, 0, MAX_FILES_IN_DIR*sizeof(struct csc452_file_directory));
			
			FILE* disk = fopen(".disk", "r+b");
			int location_on_disk = BLOCK_SIZE*new_dir.nStartBlock; 
			fseek(disk, location_on_disk, SEEK_SET);
			int num_items_read = fread(&dir, BLOCK_SIZE, 1, disk);
			
			if(num_items_read == 1){ //fread returned successfully with 1 item
				printf("**** push_dir: Found the right block ***\n");
				memset(&dir, 0, sizeof(struct csc452_directory_entry)); //Clear the directory data we just read in JUST IN CASE
				dir.nFiles = 0; //Directory begins with 0 files in it
				fwrite(&dir, BLOCK_SIZE, 1, disk); //Write the new directory data
				fclose(disk);

				//Update the root with its new data and write it to disk, as well as the FAT
				root.nDirectories++;
				root.directories[i] = new_dir;	
				block_directory.block_table[new_dir.nStartBlock] = EOF;

				push_to_disk(&block_directory, 1);
				push_to_disk(&root, 0);
				
				
			} else{ //There was an error reading in the data from disk, so just close the file
				printf("**** push_dir: No blocks associated with this location ***\n");
				fclose(disk);
			}

			return 0;
		}


	}
	return 0;
}

static int csc452_mkdir(const char *path, mode_t mode)
{
	printf("*** csc452_mkdir: (path: %s) ***\n", path);
	//I think get_attr has a problem, but proceed
	(void) path;
	(void) mode;

	csc452_root_directory root = get_root();
	printf("*** csc452_mkdir: got root! ***\n");
	//read the args
	char path_c[strlen(path)];
	strcpy(path_c, path);
	char * directory= strtok(path_c, "/");
	char * filename = strtok(NULL, "/");
	printf("*** csc452_mkdir: copied strings! ***\n");
	printf("*** csc452_mkdir: Try to catch errs! ***\n");
	//check that we are making dir under /
	printf("*** csc452_mkdir: is there a file in the path? ***\n");
	if(filename && filename[0]){
		// there is a filename in path
		printf("*** csc452_mkdir: making dir in file space filename = (%s) ***\n", filename);

		return -EPERM;
	}
	//check dir name is not too long
	printf("*** csc452_mkdir: is the directory too long? ***\n");
	if(strlen(directory) > MAX_FILENAME){
		printf("*** csc452_mkdir: dir name (%s) is too long ***\n", directory);
		return -ENAMETOOLONG;
	}
	//if root runs out of capacity
	printf("*** csc452_mkdir: is there any space on disk to store this dir? ***\n");
	if(root.nDirectories >= MAX_DIRS_IN_ROOT){
		printf("*** csc452_mkdir: no space on disk for more dirs ***\n");
		return -EPERM; 
	}
	printf("*** csc452_mkdir: No errs! ***\n");
	//look if dir already exist
	printf("*** csc452_mkdir: find_dir in root, dir: (%s) ***\n", directory);
	int index_root = find_dir(root, directory); 
	if(index_root >= 0){
		printf("****** csc452_mkdir: dir exist (%s) *******\n", path);
		printf("****** csc452_mkdir: returning -EEXIST / *******\n");
		return -EEXIST;
	}
	
	
	printf("****** csc452_mkdir: dir doesn't exist (%s) *******\n", path);
	printf("****** csc452_mkdir: creating a new one / *******\n");

	
	char targetDirectory[MAX_FILENAME+1];
	strcpy(targetDirectory, directory);
	return push_dir(root, directory);
	//get the arguments

	
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
	(void) mode; //ignore
    (void) dev;	//ignore
	//parse input
	printf("****** csc452_mknod: curr path is (%s) *******\n", path);
	char path_c[strlen(path)+1], directory[MAX_FILENAME+1], filename[MAX_FILENAME+1], extension[MAX_EXTENSION+1];
	strcpy(path_c, path); strcpy(directory, ""); strcpy(filename, ""); strcpy(extension, "");
	sscanf(path_c, "/%[^/]/%[^.].%s", directory, filename, extension);

	if(strlen(directory) > MAX_FILENAME ||
		strlen(filename) > MAX_FILENAME ||
		strlen(extension) > MAX_EXTENSION )
	{
		
		printf("****** csc452_mknod: fail dir/file/ext is too long or zero *******\n");
		printf("****** csc452_mknod: return -ENOENT / *******\n");
		return -ENAMETOOLONG;
	}
	
	
	if(strlen(filename) == 0){
		printf("****** csc452_mknod: fail len(filename) = 0 | trying to make file in / *******\n");
		printf("****** csc452_mknod: return -EPERM / *******\n");
		return -EPERM ; 
	}
	
	//look up file in dir to check if it already exist
	csc452_root_directory root = get_root();
	int index_root = find_dir(root, directory);
	struct csc452_directory dir = root.directories[index_root];

	csc452_directory_entry dir_entry;
	memset(dir_entry.files, 0, MAX_FILES_IN_DIR*sizeof(struct csc452_file_directory));		
	FILE* disk = fopen(".disk", "r+b");
	int location_on_disk = BLOCK_SIZE*dir.nStartBlock; 
	fseek(disk, location_on_disk, SEEK_SET);
	int num_items_read = fread(&dir_entry, BLOCK_SIZE, 1, disk);
	fclose(disk);

	if(num_items_read == 1){ 
		//iterate through the files
		if(dir_entry.nFiles >= MAX_FILES_IN_DIR){
			printf("****** csc452_mknod: Exceeded capacity *******\n");
			printf("****** csc452_mknod: return -EPERM / *******\n");
			return -EPERM;
		}

		if(dir_entry.nFiles > 0){
			for(int i = 0; i < MAX_FILES_IN_DIR; i++){
				printf("****** csc452_mknod: Iter#%d ('%s'=='%s') exists *******\n", i, filename, dir_entry.files[i].fname);
				if(strcmp(filename, dir_entry.files[i].fname) == 0 &&
					strcmp(extension, dir_entry.files[i].fext) == 0){
						printf("****** csc452_mknod: fail file ('%s'.'%s') exists *******\n", filename, extension);
						printf("****** csc452_mknod: return -EEXIST / *******\n");
						return -EEXIST ; 
				}
			}
		}
		
		//file doens't exist
		printf("****** csc452_mknod: File doesnt exist | create new one *******\n");
		printf("****** csc452_mknod: dir_entry.nFiles = (%d) *******\n", dir_entry.nFiles);
		
		strcpy(dir_entry.files[dir_entry.nFiles].fname, filename);
		strcpy(dir_entry.files[dir_entry.nFiles].fext, extension);
		dir_entry.files[dir_entry.nFiles].fsize = 0;
		available_blocks blocks;
		int available_block = find_available_block(blocks);
		if(available_block == -1){
			printf("****** csc452_mknod: blocks Exceeded capacity *******\n");
			printf("****** csc452_mknod: return -EPERM / *******\n");
			return -EPERM;
		}
		dir_entry.files[dir_entry.nFiles].nStartBlock = available_block;

		dir_entry.nFiles++;
		push_to_disk(&dir_entry, dir.nStartBlock);
		push_to_disk(&blocks, 1);
		//not needed
		push_to_disk(&root, 0);
		/**
		 * @brief only needs a single block when init
		 * nNextBlock is -1
		 * 
		 */
		disk = fopen(".disk", "r+b");
		fseek(disk, BLOCK_SIZE * available_block, SEEK_SET);
		csc452_disk_block new_file;
		memset(new_file.data, 0, MAX_DATA_IN_BLOCK);
		new_file.nNextBlock = -1;
		push_to_disk(&new_file, available_block);
		
	} else{ //There was an error reading in the data from disk, so just close the file
		printf("**** csc452_mknod: No blocks associated with this location ***\n");
		
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

/**
 * @brief Removes a directory from disk
 * finds the occupied block and check if it doesn't have files
 * then cleans the block
 * remove it from root
 * turn off block bit
 * 
 * @param dir 
 * @param root 
 * @param blocks 
 * @return int 
 */
static int rm_dir(int index_root, csc452_root_directory root,
	available_blocks blocks)
	{
	struct csc452_directory dir = root.directories[index_root];
	printf("*** rm_dir: removing dir (%s)***\n", dir.dname);

	printf("*** rm_dir: fetching the dir entry***\n");
	FILE* disk = fopen(".disk", "r+b");
	int location_on_disk = BLOCK_SIZE*dir.nStartBlock;
	fseek(disk, location_on_disk, SEEK_SET);
	
	csc452_directory_entry dir_entry;
	dir_entry.nFiles = 0;
	memset(dir_entry.files, 0, MAX_FILES_IN_DIR*sizeof(struct csc452_file_directory));

	int num_items_successfully_read = fread(&dir_entry, BLOCK_SIZE, 1, disk); //Read in the directory's data, such as its files contained within
	fclose(disk);
	printf("*** rm_dir: did find one ? ***\n");
	//One block was successfully read, so proceed
	if(num_items_successfully_read != 0){ 
		printf("*** rm_dir: found (%s) ***\n", dir.dname);
		//remove this block
		printf("*** rm_dir: has files? ***\n");
		if(dir_entry.nFiles == 0){
			root.nDirectories --;
			
			printf("*** rm_dir: Empty! remove it ***\n");
			memset(&dir_entry, 0, sizeof(csc452_directory_entry));
			disk = fopen(".disk", "r+b");
			fseek(disk, location_on_disk, SEEK_SET);
			fwrite(&dir_entry, BLOCK_SIZE, 1, disk);
			fclose(disk);

			int location_in_blocks = dir.nStartBlock;
			//remove the dir from disk
			memset(&root.directories[index_root], 0, sizeof(struct csc452_directory));
			push_to_disk( &root, 0);
			//remove change the bit in available_blocks to available or 0
			blocks.block_table[location_in_blocks] = 0;
			push_to_disk(&blocks, 1);
			return 0;
		}
		printf("*** rm_dir: Oops! has files, return -ENOTEMPTY***\n");
		return -ENOTEMPTY;
		
		
	}
	printf("*** rm_dir: Oops! couldn't find matching blockm return -ENOENT***\n");
	return -ENOENT;
}
/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;
	//get the path figured out
	char path_c[strlen(path)+1], directory[MAX_FILENAME+1], filename[MAX_FILENAME+1], extension[MAX_EXTENSION+1];
	strcpy(path_c, path); strcpy(directory, ""); strcpy(filename, ""); strcpy(extension, "");
	sscanf(path_c, "/%[^/]/%[^.].%s", directory, filename, extension);
	printf("****** csc452_rmdir: does it have a dir *******\n");
	
	//mot sure why I have this one?
	if(strlen(filename) > 0){ 
		printf("****** csc452_rmdir: fail filename is not 0 length *******\n");
		printf("****** csc452_rmdir: return -ENOENT / *******\n");
		return -ENOENT; 
	}
	if(strlen(extension) > 0){ 
		printf("****** csc452_rmdir: fail extension is not 0 lemgth *******\n");
		printf("****** csc452_rmdir: return -ENOENT / *******\n");
		return -ENOENT; 
	}
	//check that this path is dir
		//find dir
		//if not found return err
		//if not -ENOTDIR
	printf("****** csc452_rmdir: fetch root and blocks ? *******\n");
	csc452_root_directory root = get_root();
	available_blocks blocks = get_block_directory();
	printf("****** csc452_rmdir: is this path the / ? *******\n");
	if(strcmp(path_c, "/") == 0){
		//must remove root
		//check root is empty
		
		if(root.nDirectories == 0){
			memset(&root, 0, sizeof(csc452_root_directory));
			memset(&blocks, 0, sizeof(available_blocks));
			push_to_disk(&root, 0);
			push_to_disk(&blocks, 0);
			return 0;
		}
		return -ENOTEMPTY;
	}

	//must search that dir exist?
	printf("****** csc452_rmdir: this is second level -> list files *******\n");
	printf("****** csc452_rmdir: try to find_dir *******\n");
	int index_root = find_dir(root, directory); 
	if(index_root == -1){
		printf("****** csc452_rmdir: dir doesn't exist (%s) *******\n", path);
		printf("****** csc452_rmdir: return -ENOENT / *******\n");
		return -ENOENT;
	}
	
	
	return rm_dir(index_root, root, blocks);
	
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
