/*****************************************************************************/
/*************** Implementation of the Buffer Manager Layer ******************/
/*****************************************************************************/


#include "buf.h"
#include <vector>
#include <iostream>
#include <algorithm>

// Define buffer manager error messages here
//enum bufErrCodes  {...};

// Define error message here
static const char* bufErrMsgs[] = { 
  // error message strings go here
  "Not enough memory to allocate hash entry",
  "Inserting a duplicate entry in the hash table",
  "Removing a non-existing entry from the hash table",
  "Page not in hash table",
  "Not enough memory to allocate queue node",
  "Poping an empty queue",
  "OOOOOOPS, something is wrong",
  "Buffer pool full",
  "Not enough memory in buffer manager",
  "Page not in buffer pool",
  "Unpinning an unpinned page",
  "Freeing a pinned page"
};

// Create a static "error_string_table" object and register the error messages
// with minibase system 
static error_string_table bufTable(BUFMGR,bufErrMsgs);

//*************************************************************
//** This is the implementation of BufMgr
//************************************************************

BufMgr::BufMgr (int numbuf, Replacer *replacer) {
  numBuffers = numbuf;
  bufPool = (Page *) calloc(numBuffers, sizeof(Page));

  frames = (frame *) malloc(sizeof(frame) * numBuffers);
  whenUsed = (FrameId *) malloc(sizeof(FrameId) * numBuffers);
  hashTable = (hashEntry *) malloc(sizeof(hashEntry) * HTSIZE);

  for(unsigned int i = 0; i < numBuffers; ++i) {
    frames[i].loved = false;
    frames[i].dirty = false;
    frames[i].pageId = INVALID_PAGE;
    frames[i].pincount = 0;

    whenUsed[i] = -1;

    if(i < HTSIZE) {
    hashTable[i].pageId = -1;
    hashTable[i].frameId = -1;
    hashTable[i].next = NULL;
    }
  }

  a = HTSIZE / 3;
  b = HTSIZE / 2;
  if(getNumUnpinnedBuffers() != numBuffers) {
    exit(1);
  }
}


BufMgr::~BufMgr(){
  flushAllPages();

  if(frames != NULL) {
    free(frames);
  }
  if(whenUsed != NULL) {
    free(whenUsed);
  }

  if(hashTable != NULL) {
    hashEntry *curr = NULL;
    for(unsigned int i = 0; i <HTSIZE; ++i) {
      while(hashTable[i].next != NULL) {
        curr = hashTable[i].next;
        hashTable[i].next = curr->next;    
        free(curr);
      }
    }

    free(hashTable);
  }
}


BufMgr::hashEntry* BufMgr::findEntry(hashEntry *head, PageId pageId) {
  hashEntry *curr = head;
  while(curr->next != NULL) {
    if(curr->pageId == pageId) {
      return curr;
    }
    curr = curr->next;
  }
  if(curr->pageId == pageId) {
    return curr;
  }  

  return NULL;
}

BufMgr::hashEntry* BufMgr::findParentUsingPage(hashEntry *head, PageId pageId) {
  hashEntry *curr = head;
  if(curr->pageId == pageId) {
    return curr;
  }  
  while(curr->next != NULL) {
    if(curr->next->pageId == pageId) {
      return curr;
    }
    curr = curr->next;
  }

  return NULL;
}

// this is run assuming that the entry exists
BufMgr::hashEntry* BufMgr::findParent(hashEntry *head, FrameId frameId) {
  hashEntry *curr = head;
  if(curr->frameId == frameId) {
    return curr;
  }  
  while(curr->next != NULL) {
    if(curr->next->frameId == frameId) {
      return curr;
    }
    curr = curr->next;
  }

  cout << "findParent could not find parent of frame " << frameId << endl;
  cout << endl;
  debugFrames();
  debugHash();
  exit(1);
  return NULL;
}

void BufMgr::insertIntoTable(hashEntry *entry, int hash_loc) {
  if(hashTable[hash_loc].pageId == -1) {
    memcpy(&hashTable[hash_loc], entry, sizeof(hashEntry));
    free(entry);
  } else {
    // I place it lazily
    entry->next = hashTable[hash_loc].next;
    hashTable[hash_loc].next = entry;
  }
}

