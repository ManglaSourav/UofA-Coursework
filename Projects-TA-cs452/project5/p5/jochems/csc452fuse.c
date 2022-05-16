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


/*
 *Called whenever the system wants to know the file attributes, including
 *simply whether the file exists or not.
 *
 *man -s 2 stat will show the fields of a stat structure
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
		csc452_root_directory myRoot;
		FILE * myFile;
		myFile = fopen(".disk", "rb");
		if (myFile == NULL)
		{
			return -1;
		}

		fseek(myFile, 0, SEEK_SET);
		fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
		if (fclose(myFile) == EOF)
		{
			return -1;
		}

		if (myRoot.nDirectories == 0)
		{
			res = -ENOENT;
		}

		char myDirectory[20];
		char myFileName[20];
		char myFileExtension[20];
		int myScan = sscanf(path, "/%[^/]/%[^.].%s", myDirectory, myFileName, myFileExtension);
		int x = 0;
		for (x = 0; x < myRoot.nDirectories; x += 1)
		{
			if (strcmp(path, myRoot.directories[x].dname) == 0)
			{
				stbuf->st_nlink = 2;
				stbuf->st_mode = S_IFDIR | 0755;
				return 0;
			}
			else if (myScan == 3 && strcmp(myDirectory, myRoot.directories[x].dname + 1) == 0)
			{
				myFile = fopen(".disk", "rb");
				if (myFile == NULL)
				{
					return -1;
				}

				fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
				struct csc452_directory_entry myDirectoryEntry;
				fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
				int y = 0;
				for (y = 0; y < myDirectoryEntry.nFiles; y += 1)
				{
					if (strcmp(myDirectoryEntry.files[y].fname, myFileName) == 0)
					{
						if (strcmp(myDirectoryEntry.files[y].fext, myFileExtension) == 0)
						{
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 2;
							stbuf->st_size = myDirectoryEntry.files[y].fsize;
							if (fclose(myFile) == EOF)
							{
								return -1;
							}

							return 0;
						}
					}
				}

				return -ENOENT;
			}
			else
			{
				res = -ENOENT;
			}
		}
	}

	return res;
}

/*
 *Called whenever the contents of a directory are desired. Could be from an 'ls'
 *or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") == 0)
	{
		FILE * myFile;
		myFile = fopen(".disk", "rb");
		if (myFile == NULL)
		{
			return -1;
		}

		struct csc452_root_directory myRoot;
		fseek(myFile, 0, SEEK_SET);
		fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
		if (fclose(myFile) == EOF)
		{
			return -1;
		}

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
		int x = 0;
		for (x = 0; x < myRoot.nDirectories; x++)
		{
			filler(buf, myRoot.directories[x].dname + 1, NULL, 0);
		}
	}
	else
	{
		FILE * myFile;
		myFile = fopen(".disk", "rb+");
		if (myFile == NULL)
		{
			return -1;
		}

		struct csc452_root_directory myRoot;
		fseek(myFile, 0, SEEK_SET);
		fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
		int x = 0;
		int detect = 0;
		for (x = 0; x < myRoot.nDirectories; x++)
		{
			if (strcmp(path, myRoot.directories[x].dname) == 0)
			{
				fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
				struct csc452_directory_entry myDirectoryEntry;
				fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
				int y = 0;
				for (y = 0; y < myDirectoryEntry.nFiles; y++)
				{
					char curr[MAX_FILENAME + MAX_EXTENSION + 2];
					strcpy(curr, myDirectoryEntry.files[y].fname);
					strcat(curr, ".");
					strcat(curr, myDirectoryEntry.files[y].fext);
					filler(buf, curr, NULL, 0);
				}

				detect = 1;
				break;
			}
		}

		if (fclose(myFile) == EOF)
		{
			return -1;
		}

		if (detect == 0)
		{
			return -ENOENT;
		}

		filler(buf, ".", NULL, 0);
	}

	return 0;
}

/*
 *Creates a directory. We can ignore mode since we're not dealing with
 *permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) mode;
	char myDirectory[20];
	char myFileName[20];
	char myFileExtension[20];
	int myScan = sscanf(path, "/%[^/]/%[^.].%s", myDirectory, myFileName, myFileExtension);
	if (myScan > 1)
	{
		return -EPERM;
	}

	if (strlen(myDirectory) > MAX_FILENAME)
	{
		return -ENAMETOOLONG;
	}

	FILE * myFile;
	myFile = fopen(".disk", "rb+");
	if (myFile == NULL)
	{
		return -1;
	}

	fseek(myFile, 0, SEEK_SET);
	struct csc452_root_directory myRoot;
	fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
	if (fclose(myFile) == EOF)
	{
		return -1;
	}

	int x = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		if (strcmp(myRoot.directories[x].dname, path) == 0)
		{
			return -EEXIST;
		}
	}

	if (myRoot.nDirectories == MAX_DIRS_IN_ROOT)
	{
		return 0;
	}

	struct csc452_directory newDir;
	strcpy(newDir.dname, path);
	newDir.nStartBlock = myRoot.nDirectories + 1;
	myRoot.directories[myRoot.nDirectories] = newDir;
	myRoot.nDirectories++;
	myFile = fopen(".disk", "rb+");
	if (myFile == NULL)
	{
		return -1;
	}

	fseek(myFile, 0, SEEK_SET);
	fwrite(&myRoot, sizeof myRoot, 1, myFile);
	struct csc452_directory_entry myDirectoryEntry;
	fseek(myFile, newDir.nStartBlock *BLOCK_SIZE, SEEK_SET);
	fwrite(&myDirectoryEntry, sizeof myDirectoryEntry, 1, myFile);
	if (fclose(myFile) == EOF)
	{
		return -1;
	}

	return 0;
}

/*
 *Does the actual creation of a file. Mode and dev can be ignored.
 *
 *Note that the mknod shell command is not the one to test this.
 *mknod at the shell is used to create "special" files and we are
 *only supporting regular files.
 *
 */
