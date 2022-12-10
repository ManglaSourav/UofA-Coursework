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
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int result = 0;
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		csc452_root_directory root;
		FILE *file;
		file = fopen(".disk", "rb");
		if(file == NULL) {
			return -1;
		}

		fseek(file, 0, SEEK_SET);
		fread(&root, sizeof(struct csc452_root_directory), 1, file);
		if(fclose(file) == EOF) {
			return -1;
		}
		if(root.nDirectories == 0) {
			result = -ENOENT;
		}

		char d[20];
		char f[20];
		char e[20];
		int val = sscanf(path, "/%[^/]/%[^.].%s", d, f, e);

		int i = 0;
		for(i = 0; i < root.nDirectories; i++) {
			if(strcmp(path, root.directories[i].dname) == 0) {
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				return 0;
			} else if(val == 3 && strcmp(d, root.directories[i].dname + 1) == 0) {
				file = fopen(".disk", "rb");
				if(file == NULL) {
					return -1;
				}
				fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
				struct csc452_directory_entry dentry;
				fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);

				int j = 0;
				for(j = 0; j < dentry.nFiles; j++) {
					if(strcmp(dentry.files[j].fname, f) == 0) {
						if(strcmp(dentry.files[j].fext, e) == 0) {
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 2;
							stbuf->st_size = dentry.files[j].fsize;
							if(fclose(file) == EOF) {
								return -1;
							}
							return 0;
						}
					}
				}
				return -ENOENT;
			} else {
				result = -ENOENT;
			}
		}
	}
	return result;
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

	if (strcmp(path, "/") == 0) {
		
		FILE *file;
		file = fopen(".disk", "rb");
		if(file == NULL) {
			return -1;
		}
		struct csc452_root_directory root;
		fseek(file, 0, SEEK_SET);
		fread(&root, sizeof(struct csc452_root_directory), 1, file);
		if(fclose(file) == EOF) {
			return -1;
		}

		int i = 0;
		for(i = 0; i < root.nDirectories; i++) {
			filler(buf, root.directories[i].dname + 1, NULL, 0);
		}
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);
	} else {

		FILE *file;
		file = fopen(".disk", "rb+");
		if(file == NULL) {
			return -1;
		}
		struct csc452_root_directory root;
		fseek(file, 0, SEEK_SET);
		fread(&root, sizeof(struct csc452_root_directory), 1, file);

		int i = 0;
		int ok = 0;
		for(i = 0; i < root.nDirectories; i++) {
			if(strcmp(path, root.directories[i].dname) == 0) {
				fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
				struct csc452_directory_entry dentry;
				fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);

				int j = 0;
				for(j = 0; j < dentry.nFiles; j++) {
					char t[MAX_FILENAME + MAX_EXTENSION + 2];
					strcpy(t, dentry.files[j].fname);
					strcat(t, ".");
					strcat(t, dentry.files[j].fext);
					filler(buf, t, NULL, 0);
				}
				ok = 1;
				break;
			}
		}

		if(fclose(file) == EOF) {
			return -1;
		}
		if(ok == 0) {
			return -ENOENT;
		}
		filler(buf, ".", NULL, 0);
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

	char d[20];
	char f[20];
	char e[20];
	int val = sscanf(path, "/%[^/]/%[^.].%s", d, f, e);
	if(val > 1) {
		return -EPERM;
	}
	if(strlen(d) > MAX_FILENAME) {
		return -ENAMETOOLONG;
	}

	FILE *file;
	file = fopen(".disk", "rb+");
	if(file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, file);
	if(fclose(file) == EOF) {
		return -1;
	}

	int i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname, path) == 0) {
			return -EEXIST;
		}
	}
	if(root.nDirectories == MAX_DIRS_IN_ROOT) {
		return 0;
	}

	struct csc452_directory newDir;
	strcpy(newDir.dname, path);
	newDir.nStartBlock = root.nDirectories + 1;
	root.directories[root.nDirectories] = newDir;
	root.nDirectories++;

	file = fopen(".disk", "rb+");
	if(file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	fwrite(&root, sizeof root, 1, file);
	struct csc452_directory_entry dentry;
	fseek(file, newDir.nStartBlock * BLOCK_SIZE, SEEK_SET);
	fwrite(&dentry, sizeof dentry, 1, file);
	if(fclose(file) == EOF) {
		return -1;
	}
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
		return -1;
	}	
	char d[20];
	char f[20];
	char e[20];
	int val = sscanf(path, "/%[^/]/%[^.].%s", d, f, e);

	if(val == 1) {
		return -EISDIR;
	}
	if(val == 2) {
		return -1;
	}
	if(strlen(f) > MAX_FILENAME) {
		return -ENAMETOOLONG;
	}
	if(strlen(e) > MAX_EXTENSION) {
		return -ENAMETOOLONG;
	}

	FILE *file;
	file = fopen(".disk", "rb+");
	if(file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, file);

	int i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, d) == 0) {		
			fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dentry;
			fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);

			int j = 0;
			for(j = 0; j < dentry.nFiles; j++) {
				if(strcmp(f, dentry.files[j].fname) == 0) {
					if(strcmp(e, dentry.files[j].fext) == 0) {
						if(fclose(file) == EOF) {
							return -1;
						}
						return -EEXIST;
					}
				}
			}

			if(dentry.nFiles == MAX_FILES_IN_DIR) {
				if(fclose(file) == EOF) {
					return -1;
				}
				return 0;
			}

			struct csc452_file_directory fentry;
			strcpy(fentry.fname, f);
			strcpy(fentry.fext, e);
			fentry.fsize = 0;
			fentry.nStartBlock = dentry.nFiles + 1;
			dentry.files[dentry.nFiles] = fentry;
			dentry.nFiles++;

			struct csc452_disk_block fblock;
			fseek(file, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) + 
						fentry.nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			fwrite(&fblock, sizeof(struct csc452_disk_block), 1, file);
			fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			fwrite(&dentry, sizeof(struct csc452_directory_entry), 1, file);
			break;
		}
	}
	if(fclose(file) == EOF) {
		return -1;
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
	(void) fi;
	if(strcmp(path, "/") == 0) {
		return -EISDIR;
	}

	char d[20];
	char f[20];
	char e[20];
	int val = sscanf(path, "/%[^/]/%[^.].%s", d, f, e);
	if(val == 1) {
		return -EISDIR;
	}
	if(val == 2) {
		return -1;
	}
	FILE *file;
	file = fopen(".disk", "rp+");
	if(file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, file);

	int i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, d) == 0) {
			fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dentry;
			fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);

			int j = 0;
			for(j = 0; j < dentry.nFiles; j++) {
				if(strcmp(dentry.files[j].fname, f) == 0) {
					if(strcmp(dentry.files[j].fext, e) == 0) {

						fseek(file, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * 
							(root.directories[i].nStartBlock - 1)) + dentry.files[j].nStartBlock 
							- 1) * BLOCK_SIZE, SEEK_SET);
						struct csc452_disk_block block;
						fread(&block, sizeof(struct csc452_disk_block), 1, file);

						char *t = "";
						t = block.data;
						t += offset;
						strncpy(buf, t, dentry.files[j].fsize);
						buf[dentry.files[j].fsize] = '\0';
						break;
					}
				}
			}
			break;
		}
	}
	if(fclose(file) == EOF) {
		return -1;
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
	(void) fi;
	if(strcmp(path, "/") == 0) {
		return -1;
	}
	if(size <= 0) {
		return -1;
	}
	if(offset > MAX_DATA_IN_BLOCK) {
		return -EFBIG;
	}

	char d[20];
	char f[20];
	char e[20];
	int val = sscanf(path, "/%[^/]/%[^.].%s", d, f, e);

	if(val == 1) {
		return -EISDIR;
	}
	if(val == 2) {
		return -1;
	}
	if(strlen(f) > MAX_FILENAME) {
		return -ENAMETOOLONG;
	}
	if(strlen(e) > MAX_EXTENSION) {
		return -ENAMETOOLONG;
	}

	FILE *file;
	file = fopen(".disk", "rp+");
	if(file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, file);

	int i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, d) == 0) {
			fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dentry;
			fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);
			int j = 0;
			for(j = 0; j < dentry.nFiles; j++) {
				if(strcmp(dentry.files[j].fname, f) == 0) {
					if(strcmp(dentry.files[j].fext, e) == 0) {
						fseek(file, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) + dentry.files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
						struct csc452_disk_block block;
						fread(&block, sizeof(struct csc452_disk_block), 1, file);

						block.data[offset] = '\0';
						char t[size];
						t[0] = '\0';
						strncpy(t, buf, size);
						strcat(block.data, t);
						block.data[dentry.files[j].fsize + size] = '\0';

						fseek(file, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) + dentry.files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
						fwrite(&block, sizeof(struct csc452_disk_block), 1, file);
						dentry.files[j].fsize = strlen(block.data);
						fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
						fwrite(&dentry, sizeof(struct csc452_directory_entry), 1, file);
						break;
					}
				}
			}
			break;
		}
	}
	if(fclose(file) == EOF) {
		return -1;
	}
	return 0;
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
	char d[20];
	char f[20];
	char e[20];
	int val = sscanf(path, "/%[^/]/%[^.].%s", d, f, e);
	if(val > 1) {
		return -ENOTDIR;
	}

	FILE *file;
	file = fopen(".disk", "rb");
	if(file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, file);

	int i = 0;
	int ok = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, d) == 0) {
			fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dentry;
			fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);
			if(dentry.nFiles > 0) {
				fclose(file);
				return -ENOTEMPTY;
			}
			ok = 1;
			break;
		}
	}
	if(ok == 0) {
		if(fclose(file) == EOF) {
			return -1;
		}
		return -ENOENT;
	}
	struct csc452_directory_entry allDirs[root.nDirectories];
	struct csc452_disk_block allBlocks[root.nDirectories][MAX_FILES_IN_DIR];

	i = 0;
	int skip = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(d, root.directories[i].dname + 1) == 0) {
			skip = 1;
			continue;
		}
		fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
		struct csc452_directory_entry dentry;
		fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);
		int j = 0;
		for(j = 0; j < dentry.nFiles; j++) {
			fseek(file, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) 
						+ dentry.files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			struct csc452_disk_block block;
			fread(&block, sizeof(struct csc452_disk_block), 1, file);
			allBlocks[i - skip][j] = block;
		}
		allDirs[i - skip] = dentry;
	}
	if(fclose(file) == EOF) {
		return -1;
	}
	file = fopen(".disk", "wb");
	if(file == NULL) {
		return -1;
	}

	struct csc452_directory newArr[root.nDirectories - 1];

	i = 0;
	skip = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(d, root.directories[i].dname + 1) == 0) {
			skip = 1;
			continue;
		}
		newArr[i - skip] = root.directories[i];
		newArr[i - skip].nStartBlock -= skip;
	}

	memcpy(root.directories, newArr, sizeof(root.directories));
	root.nDirectories--;
	fseek(file, 0, SEEK_SET);
	fwrite(&root, sizeof(struct csc452_root_directory), 1, file);

	i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&allDirs[i], sizeof(struct csc452_directory_entry), 1, file);
		int j = 0;
		for(j = 0; j < allDirs[i].nFiles; j++) {
			fseek(file, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) 
						+ allDirs[i].files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			fwrite(&allBlocks[i][j], sizeof(struct csc452_disk_block), 1, file);
		}
	}
	if(fclose(file) == EOF) {
		return -1;
	}
	return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
	if(strcmp(path, "/") == 0) {
		return -1;
	}

	char d[20];
	char f[20];
	char e[20];
	int val = sscanf(path, "/%[^/]/%[^.].%s", d, f, e);
	if(val == 1) {
		return -EISDIR;
	}

	FILE *file;
	file = fopen(".disk", "rb");
	if(file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	struct csc452_root_directory root;
	fread(&root, sizeof(struct csc452_root_directory), 1, file);

	int i = 0;
	int ok = 0;
	for(i = 0; i < root.nDirectories; i++) {
		if(strcmp(root.directories[i].dname + 1, d) == 0) {
			fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
			struct csc452_directory_entry dentry;
			fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);
			int j = 0;
			for(j = 0; j < dentry.nFiles; j++) {
				if(strcmp(dentry.files[j].fname, f) == 0) {
					if(strcmp(dentry.files[j].fext, e) == 0) {
						ok = 1;
						break;
					}
				}
			}
			break;
		}
	}
	if(ok == 0) {
		if(fclose(file) == EOF) {
			return -1;
		}
		return -ENOENT;
	}

	struct csc452_directory_entry allDirs[root.nDirectories];
	struct csc452_disk_block allBlocks[root.nDirectories][MAX_FILES_IN_DIR];

	i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
		struct csc452_directory_entry dentry;
		fread(&dentry, sizeof(struct csc452_directory_entry), 1, file);
		int j = 0;
		int skip = 0;
		for(j = 0; j < dentry.nFiles; j++) {
			if(strcmp(d, root.directories[i].dname + 1) == 0) {
				if(strcmp(f, dentry.files[j].fname) == 0) {
					if(strcmp(e, dentry.files[j].fext) == 0) {
						skip = 1;
						continue;
					}
				}
			}
			dentry.files[j].nStartBlock -= skip;
			fseek(file, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) 
						+ dentry.files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			struct csc452_disk_block block;
			fread(&block, sizeof(struct csc452_disk_block), 1, file);
			allBlocks[i][j] = block;
		}
		if(strcmp(d, root.directories[i].dname + 1) == 0) {
			dentry.nFiles--;
		}
		allDirs[i] = dentry;
	}
	if(fclose(file) == EOF) {
		return -1;
	}
	file = fopen(".disk", "wb");
	if(file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	fwrite(&root, sizeof(struct csc452_root_directory), 1, file);
	i = 0;
	for(i = 0; i < root.nDirectories; i++) {
		fseek(file, root.directories[i].nStartBlock * BLOCK_SIZE, SEEK_SET);
		fwrite(&allDirs[i], sizeof(struct csc452_directory_entry), 1, file);
		int j = 0;
		for(j = 0; j < allDirs[i].nFiles; j++) {
			fseek(file, (MAX_DIRS_IN_ROOT + 1 + (MAX_FILES_IN_DIR * (root.directories[i].nStartBlock - 1)) 
						+ allDirs[i].files[j].nStartBlock - 1) * BLOCK_SIZE, SEEK_SET);
			fwrite(&allBlocks[i][j], sizeof(struct csc452_disk_block), 1, file);
		}
	}
	if(fclose(file) == EOF) {
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