//*************************************************************
//** This is the implementation of pinPage
//************************************************************
Status BufMgr::pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage) {
  if(PageId_in_a_DB == INVALID_PAGE)
    return FAIL;

  FrameId repFrame = -1;
  unsigned int hash_loc = (a * PageId_in_a_DB + b) % HTSIZE;
  hashEntry *curr = findEntry(&hashTable[hash_loc], PageId_in_a_DB);
  Status rc = OK;

  // first, we check to see if the page already exists.
  if(curr != NULL) {
    frames[curr->frameId].pincount++;
    updateFrameId(curr->frameId);
    page = &bufPool[curr->frameId];
    return OK;
  }

  //otherwise, we need to either find a free frame,
  if(getNumFreeBuffers() > 0) {
    unsigned int i = 0;
    for(i = 0; i < numBuffers; ++i) {
      if(frames[i].pageId == INVALID_PAGE)
        break;
    }
    repFrame = i;    
  } else {
    // or use our replacement strategy to remove old entry from the table
    repFrame = locateReplacee();
    if(repFrame == -1)
      return FAIL;

    unsigned int old_ind = (a * frames[repFrame].pageId + b) % HTSIZE;
    curr = findParent(&hashTable[old_ind], repFrame);

    // this should never happen
    if(frames[repFrame].pageId != INVALID_PAGE
        && frames[repFrame].dirty) {
      rc = flushPage(frames[repFrame].pageId);
      assert(rc == OK);
    }

    // if we want to replace the head of the list, 
    if(frames[repFrame].pageId == hashTable[old_ind].pageId
        && repFrame == hashTable[old_ind].frameId) {
      if(curr->next != NULL) {        
        curr->frameId = curr->next->frameId;
        curr->pageId = curr->next->pageId;
        hashEntry *temp = curr->next;
        curr->next = curr->next->next;
        free(temp);        
      } else {
        curr->frameId = INVALID_PAGE;
        curr->pageId = INVALID_PAGE;
      }
    } else {
      // no need to check for null because we find the parent. 
      if(curr->next->frameId != repFrame) {
        cout << "Issues with findParent" << endl;
        exit(1);
      }
      hashEntry *temp = curr->next;
      curr->next = curr->next->next;
      free(temp);
    }
  }

  //then, we add it to where it belongs.
  curr = (hashEntry *) malloc(sizeof(hashEntry));
  curr->frameId = repFrame;
  curr->pageId = PageId_in_a_DB;
  curr->next = NULL;
  frames[repFrame].pageId = PageId_in_a_DB;
  frames[repFrame].dirty = false;
  frames[repFrame].pincount++;
  
  if(!emptyPage) {
    rc = MINIBASE_DB->read_page(PageId_in_a_DB, &bufPool[repFrame]);
    if(rc != OK) {
      //cout << PageId_in_a_DB << endl;
      return FAIL;
    }
  }
  page = &bufPool[repFrame];

  insertIntoTable(curr, hash_loc);
  updateFrameId(repFrame);

  determineDup();

  return OK;
}//end pinPage

//*************************************************************
//** This is the implementation of unpinPage
//************************************************************
Status BufMgr::unpinPage(PageId page_num, int dirty, int hate){
  
  int ind = (a * page_num + b) % HTSIZE;
  hashEntry *curr = &hashTable[ind];

  while(curr->next != NULL) {
    if(curr->pageId == page_num) {
      if(frames[curr->frameId].pincount == 0)
        return FAIL;

      frames[curr->frameId].loved = !hate;
      frames[curr->frameId].pincount--;
      if(dirty) {
        frames[curr->frameId].dirty = true;  
      }
      return OK;
    }
    curr = curr->next;
  }

  if(curr->pageId == page_num) {
    //cout << curr->frameId << " " << curr->pageId << endl;
    frames[curr->frameId].loved = !hate;
    frames[curr->frameId].pincount--;
    if(dirty) {
      frames[curr->frameId].dirty = true;  
    }
    //debugFrames();
    return OK;
  }

  //cout << "*****" << endl;
  //debugFrames();
  //debugHash();

  return FAIL;
}

//*************************************************************
//** This is the implementation of newPage
//************************************************************
Status BufMgr::newPage(PageId& firstPageId, Page*& firstpage, int howmany) {
  Status rc = MINIBASE_DB->allocate_page(firstPageId, howmany);
  if(rc != OK)
    return rc;

  return pinPage(firstPageId, firstpage, true);  
}

//*************************************************************
//** This is the implementation of freePage
//************************************************************
Status BufMgr::freePage(PageId globalPageId){
  int ind = (a * globalPageId + b) % HTSIZE;
  hashEntry* curr = findParentUsingPage(&hashTable[ind], globalPageId);
  FrameId curr_frame = INVALID_PAGE;

  if(curr == NULL) {
    return MINIBASE_DB->deallocate_page(globalPageId);
  }

  if(curr->next != NULL && curr->pageId != globalPageId) {
    curr_frame = curr->next->frameId;
  } else {
    curr_frame = curr->frameId;
  }

  //cout << "curr_frame:" << curr_frame << " pageId: " << 

  if(frames[curr_frame].pincount > 0) {
    return FAIL;
  }

  // if we want to free the head of the list, 
  if(curr->pageId == hashTable[ind].pageId
      && curr_frame == hashTable[ind].frameId) {
        //cout << "Trying to free head!" << endl;
    if(curr->next != NULL) {        
      curr->frameId = curr->next->frameId;
      curr->pageId = curr->next->pageId;
      hashEntry *temp = curr->next;
      curr->next = curr->next->next;
      free(temp);        
    } else {
      curr->frameId = INVALID_PAGE;
      curr->pageId = INVALID_PAGE;
    }
  } else {
    hashEntry *temp = curr->next;
    curr->next = curr->next->next;
    free(temp);
  }
  
  frames[curr_frame].pageId = INVALID_PAGE;
  frames[curr_frame].loved = false;
  frames[curr_frame].dirty = false;

  return MINIBASE_DB->deallocate_page(globalPageId);
}