static int csc452_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;
	if (strcmp(path, "/") == 0)
	{
		return -1;
	}

	char myDirectory[20];
	char myFileName[20];
	char myFileExtension[20];
	int myScan = sscanf(path, "/%[^/]/%[^.].%s", myDirectory, myFileName, myFileExtension);
	if (myScan == 2)
	{
		return -1;
	}

	if (myScan == 1)
	{
		return -EISDIR;
	}

	if (strlen(myFileName) > MAX_FILENAME || strlen(myFileExtension) > MAX_EXTENSION)
	{
		return -ENAMETOOLONG;
	}

	FILE * myFile;
	myFile = fopen(".disk", "rb+");
	if (myFile == NULL)
	{
		return -1;
	}

	fseek(myFile, 0, SEEK_SET);
	struct csc452_root_directory myRoot;
	fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
	int x = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		if (strcmp(myRoot.directories[x].dname + 1, myDirectory) == 0)
		{
			fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry myDirectoryEntry;
			fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
			int y = 0;
			for (y = 0; y < myDirectoryEntry.nFiles; y++)
			{
				if (strcmp(myFileName, myDirectoryEntry.files[y].fname) == 0)
				{
					if (strcmp(myFileExtension, myDirectoryEntry.files[y].fext) == 0)
					{
						if (fclose(myFile) == EOF)
						{
							return -1;
						}

						return -EEXIST;
					}
				}
			}

			if (myDirectoryEntry.nFiles == MAX_FILES_IN_DIR)
			{
				if (fclose(myFile) == EOF)
				{
					return -1;
				}

				return 0;
			}

			struct csc452_file_directory myEntry;
			strcpy(myEntry.fname, myFileName);
			strcpy(myEntry.fext, myFileExtension);
			myEntry.fsize = 0;
			myEntry.nStartBlock = myDirectoryEntry.nFiles + 1;
			myDirectoryEntry.files[myDirectoryEntry.nFiles] = myEntry;
			myDirectoryEntry.nFiles++;
			struct csc452_disk_block fileBlock;
			fseek(myFile, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR *(myRoot.directories[x].nStartBlock - 1)) +
				myEntry.nStartBlock - 1) *BLOCK_SIZE, SEEK_SET);
			fwrite(&fileBlock, sizeof(struct csc452_disk_block), 1, myFile);
			fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
			fwrite(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
			break;
		}
	}

	if (fclose(myFile) == EOF)
	{
		return -1;
	}

	return 0;
}

