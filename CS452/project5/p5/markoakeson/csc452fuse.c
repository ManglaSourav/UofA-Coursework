/*
 * Author: Partially Mark Oakeson
 * Class: CSC 452
 * Project 5, Filesystems
 * NOTE: Project partially works,  had a lot of trouble in general and ran out of time implementing any of the read/write to files
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
struct csc452_directory_entry{
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

struct csc452_root_directory{
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
 * Method retrieves root file ".disk" and reads it into the
 * root_directory struct
 * and returns it
 * @return A cs452_root_directory struct that is the root
 */
static csc452_root_directory getRoot(){
    FILE* rootDisk = fopen(".disk", "r+b");
    if(rootDisk == NULL){
        printf("ERROR NULL\n");
    }
    fseek(rootDisk, 0, SEEK_SET);
    csc452_root_directory retval;
    fread(&retval, BLOCK_SIZE, 1, rootDisk);
    fclose(rootDisk);
    return retval;
}

/**
 * Method returns whether inserted string is a directory or not
 * @param directory A char pointer for to check if it is a directory
 * @return 1 if true, 0 if false
 */
static int isDirectory(char* directory){
    csc452_root_directory cur;
    cur = getRoot();
    for(int i = 0; i <= cur.nDirectories; i++){
        if(strcmp(directory, cur.directories[i].dname) == 0){
            return 1;
        }
    }
    return 0;
}

/**
 * Method retrieves the address of the directory to assign to a
 * csc452_directory_entry struct
 * @param directory A char pointer for to check if it is a directory
 * @return An long for the address of the directory start position
 */
static long getDirectory(char* directory){
    long address = 0;
    csc452_root_directory cur;

    cur = getRoot();
    for(int i = 0; i <= cur.nDirectories; i++){
        if(strcmp(directory, cur.directories[i].dname) == 0){
            address = cur.directories[i].nStartBlock;
            return address;
        }
    }
    return address;
}
/**
 * Method returns whether inserted strings correspond to a file
 * existing in the system
 * @param directory A char pointer for to check if it is a directory
 * @param filename A char pointer for to check if it is a file
 * @return 1 if true, 0 if false
 */
static long fileExists(char* directory, char* filename){
    csc452_root_directory cur;
    csc452_directory_entry entry;
    cur = getRoot();
    for(int i = 0; i <= cur.nDirectories; i++){
        if(strcmp(directory, cur.directories[i].dname) == 0){
            FILE* disk = fopen(".disk", "r+b");
            fseek(disk,cur.directories[i].nStartBlock , SEEK_SET);

            fread(&entry, BLOCK_SIZE,1,disk);
            fclose(disk);
            for(int x = 0; x <= entry.nFiles; x++){
//                printf("FILE = %s\n", entry.files[x].fname);
//                printf("Location = %s\n", entry.files[x].nStartBlock);
                if(strcmp(filename, entry.files[x].fname) == 0){
                    return 1;
                }
            }
        }
    }
    return 0;
}

/**
 * Method retrieves the address of the file to be assigned to a struct
 * @param directory A char pointer for to check if it is a directory
 * @param filename A char pointer for to check if it is a file
 * @return A long for the address of the directory start positiong=
 */
static long getFile(char* directory, char* filename){
    csc452_root_directory cur;
    csc452_directory_entry entry;
//    FILE* disk = fopen(".disk", "r+b");
//    fseek(disk,address , SEEK_SET);
//
//    fread(&entry, BLOCK_SIZE,1,disk);
//    fclose(disk);

    cur = getRoot();
    for(int i = 0; i <= cur.nDirectories; i++){
        if(strcmp(directory, cur.directories[i].dname) == 0){
            FILE* disk = fopen(".disk", "r+b");
            fseek(disk,cur.directories[i].nStartBlock , SEEK_SET);

            fread(&entry, BLOCK_SIZE,1,disk);
            fclose(disk);
//            printf("FILES = %d\n", entry.nFiles);
            for(int x = 0; x <= entry.nFiles; x++){
                if(strcmp(filename, entry.files[x].fname) == 0){
                    return entry.files[x].nStartBlock;
                }
            }
        }
    }
    return 0;
}
struct csc452_disk_block{
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
//    printf("MADE IT TO GET ATTR\n");
    char directory[MAX_FILENAME+1]="\0";
    char filename[MAX_FILENAME+1]="\0";
    char extension[MAX_EXTENSION+1]="\0";

    sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); // Read in attributes

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else  {
        int exists = 0;
        exists = isDirectory(directory);

        int test = 0;
        int isFile = fileExists(directory, filename);
        if(strcmp(filename, "") == 0){
            test = 1;
        }
        if(exists && test) { // Directory exists and path !exists
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
        }
        else if(isFile && exists) { //If the path does exist and is a file:

            stbuf->st_mode = S_IFREG | 0666;
            stbuf->st_nlink = 2;
            stbuf->st_size = BLOCK_SIZE;
        }
        else {//Else return that path doesn't exist
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
    char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION +1];
//    printf("READDIR\n");

    sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); // Read in attributes
    directory[MAX_FILENAME] = '\0';
    filename[MAX_FILENAME] = '\0';
    extension[MAX_EXTENSION] = '\0';

    csc452_root_directory root;
    root = getRoot();

    //A directory holds two entries, one that represents itself (.)
    //and one that represents the directory above us (..)
    if (strcmp(path, "/") == 0) {
        filler(buf, ".", NULL,0);
        filler(buf, "..", NULL, 0);
        for(int i = 0; i < MAX_DIRS_IN_ROOT; i++){
            if(strcmp(root.directories[i].dname, "") != 0){ // Loop through all names that are not null to print
                filler(buf, root.directories[i].dname, NULL, 0);
            }
        }
    }
    else if(filename == '\0' && isDirectory(directory)){ // Show contents inside sub directory, and files
        filler(buf, ".", NULL,0);
        filler(buf, "..", NULL, 0);

        csc452_directory_entry curEntry;
        int address = getDirectory(directory);

        FILE* disk = fopen(".disk", "r+b");
        fseek(disk,address , SEEK_SET);

        fread(&curEntry, BLOCK_SIZE,1,disk);
        fclose(disk);

        for(int i = 0; i < curEntry.nFiles; i++){ // Process files for printing 'ls
            char fileName[MAX_FILENAME+1];
            strcpy(fileName, curEntry.files[i].fname);
            if(strcmp(curEntry.files[i].fext, "") != 0){
                strcat(fileName, ".");
            }
            strcat(fileName, curEntry.files[i].fext);
            if(strcmp(curEntry.files[i].fname, "") != 0){
                filler(buf, fileName, NULL, 0);
            }

        }
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
    printf("MADE IT to mkdir \n");

    (void) mode;
    (void) path;


    char directory[MAX_FILENAME+1]="\0";
    char filename[MAX_FILENAME+1]="\0";
    char extension[MAX_EXTENSION+1]="\0";

    sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); // Read in attributes


    if(strlen(directory) > MAX_FILENAME){ // Check if filename over limit of 8 chars
        return -ENAMETOOLONG;
    }


//    if(strcmp(filename, "") == 0){
//        return -EPERM; // File is being inserted in main directory
//    }


    csc452_root_directory rootDirectory = getRoot();


    if(isDirectory(directory)){
        return -EEXIST;
    }
    csc452_directory_entry directoryEntry;
    directoryEntry.nFiles = 0;

    int availableIndex = 0;
    for(int i = 1; i < MAX_DIRS_IN_ROOT;i++){ // Loop to find the next available index for inserting record
        if(strcmp(rootDirectory.directories[i].dname,"") == 0){
            availableIndex = i;
            break;
        }
    }

    strcpy(rootDirectory.directories[availableIndex].dname, directory);
    printf("dname = %str\n", rootDirectory.directories[availableIndex].dname);
    long address = availableIndex * BLOCK_SIZE;

    rootDirectory.nDirectories++;
    if(rootDirectory.nDirectories > MAX_DIRS_IN_ROOT){
        return -1; // ERROR for inserting too many directories
    }

    rootDirectory.directories[availableIndex].nStartBlock = address;
//    printf("ADDRESS = %lu \n", address);

