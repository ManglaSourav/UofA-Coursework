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

#define MAX_ENTRIES_IN_INDEX_BLOCK (BLOCK_SIZE/sizeof(long))

struct csc452_index_block {
	long entries[MAX_ENTRIES_IN_INDEX_BLOCK];
};
typedef struct csc452_index_block csc452_index_block;

static long lastBlock=0;

static long getFreeBlock(FILE *file) {
	long i, currLoc=-1, blockCT=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1));
	char currByte; 
	for (i=0; i<blockCT; ++i) {
		lastBlock=(lastBlock+1)%blockCT;
		if (!lastBlock) {
			continue;
		}
		if (5242880-(lastBlock/8)-1!=currLoc) {
			if (fseek(file, 5242880-(lastBlock/8)-1, SEEK_SET)||fread(&currByte, 1, 1, file)!=1) {
				return -2;
			}
			currLoc=5242880-(lastBlock/8)-1;
		}
		if (!(currByte&(1<<(lastBlock%8)))) {
			return lastBlock;
		}
	}
	return -1;
}

static long* request(FILE *file, size_t num) {
	long *indices=malloc(sizeof(long)*num);
	*indices=0;
	long hold=lastBlock;
	int i;
	for (i=0; i<num; ++i) {
		long index=getFreeBlock(file);
		if (index<0||index==*indices) {
			free(indices);
			lastBlock=hold;
			return NULL;
		}
		*(indices+i)=index;
	}
	return indices;
}

