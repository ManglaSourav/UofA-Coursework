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

// 10,000 blocks (5 MB / 512 bytes)
// block 0: root directory
#define ROOT_BLOCK 0
// blocks 1 - 9998: directory entries and files
#define DATA_FIRST_BLOCK 1
#define DATA_LAST_BLOCK 9998
// block 9999: bitmap for free blocks
#define FREE_SPACE_BLOCK 9999

#define NOT_FOUND -1

typedef struct csc452_free_space_tracking {
    long cur;    // first (potentially) open block number as offset from DATA_FIRST_BLOCK
    char data[MAX_DATA_IN_BLOCK];
} csc452_free_space_tracking;

/*
 * Finds the next free block in the free space
 */
static long next_free_block(csc452_free_space_tracking free_space) {
    for (long block = free_space.cur; block + DATA_FIRST_BLOCK <= DATA_LAST_BLOCK; ++block) {
        char info = free_space.data[block / sizeof(char)];  // info for 8 blocks
        char is_claimed = info & (0x1 << (block % sizeof(char)));
        if (!is_claimed) {
            return block + DATA_FIRST_BLOCK;
        }
    }
    return NOT_FOUND;
}

/*
 * Claims the specified block in the free space
 */
static void claim_block(csc452_free_space_tracking *free_space, long block) {
    block -= DATA_FIRST_BLOCK;
    char info = free_space->data[block / sizeof(char)];  // info for 8 blocks
    info |= (0x1 << (block % sizeof(char)));
    free_space->data[block / sizeof(char)] = info;
    free_space->cur = block + 1;
}

/*
 * Unclaims the specified block in the free space
 */
static void unclaim_block(csc452_free_space_tracking *free_space, long block) {
    block -= DATA_FIRST_BLOCK;
    char info = free_space->data[block / sizeof(char)];  // info for 8 blocks
    info &= ~(0x1 << (block % sizeof(char)));
    free_space->data[block / sizeof(char)] = info;
    if (block < free_space->cur) {
        // Move cur back to this earlier free block
        free_space->cur = block;
    }
}


// typedef the structs inside of structs for convenience
typedef struct csc452_directory csc452_directory;
typedef struct csc452_file_directory csc452_file_directory;

/*
 * Helper method for reading the specified block in the file.
 * Reads the `block`th block from the `disk` file into `ptr`.
 */
static void read_block(void *ptr, FILE *disk, const long block) {
	fseek(disk, block * BLOCK_SIZE, SEEK_SET);
	fread(ptr, BLOCK_SIZE, 1, disk);
}

/*
 * Helper method for writing the specified block to the file.
 * Writes `ptr` to the `block`th block in the `disk` file.
 */
static void write_block(const void *ptr, FILE *disk, const long block) {
    fseek(disk, block * BLOCK_SIZE, SEEK_SET);
	fwrite(ptr, BLOCK_SIZE, 1, disk);
}

/*
 * Helper method for finding the index of the directory with the given
 * name out of an array of `n` `directories`.
 * Returns NOT_FOUND on failure, or the index of the directory
 */
static int find_directory(csc452_directory *directories, int n, const char *directory) {
	for (int i = 0; i < n; ++i) {
		if (strcmp(directories->dname, directory) == 0) {
			// Found the directory with the given name
			return i;
		}
		++directories;
	}
	return NOT_FOUND;
}

/*
 * Helper method for finding the index of the file with the given
 * filename and extension out of an array of `n` `files`.
 * Returns NOT_FOUND on failure, or the index of the directory
 */
static int find_file(csc452_file_directory *files, int n, const char *filename,
		const char *extension) {
	for (int i = 0; i < n; ++i) {
		if (strcmp(files->fname, filename) == 0 && strcmp(files->fext, extension) == 0) {
			// Found the file with the given name + extension
			return i;
		}
		++files;
	}
	return NOT_FOUND;
}

