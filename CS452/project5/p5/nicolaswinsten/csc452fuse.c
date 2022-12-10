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
#include <math.h>

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

csc452_directory_entry *allocDirEntry() {
  csc452_directory_entry *entry = (csc452_directory_entry *) malloc(sizeof(csc452_directory_entry));
  bzero(entry, sizeof(csc452_directory_entry));
  return entry;
}

// read the root directory entry off .disk
int getRootEntry(csc452_root_directory *rootEntry) {
  int ret = 0;
  FILE *disk = fopen(".disk", "rb");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return -1;
  }
  
  if (fread(rootEntry, sizeof(csc452_root_directory), 1, disk) == 0) {
    printf("something went wrong, reading the root directory entry from disk\n");
    ret = -1;
  }
  
  fclose(disk);
  return ret;
}

// return ptr to directory entry for the directory if it exists
// otherwise return NULL
//
// extra param will store the directory's block number
csc452_directory_entry *getDirEntry(char *dirName, long *startBlock) {
  csc452_directory_entry *dirEntry = NULL;
  if (strlen(dirName) == 0) return NULL;
  
  // read the root directory
  csc452_root_directory *root = (csc452_root_directory *) malloc(sizeof(csc452_root_directory));
  getRootEntry(root);
  
  FILE *disk = fopen(".disk", "rb");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return NULL;
  }
  
  // loop through the root's directory array
  int i;
  for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
    struct csc452_directory currDir = root->directories[i];
    if (strlen(currDir.dname) == 0) continue; // this cell in the root entry is empty
    
    printf("I see currDir: %s\n", currDir.dname);
    if (strcmp(dirName, currDir.dname) == 0) {
      *startBlock = currDir.nStartBlock;
      // move to directory entry location
      if (fseek(disk, (*startBlock) * BLOCK_SIZE, SEEK_SET) == -1) {
        printf("Something went wrong seeking to directory block for %s\n", dirName);
      } else { // read the entry
        dirEntry = allocDirEntry();
        if (fread(dirEntry, sizeof(csc452_directory_entry), 1, disk) == 0) {
          printf("Something went wrong reading directory entry for %s\n", dirName);
          free(dirEntry);
          dirEntry = NULL;
        }
      }
      break;
    }
  }
  fclose(disk);
  free(root);
  return dirEntry;
}

void writeDirEntry(csc452_directory_entry *entry, long blockNumber) {
  FILE *disk = fopen(".disk", "rb+");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return;
  }
 
  fseek(disk, blockNumber*BLOCK_SIZE, SEEK_SET);
  fwrite(entry, sizeof(csc452_directory_entry), 1, disk);

  fclose(disk);
}

// returns the index of the file in the given directory
int getFileIndex(csc452_directory_entry *dir, char *filename, char *ext) {
  char fullName[MAX_FILENAME + MAX_EXTENSION + 2];
  strcpy(fullName, filename);
  if (strlen(ext) != 0) strcat(fullName, ".");
  strcat(fullName, ext);
  
  int i;
  for (i = 0; i < MAX_FILES_IN_DIR; i++) {
    char otherName[MAX_FILENAME + MAX_EXTENSION + 2];
    strcpy(otherName, dir->files[i].fname);
    if (strlen(dir->files[i].fext) != 0) strcat(otherName, ".");
    strcat(otherName, dir->files[i].fext);
    
    if (strcmp(fullName, otherName) == 0)
      return i;
  }
  
  return -1;
}

csc452_disk_block *readFileBlock(long);
void clearBlock(long);
long findFirstFreeBlock();
void writeDataBlock(csc452_disk_block *, long);
int updateBitmap(long, int);

void eraseFile(long blockNumber) {
  if (blockNumber == 0) return;
  csc452_disk_block *block;
  do {
    printf("erasing block at %ld\n", blockNumber);
    block = readFileBlock(blockNumber);
    clearBlock(blockNumber);
    updateBitmap(blockNumber, 0);
    blockNumber = block->nNextBlock; 
    free(block);
  } while (blockNumber != 0);
}


