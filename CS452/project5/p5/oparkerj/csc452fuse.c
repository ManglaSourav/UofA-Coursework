/*
	FUSE: Filesystem in Userspace

	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

// The disk file where the file system is stored
static const char *diskFile = ".disk";

// Success results from the getFileInfo function
#define TYPE_DIR 1
#define TYPE_FILE 2

// Flags for getFileInfo
#define DIR_ONLY 1
#define VALID_DIR (1 << 1)
#define DIR_ENTRY (1 << 2)
#define WANT_FILE (1 << 3)

// Macro that reads the disk root into the given struct
#define READ_ROOT(disk, info) readBlock((disk), 0, &(info))

// Used as a return statement which also closes the disk before returning
#define CLEAN_RETURN(disk, retval) {fclose(disk); return (retval);}

// Get the nth bit of value v
#define BIT(v, n) (((v) >> (n)) & 1)

// Set the nth bit of v to the given value
#define SET_BIT(v, n, s) v = ((v) & (~(1 << (n)))) | (((s) ? 1 : 0) << (n))

// Print macro that can be disabled by changing DEBUG to 0
#define DEBUG 0
#define PRINT(...) {if (DEBUG) {printf(__VA_ARGS__);}}

// Size of a disk block
#define BLOCK_SIZE 512

// we'll use 8.3 filenames
#define MAX_FILENAME 8
#define MAX_EXTENSION 3

// How many files can there be in one directory?
#define MAX_FILES_IN_DIR ((BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long)))

// The attribute packed means to not align these things
struct csc452_directory_entry
{
    int nFiles; //How many files are in this directory. (< MAX_FILES_IN_DIR)

    struct csc452_file_directory
    {
        char fname[MAX_FILENAME + 1];    // filename (plus null char)
        char fext[MAX_EXTENSION + 1];    // extension (plus null char)
        size_t fsize;                    // file size
        long nStartBlock;                // where the first block is on disk
    } __attribute__((packed)) files[MAX_FILES_IN_DIR];    // There is an array of these

    // This is some space to get this to be exactly the size of the disk block.
    // Don't use it for anything.
    char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct csc452_file_directory) - sizeof(int)];
};

typedef struct csc452_root_directory csc452_root_directory;

#define MAX_DIRS_IN_ROOT ((BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long)))

struct csc452_root_directory
{
    int nDirectories; // How many subdirectories are in the root (< MAX_DIRS_IN_ROOT)

    struct csc452_directory
    {
        char dname[MAX_FILENAME + 1];    // directory name (plus null char)
        long nStartBlock;                // where the directory block is on disk
    } __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];    // There is an array of these

    // This is some space to get this to be exactly the size of the disk block.
    // Don't use it for anything.
    char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct csc452_directory) - sizeof(int)];
};

typedef struct csc452_directory_entry csc452_directory_entry;

// How much data can one block hold?
#define MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(long))
// Number of blocks the bitmap stores
#define BLOCK_COUNT (MAX_DATA_IN_BLOCK << 3)

struct csc452_disk_block
{
    // Space in the block can be used for actual data storage.
    char data[MAX_DATA_IN_BLOCK];
    // Link to the next block in the chain
    long nNextBlock;
};

typedef struct csc452_disk_block csc452_disk_block;


// Union that lets us use the same space for different block types
typedef union
{
    csc452_root_directory root;
    csc452_directory_entry dir;
    struct csc452_file_directory file;
    struct csc452_directory *dir_entry;
    csc452_disk_block block;
} file_info;


// Open the disk file
FILE *getDiskFile(int write)
{
    return fopen(diskFile, write ? "r+b" : "rb");
}

// Read a block by index, Returns 0 on success
int readBlock(FILE *disk, long blockIndex, void *result)
{
    PRINT("-- reading block %ld\n", blockIndex)

    if (fseek(disk, blockIndex * BLOCK_SIZE, SEEK_SET))
    {
        return 1;
    }
    return !fread(result, BLOCK_SIZE, 1, disk);
}

/*
 * Set a bit in the free space tracking structure. The disk is only written to
 * if the value of the bit changed. This assumes that the index of the tracking bit is within
 * the bounds of the tracking structure.
 */
