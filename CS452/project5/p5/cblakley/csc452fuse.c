/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452

        Author: Cole Blakley
        Description: Implements a simple FUSE filesystem.
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
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>

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

#define DISK_SIZE 5242880 //5 MB disk
#define BLOCK_COUNT (DISK_SIZE / BLOCK_SIZE)
#define BITMAP_SIZE (BLOCK_COUNT / 8)
#define MAX_FULL_FILENAME (MAX_FILENAME + 1 + MAX_EXTENSION) //8.3

static char* open_disk(int* disk_fd_out)
{
    *disk_fd_out = open(".disk", O_RDWR);
    char* disk = mmap(NULL, DISK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                      *disk_fd_out, 0);
    assert(disk != MAP_FAILED);
    return disk;
}

static void close_disk(char* disk_file, int disk_fd)
{
    munmap(disk_file, DISK_SIZE);
    close(disk_fd);
}

static int is_used(char* disk, long block_num)
{
    int byte_offset = block_num / 8;
    int bit_offset = block_num % 8;
    char* byte_pos = disk + byte_offset;
    return ((*byte_pos) >> bit_offset) & 1;
}

static void toggle_bit(char* disk, long block_num)
{
    int byte_offset = block_num / 8;
    int bit_offset = block_num % 8;
    char* byte_pos = disk + byte_offset;
    *byte_pos ^= 1 << bit_offset;
}

static csc452_root_directory* get_root_dir(char* disk)
{
    // Is first block on disk after bitmap
    return (csc452_root_directory*)(disk + BITMAP_SIZE);
}

static
csc452_directory_entry* get_dir_entry(char* disk, long block_num)
{
    char* offset = disk + BITMAP_SIZE + (BLOCK_SIZE * block_num);
    return (csc452_directory_entry*)offset;
}

static
csc452_disk_block* get_block(char* disk, long block_num)
{
    char* offset = disk + BITMAP_SIZE + (BLOCK_SIZE * block_num);
    return (csc452_disk_block*)offset;
}

static
long get_next_free_block_num(char* disk)
{
    // block 0 is always root, so don't check its bit
    for(long i = 1; i < BLOCK_COUNT; ++i) {
        if(!is_used(disk, i)) {
            return i;
        }
    }
    return -1;
}

static
csc452_directory_entry* get_dir_entry_by_name(char* disk, csc452_root_directory* root,
                                              const char* dir_name, int name_len)
{
    for(int i = 0; i < root->nDirectories; ++i) {
        struct csc452_directory* entry = &root->directories[i];
        if(strncmp(entry->dname, dir_name, name_len) == 0 &&
           strlen(entry->dname) == name_len) {
            return get_dir_entry(disk, entry->nStartBlock);
        }
    }
    return NULL;
}

static
struct csc452_file_directory*
get_file_entry_by_name(csc452_directory_entry* dir,
                       const char* fname, int fname_len,
                       const char* ext)
{
    for(int i = 0; i < dir->nFiles; ++i) {
        struct csc452_file_directory* entry = &(dir->files[i]);
        if(strncmp(fname, entry->fname, fname_len) == 0 &&
           strlen(fname) == fname_len &&
           strcmp(ext, entry->fext) == 0) {
            return entry;
        }
    }
    return NULL;
}

static
int parse_dir_part(const char* slash, const char** start_out)
{
    *start_out = slash + 1;
    const char* next_slash = strchr(*start_out, '/');
    if(next_slash == NULL) {
        return strlen(*start_out);
    } else {
        return next_slash - *start_out;
    }
}

