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

// -------------------------------------------------------------------------------------------------------------- //

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf) {

	if (strcmp(path, "/") == 0) {

		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;

	} else {

        char dir[100] = "", file[100] = "", ext[100] = "";

        sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);

        if ((strlen(dir) > MAX_FILENAME) || (strlen(file) > MAX_FILENAME) || (strlen(ext) > MAX_EXTENSION)) {
            return -ENAMETOOLONG;
        }

        if (strlen(dir) == 0) {
            return -ENOENT;
        }

        csc452_root_directory root;

        FILE* disk = fopen(".disk", "rb");
        fread(&root, sizeof(root), 1, disk);
        fclose(disk);

        long start = 0;

        for (int i = 0; i < root.nDirectories; ++i) {       // directory

            if (strcmp(root.directories[i].dname, dir) == 0) {      // if path exists

                start = root.directories[i].nStartBlock;

                stbuf->st_mode  = S_IFDIR | 0755;
		        stbuf->st_nlink = 2;
                break;
            }
        }

        if (start == 0) {
            return -ENOENT;
        }

        if ((strlen(file) == 0) && (strlen(ext) == 0)) {        // directory path

            return 0;

        } else if ((strlen(file) == 0) || (strlen(ext) == 0)) {

            return -ENOENT;

        } else {

            csc452_directory_entry dirEntry;

            disk = fopen(".disk", "rb");
            fseek(disk, start * BLOCK_SIZE, SEEK_SET);
            fread(&dirEntry, sizeof(dirEntry), 1, disk);
            fclose(disk);

            int found = 0;

            for (int i = 0; i < dirEntry.nFiles; ++i) {

                if ( (strcmp(dirEntry.files[i].fname, file) == 0) &&
                     (strcmp(dirEntry.files[i].fext , ext ) == 0) ) {

                    found = 1;

                    stbuf->st_mode  = S_IFREG | 0666;
                    stbuf->st_nlink = 2;
	                stbuf->st_size  = dirEntry.files[i].fsize;
                    break;
                }
            }

            if (!found) {
                return -ENOENT;
            }
        }
	}

	return 0;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi) {

	// Since we're building with -Wall (all warnings reported) we need to "use" every parameter,
    // so let's just cast them to void to satisfy the compiler
	(void) offset;
	(void) fi;

	// A directory holds two entries, one that represents itself (.)
	// and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

        csc452_root_directory root;

        FILE* disk = fopen(".disk", "rb");
        fread(&root, sizeof(root), 1, disk);
        fclose(disk);

        for (int i = 0; i < root.nDirectories; ++i) {
            filler(buf, root.directories[i].dname, NULL, 0);
        }

	} else {

        if (strchr(path + 1, '/') != NULL) {
            return -ENOTDIR;
        }

        if (strlen(path + 1) > MAX_FILENAME) {
            return -ENAMETOOLONG;
        }

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

        csc452_root_directory root;

        FILE* disk = fopen(".disk", "rb");
        fread(&root, sizeof(root), 1, disk);
        fclose(disk);

        long start = 0;

        for (int i = 0; i < root.nDirectories; ++i) {

            if (strcmp(root.directories[i].dname, path + 1) == 0) {
                start = root.directories[i].nStartBlock;
                break;
            }
        }

        if (start > 0) {

            csc452_directory_entry dirEntry;

            disk = fopen(".disk", "rb");
            fseek(disk, start * BLOCK_SIZE, SEEK_SET);
            fread(&dirEntry, sizeof(dirEntry), 1, disk);
            fclose(disk);

            for (int i = 0; i < dirEntry.nFiles; ++i) {

                char file[MAX_FILENAME + MAX_EXTENSION + 2] = "";

                strcat(file, dirEntry.files[i].fname);
                strcat(file, ".");
                strcat(file, dirEntry.files[i].fext);

                filler(buf, file, NULL, 0);
            }

        } else {
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

    if (strcmp(path, "/") == 0) {
        return -EPERM;
    }

    if (strchr(path + 1, '/') != NULL) {
        return -EPERM;
    }

    if (strlen(path + 1) > MAX_FILENAME) {
        return -ENAMETOOLONG;
    }

    csc452_root_directory root;

    FILE* disk = fopen(".disk", "rb");
    fread(&root, sizeof(root), 1, disk);
    fclose(disk);

    if (root.nDirectories == MAX_DIRS_IN_ROOT) {
        return -ENOSPC;
    }

    for (int i = 0; i < root.nDirectories; ++i) {
        if (strcmp(root.directories[i].dname, path + 1) == 0) {
            return -EEXIST;
        }
    }

    // FIXME: get block number (1 to MAX_DIRS_IN_ROOT) from free-space tracking structure
    // TODO: use that block number for index as well as dirEntry
    strcpy(root.directories[root.nDirectories].dname, path + 1);
    root.directories[root.nDirectories].nStartBlock = root.nDirectories + 1;

    root.nDirectories++;

    disk = fopen(".disk", "wb");
    fwrite(&root, sizeof(root), 1, disk);
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
static int csc452_mknod(const char *path, mode_t mode, dev_t dev) {

    (void) mode;
    (void) dev;

    if ((strcmp(path, "/") == 0) || (strchr(path + 1, '/') == NULL)) {
        return -EPERM;
    }

    char dir[100] = "", file[100] = "", ext[100] = "";

    sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);

    if ((strlen(dir) > MAX_FILENAME) || (strlen(file) > MAX_FILENAME) || (strlen(ext) > MAX_EXTENSION)) {
        return -ENAMETOOLONG;
    }

    csc452_root_directory root;

    FILE* disk = fopen(".disk", "rb");
    fread(&root, sizeof(root), 1, disk);
    fclose(disk);

    long start = 0;

    for (int i = 0; i < root.nDirectories; ++i) {

        if (strcmp(root.directories[i].dname, dir) == 0) {
            start = root.directories[i].nStartBlock;
            break;
        }
    }

    if (start > 0) {

        csc452_directory_entry dirEntry;

        disk = fopen(".disk", "rb");
        fseek(disk, start * BLOCK_SIZE, SEEK_SET);
        fread(&dirEntry, sizeof(dirEntry), 1, disk);
        fclose(disk);

        if (dirEntry.nFiles == MAX_FILES_IN_DIR) {
            return -ENOSPC;
        }

        for (int i = 0; i < dirEntry.nFiles; ++i) {

            if ( (strcmp(dirEntry.files[i].fname, file) == 0) &&
                 (strcmp(dirEntry.files[i].fext , ext ) == 0) ) {

                return -EEXIST;
            }
        }

        // FIXME: get block number from free-space tracking structure
        // TODO: use that block number for index as well as file data
        strcpy(dirEntry.files[dirEntry.nFiles].fname, file);
        strcpy(dirEntry.files[dirEntry.nFiles].fext , ext );

        long fileStart = dirEntry.files[dirEntry.nFiles].nStartBlock = 1 + MAX_DIRS_IN_ROOT + dirEntry.nFiles + 1;

        dirEntry.nFiles++;

        disk = fopen(".disk", "wb");
        fseek(disk, fileStart * BLOCK_SIZE, SEEK_SET);
        fwrite(&dirEntry, sizeof(dirEntry), 1, disk);
        fclose(disk);

    } else {
        return -ENOENT;
    }

	return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	(void) fi;

	// check to make sure path exists
    if ((strcmp(path, "/") == 0) || (strchr(path + 1, '/') == NULL)) {
        return -EISDIR;
    }

    char dir[100] = "", file[100] = "", ext[100] = "";

    sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);

    if ((strlen(dir) > MAX_FILENAME) || (strlen(file) > MAX_FILENAME) || (strlen(ext) > MAX_EXTENSION)) {
        return -ENAMETOOLONG;
    }

    csc452_root_directory root;

    FILE* disk = fopen(".disk", "rb");
    fread(&root, sizeof(root), 1, disk);
    fclose(disk);

    long start = 0;

    for (int i = 0; i < root.nDirectories; ++i) {

        if (strcmp(root.directories[i].dname, dir) == 0) {
            start = root.directories[i].nStartBlock;
            break;
        }
    }

    if (start > 0) {

        csc452_directory_entry dirEntry;

        disk = fopen(".disk", "rb");
        fseek(disk, start * BLOCK_SIZE, SEEK_SET);
        fread(&dirEntry, sizeof(dirEntry), 1, disk);
        fclose(disk);

        int index = -1;
        long fileStart = 0;

        for (int i = 0; i < dirEntry.nFiles; ++i) {

            if ( (strcmp(dirEntry.files[i].fname, file) == 0) &&        // path exists
                 (strcmp(dirEntry.files[i].fext , ext ) == 0) ) {

                index = i;
                fileStart = dirEntry.files[i].nStartBlock;
                break;
            }
        }

        if (fileStart == 0) {
            return -ENOENT;
        }

	    // check that size is > 0
        if (size > 0) {

            // check that offset is <= to the file size
            if (offset > dirEntry.files[index].fsize) {
                return -EFBIG;
            }

	        // read in data
            csc452_disk_block fileData;

            disk = fopen(".disk", "rb");
            fseek(disk, fileStart * BLOCK_SIZE, SEEK_SET);
            fread(&fileData, sizeof(fileData), 1, disk);

            while (offset > MAX_DATA_IN_BLOCK) {
                fileStart = fileData.nNextBlock;
                offset -= MAX_DATA_IN_BLOCK;
                fseek(disk, fileStart * BLOCK_SIZE, SEEK_SET);
                fread(&fileData, sizeof(fileData), 1, disk);
            }

            int amt = MAX_DATA_IN_BLOCK - offset;

            if (size <= amt) {
                memcpy(buf, fileData.data + offset, size);
                fclose(disk);
                return size;
            }

            int done = amt;
            int left = size - amt;

            while (left > 0) {

                fileStart = fileData.nNextBlock;
                fseek(disk, fileStart * BLOCK_SIZE, SEEK_SET);
                fread(&fileData, sizeof(fileData), 1, disk);

                if (left > MAX_DATA_IN_BLOCK) {

                    memcpy(buf + done, fileData.data, MAX_DATA_IN_BLOCK);

                } else {

                    memcpy(buf + done, fileData.data, left);
                }

                done += MAX_DATA_IN_BLOCK;
                left -= MAX_DATA_IN_BLOCK;
            }

            fclose(disk);
        }

    } else {
        return -ENOENT;
    }

	// return success, or error
	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	(void) fi;

    if ((strcmp(path, "/") == 0) || (strchr(path + 1, '/') == NULL)) {
        return -EPERM;
    }

    char dir[100] = "", file[100] = "", ext[100] = "";

    sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);

    if ((strlen(dir) > MAX_FILENAME) || (strlen(file) > MAX_FILENAME) || (strlen(ext) > MAX_EXTENSION)) {
        return -ENAMETOOLONG;
    }

    csc452_root_directory root;

    FILE* disk = fopen(".disk", "rb");
    fread(&root, sizeof(root), 1, disk);
    fclose(disk);

    long start = 0;

    for (int i = 0; i < root.nDirectories; ++i) {

        if (strcmp(root.directories[i].dname, dir) == 0) {
            start = root.directories[i].nStartBlock;
            break;
        }
    }

    if (start > 0) {

        csc452_directory_entry dirEntry;

        disk = fopen(".disk", "rb");
        fseek(disk, start * BLOCK_SIZE, SEEK_SET);
        fread(&dirEntry, sizeof(dirEntry), 1, disk);
        fclose(disk);

        int index = -1;
        long fileStart = 0;

        for (int i = 0; i < dirEntry.nFiles; ++i) {

            if ( (strcmp(dirEntry.files[i].fname, file) == 0) &&        // path exists
                 (strcmp(dirEntry.files[i].fext , ext ) == 0) ) {

                index = i;
                fileStart = dirEntry.files[i].nStartBlock;
                break;
            }
        }

        if (fileStart == 0) {
            return -ENOENT;
        }

        if (size > 0) {

            // check that offset is <= to the file size
            if (offset > dirEntry.files[index].fsize) {
                return -EFBIG;
            }

            // write data
            csc452_disk_block fileData;

            disk = fopen(".disk", "rb");
            fseek(disk, fileStart * BLOCK_SIZE, SEEK_SET);
            fread(&fileData, sizeof(fileData), 1, disk);

            while (offset > MAX_DATA_IN_BLOCK) {
                fileStart = fileData.nNextBlock;
                offset -= MAX_DATA_IN_BLOCK;
                fseek(disk, fileStart * BLOCK_SIZE, SEEK_SET);
                fread(&fileData, sizeof(fileData), 1, disk);
            }

            fclose(disk);

            // TODO: add support for appends

            if (size > (MAX_DATA_IN_BLOCK - offset)) {
                return -ENOSPC;
            }

            memcpy(fileData.data + offset, buf, size);

            dirEntry.files[index].fsize += size;

            disk = fopen(".disk", "wb");
            fseek(disk, fileStart * BLOCK_SIZE, SEEK_SET);
            fwrite(&fileData, sizeof(fileData), 1, disk);
            fclose(disk);
        }

    } else {
        return -ENOENT;
    }

    // return success, or error
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path) {

    if (strcmp(path, "/") == 0) {
        return -EPERM;
    }

    if (strchr(path + 1, '/') != NULL) {
        return -ENOTDIR;
    }

    if (strlen(path + 1) > MAX_FILENAME) {
        return -ENOENT;
    }

    csc452_root_directory root;

    FILE* disk = fopen(".disk", "rb");
    fread(&root, sizeof(root), 1, disk);
    fclose(disk);

    int index = -1;
    long start = 0;

    for (int i = 0; i < root.nDirectories; ++i) {

        if (strcmp(path + 1, root.directories[i].dname) == 0) {
            index = i;
            start = root.directories[i].nStartBlock;
            break;
        }
    }

    if (start > 0) {

        csc452_directory_entry dirEntry;

        disk = fopen(".disk", "rb");
        fseek(disk, start * BLOCK_SIZE, SEEK_SET);
        fread(&dirEntry, sizeof(dirEntry), 1, disk);
        fclose(disk);

        if (dirEntry.nFiles > 0) {
            return -ENOTEMPTY;
        }

        root.directories[index].dname[0] = '\0';
        root.directories[index].nStartBlock = 0;

        root.nDirectories--;

        // FIXME: update free space tracking structure

        disk = fopen(".disk", "wb");
        fwrite(&root, sizeof(root), 1, disk);
        fclose(disk);

    } else {
        return -ENOENT;
    }

    return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path) {

    // check to make sure path exists
    if ((strcmp(path, "/") == 0) || (strchr(path + 1, '/') == NULL)) {
        return -EISDIR;
    }

    char dir[100] = "", file[100] = "", ext[100] = "";

    sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext);

    if ((strlen(dir) > MAX_FILENAME) || (strlen(file) > MAX_FILENAME) || (strlen(ext) > MAX_EXTENSION)) {
        return -ENAMETOOLONG;
    }

    csc452_root_directory root;

    FILE* disk = fopen(".disk", "rb");
    fread(&root, sizeof(root), 1, disk);
    fclose(disk);

    long start = 0;

    for (int i = 0; i < root.nDirectories; ++i) {

        if (strcmp(root.directories[i].dname, dir) == 0) {
            start = root.directories[i].nStartBlock;
            break;
        }
    }

    if (start == 0) {
        return -ENOENT;
    }

    csc452_directory_entry dirEntry;

    disk = fopen(".disk", "rb");
    fseek(disk, start * BLOCK_SIZE, SEEK_SET);
    fread(&dirEntry, sizeof(dirEntry), 1, disk);
    fclose(disk);

    int index = -1;
    long fileStart = 0;

    for (int i = 0; i < dirEntry.nFiles; ++i) {

        if ( (strcmp(dirEntry.files[i].fname, file) == 0) &&        // path exists
             (strcmp(dirEntry.files[i].fext , ext ) == 0) ) {

            index = i;
            break;
        }
    }

    if (fileStart == 0) {
        return -ENOENT;
    }

    dirEntry.files[index].fname[0] = '\0';
    dirEntry.files[index].fext[0] = '\0';
    dirEntry.files[index].fsize = 0;
    dirEntry.files[index].nStartBlock = 0;

    dirEntry.nFiles--;

    // FIXME: update free space tracking structure

    disk = fopen(".disk", "wb");
    fseek(disk, start * BLOCK_SIZE, SEEK_SET);
    fwrite(&dirEntry, sizeof(dirEntry), 1, disk);
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