void setTrackingBit(FILE *disk, long blockIndex, int set)
{
    long offset = blockIndex;
    long current = 1;

    PRINT("setting tracking bit %ld to %d\n", blockIndex, (set ? 1 : 0))

    // Advance to the block at the given index
    csc452_disk_block free;
    readBlock(disk, current, &free);
    while (offset > BLOCK_COUNT)
    {
        current = free.nNextBlock;
        readBlock(disk, current, &free);
        offset -= BLOCK_COUNT;
    }

    // Only write if the bit value changed
    if (BIT(free.data[offset / 8], 7 - offset % 8) != (set ? 1 : 0))
    {
        // Flip the bit
        SET_BIT(free.data[offset / 8], 7 - offset % 8, set);
        fseek(disk, current * BLOCK_SIZE, SEEK_SET);
        fwrite(&free, BLOCK_SIZE, 1, disk);
    }
}

/*
 * Write a block to the disk file. This also sets the "in-use"
 * bit for the block in the tracking structure.
 */
void writeBlock(FILE *disk, long blockIndex, void *block)
{
    PRINT("++ writing to block %ld\n", blockIndex)
    setTrackingBit(disk, blockIndex, 1);

    // Write the block
    fseek(disk, blockIndex * BLOCK_SIZE, SEEK_SET);
    fwrite(block, BLOCK_SIZE, 1, disk);
}

// Fill a block with zeroes
void zeroBlock(FILE *disk, long blockIndex)
{
    char data[BLOCK_SIZE] = {0};
    writeBlock(disk, blockIndex, data);
}

// Forward declaration
int findFreeBlock(FILE *disk, long *index);

/*
 * Read data to or from disk, starting at the given block. This will read size
 * amount of data starting at the given offset. This function handles the
 * fact that the data is split across multiple blocks. If write is specified,
 * the data is written to the disk, otherwise it is read from the disk into
 * the pointer.
 */
int splitData(FILE *disk, char *data, off_t offset, size_t size, long startBlock, int write)
{
    long previous = 0;
    long current = startBlock;
    size_t read = offset;
    csc452_disk_block block;

    PRINT("Performing segmented write. block: %ld, start: %lld, len: %u\n", startBlock, offset, size)

    // Advance to the starting block that holds the data
    while (read >= MAX_DATA_IN_BLOCK)
    {
        PRINT("skipping block %ld\n", current)
        readBlock(disk, current, &block);
        current = block.nNextBlock;
        read -= MAX_DATA_IN_BLOCK;
    }
    read = 0;

    // Continue reading blocks until we have satisfied the request
    while (read < size)
    {
        // If we are trying to access a block of the file
        // that does not yet exist, try to add a new block to the file.
        if (previous && !current)
        {
            int fail = findFreeBlock(disk, &current);
            if (fail) return 1;
            // Update the last block with the location of the new block
            block.nNextBlock = current;
            writeBlock(disk, previous, &block);
        }

        readBlock(disk, current, &block);

        // Figure out the bounds of data to read in the current block
        size_t start = (current == startBlock ? offset % MAX_DATA_IN_BLOCK : 0);
        size_t len = (size - read < MAX_DATA_IN_BLOCK ? size - read : MAX_DATA_IN_BLOCK);
        if (start + len >= MAX_DATA_IN_BLOCK)
        {
            len = MAX_DATA_IN_BLOCK - start;
        }

        // Read or write the data
        if (write)
        {
            memcpy(block.data + start, data + read, len);
            writeBlock(disk, current, &block);
        }
        else
        {
            memcpy(data + read, block.data + start, len);
        }

        PRINT("advancing to next block\n")

        read += len;
        previous = current;
        current = block.nNextBlock;
    }
    PRINT("done writing\n")

    return 0;
}