// update a file's size be deallocating/allocating blocks as necessary
void updateFileSize(csc452_directory_entry *dir, long dirBlockNumber, char *filename, char *ext, size_t newSize) {
  int i = getFileIndex(dir, filename, ext);
  if (i == -1) {
    printf("couldnt find file %s.%s\n", filename, ext);
    return;
  }
  
  dir->files[i].fsize = newSize;
  writeDirEntry(dir, dirBlockNumber);
  
  long neededBlocks = (newSize / MAX_DATA_IN_BLOCK) + 1;
  printf("file %s.%s was at index %d and it needs %ld blocks\n", filename, ext, i, neededBlocks);
  
  csc452_disk_block *block;
  long fileBlock = dir->files[i].nStartBlock;
  for (i = 0; i < neededBlocks; i++) {
    block = readFileBlock(fileBlock);
    printf("looking at file block %ld with next block %ld\n", fileBlock, block->nNextBlock);
    
    if (block->nNextBlock == 0 && i < neededBlocks - 1) {
      // another block is needed so allocate it
      long newBlock = findFirstFreeBlock();
      block->nNextBlock = newBlock;
      printf("allocating new block %ld\n", newBlock);
      writeDataBlock(block, fileBlock);
      updateBitmap(newBlock, 1);
      fileBlock = newBlock;
    } else if (i == neededBlocks - 1) {
      // cut off the file block chain
      int temp = fileBlock;
      fileBlock = block->nNextBlock;
      block->nNextBlock = 0;
      writeDataBlock(block, temp);
    } else {
      fileBlock = block->nNextBlock;
    }
    free(block);
  }
  
  // erase any extra blocks
  eraseFile(fileBlock);
}

void writeRootEntry(csc452_root_directory *entry) {
  FILE *disk = fopen(".disk", "rb+");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return;
  }
 
  fseek(disk, 0, SEEK_SET);
  fwrite(entry, sizeof(csc452_root_directory), 1, disk);

  fclose(disk);
}

csc452_disk_block *readFileBlock(long blockNumber) {
  FILE *disk = fopen(".disk", "rb");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return NULL;
  }
  
  csc452_disk_block *block = (csc452_disk_block *) malloc(sizeof(csc452_disk_block));
 
  fseek(disk, blockNumber*BLOCK_SIZE, SEEK_SET);
  fread(block, sizeof(csc452_disk_block), 1, disk);

  fclose(disk);
  return block;
}

void writeDataBlock(csc452_disk_block *block, long blockNumber) {
  FILE *disk = fopen(".disk", "rb+");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return;
  }
 
  fseek(disk, blockNumber*BLOCK_SIZE, SEEK_SET);
  fwrite(block, sizeof(csc452_disk_block), 1, disk);

  fclose(disk);
}

void clearBlock(long blockNumber) {
  FILE *disk = fopen(".disk", "rb+");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return;
  }
  
  printf("clearing block %ld\n", blockNumber);
  
  csc452_disk_block *block = (csc452_disk_block *) malloc(sizeof(csc452_disk_block));
  bzero(block, sizeof(csc452_disk_block));
  fseek(disk, blockNumber*BLOCK_SIZE, SEEK_SET);
  fwrite(block, sizeof(csc452_disk_block), 1, disk);

  fclose(disk);
}

// returns 1 if a file exists with that name in the given directory
// otherwise return 0
int fileExists(csc452_directory_entry *dir, char *fileName, char *extension, size_t *fileSize, long *startBlock) {
  printf("searching for file %s.%s\n", fileName, extension);
  
  int i = getFileIndex(dir, fileName, extension);
  if (i == -1) return 0;
  
  *fileSize = dir->files[i].fsize;
  *startBlock = dir->files[i].nStartBlock;
  
  return 1;
}

