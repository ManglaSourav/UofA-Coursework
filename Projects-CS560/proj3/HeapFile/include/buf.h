///////////////////////////////////////////////////////////////////////////////
/////////////  The Header File for the Buffer Manager /////////////////////////
///////////////////////////////////////////////////////////////////////////////


#ifndef BUF_H
#define BUF_H

#include "db.h"
#include "page.h"
#include "new_error.h"

#define NUMBUF 20   
// Default number of frames, artifically small number for ease of debugging.

#define HTSIZE 7
// Hash Table size

typedef int FrameId;

/*******************ALL BELOW are purely local to buffer Manager********/



// You could add more enums for internal errors in the buffer manager.
enum bufErrCodes  {HASHMEMORY, HASHDUPLICATEINSERT, HASHREMOVEERROR, HASHNOTFOUND, QMEMORYERROR, QEMPTY, INTERNALERROR, 
			BUFFERFULL, BUFMGRMEMORYERROR, BUFFERPAGENOTFOUND, BUFFERPAGENOTPINNED, BUFFERPAGEPINNED};

class Replacer; // may not be necessary as described below in the constructor

class BufMgr {

private: 
    typedef struct frame {
        bool loved;
        bool dirty;
        PageId pageId;
        int pincount;  
    } frame;
    typedef struct hashEntry {
        PageId pageId;
        FrameId frameId;
        struct hashEntry* next;
    } hashEntry;

   unsigned int    numBuffers;
    frame *frames; // holds metadata about all the frames
    FrameId* whenUsed; // array of FrameIds ordered based on time of use
    hashEntry * hashTable;
    unsigned int a;
    unsigned int b;
    hashEntry* findEntry(hashEntry *head, PageId pageId);
    hashEntry* findParent(hashEntry *head, FrameId frameId);
    hashEntry* findParentUsingPage(hashEntry *head, PageId pageId);
    void insertIntoTable(hashEntry *entry, int hash_loc);
    void updateFrameId(int id);
    FrameId locateReplacee();
    void determineDup();
    unsigned int getNumFreeBuffers();
public:
    Page* bufPool; // The actual buffer pool
    void debugFrames();
    void debugHash();
    BufMgr (int numbuf, Replacer *replacer = 0); 
   	// Initializes a buffer manager managing "numbuf" buffers.
	// Disregard the "replacer" parameter for now. In the full 
  	// implementation of minibase, it is a pointer to an object
	// representing one of several buffer pool replacement schemes.

    ~BufMgr();           // Flush all valid dirty pages to disk

    Status pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage=FALSE);
        // Check if this page is in buffer pool, otherwise
        // find a frame for this page, read in and pin it.
        // also write out the old page if it's dirty before reading
        // if emptyPage==TRUE, then actually no read is done to bring
        // the page

    Status unpinPage(PageId globalPageId_in_a_DB, int dirty=FALSE, int hate=FALSE);
        // hate should be TRUE if the page is hated and FALSE otherwise
        // if pincount>0, decrement it and if it becomes zero,
        // put it in a group of replacement candidates.
        // if pincount=0 before this call, return error.

    Status newPage(PageId& firstPageId, Page*& firstpage, int howmany=1); 
        // call DB object to allocate a run of new pages and 
        // find a frame in the buffer pool for the first page
        // and pin it. If buffer is full, ask DB to deallocate 
        // all these pages and return error

    Status freePage(PageId globalPageId); 
        // User should call this method if it needs to delete a page
        // this routine will call DB to deallocate the page 

    Status flushPage(PageId pageid);
        // Used to flush a particular page of the buffer pool to disk
        // Should call the write_page method of the DB class

    Status flushAllPages();
	// Flush all pages of the buffer pool to disk, as per flushPage.

    /*** Methods for compatibility with project 1 ***/
    Status pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage, const char *filename);
	// Should be equivalent to the above pinPage()
	// Necessary for backward compatibility with project 1

    Status unpinPage(PageId globalPageId_in_a_DB, int dirty, const char *filename);
	// Should be equivalent to the above unpinPage()
	// Necessary for backward compatibility with project 1

    unsigned int getNumUnpinnedBuffers();
	// Get number of unpinned buffers

    unsigned int getNumBuffers() const { return numBuffers; }
	// Get number of buffers

};

#endif