// Search the root for a dir by name and read the result into the directory struct.
int findDirectory(FILE *disk, csc452_root_directory *root, const char *name, file_info *result, int entryOnly)
{
    const struct csc452_directory *entries = root->directories;

    for (int i = 0; i < MAX_DIRS_IN_ROOT; i++)
    {
        if (strcmp(entries[i].dname, name) == 0)
        {
            if (result == NULL) return 0;
            if (entryOnly)
            {
                result->dir_entry = (struct csc452_directory *) &entries[i];
                return 0;
            }
            return readBlock(disk, entries[i].nStartBlock, result);
        }
    }

    return 1;
}

/*
 * Search a directory for a file by name. Return the starting block of the file.
 * Also sets fileIndex to the index of the file in the directory array.
 */
long findFile(const csc452_directory_entry *dir, const char *name, const char *ext, int *fileIndex)
{
    const struct csc452_file_directory *entries = dir->files;

    PRINT("looking for file in directory: %s.%s\n", name, ext)

    for (int i = 0; i < MAX_FILES_IN_DIR; i++)
    {
        if (!entries[i].fname[0]) continue;
        PRINT("entry %d: %s.%s\n", i, entries[i].fname, entries[i].fext)
        if (strcmp(entries[i].fname, name) == 0 && strcmp(entries[i].fext, ext) == 0)
        {
            PRINT("Match!!!\n")
            if (fileIndex != NULL)
            {
                *fileIndex = i;
            }
            return entries[i].nStartBlock;
        }
    }

    return 0;
}

/*
 * Query the disk for a file. By default, this function will return 0 if the file does not exist
 * or represents an invalid path, or TYPE_DIR or TYPE_FILE.
 * This returns -1 when a directory or part of a filename is too long.
 * If the path represents a directory, the info is set to the directory struct.
 * If the path represents a file, the info is set to the file struct.
 *
 * If flags contains DIR_ONLY, then this function will stop after the directory portion of the path,
 * even if a file is specified.
 *
 * If VALID_DIR flag is set, this function returns a boolean value that indicates whether the path
 * could be a valid directory, but will be -1 if the name is too long.
 *
 * If DIR_ENTRY flags is set, this function behaves as if no flag is set, except info is set to the
 * pointer to the directory entry in the root rather than the block from the file.
 * Even if the path represents a file, the pointer to the directory entry is set.
 */
int getFileInfo(FILE *disk, const char *path, int flags, const file_info *info, file_info *result)
{
    char name[MAX_FILENAME + 1];
    char ext[MAX_EXTENSION + 1];
    size_t fullLen = strlen(path);

    PRINT("GET FILE INFO: %s\n", path)

    // Get and validate the directory part of the path
    const char *dirEnd = strchr(path + 1, '/');
    size_t len = dirEnd ? dirEnd - path - 1 : fullLen - 1;
    if (len > MAX_FILENAME) return -1;
    if (!dirEnd)
    {
        // If there is no trailing slash, adjust the end index
        dirEnd = path + fullLen;
    }

    // Read the directory from the disk file
    strncpy(name, path + 1, len);
    name[len] = '\0';
    PRINT("DIR: %s\n", name)
    if (!(flags & VALID_DIR))
    {
        int fail = findDirectory(disk, (csc452_root_directory *) info, name, result, (flags & DIR_ENTRY));
        PRINT("LOOKUP DIR: %d\n", !fail)
        if (fail) return (flags & WANT_FILE) ? TYPE_DIR : 0;
    }
    if (*dirEnd == '\0' || (flags & DIR_ONLY)) return TYPE_DIR;
    if (flags & VALID_DIR) return fullLen == dirEnd - path + 1;

    struct csc452_directory *entry;
    if (flags & DIR_ENTRY)
    {
        entry = result->dir_entry;
        readBlock(disk, entry->nStartBlock, result);
    }

    // Get and validate the filename part of the path
    const char *fileEnd = strchr(dirEnd + 1, '/');
    if (fileEnd) return 0;
    fileEnd = strchr(dirEnd + 1, '.');
    if (!fileEnd) return 0;
    len = fileEnd - dirEnd - 1;
    // Validate length
    if (len > MAX_FILENAME) return -1;
    if (fullLen - (fileEnd - path + 1) > MAX_EXTENSION) return -1;
    strncpy(name, dirEnd + 1, len);
    name[len] = '\0';
    strcpy(ext, fileEnd + 1);
    PRINT("FILE NAME: %s\n", name)
    PRINT("FILE EXT: %s\n", ext)

    // Search for the file
    int index;
    long block = findFile((const csc452_directory_entry *) result, name, ext, &index);
    PRINT("LOOKUP FILE: %ld\n", block)

    // Restore value if changed
    if (flags & DIR_ENTRY)
    {
        result->dir_entry = entry;
    }

    if (!block) return 0;
    if (!(flags & DIR_ENTRY))
    {
        result->file = result->dir.files[index];
    }
    return TYPE_FILE;
}