//*************************************************************
//** This is the implementation of flushPage
//************************************************************
Status BufMgr::flushPage(PageId pageid) {
  //cout << "Flushing page " << pageid << endl;
  int ind = (a * pageid + b) % HTSIZE;
  hashEntry *curr = &hashTable[ind];

  while(curr->next != NULL) {
    if(curr->pageId == pageid) {
      frames[curr->frameId].dirty = false;
      return MINIBASE_DB->write_page(pageid, &bufPool[curr->frameId]);
    }
    curr = curr->next;
  }

  if(curr->pageId == pageid) {
    frames[curr->frameId].dirty = false;
    return MINIBASE_DB->write_page(pageid, &bufPool[curr->frameId]);
  }

  return FAIL;
}
    
//*************************************************************
//** This is the implementation of flushAllPages
//************************************************************
Status BufMgr::flushAllPages(){
  Status rc = OK;
  for(unsigned int i = 0; i < numBuffers; ++i) {
    if(frames[i].pageId != INVALID_PAGE && frames[i].dirty) {
      rc = MINIBASE_DB->write_page(frames[i].pageId, &bufPool[i]);
      if(rc != OK)
        return rc;
      frames[i].dirty = false;
    }
  }

  return OK;
}


/*** Methods for compatibility with project 1 ***/
//*************************************************************
//** This is the implementation of pinPage
//************************************************************
Status BufMgr::pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage, const char *filename){
  return pinPage(PageId_in_a_DB, page, emptyPage);
}

//*************************************************************
//** This is the implementation of unpinPage
//************************************************************
Status BufMgr::unpinPage(PageId globalPageId_in_a_DB, int dirty, const char *filename){
  return unpinPage(globalPageId_in_a_DB, dirty);
}

//*************************************************************
//** This is the implementation of getNumUnpinnedBuffers
//************************************************************
unsigned int BufMgr::getNumUnpinnedBuffers(){
  int count = 0;
  for(unsigned int i = 0; i < numBuffers; ++i) {
    if(frames[i].pincount > 0) {
      count++;
    }
  }
  return (numBuffers - count);
}

unsigned int BufMgr::getNumFreeBuffers(){
  int count = 0;
  for(unsigned int i = 0; i < numBuffers; ++i) {
    if(frames[i].pageId != INVALID_PAGE)
      count++;
  }
  return (numBuffers - count);
}

FrameId BufMgr::locateReplacee()
{
  FrameId lru_loved = -1;
  for (unsigned int i = 0; i < numBuffers; i++)
  {
    frame cur_frame = frames[whenUsed[i]];
    if (cur_frame.pincount == 0 && !cur_frame.loved)
    {
      return whenUsed[i];
    }
    else if (cur_frame.pincount == 0 && cur_frame.loved)
    {
      lru_loved = whenUsed[i];
    }
  }
  return lru_loved;
}


void  BufMgr::updateFrameId(int id)
{
  unsigned int i;
  bool found = false;
  for (i = 0; i < numBuffers; ++i)
  {
    if (whenUsed[i] == id)
    {
      found = true;
      break;
    }
  }
  if (found)
  {
    //  right shift
    for (int j = i; j > 0; j--)
    {
      whenUsed[j] = whenUsed[j - 1];
    }
  }
  else
  {
    //      id not found in whenUsed, right shift all the element
    for (int j = numBuffers; j > 0; j--)
    {
      whenUsed[j] = whenUsed[j - 1];
    }
  }
  //    insert id in the front
  whenUsed[0] = id;
}


void BufMgr::debugFrames() {
  cout << "What is going on " << endl;
  for(unsigned int i = 0; i < numBuffers; ++i) {
    if(frames[i].pageId != INVALID_PAGE) {
      cout << "Frame " << i << "  pageId, dirty, loved, pincount: " << frames[i].pageId << " " << frames[i].dirty << " " << frames[i].loved << " " << frames[i].pincount << endl;
    }
  }
}

void BufMgr::debugHash() {
  for(int i = 0; i < HTSIZE; ++i) {
    cout << "For index " << i << " of the table:" << endl;
    hashEntry *curr = &hashTable[i];
    while(curr != NULL && curr->pageId != INVALID_PAGE) {
      cout << "PageId, FrameId: " << curr->pageId << " " << curr->frameId << endl;
      curr = curr->next;
    }
    cout << endl;
  }
}

void BufMgr::determineDup() {
  std::vector<PageId> v = {};

  for(int i = 0; i < HTSIZE; ++i) {
    hashEntry *curr = &hashTable[i];
    while(curr != NULL && curr->pageId != INVALID_PAGE) {
      if(std::find(v.begin(), v.end(), curr->pageId) != v.end()) {
        cout << endl << "************ Duplicate found" << endl;
        debugFrames();
        debugHash();
        exit(1);
      }
      v.push_back(curr->pageId);
      curr = curr->next;
    }
  }
}