//    printf("MADE IT TO DISK 1 \n");
    // Write updates to rootDirectory
    FILE* disk = fopen(".disk", "r+b");
    if(disk == NULL){
        printf("ERROR NULL\n");
    }

    fseek(disk, 0, SEEK_SET);
    fwrite(&rootDirectory, BLOCK_SIZE, 1, disk); //Write updates to root

    fseek(disk, address, SEEK_SET);
    directoryEntry.nFiles = 0;
    fwrite(&directoryEntry, BLOCK_SIZE, 1, disk); // WRite updates for directory
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
//    printf("MKNOD\n");
    (void) path;
    (void) mode;
    (void) dev;

    char directory[MAX_FILENAME+1]="\0";
    char filename[MAX_FILENAME+1]="\0";
    char extension[MAX_EXTENSION+1]="\0";

    sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); // Read in attributes

    csc452_root_directory root;
    root = getRoot();

    //Error handling
    if(strcmp(filename, "") == 0){ // Error, trying to create in wrong directory
        return -EPERM;
    }
    if(fileExists(directory, filename)){
        return -EEXIST;
    }
    if(strlen(directory) > MAX_FILENAME){
        return -ENAMETOOLONG;
    }
    if(strlen(extension) > MAX_EXTENSION){
        return -ENAMETOOLONG;
    }

    csc452_disk_block block;

    csc452_directory_entry entry;
    int address = getDirectory(directory);

    FILE* disk = fopen(".disk", "r+b");
    fseek(disk,address , SEEK_SET);

    fread(&entry, BLOCK_SIZE,1,disk);
    fclose(disk);

    //Copy over filename and extension to create
    strcpy(entry.files[entry.nFiles].fname, filename);
    strcpy(entry.files[entry.nFiles].fext, extension);
    long nextSpot = root.nDirectories * BLOCK_SIZE;
//    printf("NEXT SPOT = %lu\n", nextSpot);
    entry.files[entry.nFiles].nStartBlock = nextSpot;
    entry.nFiles++;
//    printf("MKDIR NFILES = %d\n", entry.nFiles);