// Check if a block can fit fully in the disk file by index.
int isValidBlock(FILE *disk, long index)
{
    PRINT("checking if block %ld is valid\n", index)
    fseek(disk, 0, SEEK_END);
    size_t length = ftell(disk);
    PRINT("testing: %ld < %u: %d\n", index, length / BLOCK_SIZE, index < length / BLOCK_SIZE)
    return index < length / BLOCK_SIZE;
}

/*
 * Extend the bitmap in the disk file. This function checks that two blocks
 * can fit at the given index. Because there's no point in extending the bitmap
 * if we can't give out another free block.
 *
 * The previous block has the next pointer set to the new block, and both are
 * written to the disk.
 */
int writeFreeBlock(FILE *disk, csc452_disk_block *prev, long prevIndex, long index)
{
    // Create empty disk block
    csc452_disk_block block;
    memset(block.data, 0, sizeof(block.data));
    block.nNextBlock = 0;
    block.data[0] = (char) 0x80;

    // Check if the file can hold more blocks
    if (!isValidBlock(disk, index + 1)) return 1;

    // Write the new disk block and update the previous one
    prev->nNextBlock = index;
    fseek(disk, prevIndex * BLOCK_SIZE, SEEK_SET);
    fwrite(prev, sizeof(csc452_disk_block), 1, disk);
    fseek(disk, index * BLOCK_SIZE, SEEK_SET);
    fwrite(&block, sizeof(csc452_disk_block), 1, disk);
    return 0;
}

/*
 * Search bitmaps in the file to find a block that is free.
 * Returns 0 on success, or nonzero if there are no free blocks.
 */
int findFreeBlock(FILE *disk, long *index)
{
    csc452_disk_block block;
    readBlock(disk, 1, &block);
    block.data[0] |= (char) 0xc0;

    PRINT("searching for free block\n")

    long offset = 0;
    for (;;)
    {
        // Search the current block
        for (int i = 0; i < MAX_DATA_IN_BLOCK; i++)
        {
            PRINT("scanning byte %d\n", i)
            unsigned char data = block.data[i];
            if (data < 0xff)
            {
                PRINT("byte has free bit\n")
                // Get the exact free block
                for (int b = 7; b >= 0; b--)
                {
                    if (!((data >> b) & 1))
                    {
                        PRINT("using bit %d\n", b)
                        *index = i * 8 + offset + (7 - b);
                        // Make sure it is within the bounds of the disk file
                        return isValidBlock(disk, *index) ? 0 : 1;
                    }
                }
            }
        }

        PRINT("trying next block\n")

        // If there is no next block, try and create it
        if (!block.nNextBlock)
        {
            int full = writeFreeBlock(disk, &block, offset ? (long) (offset / BLOCK_COUNT) : 1, (long) (offset / BLOCK_COUNT + BLOCK_COUNT));
            if (full) return 1;
        }

        PRINT("reading next block\n")

        // Read the next block in the bitmap
        readBlock(disk, block.nNextBlock, &block);
        offset += BLOCK_COUNT;
    }
}