static void combine_filename(char *buf, const char *filename, const char *extension) {
	// Copy in the file name
	int i = 0;
	while (filename[i] != '\0') {
		buf[i] = filename[i];
		++i;
	}

	// Copy in the extension
	buf[i] = '.';
	strcpy(buf + i + 1, extension);
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int res = -ENOENT;

	if (strcmp(path, "/") == 0) {
		// Home directory
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0;
	} else {
		// Get the directory / file path
		char directory[MAX_FILENAME + 1];
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];
		int ret = sscanf(path, "/%8[^/]/%8[^.].%3s", directory, filename, extension);
		if (ret != 1 && ret != 3) {
			// Not a valid directory/file path
			return -ENOENT;
		}

		// Read the root directory entries
		FILE *disk = fopen(".disk", "rb");
		csc452_root_directory root;
		read_block(&root, disk, ROOT_BLOCK);

		int dir_index = find_directory(root.directories, root.nDirectories, directory);
		if (ret == 1) {
			// Path to directory
			if (dir_index != NOT_FOUND) {
				// Directory exists
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				res = 0;
			}
		} else if (ret == 3) {
			// Path to file
			if (dir_index != NOT_FOUND) {
				// Directory exists; read the subdirectory entry
				csc452_directory_entry entry;
				read_block(&entry, disk, root.directories[dir_index].nStartBlock);
				int file_index = find_file(entry.files, entry.nFiles, filename, extension);
				if (file_index != NOT_FOUND) {
					// File exists in directory
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 2;
					stbuf->st_size = entry.files[file_index].fsize;
					res = 0;
				}
			}
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

	//A directory holds two entries, one that represents itself (.)
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
	}
	else {
		// Get the directory name
		char directory[MAX_FILENAME + 2];
		char subdir;
		int ret = sscanf(path, "/%9[^/]/%8[^/]", directory, &subdir);
		if (ret > 1 || strlen(directory) > MAX_FILENAME) {
			// Not under the root directory only
			return -ENOENT;
		}

		// Read the root directory entries
		FILE *disk = fopen(".disk", "rb");
		csc452_root_directory root;
		read_block(&root, disk, ROOT_BLOCK);

		// Make sure the name does not already exist
		int dir_index = find_directory(root.directories, root.nDirectories, directory);
		if (dir_index == NOT_FOUND) {
			// Directory not found
			fclose(disk);
			return -ENOENT;
		} else {
			// List each file in this directory
			csc452_directory_entry entry;
			read_block(&entry, disk, root.directories[dir_index].nStartBlock);
			char filename[MAX_FILENAME + MAX_EXTENSION + 2];
			for (int i = 0; i < entry.nFiles; ++i) {
				// Fill `filename` buffer with the file name and add it
				combine_filename(filename, entry.files[i].fname, entry.files[i].fext);
				filler(buf, filename, NULL, 0);
			}
		}
        fclose(disk);
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

	// Get the name of the directory to create
	char directory[MAX_FILENAME + 2];
	char subdir;
	int ret = sscanf(path, "/%9[^/]/%8[^/]", directory, &subdir);
	if (ret > 1) {
		// Not under the root directory only
		return -EPERM;
	} else if (strlen(directory) > MAX_FILENAME) {
		// Name beyond 8 characters
		return -ENAMETOOLONG;
	}

	// Read the root directory entries
	FILE *disk = fopen(".disk", "rb+");
	csc452_root_directory root;
	read_block(&root, disk, ROOT_BLOCK);

    // Make sure the root directory is not full
    if (root.nDirectories > MAX_DIRS_IN_ROOT) {
        fclose(disk);
        return -ENOSPC;
    }

	// Make sure the name does not already exist
	if (find_directory(root.directories, root.nDirectories, directory) != NOT_FOUND) {
		// Directory already exists
		fclose(disk);
		return -EEXIST;
	}

    // Find a block to claim in free space tracking
    csc452_free_space_tracking free_space;
    read_block(&free_space, disk, FREE_SPACE_BLOCK);
    long dir_block = next_free_block(free_space);
    claim_block(&free_space, dir_block);

	// Add the new directory entry
	csc452_directory *dir = &root.directories[root.nDirectories++];
	strcpy(dir->dname, directory);
	dir->nStartBlock = dir_block;

    // Make sure the directory entry is clean (0 files)
    csc452_directory_entry dir_entry;
    read_block(&dir_entry, disk, dir_block);
    dir_entry.nFiles = 0;

	// Write the changed root directory entry to disk
    write_block(&root, disk, ROOT_BLOCK);
    write_block(&dir_entry, disk, dir_block);
    write_block(&free_space, disk, FREE_SPACE_BLOCK);
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
	(void) mode;
	(void) dev;

    // Get the directory / file path
    char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 2];
    char extension[MAX_EXTENSION + 2];
    int ret = sscanf(path, "/%8[^/]/%9[^.].%4s", directory, filename, extension);

    if (ret == 1) {
        // Trying to create in root directory
        return -EPERM;
    } else if (ret == 3) {
        if (strlen(filename) > MAX_FILENAME || strlen(extension) > MAX_EXTENSION) {
            // File name / extension was too long
            return -ENAMETOOLONG;
        }
    }

    // Read the root directory entries
    FILE *disk = fopen(".disk", "rb+");
    csc452_root_directory root;
    read_block(&root, disk, ROOT_BLOCK);

    // Find the directory to add the file to
    int dir_index = find_directory(root.directories, root.nDirectories, directory);
    if (dir_index == NOT_FOUND) {
        // Directory does not exist
        fclose(disk);
        return -ENOENT;
    }

    // Get the directory entry and make sure the file does not exist yet
    csc452_directory_entry dir_entry;
    long dir_block = root.directories[dir_index].nStartBlock;
    read_block(&dir_entry, disk, dir_block);
    if (dir_entry.nFiles > MAX_FILES_IN_DIR) {
        // Full on files
        fclose(disk);
        return -ENOSPC;
    }
    int file_index = find_file(dir_entry.files, dir_entry.nFiles, filename, extension);
    if (file_index != NOT_FOUND) {
        // File already exists
        fclose(disk);
        return -EEXIST;
    }

    // Find a block to claim in free space tracking
    csc452_free_space_tracking free_space;
    read_block(&free_space, disk, FREE_SPACE_BLOCK);
    long file_block = next_free_block(free_space);
    claim_block(&free_space, file_block);

    // Add the new file
    csc452_file_directory *file_entry = &dir_entry.files[dir_entry.nFiles++];
    strcpy(file_entry->fname, filename);
    strcpy(file_entry->fext, extension);
    file_entry->fsize = 0;
    file_entry->nStartBlock = file_block;

    // Make sure the file block is clean (next block rest to 0, acting as null link)
    csc452_disk_block file_data;
    read_block(&file_data, disk, file_block);
    file_data.nNextBlock = 0;

    // Write the changed directory entry to disk
    write_block(&dir_entry, disk, dir_block);
    write_block(&file_data, disk, file_block);
    write_block(&free_space, disk, FREE_SPACE_BLOCK);
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
    (void) fi;

    // Get the directory / file path
    char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
    int ret = sscanf(path, "/%8[^/]/%8[^.].%3s", directory, filename, extension);
    if (ret != 3) {
        return -EISDIR;
    }

    // Read the root directory entries
    FILE *disk = fopen(".disk", "rb");
    csc452_root_directory root;
    read_block(&root, disk, ROOT_BLOCK);

    // Find the directory with the file
    int dir_index = find_directory(root.directories, root.nDirectories, directory);
    if (dir_index == NOT_FOUND) {
        // Directory does not exist
        fclose(disk);
        return -ENOENT;
    }

    // Get the directory entry and make sure the file exists
    csc452_directory_entry entry;
    long dir_block = root.directories[dir_index].nStartBlock;
    read_block(&entry, disk, dir_block);
    int file_index = find_file(entry.files, entry.nFiles, filename, extension);
    if (file_index == NOT_FOUND) {
        // File does not exist
        fclose(disk);
        return -ENOENT;
    } else if (!(size > 0)) {
        // No data to write
        fclose(disk);
        return 0;
    }

    csc452_file_directory *file_entry = &entry.files[file_index];
    if (offset > file_entry->fsize) {
        // Offset is beyond the file size
        fclose(disk);
        return -EFBIG;
    }

    // Get the file entry and follow links to first page to read from
    csc452_disk_block file_data;
    long file_block = file_entry->nStartBlock;
    read_block(&file_data, disk, file_block);
    while (offset >= MAX_DATA_IN_BLOCK) {
        // Follow link to next block
        offset -= MAX_DATA_IN_BLOCK;
        file_block = file_data.nNextBlock;
        read_block(&file_data, disk, file_block);
    }

    size_t size_read = 0;
    while (size > 0) {
        // Write to current `file_data`
        size_t read_amt;
        if (size + offset >= MAX_DATA_IN_BLOCK) {
            read_amt = MAX_DATA_IN_BLOCK - offset;
        } else {
            read_amt = size;
        }
        memcpy(buf + size_read, file_data.data + offset, read_amt);
        size -= read_amt;
        size_read += read_amt;

        if (size > 0) {
            // Get next link to continue writing on next page
            file_block = file_data.nNextBlock;
            read_block(&file_data, disk, file_block);
        }
        offset = 0;
    }

    fclose(disk);
	return size_read;
}