// Update disk at root and entry
    FILE *updateDisk = fopen(".disk", "r+b");
    fseek(updateDisk, 0, SEEK_SET);
    root.nDirectories++;
    fwrite(&root, BLOCK_SIZE, 1, updateDisk);

    fseek(updateDisk,address, SEEK_SET);
    fwrite(&entry, BLOCK_SIZE,1,updateDisk);

    fseek(updateDisk,nextSpot, SEEK_SET);
    fwrite(&block, BLOCK_SIZE,1,updateDisk);
    fclose(updateDisk);

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

    char directory[MAX_FILENAME+1]="\0";
    char filename[MAX_FILENAME+1]="\0";
    char extension[MAX_EXTENSION+1]="\0";
    sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); // Read in attributes

    int test = fileExists(directory, filename);
    if(isDirectory(directory)  || !test){    //check to make sure path exists
        return -EISDIR;
    }

    //check that size is > 0
    //check that offset is <= to the file size
    int index = 0;
    csc452_directory_entry cur;
    csc452_directory_entry entry;

    for(int i = 0; i < cur.nFiles; i++){
        if(strcmp(filename, cur.files[i].fname)==0){ // REtrieve requested entry
            entry = cur;
            index = i;
            break;
        }
    }

    if(entry.files[index].fsize > 0){//check that size is > 0
        if(entry.files[index].fsize > size){    //check that offset is <= to the file size

            return -EFBIG;
        }

    }

    //read in data
    FILE * disk = fopen(".disk", "r");
    fseek(disk, entry.files->nStartBlock, SEEK_SET);
    fread(buf + entry.files->nStartBlock, BLOCK_SIZE, 1, disk);
    fclose(disk);
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

    char directory[MAX_FILENAME+1]="\0";
    char filename[MAX_FILENAME+1]="\0";
    char extension[MAX_EXTENSION+1]="\0";
    sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); // Read in attributes


    //check to make sure path exists
    long startOfFile;
    if(isDirectory(directory) && fileExists(directory, filename)){
         startOfFile = getFile(directory, filename);
    }

    csc452_directory_entry cur;
    csc452_directory_entry entry;
    long address;
    address = getDirectory(directory);

    FILE* disk = fopen(".disk", "r+b");
    if(disk == NULL){
        printf("ERROR NULL\n");
    }

    fseek(disk, address, SEEK_SET);
    //Write updated struct to disk with the removed file
    fwrite(&entry, BLOCK_SIZE, 1, disk);

    fclose(disk);

    int index = 0;
    for(int i = 0; i < cur.nFiles; i++){
        if(strcmp(filename, cur.files[i].fname)==0){
            entry = cur;
            index = i;
            break;
        }
    }

    if(entry.files[index].fsize > 0){//check that size is > 0
        if(entry.files[index].fsize > size){    //check that offset is <= to the file size

            return -EFBIG;
        }

    }

    FILE *updateDisk = fopen(".disk", "r+b");
    csc452_disk_block block;

    fseek(updateDisk, entry.files[index].nStartBlock , SEEK_SET);
    fread(&block, BLOCK_SIZE, 1, updateDisk);



    //write data
    if((strlen(block.data) + size) <= BLOCK_SIZE){
        strcpy(block.data + startOfFile, buf);
        fseek(disk, entry.files[index].nStartBlock, SEEK_SET);
        fwrite(&block, BLOCK_SIZE, 1, disk);
    }
    else{
        fclose(updateDisk);
        return -ENOSPC;
    }

    fclose(updateDisk);

    //return success, or error

    return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{ //Can only remove after another directory added
    (void) path;

    csc452_root_directory root;
    root = getRoot();

    char directory[MAX_FILENAME+1]="\0";
    char filename[MAX_FILENAME+1]="\0";
    char extension[MAX_EXTENSION+1]="\0";

    sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); // Read in attributes

    if(!isDirectory(directory)){
        return -ENOTDIR;
    }

    for(int i = 0; i < MAX_DIRS_IN_ROOT; i++){
        if(strcmp(directory, root.directories[i].dname) == 0){
            csc452_directory_entry toRemove; //= malloc(sizeof(csc452_directory_entry));

            int address = getDirectory(directory);

            FILE* disk = fopen(".disk", "r+b");
            fseek(disk,address , SEEK_SET);

            fread(&toRemove, BLOCK_SIZE,1,disk);
            fclose(disk);

            root.directories[i].nStartBlock = 0; // RESET start block since we remove directory
            root.nDirectories--;
            strcpy(root.directories[i].dname, "");
            printf("Files to Remove = %d\n", toRemove.nFiles);
            if(toRemove.nFiles != 0){
                return -ENOTEMPTY;
            }

            // Write updates to disk
            FILE* updateDisk = fopen(".disk", "r+b");
            if(updateDisk == NULL){
                printf("ERROR NULL\n");
            }

            fseek(updateDisk, 0, SEEK_SET);
            fwrite(&root, BLOCK_SIZE, 1, updateDisk);

            fclose(updateDisk);
        }
    }

    return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
    (void) path;


    char directory[MAX_FILENAME+1]="\0";
    char filename[MAX_FILENAME+1]="\0";
    char extension[MAX_EXTENSION+1]="\0";

    sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); // Read in attributes

    if(isDirectory(directory)){
        return -EISDIR;
    }
    else if(!fileExists(directory, filename)){
        return -ENOENT;
    }

    csc452_directory_entry entry;
    int address;
    address = getDirectory(directory);

    FILE* disk = fopen(".disk", "r+b");
    if(disk == NULL){
        printf("ERROR NULL\n");
    }

    fseek(disk, address, SEEK_SET);
    //Write updated struct to disk with the removed file
    fwrite(&entry, BLOCK_SIZE, 1, disk);

    fclose(disk);

    for(int i = 0; i < entry.nFiles; i++){
        if(strcmp(entry.files[i].fname, filename) == 0 ){
                entry.files[i].nStartBlock = 0; // RESET start block since we remove file
                entry.nFiles--;
                strcpy(entry.files[i].fname, "");
            csc452_directory_entry toRemove; //= malloc(sizeof(csc452_directory_entry));

            int address = getDirectory(directory);

            FILE* disk = fopen(".disk", "r+b");
            fseek(disk,address , SEEK_SET);

            fread(&toRemove, BLOCK_SIZE,1,disk);
            fclose(disk);
//                printf("Files to Remove = %d\n", toRemove.nFiles);
                if(toRemove.nFiles != 0){
                    return -ENOTEMPTY;
                }

                FILE* updateDisk = fopen(".disk", "r+b");
                if(updateDisk == NULL){
                    printf("ERROR NULL\n");
                }

                fseek(updateDisk, entry.files[i].nStartBlock, SEEK_SET);
                //Write updated struct to disk with the removed file
                fwrite(&entry, BLOCK_SIZE, 1, updateDisk);

                fclose(updateDisk);
            break;
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