/*
 * Extract a specific part of the path to the destination.
 * 0 = directory name
 * 1 = file name
 * else = file extension
 */
void copyPathComponent(const char *path, int index, char *dest)
{
    path++;
    // Read the directory part
    char *dirEnd = strchr(path, '/');
    if (!dirEnd)
    {
        dirEnd = (char *) path + strlen(path);
    }
    if (index == 0)
    {
        strncpy(dest, path, dirEnd - path);
        dest[dirEnd - path] = '\0';
        return;
    }

    // Read the filename part
    char *dot = strchr(dirEnd + 1, '.');
    if (index == 1)
    {
        strncpy(dest, dirEnd + 1, dot - dirEnd - 1);
        dest[dot - dirEnd - 1] = '\0';
        return;
    }

    // Read the extension
    strcpy(dest, dot + 1);
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

    if (strcmp(path, "/") == 0)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else
    {
        FILE *disk = getDiskFile(0);
        file_info info;
        READ_ROOT(disk, info);
        int result = getFileInfo(disk, path, 0, &info, &info);
        fclose(disk);
        PRINT("getattr fileinfo: %d\n", result)

        // Fill in the info depending on the file type
        if (result == TYPE_DIR)
        {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
        }
        else if (result == TYPE_FILE)
        {
            PRINT("is file; size: %u\n", info.file.fsize)
            stbuf->st_mode = S_IFREG | 0666;
            stbuf->st_nlink = 2;
            stbuf->st_size = (long) info.file.fsize;
        }
        else
        {
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

    FILE *disk = getDiskFile(0);
    file_info info;
    READ_ROOT(disk, info);

    //A directory holds two entries, one that represents itself (.)
    //and one that represents the directory above us (..)
    if (strcmp(path, "/") == 0)
    {
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        // Iterate and add directories
        for (int i = 0; i < MAX_DIRS_IN_ROOT; i++)
        {
            if (!info.root.directories[i].dname[0]) continue;
            filler(buf, info.root.directories[i].dname, NULL, 0);
        }
    }
    else
    {
        int success = getFileInfo(disk, path, DIR_ONLY, &info, &info);
        if (success <= 0) CLEAN_RETURN(disk, -ENOENT)

        char name[MAX_FILENAME + MAX_EXTENSION + 2];

        // Iterate and add files
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);
        for (int i = 0; i < MAX_FILES_IN_DIR; i++)
        {
            if (!info.dir.files[i].fname[0]) continue;
            sprintf(name, "%s.%s", info.dir.files[i].fname, info.dir.files[i].fext);
            filler(buf, name, NULL, 0);
        }
    }

    fclose(disk);
    return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
    (void) mode;

    FILE *disk = getDiskFile(1);
    file_info info;
    READ_ROOT(disk, info);

    // Validate the request
    int success = getFileInfo(disk, path, VALID_DIR, &info, &info);
    if (success < 0) CLEAN_RETURN(disk, -ENAMETOOLONG)
    if (!success) CLEAN_RETURN(disk, -EPERM)
    success = getFileInfo(disk, path, DIR_ONLY, &info, &info);
    if (success > 0) CLEAN_RETURN(disk, -EEXIST)
    PRINT("mkdir root dirs: %d, max: %u\n", info.root.nDirectories, MAX_DIRS_IN_ROOT)
    if (info.root.nDirectories == MAX_DIRS_IN_ROOT) CLEAN_RETURN(disk, -ENOSPC)

    // Find a free block
    long blockIndex;
    int fail = findFreeBlock(disk, &blockIndex);
    if (fail) CLEAN_RETURN(disk, -ENOSPC)

    PRINT("mkdir free block: %ld\n", blockIndex)

    // Update the root block with the directory
    info.root.nDirectories++;
    zeroBlock(disk, blockIndex);

    // Find the next free spot to put the directory entry
    for (int i = 0; i < MAX_DIRS_IN_ROOT; i++)
    {
        if (!info.root.directories[i].dname[0])
        {
            PRINT("mkdir using slot: %d\n", i)
            copyPathComponent(path, 0, info.root.directories[i].dname);
            info.root.directories[i].nStartBlock = blockIndex;
            writeBlock(disk, 0, &info);
            CLEAN_RETURN(disk, 0)
        }
    }

    CLEAN_RETURN(disk, -ENOSPC)
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

    FILE *disk = getDiskFile(1);
    file_info root;
    file_info info;
    READ_ROOT(disk, root);

    // Validate the request
    int result = getFileInfo(disk, path, DIR_ENTRY | WANT_FILE, &root, &info);
    if (result == TYPE_DIR) CLEAN_RETURN(disk, -EPERM)
    if (result == TYPE_FILE) CLEAN_RETURN(disk, -EEXIST)
    if (result < 0) CLEAN_RETURN(disk, -ENAMETOOLONG)

    // Read the directory
    long dirBlock = info.dir_entry->nStartBlock;
    readBlock(disk, dirBlock, &info);
    if (info.dir.nFiles == MAX_FILES_IN_DIR) CLEAN_RETURN(disk, -ENOSPC)

    // Find free block
    long blockIndex;
    int fail = findFreeBlock(disk, &blockIndex);
    if (fail) CLEAN_RETURN(disk, -ENOSPC)

    // Update the directory entry
    info.dir.nFiles++;
    zeroBlock(disk, blockIndex);

    // Find the next free spot to put the file entry
    for (int i = 0; i < MAX_FILES_IN_DIR; i++)
    {
        if (info.dir.files[i].fname[0]) continue;
        PRINT("putting file in slot: %d\n", i)
        copyPathComponent(path, 1, info.dir.files[i].fname);
        copyPathComponent(path, 2, info.dir.files[i].fext);
        info.dir.files[i].fsize = 0;
        info.dir.files[i].nStartBlock = blockIndex;
        writeBlock(disk, dirBlock, &info);
        CLEAN_RETURN(disk, 0)
    }

    CLEAN_RETURN(disk, -ENOSPC)
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

    FILE *disk = getDiskFile(0);
    file_info info;
    READ_ROOT(disk, info);

    // Validate the request
    int result = getFileInfo(disk, path, 0, &info, &info);
    if (result == TYPE_DIR) CLEAN_RETURN(disk, -EISDIR)
    if (result <= 0) CLEAN_RETURN(disk, -ENOENT)
    if (size <= 0) CLEAN_RETURN(disk, -EINVAL)
    if (offset < 0 || offset > info.file.fsize) CLEAN_RETURN(disk, -EINVAL)
    // The OS may ask for more data than exists in the file.
    // If so reduce size to be at the end of the file.
    if (offset + size > info.file.fsize)
    {
        size = info.file.fsize - offset;
    }

    // Read the data from the disk
    int fail = splitData(disk, buf, offset, size, info.file.nStartBlock, 0);
    if (fail) CLEAN_RETURN(disk, -EIO)

    fclose(disk);
    return (int) size;
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

    FILE *disk = getDiskFile(1);
    file_info root;
    file_info info;
    READ_ROOT(disk, root);

    // Validate the write operation
    int result = getFileInfo(disk, path, DIR_ENTRY, &root, &info);
    PRINT("write result: %d\n", result)
    if (result == TYPE_DIR) CLEAN_RETURN(disk, -EISDIR)
    if (result <= 0) CLEAN_RETURN(disk, -ENOENT)
    if (size <= 0) CLEAN_RETURN(disk, -EINVAL)

    // Keep some information about the directory
    // in case the file size needs to be updated.
    long directoryBlock = info.dir_entry->nStartBlock;
    readBlock(disk, directoryBlock, &info);
    char name[MAX_FILENAME + 1];
    char ext[MAX_EXTENSION + 1];
    copyPathComponent(path, 1, name);
    copyPathComponent(path, 2, ext);
    int fileIndex;
    findFile(&info.dir, name, ext, &fileIndex);
    struct csc452_file_directory *fileInfo = &info.dir.files[fileIndex];

    if (offset < 0 || offset > fileInfo->fsize) CLEAN_RETURN(disk, -EFBIG)

    // Write the data
    int fail = splitData(disk, (char *) buf, offset, size, fileInfo->nStartBlock, 1);
    if (fail) CLEAN_RETURN(disk, -ENOSPC)

    // Check if the file size needs to be updated
    if (offset + size > fileInfo->fsize)
    {
        PRINT("updating file size from %u to %lld\n", fileInfo->fsize, offset + size)
        fileInfo->fsize = offset + size;
        writeBlock(disk, directoryBlock, &info);
    }

    fclose(disk);
    return (int) size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
    FILE *disk = getDiskFile(1);
    file_info root;
    READ_ROOT(disk, root);
    file_info info;

    // Validate the request
    int result = getFileInfo(disk, path, 0, &root, &info);
    if (result == TYPE_FILE) CLEAN_RETURN(disk, -ENOTDIR)
    if (result <= 0) CLEAN_RETURN(disk, -ENOENT)
    // Make sure the directory is empty
    if (info.dir.nFiles) CLEAN_RETURN(disk, -ENOTEMPTY)

    // The directory is empty, it is safe to delete
    getFileInfo(disk, path, DIR_ENTRY, &root, &info);
    root.root.nDirectories--;
    info.dir_entry->dname[0] = '\0';
    writeBlock(disk, 0, &root);
    setTrackingBit(disk, info.dir_entry->nStartBlock, 0);

    fclose(disk);
    return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
    FILE *disk = getDiskFile(1);
    file_info root;
    file_info file;
    READ_ROOT(disk, root);

    // Check that the file exists
    int result = getFileInfo(disk, path, 0, &root, &file);
    if (result == TYPE_DIR) CLEAN_RETURN(disk, -EISDIR)
    if (result <= 0) CLEAN_RETURN(disk, -ENOENT)

    // Store the basic file info
    char name[MAX_FILENAME + 1];
    char ext[MAX_EXTENSION + 1];
    strcpy(name, file.file.fname);
    strcpy(ext, file.file.fext);
    PRINT("unlinking: %s.%s\n", name, ext)
    long current = file.file.nStartBlock;

    PRINT("removing file entry from directory\n")
    // Remove the file from the directory
    getFileInfo(disk, path, DIR_ENTRY, &root, &file);
    long blockIndex = file.dir_entry->nStartBlock;
    readBlock(disk, blockIndex, &root);
    for (int i = 0; i < MAX_FILES_IN_DIR; i++)
    {
        if (strcmp(root.dir.files[i].fname, name) == 0 && strcmp(root.dir.files[i].fext, ext) == 0)
        {
            PRINT("removing file from slot %d\n", i)
            root.dir.nFiles--;
            root.dir.files[i].fname[0] = '\0';
            writeBlock(disk, blockIndex, &root);
            break;
        }
    }

    PRINT("freeing file blocks, start: %ld\n", current)
    // Free up the blocks used by the file
    while (current)
    {
        readBlock(disk, current, &file);
        long next = file.block.nNextBlock;
        PRINT("marking block %ld as free\n", current)
        setTrackingBit(disk, current, 0);
        current = next;
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
static int csc452_flush(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    (void) fi;

    return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations csc452_oper = {
        .getattr  = csc452_getattr,
        .readdir  = csc452_readdir,
        .mkdir    = csc452_mkdir,
        .read     = csc452_read,
        .write    = csc452_write,
        .mknod    = csc452_mknod,
        .truncate = csc452_truncate,
        .flush    = csc452_flush,
        .open     = csc452_open,
        .unlink   = csc452_unlink,
        .rmdir    = csc452_rmdir
};

//Don't change this.
int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &csc452_oper, NULL);
}