/*
 * Helper function for freeing a chain of blocks used for a file.
 * Modifies free_space with the unclaimed blocks, but does not write
 * it back to the file (must be done by caller).
 */
static void free_file(FILE *disk, long file_block,
        csc452_free_space_tracking *free_space) {
    csc452_disk_block file_data;
    while (file_block != 0) {
        read_block(&file_data, disk, file_block);   // read file block being freed
        long next_block = file_data.nNextBlock;     // save next block
        file_data.nNextBlock = 0;                   // remove link to next block

        unclaim_block(free_space, file_block);      // unclaim the block being freed
        write_block(&file_data, disk, file_block);  // overwrite with link removed
        file_block = next_block;                    // advance to next block
    }
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

    // Get the directory / file path
    char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
    int ret = sscanf(path, "/%8[^/]/%8[^.].%3s", directory, filename, extension);
    if (ret != 3) {
        return -EISDIR;
    }

    // Read the root directory entries
    FILE *disk = fopen(".disk", "rb+");
    csc452_root_directory root;
    read_block(&root, disk, ROOT_BLOCK);

    // Find the directory with the file
    int dir_index = find_directory(root.directories, root.nDirectories, directory);
    if (dir_index == NOT_FOUND) {
        // Directory does not exist
        fclose(disk);
        return -ENOENT;
    }

    // Get the directory entry and make sure the file exists
    csc452_directory_entry entry;
    long dir_block = root.directories[dir_index].nStartBlock;
    read_block(&entry, disk, dir_block);
    int file_index = find_file(entry.files, entry.nFiles, filename, extension);
    if (file_index == NOT_FOUND) {
        // File does not exist
        fclose(disk);
        return -ENOENT;
    } else if (!(size > 0)) {
        // No data to write
        fclose(disk);
        return 0;
    }

    csc452_file_directory *file_entry = &entry.files[file_index];
    if (offset > file_entry->fsize) {
        // Offset is beyond the file size
        fclose(disk);
        return -EFBIG;
    }

    // Get the file entry and follow links to first page to write to
    csc452_disk_block file_data;
    long file_block = file_entry->nStartBlock;
    read_block(&file_data, disk, file_block);
    off_t original_offset = offset;      // save original offset value
    while (offset >= MAX_DATA_IN_BLOCK) {
        // Follow link to next block
        offset -= MAX_DATA_IN_BLOCK;
        file_block = file_data.nNextBlock;
        read_block(&file_data, disk, file_block);
    }

    // Read free space tracking
    csc452_free_space_tracking free_space;
    read_block(&free_space, disk, FREE_SPACE_BLOCK);

    size_t size_written = 0;
    while (size > 0) {
        // Write to current `file_data`
        size_t write_amt;
        if (size + offset >= MAX_DATA_IN_BLOCK) {
            write_amt = MAX_DATA_IN_BLOCK - offset;
        } else {
            write_amt = size;
        }

        // Copy some of the data from the buffer to the file data
        memcpy(file_data.data + offset, buf + size_written, write_amt);
        size -= write_amt;
        size_written += write_amt;

        if (size > 0) {
            // Get next link to continue writing on next page
            if (file_data.nNextBlock == 0) {
                // Need to claim next page and set a link
                long next_block = next_free_block(free_space);
                if (next_block == NOT_FOUND) {
                    // Out of space for more data
                    fclose(disk);
                    return -ENOSPC;
                }
                claim_block(&free_space, next_block);
                file_data.nNextBlock = next_block;
            }

            // Write the just-written block and move on to next file block
            write_block(&file_data, disk, file_block);
            file_block = file_data.nNextBlock;
            read_block(&file_data, disk, file_block);
        } else {
            // Write last page of file data
            write_block(&file_data, disk, file_block);
        }

        // Write to block and get next link
        offset = 0;
    }

    // Save the new file size in the file entry
    if (original_offset == 0) {
        // overwrote the file
        file_entry->fsize = size_written;

        // Read the last file block written to and free any remaining
        // blocks in the chain
        read_block(&file_data, disk, file_block);
        free_file(disk, file_data.nNextBlock, &free_space);
    } else {
        // appending to the file
        file_entry->fsize += size_written + original_offset - file_entry->fsize;
    }

	// Write data
    write_block(&entry, disk, dir_block);
    write_block(&free_space, disk, FREE_SPACE_BLOCK);
    fclose(disk);
	return size_written;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	// Get the name of the directory to remove
	char directory[MAX_FILENAME + 2];
	char subdir;
	int ret = sscanf(path, "/%9[^/]/%8[^/]", directory, &subdir);
	if (ret > 1) {
		// Not under the root directory only
		return -ENOTDIR;
	}

	// Read the root directory entries
	FILE *disk = fopen(".disk", "rb+");
	csc452_root_directory root;
	read_block(&root, disk, ROOT_BLOCK);

	// Make sure the name already exists
	int dir_index = find_directory(root.directories, root.nDirectories, directory);
	if (dir_index == NOT_FOUND) {
		// Directory already exists
		fclose(disk);
		return -ENOENT;
	}

	// Make sure there aren't any files in the directory
	csc452_directory_entry entry;
    long dir_block = root.directories[dir_index].nStartBlock;
	read_block(&entry, disk, dir_block);
	if (entry.nFiles != 0) {
		// Directory not empty
		fclose(disk);
		return -ENOTEMPTY;
	}

    // Unclaim the block in free space tracking
    csc452_free_space_tracking free_space;
    read_block(&free_space, disk, FREE_SPACE_BLOCK);
    unclaim_block(&free_space, dir_block);

	// Remove the directory from the root
    --root.nDirectories;
    if (root.nDirectories > 0) {
        // Move the last entry to the deleted spot
        root.directories[dir_index] = root.directories[root.nDirectories];
    }

	// Write the changed root directory entry to disk
    write_block(&root, disk, ROOT_BLOCK);
    write_block(&free_space, disk, FREE_SPACE_BLOCK);
	fclose(disk);
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
    // Get the directory / file path
    char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
    int ret = sscanf(path, "/%8[^/]/%8[^.].%3s", directory, filename, extension);
    if (ret != 3) {
        return -EISDIR;
    }

    // Read the root directory entries
    FILE *disk = fopen(".disk", "rb+");
    csc452_root_directory root;
    read_block(&root, disk, ROOT_BLOCK);

    // Find the directory with the file
    int dir_index = find_directory(root.directories, root.nDirectories, directory);
    if (dir_index == NOT_FOUND) {
        // Directory does not exist
        fclose(disk);
        return -ENOENT;
    }

    // Get the directory entry and make sure the file exists
    csc452_directory_entry dir_entry;
    long dir_block = root.directories[dir_index].nStartBlock;
    read_block(&dir_entry, disk, dir_block);
    int file_index = find_file(dir_entry.files, dir_entry.nFiles, filename, extension);
    if (file_index == NOT_FOUND) {
        fclose(disk);
        return -ENOENT;
    }

    // Read free space tracking
    csc452_free_space_tracking free_space;
    read_block(&free_space, disk, FREE_SPACE_BLOCK);

    // Follow links to remove the whole file
    long file_block = dir_entry.files[file_index].nStartBlock;
    free_file(disk, file_block, &free_space);

    // Remove the file entry from the directory
    --dir_entry.nFiles;
    if (dir_entry.nFiles > 0) {
        dir_entry.files[file_index] = dir_entry.files[dir_entry.nFiles];
    }

    write_block(&dir_entry, disk, dir_block);
    write_block(&free_space, disk, FREE_SPACE_BLOCK);
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