static void* load(FILE *file, long index) {
	if (index>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
		fprintf(stderr, "The requested block at %ld doesn't exist\n", index);
		return NULL;
	}
	if (fseek(file, index * BLOCK_SIZE, SEEK_SET)) {
		fprintf(stderr, "Couldn't seek block at %ld\n", index);
		return NULL;
	}
	void *block=malloc(BLOCK_SIZE);
	if (fread(block, BLOCK_SIZE, 1, file)!=1) {
		fprintf(stderr, "Couldn't load block at %ld\n", index);
		free(block);
		return NULL;
	}
	return block;
}

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int out=0;
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/")==0) {
		stbuf->st_mode=S_IFDIR|0755;
		stbuf->st_nlink=2;
		return 0;
	} else {
		char directory[MAX_FILENAME+1], name[MAX_FILENAME+1], ext[MAX_EXTENSION+1];
		int ct=sscanf(path, "/%[^/]/%[^.].%s", directory, name, ext);
		if (ct<1) {
			return -ENOENT;
		}
		FILE *disk=fopen(".disk", "r+b");
		if (!disk) {
			fprintf(stderr, "disk doesn't exist or isn't readable\n");
			return -ENXIO;
		}
		if (fseek(disk, 0, SEEK_END)||ftell(disk)!=5242880) {
			fclose(disk);
			fprintf(stderr, "disk is not valid\n");
			return -ENXIO;
		}
		csc452_root_directory* rootBlock=(csc452_root_directory*)load(disk, 0);
		if (!rootBlock) {  
			fclose(disk);
			return -EIO;
		} else { 
			size_t dirI;
			for (dirI=0; dirI<rootBlock->nDirectories; ++dirI) {
				if (strcmp(rootBlock->directories[dirI].dname, directory)==0) {
					break;
				}
			}
			if (dirI==rootBlock->nDirectories) { 
				free(rootBlock);
				fclose(disk);
				return -ENOENT;
			} else {
				if (ct==1) {
					stbuf->st_mode=S_IFDIR|0755;
					stbuf->st_nlink=2;
					out=0;
				} else {
					csc452_directory_entry* dir=(csc452_directory_entry*)load(disk, rootBlock->directories[dirI].nStartBlock);
					if (!dir) {
						free(rootBlock);
						fclose(disk);
						return -EIO;
					} else {
						out=-ENOENT;
						size_t fileI;
						for (fileI=0; fileI<dir->nFiles; ++fileI) {
							if (strcmp(dir->files[fileI].fname, name)==0&&strcmp(dir->files[fileI].fext, ext)==0) {
								stbuf->st_mode=S_IFREG|0666;
								stbuf->st_nlink=1; 
								stbuf->st_size=dir->files[fileI].fsize; 
								out=0;
								break;
							}
						}
						free(dir);
					}
				}
			}
			free(rootBlock);
		}
		fclose(disk);
	}
	return out;
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
	char directory[MAX_FILENAME+1], name[MAX_FILENAME+1], ext[MAX_EXTENSION+1];
	int ct=sscanf(path, "/%[^/]/%[^.].%s", directory, name, ext);
	if (ct>1) {
		return -ENOENT;
	}
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	FILE *disk=fopen(".disk", "r+b");
	if (!disk) {
		fprintf(stderr, "disk doesn't exist or isn't readable\n");
		return -ENXIO;
	}
	if (fseek(disk, 0, SEEK_END)||ftell(disk)!=5242880) {
		fclose(disk);
		fprintf(stderr, "disk is not valid\n");
		return -ENXIO;
	}
	printf("opened disk\n");
	int out=-ENOENT;
	csc452_root_directory* rootBlock=(csc452_root_directory*)load(disk, 0);
	if (!rootBlock) { 
		fclose(disk);
		return -EIO;
	} else { 
		size_t dirI;
		if (!strcmp(path, "/")) {
			for (dirI=0; dirI<rootBlock->nDirectories; ++dirI) {
				filler(buf, rootBlock->directories[dirI].dname, NULL, 0);
			}
			out=0;
		} else if (ct==1) {
			size_t dirI;
			for (dirI=0; dirI<rootBlock->nDirectories; ++dirI) { 
				if (strcmp(rootBlock->directories[dirI].dname, directory)==0) {
					break;
				}
			}
			if (dirI<rootBlock->nDirectories) {
				csc452_directory_entry* dir=(csc452_directory_entry*)load(disk, rootBlock->directories[dirI].nStartBlock);
				if (!dir) {
					out=-EIO;
				} else {
					size_t fileI;
					for (fileI=0; fileI<dir->nFiles; ++fileI) {
						char name[MAX_FILENAME+MAX_EXTENSION+1];
						strcpy(name, dir->files[fileI].fname);
						strcat(name, ".");
						strcat(name, dir->files[fileI].fext);
						filler(buf, name, NULL, 0);
					}
					out=0;
					free(dir);
				}
			}
		}
		free(rootBlock);
	}
	fclose(disk);
	return out;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) mode;
	char directory[MAX_FILENAME+1], name[MAX_FILENAME+1], ext[MAX_EXTENSION+1];
	int ct=sscanf(path, "/%[^/]/%[^.].%s", directory, name, ext);
	if (ct>1) {
		return -EPERM;
	}
	if (strlen(directory)>MAX_FILENAME) {
		return -ENAMETOOLONG;
	}
	FILE *disk=fopen(".disk", "r+b");
	if (!disk) {
		fprintf(stderr, "disk doesn't exist or isn't readable\n");
		return -ENXIO;
	}
	if (fseek(disk, 0, SEEK_END)||ftell(disk)!=5242880) {
		fclose(disk);
		fprintf(stderr, "disk is not valid\n");
		return -ENXIO;
	}
	int out=0;
	csc452_root_directory* rootBlock=(csc452_root_directory*)load(disk, 0);
	if (!rootBlock) { 
		fclose(disk);
		return -EIO;
	} else { 
		size_t dirI;
		for (dirI=0; dirI<rootBlock->nDirectories; ++dirI) {
			if (strcmp(rootBlock->directories[dirI].dname, directory)==0) {
				out=-EEXIST;
				break;
			}
		}
		if (!out&&rootBlock->nDirectories==MAX_DIRS_IN_ROOT) {
			out=-ENOSPC;
		}
		if (!out) {
			long newBlockI=getFreeBlock(disk);
			if (newBlockI>0) {
				strcpy(rootBlock->directories[rootBlock->nDirectories].dname, directory);
				rootBlock->directories[rootBlock->nDirectories].nStartBlock=newBlockI;
				rootBlock->nDirectories++;
				int bitdat=0;
				if (newBlockI>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
					fprintf(stderr, "The requested block at %ld doesn't exist\n", newBlockI);
					bitdat=-1;
				}
				char byte;
				if (fseek(disk, 5242880-(newBlockI/8)-1, SEEK_SET)||fread(&byte, 1, 1, disk)!=1) {
					bitdat=-1;
				}
				byte|=(1<<newBlockI%8);
				if (fseek(disk, 5242880-(newBlockI/8)-1, SEEK_SET)||fwrite(&byte, 1, 1, disk)!=1) {
					bitdat=-1;
				}
				int savedat=0;
				if (0>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
					fprintf(stderr, "The requested block at %ld doesn't exist\n", (long)0);
					savedat=-1;
				}
				if (fseek(disk, 0*BLOCK_SIZE, SEEK_SET)) {
					fprintf(stderr, "Couldn't seek block at %ld\n", (long)0);
					savedat=-1;
				}
				if (fwrite(rootBlock, BLOCK_SIZE, 1, disk)!=1) {
					fprintf(stderr, "Couldn't load block at %ld\n", (long)0);
					savedat=-1;
				}
				if (savedat||bitdat) {
					out=-EIO;
				}
			} else if (newBlockI==-1) {
				out=-ENOSPC;
			} else {
				out=-EIO;
			}
		}
		free(rootBlock);
	}
	fclose(disk);
	return out;
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
	char directory[MAX_FILENAME+1], name[MAX_FILENAME+1], ext[MAX_EXTENSION+1];
	int ct=sscanf(path, "/%[^/]/%[^.].%s", directory, name, ext);
	if (ct<3) {
		return -EPERM;
	}
	if (strlen(directory)>MAX_FILENAME) {
		return -ENOENT;
	}
	if (strlen(name)>MAX_FILENAME||strlen(ext)>MAX_EXTENSION) {
		return -ENAMETOOLONG;
	}
	FILE *disk=fopen(".disk", "r+b");
	if (!disk) {
		fprintf(stderr, "disk doesn't exist or isn't readable\n");
		return -ENXIO;
	}
	if (fseek(disk, 0, SEEK_END)||ftell(disk)!=5242880) {
		fclose(disk);
		fprintf(stderr, "disk is not valid\n");
		return -ENXIO;
	}
	int out=-ENOENT;
	csc452_root_directory* rootBlock=(csc452_root_directory*)load(disk, 0);
	if (!rootBlock) { 
		fclose(disk);
		return -EIO;
	} else {
		size_t dir_idx;
		for (dir_idx=0; dir_idx<rootBlock->nDirectories; ++dir_idx) {
			if (strcmp(rootBlock->directories[dir_idx].dname, directory)==0) {
				out=0;
				break;
			}
		}
		if (!out) {
			long dirI=rootBlock->directories[dir_idx].nStartBlock;
			csc452_directory_entry* dir=(csc452_directory_entry*)load(disk, dirI);
			if (!dir) {
				free(rootBlock);
				fclose(disk);
				return -EIO;
			} else {
				size_t fileI;
				for (fileI=0; fileI<dir->nFiles; ++fileI) {
					if (strcmp(dir->files[fileI].fname, name)==0&&strcmp(dir->files[fileI].fext, ext)==0) {
						out=-EEXIST;
						break;
					}
				}
				if (!out&&dir->nFiles==MAX_FILES_IN_DIR) {
					out=-ENOSPC;
				}
				if (!out) {
					long newBlockI=getFreeBlock(disk);
					if (newBlockI>0) { 
						strcpy(dir->files[dir->nFiles].fname, name);
						strcpy(dir->files[dir->nFiles].fext, ext);
						dir->files[dir->nFiles].nStartBlock=newBlockI;
						dir->nFiles++; 
						int bitdat=0;
						if (newBlockI>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
							fprintf(stderr, "The requested block at %ld doesn't exist\n", newBlockI);
							bitdat=-1;
						}
						char byte;
						if (fseek(disk, 5242880-(newBlockI/8)-1, SEEK_SET)||fread(&byte, 1, 1, disk)!=1) {
							bitdat=-1;
						}
						byte|=(1<<newBlockI%8);
						if (fseek(disk, 5242880-(newBlockI/8)-1, SEEK_SET)||fwrite(&byte, 1, 1, disk)!=1) {
							bitdat=-1;
						}
						int savedat=0;
						if (dirI>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
							fprintf(stderr, "The requested block at %ld doesn't exist\n", dirI);
							savedat=-1;
						}
						if (fseek(disk, dirI * BLOCK_SIZE, SEEK_SET)) {
							fprintf(stderr, "Couldn't seek block at %ld\n", dirI);
							savedat=-1;
						}
						if (fwrite(dir, BLOCK_SIZE, 1, disk)!=1) {
							fprintf(stderr, "Couldn't load block at %ld\n", dirI);
							savedat=-1;
						}
						if (savedat||bitdat) {
							out=-EIO;
						}
					} else if (newBlockI==-1) {
						out=-ENOSPC;
					} else {
						out=-EIO;
					}
				}
				free(dir);
			}
		}
		free(rootBlock);
	}
	fclose(disk);
	return out;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) fi;
	if (!size) {
		return -EPERM;
	}
	char directory[MAX_FILENAME+1], name[MAX_FILENAME+1], ext[MAX_EXTENSION+1];
	int ct=sscanf(path, "/%[^/]/%[^.].%s", directory, name, ext);
	if (ct==EOF) {
		return -EISDIR;
	}
	if (strlen(directory)>MAX_FILENAME||strlen(name)>MAX_FILENAME||strlen(ext)>MAX_EXTENSION) {
		return -ENOENT;
	}
	FILE *disk=fopen(".disk", "r+b");
	if (!disk) {
		fprintf(stderr, "disk doesn't exist or isn't readable\n");
		return -ENXIO;
	}
	if (fseek(disk, 0, SEEK_END)||ftell(disk)!=5242880) {
		fclose(disk);
		fprintf(stderr, "disk is not valid\n");
		return -ENXIO;
	}
	int out=-ENOENT;
	csc452_root_directory* rootBlock=(csc452_root_directory*)load(disk, 0);
	if (!rootBlock) { 
		out=-EIO;
		fclose(disk);
		return out?out:size;
	} else { 
		size_t dirI;
		for (dirI=0; dirI<rootBlock->nDirectories; ++dirI) {
			if (strcmp(rootBlock->directories[dirI].dname, directory)==0) {
				out=0;
				break;
			}
		}
		if (!out&&ct==1) {
			out=-EISDIR;
			free(rootBlock);
			fclose(disk);
			return out?out:size;
		} else if(!out&&ct==2) {
			out=-ENOENT;
			free(rootBlock);
			fclose(disk);
			return out?out:size;
		} else if(!out) { 
			out=-ENOENT;
			csc452_directory_entry* dir=(csc452_directory_entry*)load(disk, rootBlock->directories[dirI].nStartBlock);
			if (!dir) {
				out=-EIO;
				free(rootBlock);
				fclose(disk);
				return out?out:size;
			} else {
				size_t fileI;
				for (fileI=0; fileI<dir->nFiles; ++fileI) {
					if (strcmp(dir->files[fileI].fname, name)==0&&strcmp(dir->files[fileI].fext, ext)==0) {
						out=0;
						break;
					}
				}
				if (!out) {
					if (offset>dir->files[fileI].fsize) {
						out=-EFBIG;
						free(dir);
						free(rootBlock);
						fclose(disk);
						return out?out:size;
					} else {
						if (offset+size>dir->files[fileI].fsize) {
							size=dir->files[fileI].fsize-offset;
						}
						long index=dir->files[fileI].nStartBlock;
						csc452_disk_block* file=(csc452_disk_block*)load(disk, index);
						while (offset>MAX_DATA_IN_BLOCK) {
							if (!file||!file->nNextBlock) {
								out=-EIO;
								break;
							}
							index=file->nNextBlock;
							free(file);
							file=(csc452_disk_block*)load(disk, index);
							offset-=MAX_DATA_IN_BLOCK;
						}
						if (!out) {
							size_t left=size;
							while (left&&!out) {
								if (!file) { 
									out=-EIO;
									break;
								}
								size_t readSize=MAX_DATA_IN_BLOCK-offset;
								if (left<readSize) {
									readSize=left;
								}
								memcpy(buf, file->data+offset, readSize);
								left-=readSize;
								if (left&&!out) {
									index=file->nNextBlock;
									free(file);
									file=(csc452_disk_block*)load(disk, index);
									buf+=readSize;
									offset=0;
								}
							}
						}
						if (file) {
							free(file);
						}
					}
				}
				free(dir);
			}
		}
		free(rootBlock);
	}
	fclose(disk);
	return out?out:size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) fi;
	if (!size) {
		return -EPERM;
	}
	size_t total=size+offset;
	char directory[MAX_FILENAME+1], name[MAX_FILENAME+1], ext[MAX_EXTENSION+1];
	int ct=sscanf(path, "/%[^/]/%[^.].%s", directory, name, ext);
	if (ct<3) {
		return -ENOENT;
	}
	if (strlen(directory)>MAX_FILENAME||strlen(name)>MAX_FILENAME||strlen(ext)>MAX_EXTENSION) {
		return -ENOENT;
	}
	FILE *disk=fopen(".disk", "r+b");
	if (!disk) {
		fprintf(stderr, "disk doesn't exist or isn't readable\n");
		return -ENXIO;
	}
	if (fseek(disk, 0, SEEK_END)||ftell(disk)!=5242880) { 
		fclose(disk);
		fprintf(stderr, "disk is not valid\n");
		return -ENXIO;
	}
	int out=-ENOENT;
	csc452_root_directory* rootBlock=(csc452_root_directory*)load(disk, 0);
	if (!rootBlock) {
		out=-EIO;
		fclose(disk);
		return out?out:size;
	} else {
		size_t dirI;
		for (dirI=0; dirI<rootBlock->nDirectories; ++dirI) {
			if (strcmp(rootBlock->directories[dirI].dname, directory)==0) {
				out=0;
				break;
			}
		}
		if( !out) {
			out=-ENOENT;
			long inedex=rootBlock->directories[dirI].nStartBlock;
			csc452_directory_entry* dir=(csc452_directory_entry*)load(disk, inedex);
			if (!dir) {
				out=-EIO;
				free(rootBlock);
				fclose(disk);
				return out?out:size;
			} else {
				size_t fileI;
				for (fileI=0; fileI<dir->nFiles; ++fileI) {
					if (strcmp(dir->files[fileI].fname, name)==0&&strcmp(dir->files[fileI].fext, ext)==0) {
						out=0;
						break;
					}
				}
				if (!out) {
					if (offset>dir->files[fileI].fsize) {
						out=-EFBIG;
						free(dir);
						free(rootBlock);
						fclose(disk);
						return out?out:size;
					} else {
						size_t writeSize=dir->files[fileI].fsize-offset;
						long inedex=dir->files[fileI].nStartBlock;
						csc452_disk_block* file=(csc452_disk_block*)load(disk, inedex);
						while (offset>MAX_DATA_IN_BLOCK) {
							if (!file||!file->nNextBlock) {
								out=-EIO;
								break;
							}
							inedex=file->nNextBlock;
							free(file);
							file=(csc452_disk_block*)load(disk, inedex);
							offset-=MAX_DATA_IN_BLOCK;
						}
						if (!out) {
							size_t left=size;
							long* newI=NULL;
							int used=0;
							if (left>writeSize&&left-writeSize>MAX_DATA_IN_BLOCK-offset) {
								newI=request(disk, (left-writeSize+offset-1)/MAX_DATA_IN_BLOCK);
								if (!newI) {
									out=-ENOSPC;
								}
							}
							while (left&&!out) {
								if (!file) {
									out=-EIO;
									break;
								}
								size_t writeS=MAX_DATA_IN_BLOCK-offset;
								if (left<writeS) {
									writeS=left;
								}
								memcpy(file->data+offset, buf, writeS);
								left-=writeS;
								if (left&&!file->nNextBlock) {
									long nextI=*(newI+used);
									int bitdat=0;
									if (nextI>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
										fprintf(stderr, "The requested block at %ld doesn't exist\n", nextI);
										bitdat=-1;
									}
									char byte;
									if (fseek(disk, 5242880-(nextI/8)-1, SEEK_SET)||fread(&byte, 1, 1, disk)!=1) {
										bitdat=-1;
									}
									byte|=(1<<nextI%8);
									if (fseek(disk, 5242880-(nextI/8)-1, SEEK_SET)||fwrite(&byte, 1, 1, disk)!=1) {
										bitdat=-1;
									}
									if (bitdat) {
										out=-EIO;
										break;
									}
									file->nNextBlock=nextI;
									used++;
								}
								int savedat=0;
								if (inedex>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
									fprintf(stderr, "The requested block at %ld doesn't exist\n", inedex);
									savedat=-1;
								}
								if (fseek(disk, inedex * BLOCK_SIZE, SEEK_SET)) {
									fprintf(stderr, "Couldn't seek block at %ld\n", inedex);
									savedat=-1;
								}
								if (fwrite(file, BLOCK_SIZE, 1, disk)!=1) {
									fprintf(stderr, "Couldn't load block at %ld\n", inedex);
									savedat=-1;
								}
								if (savedat) {
									out=-EIO;
									break;
								}
								if (left) { 
									inedex=file->nNextBlock;
									free(file);
									file=(csc452_disk_block*)load(disk, inedex);
									buf+=writeS;
									offset=0;
								}
							}
							if (out) { 
								int i;
								for (i=0; i<used; ++i){
									if ((*(newI+i))>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
										fprintf(stderr, "The requested block at %ld doesn't exist\n", (*(newI+i)));
									}
									char byte;
									if (fseek(disk, 5242880-((*(newI+i))/8)-1, SEEK_SET)||fread(&byte, 1, 1, disk)!=1) {
									}
									byte&=(~(1<<(*(newI+i))%8));
									if (fseek(disk, 5242880-((*(newI+i))/8)-1, SEEK_SET)||fwrite(&byte, 1, 1, disk)!=1) { 
									}
								}
							}
							if (newI) {
								free(newI);
							}
						}
						if (file) { 
							free(file);
						}
						if (!out&&total>dir->files[fileI].fsize) {
							dir->files[fileI].fsize=total;
							int savedat=0;
							if (inedex>=(5242880/BLOCK_SIZE-((5242880-1)/(8*BLOCK_SIZE*BLOCK_SIZE)+1))) {
								fprintf(stderr, "The requested block at %ld doesn't exist\n", inedex);
								savedat=-1;
							}
							if (fseek(disk, inedex * BLOCK_SIZE, SEEK_SET)) {
								fprintf(stderr, "Couldn't seek block at %ld\n", inedex);
								savedat=-1;
							}
							if (fwrite(dir, BLOCK_SIZE, 1, disk)!=1) {
								fprintf(stderr, "Couldn't load block at %ld\n", inedex);
								savedat=-1;
							}
							if (savedat) {
								out=-EIO;
							}
						}
					}
				}
				free(dir);
			}
		}
		free(rootBlock);
	}
	fclose(disk);
	return out?out:size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	  (void) path;

	  return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
        (void) path;
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