// reads the bitmap off disk, copies it into memory, and returns a ptr to it
char *readBitmap() {
  FILE *disk = fopen(".disk", "rb");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return NULL;
  }
  void *map = malloc(BLOCK_SIZE);

  fseek(disk, -BLOCK_SIZE, SEEK_END);
  fread(map, BLOCK_SIZE, 1, disk);
  fclose(disk);
  
  return (char *) map;
}

// returns the first free block according to the bitmap (not really a bitmap) stored on the end of disk
long findFirstFreeBlock() {
  char *map = readBitmap();
  
  int i;
  char *ptr = map + 1;
  for (i = 0; i < BLOCK_SIZE - 1; i++) {
    if (*(ptr + i) == 0)
      return i + 1;
  }
  
  free(map);
  return -1;
}

int numFreeBlocks() {
  char *map = readBitmap();
  
  int count = 0;
  char *ptr = map + 1;
  int i;
  for (i = 0; i < BLOCK_SIZE - 1; i++) {
    if (*(ptr + i) == 0) count++;
  }
  return count;
}

// update the bitmap on disk to indicate block is free or not free
int updateBitmap(long block, int mark) {
  FILE *disk = fopen(".disk", "rb+");
  if (disk == NULL) {
    printf("I couldn't find your disk\n");
    return -1;
  }
  
  char w;
  if (mark) w = 1;
  else w = 0;
 
  fseek(disk, -BLOCK_SIZE, SEEK_END);
  fseek(disk, block, SEEK_CUR);
  fwrite(&w, 1, 1, disk);

  fclose(disk);
  return 0;
}