/*
 *Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{
	(void) fi;
	if (strcmp(path, "/") == 0)
	{
		return -EISDIR;
	}

	if (size <= 0)
	{
		return -1;
	}

	char myDirectory[20];
	char myFileName[20];
	char myFileExtension[20];
	int myScan = sscanf(path, "/%[^/]/%[^.].%s", myDirectory, myFileName, myFileExtension);
	if (myScan == 1)
	{
		return -EISDIR;
	}

	if (myScan == 2)
	{
		return -1;
	}

	FILE * myFile;
	myFile = fopen(".disk", "rb+");
	if (myFile == NULL)
	{
		return -1;
	}

	fseek(myFile, 0, SEEK_SET);
	struct csc452_root_directory myRoot;
	fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
	int x = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		if (strcmp(myRoot.directories[x].dname + 1, myDirectory) == 0)
		{
			fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry myDirectoryEntry;
			fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
			int y = 0;
			for (y = 0; y < myDirectoryEntry.nFiles; y++)
			{
				if (strcmp(myDirectoryEntry.files[y].fname, myFileName) == 0)
				{
					if (strcmp(myDirectoryEntry.files[y].fext, myFileExtension) == 0)
					{
						fseek(myFile, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR *
								(myRoot.directories[x].nStartBlock - 1)) + myDirectoryEntry.files[y].nStartBlock -
							1) *BLOCK_SIZE, SEEK_SET);
						struct csc452_disk_block myDiskBlock;
						fread(&myDiskBlock, sizeof(struct csc452_disk_block), 1, myFile);
						char *curr = "";
						curr = myDiskBlock.data;
						curr += offset;
						strncpy(buf, curr, myDirectoryEntry.files[y].fsize);
						buf[myDirectoryEntry.files[y].fsize] = '\0';
						break;
					}
				}
			}

			break;
		}
	}

	fclose(myFile);
	return size;
}

/*
 *Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	if (strcmp(path, "/") == 0)
	{
		return -1;
	}

	if (size <= 0)
	{
		return -1;
	}

	if (offset > MAX_DATA_IN_BLOCK)
	{
		return -EFBIG;
	}

	char myDirectory[20];
	char myFileName[20];
	char myFileExtension[20];
	int myScan = sscanf(path, "/%[^/]/%[^.].%s", myDirectory, myFileName, myFileExtension);
	if (myScan == 2)
	{
		return -1;
	}

	if (myScan == 1)
	{
		return -EISDIR;
	}

	if (strlen(myFileName) > MAX_FILENAME)
	{
		return -ENAMETOOLONG;
	}

	if (strlen(myFileExtension) > MAX_EXTENSION)
	{
		return -ENAMETOOLONG;
	}

	FILE * myFile;
	myFile = fopen(".disk", "rb+");
	if (myFile == NULL)
	{
		return -1;
	}

	fseek(myFile, 0, SEEK_SET);
	struct csc452_root_directory myRoot;
	fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
	int x = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		if (strcmp(myRoot.directories[x].dname + 1, myDirectory) == 0)
		{
			fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry myDirectoryEntry;
			fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
			int y = 0;
			for (y = 0; y < myDirectoryEntry.nFiles; y++)
			{
				if (strcmp(myDirectoryEntry.files[y].fname, myFileName) == 0)
				{
					if (strcmp(myDirectoryEntry.files[y].fext, myFileExtension) == 0)
					{
						fseek(myFile, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR *
								(myRoot.directories[x].nStartBlock - 1)) + myDirectoryEntry.files[y].nStartBlock -
							1) *BLOCK_SIZE, SEEK_SET);
						struct csc452_disk_block myDiskBlock;
						fread(&myDiskBlock, sizeof(struct csc452_disk_block), 1, myFile);
						myDiskBlock.data[offset] = '\0';
						char curr[size];
						curr[0] = '\0';
						strncpy(curr, buf, size);
						strcat(myDiskBlock.data, curr);
						myDiskBlock.data[myDirectoryEntry.files[y].fsize + size] = '\0';
						fseek(myFile, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR *
								(myRoot.directories[x].nStartBlock - 1)) + myDirectoryEntry.files[y].nStartBlock -
							1) *BLOCK_SIZE, SEEK_SET);
						fwrite(&myDiskBlock, sizeof(struct csc452_disk_block), 1, myFile);
						myDirectoryEntry.files[y].fsize = strlen(myDiskBlock.data);
						fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
						fwrite(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
						break;
					}
				}
			}

			break;
		}
	}

	if (fclose(myFile) == EOF)
	{
		return -1;
	}

	return size;
}

/*
 *Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	if (strcmp(path, "/") == 0)
	{
		return -ENOTDIR;
	}

	char myDirectory[20];
	char myFileName[20];
	char myFileExtension[20];
	int myScan = sscanf(path, "/%[^/]/%[^.].%s", myDirectory, myFileName, myFileExtension);
	if (myScan > 1)
	{
		return -ENOTDIR;
	}

	FILE * myFile;
	myFile = fopen(".disk", "rb");
	if (myFile == NULL)
	{
		return -1;
	}

	fseek(myFile, 0, SEEK_SET);
	struct csc452_root_directory myRoot;
	fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
	int x = 0;
	int detect = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		if (strcmp(myRoot.directories[x].dname + 1, myDirectory) == 0)
		{
			fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry myDirectoryEntry;
			fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
			if (myDirectoryEntry.nFiles > 0)
			{
				if (fclose(myFile) == EOF)
				{
					return -1;
				}

				return -ENOTEMPTY;
			}

			detect = 1;
			break;
		}
	}

	if (detect == 0)
	{
		if (fclose(myFile) == EOF)
		{
			return -1;
		}

		return -ENOENT;
	}

	struct csc452_directory_entry myDirectories[myRoot.nDirectories];
	struct csc452_disk_block myDiskBlocks[myRoot.nDirectories][MAX_FILES_IN_DIR];
	x = 0;
	detect = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		if (strcmp(myDirectory, myRoot.directories[x].dname + 1) == 0)
		{
			detect = 1;
			continue;
		}

		fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
		struct csc452_directory_entry myDirectoryEntry;
		fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
		int y = 0;
		for (y = 0; y < myDirectoryEntry.nFiles; y++)
		{
			fseek(myFile, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR *(myRoot.directories[x].nStartBlock - 1)) +
				myDirectoryEntry.files[y].nStartBlock - 1) *BLOCK_SIZE, SEEK_SET);
			struct csc452_disk_block myDiskBlock;
			fread(&myDiskBlock, sizeof(struct csc452_disk_block), 1, myFile);
			myDiskBlocks[x - detect][y] = myDiskBlock;
		}

		myDirectories[x - detect] = myDirectoryEntry;
	}

	if (fclose(myFile) == EOF)
	{
		return -1;
	}

	myFile = fopen(".disk", "wb");
	if (myFile == NULL)
	{
		return -1;
	}

	struct csc452_directory myArr[myRoot.nDirectories - 1];
	x = 0;
	detect = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		if (strcmp(myDirectory, myRoot.directories[x].dname + 1) == 0)
		{
			detect = 1;
			continue;
		}

		myArr[x - detect] = myRoot.directories[x];
		myArr[x - detect].nStartBlock -= detect;
	}

	memcpy(myRoot.directories, myArr, sizeof(myRoot.directories));
	myRoot.nDirectories--;
	fseek(myFile, 0, SEEK_SET);
	fwrite(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
	x = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
		fwrite(&myDirectories[x], sizeof(struct csc452_directory_entry), 1, myFile);
		int y = 0;
		for (y = 0; y < myDirectories[x].nFiles; y++)
		{
			fseek(myFile, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR *(myRoot.directories[x].nStartBlock - 1)) +
				myDirectories[x].files[y].nStartBlock - 1) *BLOCK_SIZE, SEEK_SET);
			fwrite(&myDiskBlocks[x][y], sizeof(struct csc452_disk_block), 1, myFile);
		}
	}

	if (fclose(myFile) == EOF)
	{
		return -1;
	}

	return 0;
}

/*
 *Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
	if (strcmp(path, "/") == 0)
	{
		return -1;
	}

	char myDirectory[20];
	char myFileName[20];
	char myFileExtension[20];
	int myScan = sscanf(path, "/%[^/]/%[^.].%s", myDirectory, myFileName, myFileExtension);
	if (myScan == 1)
	{
		return -EISDIR;
	}

	FILE * myFile;
	myFile = fopen(".disk", "rb");
	if (myFile == NULL)
	{
		return -1;
	}

	fseek(myFile, 0, SEEK_SET);
	struct csc452_root_directory myRoot;
	fread(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
	int x = 0;
	int detect = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		if (strcmp(myRoot.directories[x].dname + 1, myDirectory) == 0)
		{
			fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry myDirectoryEntry;
			fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
			int y = 0;
			for (y = 0; y < myDirectoryEntry.nFiles; y++)
			{
				if (strcmp(myDirectoryEntry.files[y].fname, myFileName) == 0)
				{
					if (strcmp(myDirectoryEntry.files[y].fext, myFileExtension) == 0)
					{
						detect = 1;
						break;
					}
				}
			}

			break;
		}
	}

	if (detect == 0)
	{
		if (fclose(myFile) == EOF)
		{
			return -1;
		}

		return -ENOENT;
	}

	struct csc452_directory_entry myDirectories[myRoot.nDirectories];
	struct csc452_disk_block myDiskBlocks[myRoot.nDirectories][MAX_FILES_IN_DIR];
	x = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
		struct csc452_directory_entry myDirectoryEntry;
		fread(&myDirectoryEntry, sizeof(struct csc452_directory_entry), 1, myFile);
		int y = 0;
		int detect = 0;
		for (y = 0; y < myDirectoryEntry.nFiles; y++)
		{
			if (strcmp(myDirectory, myRoot.directories[x].dname + 1) == 0)
			{
				if (strcmp(myFileName, myDirectoryEntry.files[y].fname) == 0)
				{
					if (strcmp(myFileExtension, myDirectoryEntry.files[y].fext) == 0)
					{
						detect = 1;
						continue;
					}
				}
			}

			myDirectoryEntry.files[y].nStartBlock -= detect;
			fseek(myFile, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR *(myRoot.directories[x].nStartBlock - 1)) +
				myDirectoryEntry.files[y].nStartBlock - 1) *BLOCK_SIZE, SEEK_SET);
			struct csc452_disk_block myDiskBlock;
			fread(&myDiskBlock, sizeof(struct csc452_disk_block), 1, myFile);
			myDiskBlocks[x][y] = myDiskBlock;
		}

		if (strcmp(myDirectory, myRoot.directories[x].dname + 1) == 0)
		{
			myDirectoryEntry.nFiles--;
		}

		myDirectories[x] = myDirectoryEntry;
	}

	if (fclose(myFile) == EOF)
	{
		return -1;
	}

	myFile = fopen(".disk", "wb");
	if (myFile == NULL)
	{
		return -1;
	}

	fseek(myFile, 0, SEEK_SET);
	fwrite(&myRoot, sizeof(struct csc452_root_directory), 1, myFile);
	x = 0;
	for (x = 0; x < myRoot.nDirectories; x++)
	{
		fseek(myFile, myRoot.directories[x].nStartBlock *BLOCK_SIZE, SEEK_SET);
		fwrite(&myDirectories[x], sizeof(struct csc452_directory_entry), 1, myFile);
		int y = 0;
		for (y = 0; y < myDirectories[x].nFiles; y++)
		{
			fseek(myFile, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR *(myRoot.directories[x].nStartBlock - 1)) +
				myDirectories[x].files[y].nStartBlock - 1) *BLOCK_SIZE, SEEK_SET);
			fwrite(&myDiskBlocks[x][y], sizeof(struct csc452_disk_block), 1, myFile);
		}
	}

	if (fclose(myFile) == EOF)
	{
		return -1;
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