static
void parse_filename_part(const char* slash,
                         const char** fname_out, int* fname_len,
                         const char** ext_out)
{
    *fname_out = slash + 1;
    const char* dot = strchr(*fname_out, '.');
    if(dot == NULL) {
        *fname_len = strlen(*fname_out);
        *ext_out = "\0";
    } else {
        *fname_len = dot - *fname_out;
        *ext_out = dot + 1;
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
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    const char* dir_name = NULL;
    int dir_len = parse_dir_part(path, &dir_name);
    const char* filename_slash = dir_name + dir_len;
    if(dir_len > MAX_FILENAME) {
        return -ENOENT;
    }

    int disk_fd;
    char* disk = open_disk(&disk_fd);
    csc452_root_directory* root = get_root_dir(disk);

    // Try to find the directory
    csc452_directory_entry* dir_entry = get_dir_entry_by_name(disk, root,
                                          dir_name, dir_len
                                        );
    if(dir_entry == NULL) {
        close_disk(disk, disk_fd);
        return -ENOENT;
    }

    if(*filename_slash != '/' || *(filename_slash + 1) == '\0') {
        //If the path does exist and is a directory:
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        close_disk(disk, disk_fd);
        return 0;
    } else {
        const char* filename = NULL;
        int filename_len = 0;
        const char* ext = NULL;
        parse_filename_part(filename_slash, &filename, &filename_len, &ext);
        if(filename_len > MAX_FILENAME) {
            close_disk(disk, disk_fd);
            return -ENOENT;
        }

        // Try to find the file in the given directory
        struct csc452_file_directory* file_info = get_file_entry_by_name(
                                                    dir_entry,
                                                    filename,
                                                    filename_len,
                                                    ext
                                                  );
        close_disk(disk, disk_fd);
        if(file_info != NULL) {
            //If the path does exist and is a file:
            stbuf->st_mode = S_IFREG | 0666;
            stbuf->st_nlink = 2;
            stbuf->st_size = file_info->fsize;
            return 0;
        } else {
            //Else return that path doesn't exist
            return -ENOENT;
        }
    }
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    filler(buf, ".", NULL,0);
    filler(buf, "..", NULL, 0);

    int disk_fd;
    char* disk = open_disk(&disk_fd);
    csc452_root_directory* root = get_root_dir(disk);

    if (strcmp(path, "/") == 0) {
        // List root directory
        for(int i = 0; i < root->nDirectories; ++i) {
            filler(buf, root->directories[i].dname, NULL,0);
        }
        close_disk(disk, disk_fd);
        return 0;
    } else {
        // List a subdirectory
        const char* dir_name = NULL;
        int dir_len = parse_dir_part(path, &dir_name);
        const char* trailing_ch = dir_name + dir_len;
        if(*trailing_ch == '\0' ||
           (*trailing_ch == '/' && *(trailing_ch + 1) == '\0')) {
            csc452_directory_entry* dir_entry = get_dir_entry_by_name(
                                                  disk, root,
                                                  dir_name, dir_len
                                                );
            if(dir_entry == NULL) {
                close_disk(disk, disk_fd);
                return -ENOENT;
            }
            char full_filename[MAX_FULL_FILENAME + 1];
            for(int i = 0; i < dir_entry->nFiles; ++i) {
                struct csc452_file_directory* entry = &dir_entry->files[i];
                snprintf(full_filename, MAX_FULL_FILENAME+1, "%s.%s",
                         entry->fname, entry->fext);
                filler(buf, full_filename, NULL,0);
            }
            close_disk(disk, disk_fd);
            return 0;
        } else {
            close_disk(disk, disk_fd);
            return -ENOENT;
        }
    }
}


/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) mode;

        const char* dir_name = NULL;
        int dir_len = parse_dir_part(path, &dir_name);
        const char* trailing_slash = dir_name + dir_len;
        if(*trailing_slash != '\0' && *(trailing_slash + 1) != '\0') {
            // There is text beyond the trailing slash, which isn't allowed
            // (No nested directories)
            return -EPERM;
        }
        if(dir_len > MAX_FILENAME) {
            return -ENAMETOOLONG;
        }

        int disk_fd;
        char* disk = open_disk(&disk_fd);
        csc452_root_directory* root = get_root_dir(disk);

        csc452_directory_entry* existing_dir = get_dir_entry_by_name(
                                                 disk, root,
                                                 dir_name, dir_len
                                               );
        if(existing_dir != NULL) {
            close_disk(disk, disk_fd);
            return -EEXIST;
        }

        long new_block_num = get_next_free_block_num(disk);
        assert(new_block_num > 0);
        int dir_num = root->nDirectories;
        ++root->nDirectories;

        strcpy(root->directories[dir_num].dname, dir_name);
        root->directories[dir_num].nStartBlock = new_block_num;
        toggle_bit(disk, new_block_num);

        close_disk(disk, disk_fd);
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
    (void) mode;
    (void) dev;

    if(strcmp(path, "/") == 0) {
        return -EPERM;
    }

    const char* dir_name = NULL;
    int dir_len = parse_dir_part(path, &dir_name);
    const char* trailing_slash = dir_name + dir_len;
    if(dir_len == 0) {
        return -EPERM;
    } else if(dir_len > MAX_FILENAME) {
        return -ENAMETOOLONG;
    }

    const char* filename = NULL;
    int filename_len = 0;
    const char* ext = NULL;
    parse_filename_part(trailing_slash, &filename, &filename_len, &ext);
    if(filename_len == 0) {
        // No filename; only a directory is given
        return -EEXIST;
    } else if(filename_len > MAX_FILENAME || strlen(ext) > MAX_EXTENSION) {
        return -ENAMETOOLONG;
    }

    int disk_fd;
    char* disk = open_disk(&disk_fd);
    csc452_root_directory* root = get_root_dir(disk);

    csc452_directory_entry* dir_entry = get_dir_entry_by_name(
                                          disk, root, dir_name, dir_len);
    if(dir_entry == NULL || dir_entry->nFiles >= MAX_FILES_IN_DIR) {
        close_disk(disk, disk_fd);
        return -EPERM;
    }
    if(get_file_entry_by_name(dir_entry, filename, filename_len, ext) != NULL) {
        close_disk(disk, disk_fd);
        return -EEXIST;
    }
    // Create a new file entry
    int file_index = dir_entry->nFiles++;
    struct csc452_file_directory* file_entry = &dir_entry->files[file_index];
    strncpy(file_entry->fname, filename, filename_len);
    file_entry->fext[0] = '\0';
    strncpy(file_entry->fext, ext, MAX_EXTENSION);
    file_entry->fsize = 0;

    // Create a new file block
    long file_block_num = get_next_free_block_num(disk);
    file_entry->nStartBlock = file_block_num;
    assert(!is_used(disk, file_block_num));
    toggle_bit(disk, file_block_num);

    // Clear/initialize the file block
    csc452_disk_block* file_block = get_block(disk, file_block_num);
    file_block->nNextBlock = 0;
    file_block->data[0] = '\0';

    close_disk(disk, disk_fd);
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
    (void) offset;
    (void) fi;

    //check to make sure path exists
    if(strcmp(path, "/") == 0) {
        return -EISDIR;
    }
    const char* dir_name = NULL;
    int dir_len = parse_dir_part(path, &dir_name);
    const char* trailing_slash = dir_name + dir_len;

    int disk_fd;
    char* disk = open_disk(&disk_fd);
    csc452_root_directory* root = get_root_dir(disk);

    csc452_directory_entry* dir_entry = get_dir_entry_by_name(
                                          disk, root, dir_name, dir_len
                                        );
    assert(dir_entry != NULL);
    const char* filename = NULL;
    int filename_len = 0;
    const char* ext = NULL;
    parse_filename_part(trailing_slash, &filename, &filename_len, &ext);
    struct csc452_file_directory* file_entry = get_file_entry_by_name(
                                                dir_entry,
                                                filename, filename_len, ext);
    assert(file_entry != NULL);
    //check that size is > 0
    if(size <= 0) {
        close_disk(disk, disk_fd);
        return 0;
    }
    //check that offset is <= to the file size
    if(offset > size) {
        close_disk(disk, disk_fd);
        return -EFBIG;
    }
    //write data
    size_t start_block_num = offset / BLOCK_SIZE;
    size_t byte_offset = offset % BLOCK_SIZE;
    long total_blocks = file_entry->fsize / BLOCK_SIZE;
    size_t block_num = 0;
    size_t bytes_to_write = size;
    csc452_disk_block* block = get_block(disk, file_entry->nStartBlock);
    while(block_num != start_block_num && block_num < total_blocks) {
        block = get_block(disk, block->nNextBlock);
        ++block_num;
    }
    if(block_num == start_block_num) {
        // Start writing in middle of file
        if(bytes_to_write < BLOCK_SIZE - byte_offset) {
            memcpy(block->data + byte_offset, buf, bytes_to_write);
            close_disk(disk, disk_fd);
            return size;
        } else {
            memcpy(block->data + byte_offset, buf, BLOCK_SIZE - byte_offset);
            buf += BLOCK_SIZE - byte_offset;
            bytes_to_write -= BLOCK_SIZE - byte_offset;
            if(block_num < total_blocks) {
                block = get_block(disk, block->nNextBlock);
                ++block_num;
            }
        }
    }

    // Write to blocks, starting at the beginning of blocks
    while(bytes_to_write > 0) {
        if(block_num >= total_blocks) {
            long new_block_num = get_next_free_block_num(disk);
            if(new_block_num == -1) {
                close_disk(disk, disk_fd);
                return -ENOSPC;
            }
            block->nNextBlock = new_block_num;
            block = get_block(disk, block->nNextBlock);
        }
        if(bytes_to_write <= BLOCK_SIZE) {
            memcpy(block->data, buf, bytes_to_write);
            close_disk(disk, disk_fd);
            return size;
        } else {
            memcpy(block->data, buf, BLOCK_SIZE);
            buf += BLOCK_SIZE;
            bytes_to_write -= BLOCK_SIZE;
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
    if(strcmp(path, "/") == 0) {
        return -ENOTDIR;
    }
    int disk_fd;
    char* disk = open_disk(&disk_fd);
    csc452_root_directory* root = get_root_dir(disk);

    const char* dir_name = NULL;
    int dir_len = parse_dir_part(path, &dir_name);
    const char* trailing_ch = dir_name + dir_len;
    int dir_index = -1;
    for(int i = 0; i < root->nDirectories; ++i) {
        struct csc452_directory* entry = &root->directories[i];
        if(strncmp(entry->dname, dir_name, dir_len) == 0) {
            dir_index = i;
            break;
        }
    }
    if(dir_index == -1) {
        close_disk(disk, disk_fd);
        return -ENOENT;
    }
    struct csc452_directory* dir_root_entry = &root->directories[dir_index];
    long dir_block_num = dir_root_entry->nStartBlock;
    csc452_directory_entry* dir_entry = get_dir_entry(disk,
                                                      dir_block_num
                                                     );
    if(*trailing_ch == '\0' ||
      (*trailing_ch == '/' && *(trailing_ch + 1) == '\0')) {
        if(dir_entry->nFiles != 0) {
            close_disk(disk, disk_fd);
            return -ENOTEMPTY;
        } else {
            // First, free/clear the directory block
            assert(is_used(disk, dir_block_num));
            toggle_bit(disk, dir_block_num);
            dir_entry->nFiles = 0;
            // Then, remove the entry in the root node
            if(root->nDirectories > 1) {
                // Swap the last directory to this spot (so no empty gap)
                *dir_root_entry = root->directories[root->nDirectories - 1];
            }
            --root->nDirectories;
            close_disk(disk, disk_fd);
            return 0;
        }
    } else {
        close_disk(disk, disk_fd);
        return -ENOTDIR;
    }
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
    if(strcmp(path, "/") == 0) {
        return -EISDIR;
    }
    const char* dir_name = NULL;
    int dir_len = parse_dir_part(path, &dir_name);
    const char* trailing_slash = dir_name + dir_len;
    if(*trailing_slash != '/') {
        return -EISDIR;
    }
    const char* filename = NULL;
    int filename_len = 0;
    const char* ext = NULL;
    parse_filename_part(trailing_slash, &filename, &filename_len, &ext);

    int disk_fd;
    char* disk = open_disk(&disk_fd);
    csc452_root_directory* root = get_root_dir(disk);

    csc452_directory_entry* dir_entry = get_dir_entry_by_name(
                                          disk, root,
                                          dir_name, dir_len);
    if(dir_entry == NULL) {
        close_disk(disk, disk_fd);
        return -ENOENT;
    }

    struct csc452_file_directory* file_entry = get_file_entry_by_name(
                                                 dir_entry, filename, filename_len,
                                                 ext);
    if(file_entry == NULL) {
        close_disk(disk, disk_fd);
        return -ENOENT;
    }

    long total_blocks = file_entry->fsize / BLOCK_SIZE;
    csc452_disk_block* block = get_block(disk, file_entry->nStartBlock);
    long block_num = file_entry->nStartBlock;
    long i = 0;
    while(i < total_blocks) {
        // Free block
        toggle_bit(disk, block_num);
        block = get_block(disk, block->nNextBlock);
        block_num = block->nNextBlock;
        ++i;
    }
    if(dir_entry->nFiles > 1) {
        *file_entry = dir_entry->files[dir_entry->nFiles - 1];
    }
    --dir_entry->nFiles;
    close_disk(disk, disk_fd);
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