// parse the given path string
// copy directory name, file name, and extension into buffers
void parsePath(const char *path, char *dir, char *file, char *ext) {
  bzero(dir, MAX_FILENAME + 1);
  bzero(file, MAX_FILENAME + 1);
  bzero(ext, MAX_EXTENSION + 1);
  
  if (sscanf(path, "/%[^/]/%[^.].%s", dir, file, ext) == 0)
    printf("path is malformed\n");
  
  dir[MAX_FILENAME] = '\0';
  file[MAX_FILENAME] = '\0';
  ext[MAX_EXTENSION] = '\0';
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
  printf("called getattr\n");
  fflush(stdout);
  int res = 0;
  // root path
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
    char dir[MAX_FILENAME+1];
    char fname[MAX_FILENAME+1];
    char ext[MAX_EXTENSION+1];
    parsePath(path, dir, fname, ext); // parse out the directory name
    
    // read directory entry and find where it starts
    long startBlock;
    csc452_directory_entry *entry = getDirEntry(dir, &startBlock);
    if (entry == NULL) { // couldn't find the directory
      return -ENOENT;
    }
    
    size_t fileSize;
    long fileBlock;
    int pathIsDir = strlen(fname) == 0;
    
    if (pathIsDir) {
      // path is a valid directory
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
    } else if (fileExists(entry, fname, ext, &fileSize, &fileBlock)) {
      // path is a valid file
      stbuf->st_mode = S_IFREG | 0666;
      stbuf->st_nlink = 2;
      stbuf->st_size = fileSize;
    } else {
      // couldn't find the file
      res = -ENOENT;
    }
    free(entry);
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
  printf("called readdir\n");
  fflush(stdout);
  int i;
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
  
  char dir[MAX_FILENAME+1];
  char filename[MAX_FILENAME+1];
  char ext[MAX_EXTENSION+1];
  parsePath(path, dir, filename, ext);

	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
    // read the root directory
    csc452_root_directory *root = (csc452_root_directory *) malloc(sizeof(csc452_root_directory));
    getRootEntry(root);
    for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
      if (strcmp(root->directories[i].dname, "") == 0) continue;
      filler(buf, root->directories[i].dname, NULL, 0);
    }
    free(root);
	}
	else {
    // read directory entry
    long startBlock;
    csc452_directory_entry *entry = getDirEntry(dir, &startBlock);
    if (entry == NULL) { // couldn't find the directory
      return -ENOENT;
    }
    
    filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
    
    // loop through files and grab their full names
		for (i = 0; i < MAX_FILES_IN_DIR; i++) {
      if (strlen(entry->files[i].fname) == 0) continue; // no file entry in this cell of the array
      
      char foundFileName[MAX_FILENAME + MAX_EXTENSION + 2];
      strcpy(foundFileName, entry->files[i].fname);
      if (strlen(entry->files[i].fext) != 0) strcat(foundFileName, ".");
      strcat(foundFileName, entry->files[i].fext);
      filler(buf, foundFileName, NULL, 0);
    }
    free(entry);
	}

	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
  printf("called mkdir\n");
  fflush(stdout);
	(void) path;
	(void) mode;
  
  char dir[MAX_FILENAME+1];
  char filename[MAX_FILENAME+1];
  char ext[MAX_EXTENSION+1];
  parsePath(path, dir, filename, ext);
  
  if (strlen(filename) > 0) {
    // the given path is not in the root
    return -EPERM;
  }
  
  if (strlen(dir) > MAX_FILENAME) {
    printf("directory name %s is too long\n", dir);
    return -ENAMETOOLONG;
  }
  
  long startBlock;
  csc452_directory_entry *entry = getDirEntry(dir, &startBlock);
  if (entry != NULL) { // directory already exists
    printf("directory %s already exists\n", dir);
    free(entry);
    return -EEXIST;
  }
  
  csc452_root_directory *root = (csc452_root_directory *) malloc(sizeof(csc452_root_directory));
  getRootEntry(root);
  
  int numDirs = root->nDirectories;
  if (numDirs == MAX_DIRS_IN_ROOT) {
    printf("too many dirs\n");
    free(root);
    return -EPERM;
  }
  
  // find first free block
  long firstFreeBlock = findFirstFreeBlock();
  printf("found free block %ld\n", firstFreeBlock);
  updateBitmap(firstFreeBlock, 1);
  
  // write directory entry to disk
  entry = allocDirEntry();
  writeDirEntry(entry, firstFreeBlock); // does this do anything really?
  
  // update root entry
  int i;
  for (i = 0; i < MAX_DIRS_IN_ROOT; i++)
    if (strcmp(root->directories[i].dname, "") == 0) {
      printf("found empty dir spot in root entry at position %d\n", i);
      break;
    }
  strcpy(root->directories[i].dname, dir);
  root->directories[i].nStartBlock = firstFreeBlock;
  root->nDirectories++;
  
  // write root entry to disk
  writeRootEntry(root);

  free(root);
  free(entry);
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
	
  char dir[MAX_FILENAME + 1];
  char filename[MAX_FILENAME + 1];
  char ext[MAX_EXTENSION + 1];
  parsePath(path, dir, filename, ext);
  
  // if filename or extension too long, error
  if (strlen(filename) > MAX_FILENAME || strlen(ext) > MAX_EXTENSION)
    return -ENAMETOOLONG;
  
  // file is trying to be made in the root
  if (strlen(filename) == 0)
    return -EPERM;
  
  // get the directory entry for the file
  long startBlock;
  csc452_directory_entry *entry = getDirEntry(dir, &startBlock);
  if (entry == NULL) { // couldn't find the directory
    return -EPERM;
  }
  
  size_t fileSize;
  long fileBlock;
  if (fileExists(entry, filename, ext, &fileSize, &fileBlock)) {
    // file already exists
    free(entry);
    return -EEXIST;
  }
  
  if (entry->nFiles == MAX_FILES_IN_DIR) {
    printf("there's no more room in this directory\n");
    free(entry);
    return -EPERM;
  }
  
  // find a free block for the file
  int freeBlock = findFirstFreeBlock();
  updateBitmap(freeBlock, 1);
  entry->nFiles++;
  
  // update the directory entry
  int i;
  for (i = 0; i < MAX_FILES_IN_DIR; i++) {
    if (strlen(entry->files[i].fname) == 0) {
      strcpy(entry->files[i].fname, filename);
      strcpy(entry->files[i].fext, ext);
      entry->files[i].fsize = 0;
      entry->files[i].nStartBlock = freeBlock;
      break;
    }
  }
  
  // write the new directory entry to disk
  writeDirEntry(entry, startBlock);
  
  // clear out the space on disk for the file block
  clearBlock(freeBlock);
  
  free(entry);
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
  
  char dir[MAX_FILENAME + 1];
  char filename[MAX_FILENAME + 1];
  char ext[MAX_EXTENSION + 1];
  parsePath(path, dir, filename, ext);
  
  // get the directory entry for the file
  long startBlock;
  csc452_directory_entry *entry = getDirEntry(dir, &startBlock);
  if (entry == NULL) { // couldn't find the directory
    return -1;
  }
  
  // tried reading a directory
  if (strlen(filename) == 0) {
    free(entry);
    return -EISDIR;
  }

	//check to make sure path exists
  size_t fileSize;
  long fileStartBlock;
  if (!fileExists(entry, filename, ext, &fileSize, &fileStartBlock)) {
    // file does not exist
    printf("file %s does not exist in dir %s\n", filename, dir);
    free(entry);
    return -1;
  }
	//check that size is > 0
	//check that offset is <= to the file size
  if (fileSize < offset) {
    printf("offset %d out of bounds for file %s\n", (int)offset, filename);
    free(entry);
    return 0;
  }
  
  free(entry);
  
  csc452_disk_block *block;
  size_t amountToRead = size < (fileSize - offset) ? size : (fileSize - offset);
  size_t amountRead = amountToRead;
  long ptr = 0;
	//read in data
  while(amountToRead > 0) {
    printf("reading from block %ld\n", fileStartBlock);
    block = readFileBlock(fileStartBlock);
    if (offset >= MAX_DATA_IN_BLOCK) {
      // skip over to the next block
      offset -= MAX_DATA_IN_BLOCK;
    } else {
      size_t leftInTheBlock = MAX_DATA_IN_BLOCK - offset;
      size_t readnow = amountToRead < leftInTheBlock ? amountToRead : leftInTheBlock;
      memcpy(buf + ptr, ((char *)block) + offset, readnow);
      offset = 0;
      ptr += readnow;
      amountToRead -= readnow;
    }
    fileStartBlock = block->nNextBlock;
    printf("next block is block %ld\n", fileStartBlock);
    free(block);
  }
  
  
	//return success, or error

	return amountRead;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
  printf("calling write with size %d and offset %d\n", (int) size, (int) offset);
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//return success, or error
  
  char dir[MAX_FILENAME + 1];
  char filename[MAX_FILENAME + 1];
  char ext[MAX_EXTENSION + 1];
  parsePath(path, dir, filename, ext);
  
  // get the directory entry for the file
  long startBlock;
  csc452_directory_entry *entry = getDirEntry(dir, &startBlock);
  if (entry == NULL) { // couldn't find the directory
    return -1;
  }
  
  // tried writing a directory
  if (strlen(filename) == 0) {
    free(entry);
    return -1;
  }

	//check to make sure path exists
  size_t fileSize;
  long fileStartBlock;
  if (!fileExists(entry, filename, ext, &fileSize, &fileStartBlock)) {
    // file does not exist
    printf("file %s does not exist in dir %s\n", filename, dir);
    free(entry);
    return -1;
  }
	//check that size is > 0
	//check that offset is <= to the file size
  if (fileSize < offset) {
    printf("offset %d out of bounds for file %s\n", (int)offset, filename);
    free(entry);
    return -EFBIG;
  }
  
  // check if there's room on disk
  int currentBlocks = (fileSize / MAX_DATA_IN_BLOCK) + 1;
  int neededBlocks = ((size + offset) / MAX_DATA_IN_BLOCK) + 1;
  
  printf("this write will need %d new blocks\n", neededBlocks - currentBlocks);
  if ( neededBlocks - currentBlocks > numFreeBlocks()) {
    printf("write too big for disk\n");
    free(entry);
    return -ENOSPC;
  }
  
  // update the file size in the directory entry and write it to disk
  // also allocates more blocks if necessary
  updateFileSize(entry, startBlock, filename, ext, offset + size);
  free(entry);
  
  csc452_disk_block *block;
  size_t amountToWrite = size;
  long ptr = 0;
	//read in data
  while(amountToWrite > 0) {
    block = readFileBlock(fileStartBlock);
    if (offset >= MAX_DATA_IN_BLOCK) {
      // skip over to the next block
      offset -= MAX_DATA_IN_BLOCK;
    } else {
      printf("writing to block %ld\n", fileStartBlock);
      size_t leftInTheBlock = MAX_DATA_IN_BLOCK - offset;
      size_t writeNow = amountToWrite < leftInTheBlock ? amountToWrite : leftInTheBlock;
      memcpy(((char *)block) + offset, buf + ptr, writeNow);
      offset = 0;
      ptr += writeNow;
      amountToWrite -= writeNow;
      
    }
    
    writeDataBlock(block, fileStartBlock);
    fileStartBlock = block->nNextBlock;
    printf("next block is block %ld\n", fileStartBlock);
    free(block);
  }
  
  
	//return success, or error;

	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	(void) path;
  char dir[MAX_FILENAME + 1];
  char filename[MAX_FILENAME + 1];
  char ext[MAX_EXTENSION + 1];
  parsePath(path, dir, filename, ext);
  
  if (strlen(filename) > 0) {
    return -ENOTDIR;
  }
  
  // get the directory entry for the file
  long startBlock;
  csc452_directory_entry *entry = getDirEntry(dir, &startBlock);
  if (entry == NULL) { // couldn't find the directory
    return -ENOENT;
  }
  
  if (entry->nFiles > 0) {
    free(entry);
    return -ENOTEMPTY;
  }
  
  csc452_root_directory *root = (csc452_root_directory *) malloc(sizeof(csc452_root_directory));
  getRootEntry(root);
  
  int i;
  for (i = 0; i < MAX_DIRS_IN_ROOT; i++) {
    struct csc452_directory currDir = root->directories[i];
    if (strcmp(dir, currDir.dname) == 0) {
      printf("deleting dir %s\n", dir);
      bzero(&(root->directories[i]), sizeof(struct csc452_directory));
      break;
    }
  }
  
  root->nDirectories--;
  
  updateBitmap(startBlock, 0);
  clearBlock(startBlock);
  writeRootEntry(root);
  free(entry);
  free(root);
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
  (void) path;
  (void) path;
  
  char dir[MAX_FILENAME + 1];
  char filename[MAX_FILENAME + 1];
  char ext[MAX_EXTENSION + 1];
  parsePath(path, dir, filename, ext);
  
  if (strlen(filename) == 0) {
    return -EISDIR;
  }
  
  // get the directory entry for the file
  long startBlock;
  csc452_directory_entry *entry = getDirEntry(dir, &startBlock);
  if (entry == NULL) { // couldn't find the directory
    return -ENOENT;
  }
  
  size_t fileSize;
  long fileBlock;
  if (!fileExists(entry, filename, ext, &fileSize, &fileBlock)) {
    // file already exists
    free(entry);
    return -ENOENT;
  }
  
  eraseFile(fileBlock);
  
  int idx = getFileIndex(entry, filename, ext);
  bzero(&(entry->files[idx]), sizeof(struct csc452_file_directory));
  entry->nFiles--;
  writeDirEntry(entry, startBlock);
  
  free(entry);
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
