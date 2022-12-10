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


static int get_directory(char *directory, struct csc452_root_directory * root){
	FILE* file;
	file =fopen(".disk", "rb");
	int res = -1;
	if(!file)
	{
		file = fopen(".disk", "wb");
	}
	else
	{
		
		while(fread(root, sizeof(struct csc452_root_directory), 1, file)>=1)
		{
			if(strcmp(root->directories->dname, directory)==0)
			{
				res = ftell(file)-sizeof(struct csc452_root_directory);
				break;
			}
			
		}
	}
	fclose(file);
	return res;
}

static int get_subdirectory(const char *files, struct csc452_directory_entry * root){
        FILE* file;
        file =fopen(".disk", "rb");
        int res = -1;
        if(!file)
        {
                file = fopen(".disk", "wb");
        }
        else
        {
                
                while(fread(root, sizeof(struct csc452_directory_entry), 1, file)>=1)
                {
                        if(strcmp(root->files->fname, files)==0)
                        {
                                res = ftell(file)-sizeof(struct csc452_directory_entry);
                                break;
                        }
                        
                }
        }
        fclose(file);
        return res;
}

void pathInfo(const char * path, char * directory, char * filename, char * extension)
{
	directory[0] ='\0';
	filename[0]='\0';
	extension[0]='\0';
	scanf(path,  "/%[^/]/%[^.].%s", directory, filename, extension);
	
	directory[MAX_FILENAME] = '\0';
	filename[MAX_FILENAME] = '\0';
	extension[MAX_EXTENSION] = '\0';
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

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {//if the path does exist, and is a directory
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}else{
		char directory[MAX_FILENAME+1];
		char filename[MAX_FILENAME+1];
		char extension[MAX_EXTENSION+1];
		pathInfo(path, directory, filename, extension);

		struct csc452_directory_entry *entry = malloc(sizeof(struct csc452_directory_entry));
		int find = get_subdirectory(filename, entry);
		if(find!=-1)
		{
			//found directory
			if(strlen(filename)==0)//if filenmae is empty, we are looing for the directory
			{
				stbuf->st_mode = S_IFDIR | 0755;
                		stbuf->st_nlink = 2;
			}
			else{
				int i, dir_found = 0;
				for(i=0; i<entry->nFiles; i++)
				{
					if(strcmp(entry->files[i].fname, filename)==0 && strcmp(entry->files[i].fext, extension)==0)
					{
						dir_found =1;
						break;
					}
				}
				if(dir_found==1)
				{
					//If the path does exist and is a file:
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 1;
					stbuf->st_size = entry->files[i].fsize;
				}
				else//file does not exist
				{
					res=-ENOENT;
				}
			}
		}
		else//directory not exist
		{
			res =-ENOENT;
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
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		
		FILE * file;
		file =fopen(".disk","rb");
		if(!file)
		{
			file =fopen(".disk", "wb");
		}
		else
		{
			struct csc452_root_directory current;
			//int i=0;
			while(fread(&current, sizeof(struct csc452_root_directory), 1, file) >=1)
			{
				char* dirname = current.directories->dname;
				filler(buf, dirname, NULL, 0); //directory to the list
			}
			
		}
		fclose(file);
	}
	else//files in subdirectories
	{
		char directory[MAX_FILENAME+1];
		char filename[MAX_FILENAME+1];
		char extension[MAX_EXTENSION+1];
		pathInfo(path, directory, filename, extension);
		struct csc452_directory_entry * root = malloc(sizeof(struct csc452_directory_entry));
		int location = get_subdirectory(filename, root);
		if(location!=-1)//directory entry was found. 
		{
			filler(buf, ".", NULL, 0);
			filler(buf, "..", NULL, 0);
			int i;
			//check if the file already exist 
			for(i=0;i<root->nFiles; i++)
			{
				char fullfilename[MAX_FILENAME +MAX_EXTENSION +2];
				strcpy(fullfilename, root->files[i].fname);
				if(strlen(root->files[i].fext)!=0)
				{
					strcat(fullfilename, ".");
					strcat(fullfilename, root->files[i].fext);
				}
				filler(buf, fullfilename, NULL, 0);
			}
		}
		else{//directory entry invalid, or not found
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
	(void) path;
	(void) mode;

	char directory[MAX_FILENAME +1];
	char filename[MAX_FILENAME +1];
	char extension[MAX_EXTENSION +1];
	
	pathInfo(path, directory, filename, extension);
	
	int res =0;

	if(strcmp(path, "/")!=0){
		return -EPERM;
	}
	if(strlen(directory)> MAX_FILENAME){//if the name is beyond 8 chars
		printf("File directory is over 8 chars.\n");
		return -ENAMETOOLONG;
	}
	//directory exist?
	struct csc452_root_directory *entry = malloc(sizeof(struct csc452_root_directory));
	int find = get_directory(directory, entry);
	if(find !=-1)//yes
	{
		return -EEXIST;
	}
	FILE *file;
	if(res==0)//not exist
	{
		file = fopen(".disk", "ab");
		if(!file)
		{
			res =-1;
		}
		else
		{
			
			csc452_root_directory current;
			
			strcpy(current.directories->dname, directory);
			
			current.nDirectories =0;
			
			if(fwrite(&current, sizeof(struct csc452_root_directory), 1, file)<1)
			{
				printf("failed to write to .disk.\n");
			}
		}
		fclose(file);
	}
	return res;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 * Note that the mknod shell command is not the one to test this.
 * mknod at the shell is used to create "special" files and we are
 * only supporting regular files.
 *
 * This function should add a new file to a subdirectory, and should update the subdirectory entry appropriately with the modified information
 * 0 on success
 * -ENAMETOOLONG if the name is beyond 8.3 chars
 *  -EPERM if the file is trying to be created in the root dir
 *  -EEIXT if the file already exists 
 */
static int csc452_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) path;
	(void) mode;
    	(void) dev;
	
	char directory[MAX_FILENAME+ 1];
	char filename[MAX_FILENAME+ 1];
	char extension[MAX_EXTENSION+ 1];

	pathInfo(path, directory, filename, extension);
	if(strlen(directory)>MAX_FILENAME || strlen(filename) >MAX_FILENAME || strlen(extension) >MAX_EXTENSION)
	{
		return -ENAMETOOLONG;
	}
	struct csc452_directory_entry *entry = malloc(sizeof(struct csc452_directory_entry));
	int diskloc = get_subdirectory(filename, entry);
	int i;
	for(i=0; i<entry->nFiles; i++)
	{
		if(strcmp(entry->files[i].fname, filename)==0 && strcmp(entry->files[i].fext, extension)==0)
		{//there is already existing with same name
			return -EEXIST;
		}
	}//directory is not full && file is trying to be created in the root dir. 
	if(entry->nFiles == MAX_FILES_IN_DIR)
	{
		return -EPERM;
	}
	int index = entry->nFiles;
	strcpy(entry->files[index].fname, filename);
	strcpy(entry->files[index].fext, extension);
	entry->files[index].fsize=0;
	entry->files[index].nStartBlock = -1;
	entry->nFiles++;

	FILE *file = fopen(".disk", "r+b");
	fseek(file, diskloc, SEEK_SET);//move file pointer to diskloc
	fwrite(entry, sizeof(struct csc452_directory_entry), 1, file);
	fclose(file);

	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 * This function should read the data in the file denoted by path into buf, starting 
 * at offset. 
 *
 * size read on sucess
 * -EISDIR if the path is directory
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	
	char directory[MAX_FILENAME +1];
	char filename[MAX_FILENAME +1];
	char extension[MAX_EXTENSION +1];

	pathInfo(path, directory, filename, extension);

	struct csc452_directory_entry *entry = malloc(sizeof(struct csc452_directory_entry));
//	int diskloc = get_subdirectory(filename, entry);

	int i, fileIndex = -1;
	for(i=0; i<entry->nFiles; i++)
	{
		if(strcmp(entry->files[i].fname, filename) ==0 && strcmp(entry->files[i].fext, extension)==0)
		{//found file
			fileIndex=i;
			break;
		}
	}
	//size > 0?
	if(size==0)
	{
		return 0;
	}
	//check that offset is <= to the file size
	if(offset>entry->files[i].fsize)
	{
		return -EFBIG;

	}
	//read in data
	int locationRead = ((entry->files[fileIndex].nStartBlock * 512) + offset);
	FILE *file = fopen(".disk", "rb");
	fseek(file, locationRead, SEEK_SET);
	int read = fread(buf, 1, size, file);
	fclose(file);

	return read;
}

/*
 * Write size bytes from buf into file starting from offset
 * 
 * size on success
 * -EFBIG if the offset is beyond the file size (but handle appends)
 * -ENOSPC if the disk is full
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	char directory[MAX_FILENAME +1];
	char filename[MAX_FILENAME +1];
	char extension[MAX_EXTENSION +1];

	pathInfo(path, directory, filename, extension);
	
	struct csc452_directory_entry * entry = malloc(sizeof(struct csc452_directory_entry));
	int diskloc = get_subdirectory(filename, entry);
	int i, index = -1;
	for(i=0; i<entry->nFiles; i++)
	{
		if(strcmp(entry->files[i].fname, filename) ==0 && strcmp(entry->files[i].fext, extension)==0)
		{//found the file
			index = i;
			break;
		}
	}
	//is size >0?
	if(size==0)
	{
		return 0;
	}
	//check that offset is <= to the file size
	if(offset> entry->files[i].fsize)
	{
		return -EFBIG;
	}

	//file not assigned in block, use bit map to find an open block. 
	FILE *file;
	if(entry->files[index].nStartBlock ==-1)
	{
		file = fopen(".disk", "r+b");
		if(!file)
		{
			return 0;//disk cannot be opened. 
		}
		//use bitmap to find a free block and assign to file
		unsigned char current;
		int freeBlockNumber =-1;
		for(i=0; i<(BLOCK_SIZE *2.5); i++)
		{
			current = fgetc(file);
			printf("Read %d from file\n", current);
			int j;
			int mask = 0b10000000;
			for(j=0; j<8; j++)
			{
				printf("mask %d and current byte %d\n", mask, current);
				if((mask & current)==0)
				{
					freeBlockNumber = ((i*8)+j)+3;
					current |= mask;

					long prev = ftell(file) -sizeof(unsigned char);
					fseek(file, prev, SEEK_SET);
					//int res = fputc(current, file);
					fclose(file);
					break;
				}
				mask >>=1;
			}
			if(freeBlockNumber !=-1)
			{
				break;
			}

		}
		if(freeBlockNumber ==-1)
		{
			printf("out of disk space\n");
			return 0;
		}
		entry->files[index].nStartBlock = freeBlockNumber;
	}
	int blockUse = entry->files[index].nStartBlock;
	file = fopen(".disk", "r+b");
	fseek(file, ((blockUse * BLOCK_SIZE) +offset), SEEK_SET);
	int element = fwrite(buf, size, 1, file);
	fclose(file);

	entry->files[index].fsize = size;
	file = fopen(".disk", "r+b");
	fseek(file, diskloc, SEEK_SET);
	fwrite(entry, sizeof(struct csc452_directory_entry), 1, file);
	fclose(file);
	if(element !=1)
	{
		size =0;
	}
	return size;
}

/*
 *
 * return the occurence of occur in the str. 
 * */
static int countOccur(const char *str, const char occur){
	int c = 0;
	int len = strlen(str);
	int i;
	char current;
	for(i=0; i<len; i++){
		current = str[i];
		if(current==occur)
		{
			c++;
		}
	}
	return c;
}

/*
 * get the directory name and dir list. 
 * */
static bool is_a_directory(const char *path_name)
{
	bool found= false;
	char buffer[sizeof(csc452_directory_entry)];
	memset(buffer, 0, sizeof(csc452_directory_entry));
	FILE *directories = fopen(".disk", "a+");
	while(fread(buffer, sizeof(csc452_directory_entry), 1, directories))
	{
		csc452_root_directory *curr_dir = (csc452_root_directory*) buffer;
		if(strcmp(path_name, curr_dir->directories->dname)==0)
		{
			found = true;
			break;

		}
	}
	fclose(directories);
	return found;
}


static void rmchar(const char *str, char target)
{
	char *src;
	char *dist;
	for(src = dist = (char*)str; *src!='\0'; src++)
	{
		*dist = *src;
		if(*dist!=target) dist++;
	}
	*dist='\0';
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;
        if(countOccur(path, '/')>1){
		printf("deletion is not possible, it's not a directory\n");
		return -ENOTDIR;
	}
	if(path[0] =='/'){
		rmchar(path,'/');
	}
	printf("rmdir on path %s\n", path);

	if(is_a_directory(path) ==false)
	{
		printf("%s is not a directory\n", path);
		return -ENOENT;
	}
	
	csc452_root_directory buffer;
	memset(&buffer, 0, sizeof(csc452_root_directory));

	FILE *directory_f = fopen(".disk", "r");
	FILE *temporary = fopen(".temp","a");

	if(temporary ==NULL)
	{
		printf("NULL by fopen");
		exit(-1);
	}

	if(directory_f==NULL)
	{
		printf("NULL by fopen,");
		exit(-1);
	} 
	while(fread(&buffer, sizeof(csc452_root_directory), 1, directory_f))
	{
		if(strcmp(buffer.directories->dname, path)==0)
		{
			if(buffer.nDirectories>0)
			{
				fclose(directory_f);
				fclose(temporary);
				system("rm .temp");
				return -ENOTEMPTY;
			}
			printf("\nRMDIR: directory has been removed.\n");
		}
		else
		{
			fwrite(&buffer, sizeof(csc452_root_directory), 1, temporary);
		}
	}
	fclose(directory_f);
	fclose(temporary);

	system("cp .temp .disk");
	system("rm .temp");

	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
        (void) path;
	char directory[MAX_FILENAME +1];
	char filename[MAX_FILENAME+1];
	char extension[MAX_EXTENSION+1];
	pathInfo(path, directory, filename, extension);
	if(strlen(filename)==0)
	{
		return -EISDIR;
	}
	struct csc452_directory_entry *entry = malloc(sizeof(struct csc452_directory_entry));
	int disklocation = get_subdirectory(filename, entry);
	int i, fileIndex =-1;
	for(i=0; i<entry->nFiles; i++)
	{
		if(strcmp(entry->files[i].fname, filename)==0 && strcmp(entry->files[i].fext, extension)==0)
		{//found file
			fileIndex=i;
			break;
		}
	}
	if(fileIndex==-1)
	{
		return -ENOENT;//file not found
	}
	//remove bitmap
	FILE *file = fopen(".disk","r+b");
	if(!file)
	{
		return -ENOENT; //could not open .disk
	}
	int byteToWrite = (entry->files[fileIndex].nStartBlock-3)/8;
	int mask = 0b10000000 >>((entry->files[fileIndex].nStartBlock-3)%8);

	fseek(file, byteToWrite, SEEK_SET);
	unsigned char current = fgetc(file);
	current ^= mask;

	fseek(file, byteToWrite, SEEK_SET);
	int res = fputc(current, file);

	fclose(file);
	
	entry->files[fileIndex] = entry->files[entry->nFiles-1];
	entry->nFiles--;
	file = fopen(".disk", "r+b");
	fseek(file, disklocation, SEEK_SET);
	fwrite(entry, sizeof(struct csc452_directory_entry), 1, file);
	fclose(file);

        return res;
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
