/*
FUSE: Filesystem in Userspace


gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452


 */
/* FIle: csc452fuse.c
 * Author: Chris Herrera (with starter code by Prof Misurda)
 * Purpose: Simulate file system syscalls in this userspace program
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
                return 0;
        } else  {
                FILE *disk; 
                disk = fopen(".disk", "rb+");   
                char directory[MAX_FILENAME+1];
                char filename[MAX_FILENAME+1] = "";
                char extension[MAX_EXTENSION+1];
                sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
                // Seek to the beginning of .disk to load the root directory
                fseek(disk, 0, SEEK_SET);

                csc452_root_directory root;
                fread(&root, sizeof(csc452_root_directory), 1, disk);
                int numDirs = root.nDirectories;

                if (strcmp(filename, "") == 0) { // filename not given, look for a directory
                        int i;
                        for (i = 0; i < numDirs; i++) {
                                if (strcmp(root.directories[i].dname, directory) == 0) {
                                        //If the path does exist and is a directory:
                                        stbuf->st_mode = S_IFDIR | 0755;
                                        stbuf->st_nlink = 2;
                                        fclose(disk);
                                        return 0;
                                }
                        }
                        // If the directory isn't found, return -ENOENT
                        res = -ENOENT;
                } else { // searching for a file within a dir
                        int i;
                        for (i = 0; i < numDirs; i++) { // find the dir first
                                if (strcmp(root.directories[i].dname, directory) == 0) {
                                        long dirStart = root.directories[i].nStartBlock;
                                        fseek(disk, dirStart, SEEK_SET);
                                        csc452_directory_entry existingDir;
                                        fread(&existingDir, sizeof(csc452_directory_entry), 1, disk);
                                        int j;
                                        int numFiles = existingDir.nFiles;
                                        for (j = 0; j < numFiles; j++) { // Scan each file name in each directory until we find the one we're searching for
                                                if (strcmp(existingDir.files[j].fname, filename) == 0) {
                                                        // file exists!
                                                        stbuf->st_mode = S_IFREG | 0666;
                                                        stbuf->st_nlink = 2;
                                                        stbuf->st_size = existingDir.files[j].fsize;
                                                        fclose(disk);
                                                        return 0;
                                                }
                                        }
                                }
                        }
                        res = -ENOENT; // return not found if we search through everything and don't find it
                }
                fclose(disk);  
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

        FILE *disk; 
        disk = fopen(".disk", "rb+");   
        char directory[MAX_FILENAME+1];
        char filename[MAX_FILENAME+1] = "";
        char extension[MAX_EXTENSION+1];
        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
        fseek(disk, 0, SEEK_SET);

        csc452_root_directory root;
        fread(&root, sizeof(csc452_root_directory), 1, disk);
        int numDirs = root.nDirectories;

        //A directory holds two entries, one that represents itself (.) 
        //and one that represents the directory above us (..)
        if (strcmp(path, "/") == 0) {
                filler(buf, ".", NULL,0);
                filler(buf, "..", NULL, 0);
                int i;
                for (i = 0; i < numDirs; i++) {
                        filler(buf, root.directories[i].dname, NULL, 0);
                }
        }
        else {
                if (strcmp(filename, "") == 0) {
                        int i;
                        for (i = 0; i < numDirs; i++) {
                                if (strcmp(root.directories[i].dname, directory) == 0) {
                                        //If the path does exist and is a directory:
                                        long dirStart = root.directories[i].nStartBlock;
                                        fseek(disk, dirStart, SEEK_SET);

                                        csc452_directory_entry existingDir;
                                        fread(&existingDir, sizeof(csc452_directory_entry), 1, disk);
                                        int j;
                                        int numFiles = existingDir.nFiles;
                                        for (j = 0; j < numFiles; j++) { // list each file in the dir
                                                filler(buf, existingDir.files[j].fname, NULL, 0);
                                        }
                                        fclose(disk);
                                        return 0;
                                }
                        }
                        fclose(disk);
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

        FILE *disk; 
        disk = fopen(".disk", "rb+");  
        char directory[MAX_FILENAME+1];
        char filename[MAX_FILENAME+1] = "";
        char extension[MAX_EXTENSION+1];
        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

        fseek(disk, 0, SEEK_SET);

        csc452_root_directory root;
        fread(&root, sizeof(csc452_root_directory), 1, disk);
        int numDirs = root.nDirectories;
        if ((strlen(filename) > MAX_FILENAME+1) | (strlen(extension) > MAX_EXTENSION+1)) {
                fclose(disk);
                return -ENAMETOOLONG;
        }
        if (numDirs >= MAX_DIRS_IN_ROOT) {
                fprintf(stderr, "Root is full!\n");
                return -1;
        }
        else {
                int bitmap[10240]; // 10240 == number of blocks
                // get bitmap to figure out where to put directory
                long freeLocation; 
                fseek(disk, -sizeof(bitmap), SEEK_END);  // bitmap is stored at the end of .disk
                fread(&bitmap, sizeof(bitmap), 1, disk);   
                for (int i = 1; i < 10240; i++) {
                        if (bitmap[i] == 0) {
                                freeLocation = i * BLOCK_SIZE;
                                bitmap[i] = 1;
                                break;
                        }
                }
                // After update, write the bitmap back to disk
                fseek(disk, -sizeof(bitmap), SEEK_END);
                fwrite(&bitmap, sizeof(bitmap), 1, disk);

                // create a directory entry and write it to disk
                csc452_directory_entry newDir;
                newDir.nFiles = 0;
                fseek(disk, freeLocation, SEEK_SET);
                fwrite(&newDir, sizeof(csc452_directory_entry), 1, disk);
                // update the directory in root
                struct csc452_directory dirInRoot;
                strncpy(dirInRoot.dname, directory, strlen(directory));
                dirInRoot.nStartBlock = freeLocation;
                root.directories[numDirs] = dirInRoot;
                root.nDirectories = numDirs + 1;
                // write root back to disk
                fseek(disk, 0, SEEK_SET);
                fwrite(&root, sizeof(csc452_root_directory), 1, disk);
        }
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

        FILE *disk; 
        disk = fopen(".disk", "rb+");   
        char directory[MAX_FILENAME+1];
        char filename[MAX_FILENAME+1] = "";
        char extension[MAX_EXTENSION+1];
        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
        fseek(disk, 0, SEEK_SET);

        csc452_root_directory root;
        fread(&root, sizeof(csc452_root_directory), 1, disk);
        int numDirs = root.nDirectories;
        if ((strlen(filename) > MAX_FILENAME+1) | (strlen(extension) > MAX_EXTENSION+1)) {
                fclose(disk);
                return -ENAMETOOLONG;
        }
        //A directory holds two entries, one that represents itself (.) 
        //and one that represents the directory above us (..)
        if (strcmp(path, "/") == 0) {
                fclose(disk);
                return -EPERM;
        }
        else {
                if (strcmp(filename, "") == 0) {
                        fclose(disk);
                        return -EPERM;
                }
                else {
                        int i;
                        for (i = 0; i < numDirs; i++) {
                                if (strcmp(root.directories[i].dname, directory) == 0) {
                                        //If the path does exist and is a directory:
                                        long dirStart = root.directories[i].nStartBlock;
                                        fseek(disk, dirStart, SEEK_SET);
                                        // now read the csc452_directory_entry
                                        csc452_directory_entry existingDir;
                                        fread(&existingDir, sizeof(csc452_directory_entry), 1, disk);
                                        int j;
                                        int numFiles = existingDir.nFiles;
                                        if (numFiles >= MAX_FILES_IN_DIR) {
                                                fclose(disk);
                                                return -EEXIST; // Directory is full!
                                        }
                                        else {
                                                // proceed to create the empty file
                                                struct csc452_file_directory newFile;
                                                int bitmap[10240];
                                                // get bitmap to find a free space for the file disk block
                                                fseek(disk, -sizeof(bitmap), SEEK_END); 
                                                fread(&bitmap, sizeof(bitmap), 1, disk);   
                                                long freeSpace; 
                                                for (j = 1; j < 10240; j++) {
                                                        if (bitmap[j] == 0) {
                                                                bitmap[j] = 1;
                                                                freeSpace = j * BLOCK_SIZE;
                                                                break;
                                                        }
                                                }
                                                newFile.nStartBlock = freeSpace;
                                                // set file metadata 
                                                strncpy(newFile.fname, filename, strlen(filename));
                                                strncpy(newFile.fext, extension, strlen(extension));
                                                newFile.fsize = 0; 
                                                existingDir.files[numFiles] = newFile;
                                                numFiles++;
                                                existingDir.nFiles = numFiles;

                                                fseek(disk, -sizeof(bitmap), SEEK_END);
                                                fwrite(&bitmap, sizeof(bitmap), 1, disk);

                                                fseek(disk, dirStart, SEEK_SET);

                                                csc452_disk_block emptyBlock; // empty block for the new file
                                                fwrite(&existingDir, sizeof(csc452_directory_entry), 1, disk);

                                                fseek(disk, freeSpace, SEEK_SET); 
                                                fwrite(&emptyBlock, sizeof(csc452_disk_block), 1, disk);
                                        }
                                        fclose(disk);
                                        return 0;
                                }
                        }
                } 
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
        FILE *disk; 
        disk = fopen(".disk", "rb+");   
        char directory[MAX_FILENAME+1];
        char filename[MAX_FILENAME+1] = "";
        char extension[MAX_EXTENSION+1];
        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
        fseek(disk, 0, SEEK_SET);

        csc452_root_directory root;
        fread(&root, sizeof(csc452_root_directory), 1, disk);
        int numDirs = root.nDirectories;

        //A directory holds two entries, one that represents itself (.) 
        //and one that represents the directory above us (..)
        if (strcmp(path, "/") == 0) {
                fclose(disk);
                return -EPERM;
        }
        else {
                if (strcmp(filename, "") == 0) {
                        fclose(disk);
                        return -EPERM;
                }
                else {
                        int i;
                        for (i = 0; i < numDirs; i++) {
                                if (strcmp(root.directories[i].dname, directory) == 0) {
                                        //If the path does exist and is a directory:
                                        long dirStart = root.directories[i].nStartBlock;
                                        fseek(disk, dirStart, SEEK_SET);
                                        // now read the csc452_directory_entry
                                        csc452_directory_entry existingDir;
                                        fread(&existingDir, sizeof(csc452_directory_entry), 1, disk);
                                        int j;
                                        int numFiles = existingDir.nFiles;
                                        long startBlock;
                                        for (j = 0; j < numFiles; j++) {                         
                                                if (strcmp(existingDir.files[j].fname, filename) == 0) {
                                                        // file exists!
                                                        startBlock = existingDir.files[j].nStartBlock;       
                                                        existingDir.files[j].fsize = size;
                                                        if ((size > 0) && (offset < startBlock + MAX_DATA_IN_BLOCK)) { 
                                                                // if size > 0 and offset is within the block
                                                                // load the diskblock
                                                                csc452_disk_block block;
                                                                fseek(disk, startBlock, SEEK_SET);
                                                                fread(&block, sizeof(csc452_disk_block), 1, disk);
                                                                // copy it into buffer
                                                                memcpy(buf, &block.data[offset], size);
                                                                fclose(disk);
                                                                return size;
                                                        }
                                                        else {
                                                                fclose(disk);
                                                                return -EFBIG;
                                                        }
                                                }
                                        }
                                        fclose(disk);
                                        return 0;
                                }
                        }
                } 
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
        (void) buf;
        (void) offset;
        (void) fi;
        (void) path;

        //check to make sure path exists
        //check that size is > 0
        //check that offset is <= to the file size
        //write data
        //return success, or error
        printf("In my write\n");
        FILE *disk; 
        disk = fopen(".disk", "rb+");   
        char directory[MAX_FILENAME+1];
        char filename[MAX_FILENAME+1] = "";
        char extension[MAX_EXTENSION+1];
        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
        fseek(disk, 0, SEEK_SET);

        csc452_root_directory root;
        fread(&root, sizeof(csc452_root_directory), 1, disk);
        int numDirs = root.nDirectories;

        //A directory holds two entries, one that represents itself (.) 
        //and one that represents the directory above us (..)
        if (strcmp(path, "/") == 0) {
                fclose(disk);
                return -EPERM;
        }
        else {
                if (strcmp(filename, "") == 0) {
                        fclose(disk);
                        return -EPERM;
                }
                else {
                        int i;
                        for (i = 0; i < numDirs; i++) {
                                if (strcmp(root.directories[i].dname, directory) == 0) {
                                        //If the path does exist and is a directory:
                                        long dirStart = root.directories[i].nStartBlock;
                                        fseek(disk, dirStart, SEEK_SET);
                                        // now read the csc452_directory_entry
                                        csc452_directory_entry existingDir;
                                        fread(&existingDir, sizeof(csc452_directory_entry), 1, disk);
                                        int j;
                                        int numFiles = existingDir.nFiles;
                                        if (numFiles >= MAX_FILES_IN_DIR) {
                                                fclose(disk);
                                                return -ENOSPC; // Directory is full!
                                        }
                                        else {
                                                // write
                                                long startBlock;
                                                for (j = 0; j < numFiles; j++) {                         
                                                        if (strcmp(existingDir.files[j].fname, filename) == 0) {
                                                                // file exists!
                                                                startBlock = existingDir.files[j].nStartBlock;       
                                                                existingDir.files[j].fsize = size;
                                                                if ((size > 0) && (offset < startBlock + MAX_DATA_IN_BLOCK)) {
                                                                        // size > 0 and within bounds of block
                                                                        csc452_disk_block block;
                                                                        fseek(disk, startBlock, SEEK_SET);
                                                                        fread(&block, sizeof(csc452_disk_block), 1, disk);
                                                                        // load the disk block and write to it, then write it back to disk
                                                                        memcpy(&block.data[offset], buf, size);
                                                                        fseek(disk, startBlock, SEEK_SET);
                                                                        fwrite(&block, sizeof(csc452_disk_block), 1, disk);
                                                                        fclose(disk);
                                                                        return size;
                                                                }
                                                                else {
                                                                        fclose(disk);
                                                                        return -EFBIG;
                                                                }
                                                        }
                                                }
                                        }
                                        fclose(disk);
                                        return 0;
                                }
                        }
                } 
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
        FILE *disk; 
        disk = fopen(".disk", "rb+");   
        char directory[MAX_FILENAME+1];
        char filename[MAX_FILENAME+1] = "";
        char extension[MAX_EXTENSION+1];
        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
        fseek(disk, 0, SEEK_SET);

        csc452_root_directory root;
        fread(&root, sizeof(csc452_root_directory), 1, disk);
        int numDirs = root.nDirectories;

        //A directory holds two entries, one that represents itself (.) 
        //and one that represents the directory above us (..)
        if (strcmp(path, "/") == 0) {
                return -ENOENT; // Can't delete root!
        }
        else {
                if (strcmp(filename, "") == 0) {
                        int i;
                        for (i = 0; i < numDirs; i++) {
                                printf("compare directories dname and parsed directoryname: %s and %s\n",root.directories[i].dname, directory);
                                printf("strcmp returns: %d\n",strcmp(root.directories[i].dname, directory));
                                if (strcmp(root.directories[i].dname, directory) == 0) {
                                        //If the path does exist and is a directory:
                                        long dirStart = root.directories[i].nStartBlock;
                                        fseek(disk, dirStart, SEEK_SET);
                                        
                                        csc452_directory_entry existingDir;
                                        fread(&existingDir, sizeof(csc452_directory_entry), 1, disk);

                                        int numFiles = existingDir.nFiles;
                                        if (numFiles == 0) {
                                                // proceed to remove
                                                int bitmapIndex = dirStart / BLOCK_SIZE;
                                                int bitmap[10240];
                                                // get bitmap
                                                fseek(disk, -sizeof(bitmap), SEEK_END); 
                                                fread(&bitmap, sizeof(bitmap), 1, disk);   
                                                bitmap[bitmapIndex] = 0; 
                                                fseek(disk, -sizeof(bitmap), SEEK_END);
                                                fwrite(&bitmap, sizeof(bitmap), 1, disk);

                                                // update root & write back
                                                int j;
                                                for (j = i; j < numDirs-1; j++) {
                                                        root.directories[j] = root.directories[j+1];
                                                }
                                                numDirs--;
                                                root.nDirectories = numDirs;
                                                fseek(disk, 0, SEEK_SET);
                                                fwrite(&root, sizeof(csc452_root_directory), 1, disk);
                                                fclose(disk);  
                                                return 0;
                                        }
                                        else { // can't remove a dir with files in it
                                                fclose(disk);
                                                return -ENOTEMPTY;
                                        }

                                        fclose(disk);
                                        return 0;
                                }
                        }
                        fclose(disk);
                        return -ENOENT;
                }
                else {
                        fclose(disk);
                        return -ENOTDIR;
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
        FILE *disk; 
        disk = fopen(".disk", "rb+");   
        char directory[MAX_FILENAME+1];
        char filename[MAX_FILENAME+1] = "";
        char extension[MAX_EXTENSION+1];
        sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
        fseek(disk, 0, SEEK_SET);

        csc452_root_directory root;
        fread(&root, sizeof(csc452_root_directory), 1, disk);
        int numDirs = root.nDirectories;

        //A directory holds two entries, one that represents itself (.) 
        //and one that represents the directory above us (..)
        if (strcmp(path, "/") == 0) {
                return -ENOENT; // Can't delete root!
        }
        else {
                if (strcmp(filename, "") != 0) {
                        int i;
                        for (i = 0; i < numDirs; i++) {
                                if (strcmp(root.directories[i].dname, directory) == 0) {
                                        long dirStart = root.directories[i].nStartBlock;
                                        fseek(disk, dirStart, SEEK_SET);
                                        csc452_directory_entry existingDir;
                                        fread(&existingDir, sizeof(csc452_directory_entry), 1, disk);
                                        int numFiles = existingDir.nFiles;
                                        int j;
                                        for (j = 0; j < numFiles; j++) {                        
                                                if (strcmp(existingDir.files[j].fname, filename) == 0) {
                                                        // found file to be deleletedi
                                                        long fileStart = existingDir.files[j].nStartBlock;

                                                        int bitmapIndex = fileStart / BLOCK_SIZE;
                                                        int bitmap[10240];
                                                        // get bitmap
                                                        fseek(disk, -sizeof(bitmap), SEEK_END); 
                                                        fread(&bitmap, sizeof(bitmap), 1, disk);   
                                                        bitmap[bitmapIndex] = 0; 
                                                        fseek(disk, -sizeof(bitmap), SEEK_END);
                                                        fwrite(&bitmap, sizeof(bitmap), 1, disk);
                                                        // update directory entry & write back
                                                        int j;
                                                        for (j = i; j < numFiles-1; j++) {
                                                                existingDir.files[j] = existingDir.files[j+1];
                                                        }
                                                        numFiles--;
                                                        existingDir.nFiles = numFiles;
                                                        fseek(disk, dirStart, SEEK_SET);
                                                        fwrite(&existingDir, sizeof(csc452_directory_entry), 1, disk);
                                                        fclose(disk);  
                                                        return 0;
                                                }
                                        }
                                        fclose(disk);
                                        return -ENOENT;
                                }
                        } 
                        fclose(disk);
                        return -ENOENT;
                }
                else {
                        fclose(disk);
                        return -EISDIR;
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
