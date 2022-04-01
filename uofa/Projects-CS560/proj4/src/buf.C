/*************** Implementation of the Buffer Manager Layer ******************/
/*=============================================================================
 |   Assignment:  Project 3 
 |       Author:  Kinsleigh Wong, Sourav Mangla
 |       NetIDs:  kinsleighwong, souravmangla
 |
 |       Course:  CSC 560
 |   Instructor:  Richard Snodgrass
 |     Due Date:  10/23/2021, 11:59pm
 *===========================================================================*/


#include "buf.h"


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
  whenUsed = (int *) malloc(sizeof(int) * numBuffers);
  hashTable = (hashEntry *) malloc(sizeof(hashEntry) * numBuffers);

  for(unsigned int i = 0; i < numBuffers; ++i) {
    frames[i].loved = false;
    frames[i].dirty = false;
    frames[i].pageId = INVALID_PAGE;
    frames[i].pincount = 0;

    whenUsed[i] = -1;

    hashTable[i].prevFrame = -1;
    hashTable[i].frameId = -1;
    hashTable[i].nextFrame = -1;
  }

  a = HTSIZE / 3;
  b = HTSIZE / 2;
}

//*************************************************************
//** This is the implementation of ~BufMgr
//************************************************************
BufMgr::~BufMgr(){
  free(bufPool);
  free(frames);
  free(whenUsed);
  free(hashTable);
}

//*************************************************************
//** This is the implementation of pinPage
//************************************************************
Status BufMgr::pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage) {
  Status rc = FAIL;

  if(PageId_in_a_DB == INVALID_PAGE)
    return FAIL;

  unsigned int hash_loc = (a * PageId_in_a_DB + b) % HTSIZE;
  unsigned int ind = hash_loc;
  unsigned int i = 0;

  // if the frame is already occupied,
  if(frames[ind].pageId != INVALID_PAGE) {
    while(hashTable[ind].nextFrame != -1) {
      // if we find the frame we are looking for, pin & give the page
      if(frames[ind].pageId == PageId_in_a_DB) {
        frames[ind].pincount++;
        page = &bufPool[ind];
        updateFrameId(ind);
        return OK;
      }
      ind = hashTable[ind].nextFrame;
    }

    if(frames[ind].pageId == PageId_in_a_DB) {
      frames[ind].pincount++;
      page = &bufPool[ind];
      updateFrameId(ind);
      return OK;
    }
    
    //otherwise, we need to find an open overflow page
    bool zeroExists = false;
    for(i = HTSIZE; i < numBuffers; ++i) {
      //if we find a a free frame, 
      if(frames[i].pageId == INVALID_PAGE) {
        hashTable[ind].nextFrame = i; //set tail to i
        hashTable[i].nextFrame = -1;
        hashTable[i].prevFrame = ind;
        ind = i; // set ind to i so we can write out the relevant frame later
        break;
      }

      if(frames[i].pincount == 0)
        zeroExists = true;
    }

    //if we reach the end without finding a free frame,
    if(i == numBuffers) {
      if(!zeroExists) {
        return FAIL; // think we're supposed to do other stuff as well
      }

      // we need to locate a replacement frame. 
      ind = locateReplacee();
      if(frames[ind].dirty) {
        cout << "Writing out" << endl;
        rc =  MINIBASE_DB->write_page(frames[ind].pageId, &bufPool[ind]);
        assert(rc == 0);
      }

      //update its current neighbors in the hash table,
      int prevFrame = hashTable[ind].prevFrame, nextFrame = hashTable[ind].nextFrame;

      // need edge case for what happens if at start and maybe at end?
      if(prevFrame == -1) {
        //extreme edge case that should never happen
        if(nextFrame == -1 && hash_loc != ind)
          return FAIL;

        //dest, src
        //copy the next page over and set it to be replaced
        memcpy(&bufPool[ind], &bufPool[nextFrame], sizeof(Page));
        ind = nextFrame;
        memcpy(&frames[ind], &frames[nextFrame], sizeof(frame));
        memcpy(&hashTable[ind], &hashTable[nextFrame], sizeof(hashEntry));
        // need to swap places in whenUsed array
        swapUsed(ind, nextFrame);

        prevFrame = hashTable[ind].prevFrame;
        nextFrame = hashTable[ind].nextFrame;


      } else {
        hashTable[prevFrame].nextFrame = nextFrame;
      }

      if(nextFrame != -1)
        hashTable[nextFrame].prevFrame = prevFrame;

      // and then move it to the end of the correct list
      while(hashTable[hash_loc].nextFrame != -1) {
        hash_loc = hashTable[hash_loc].nextFrame;
      }
      hashTable[hash_loc].nextFrame = ind;
      hashTable[ind].prevFrame = hash_loc;
      hashTable[ind].nextFrame = -1;
      
    }
  }

  if(hashTable[ind].prevFrame != -1 && ind < HTSIZE)  {
    exit(0);
  }


  updateFrameId(ind);
  rc = MINIBASE_DB->read_page(PageId_in_a_DB, &bufPool[ind]);
  //cout << ind << endl;
  if(rc != OK) {
    cout << "Failed reading a page" << endl;
    cout << ind << endl;
  }
  assert(rc == OK);

  page = &bufPool[ind];
  frames[ind].pageId = PageId_in_a_DB;
  frames[ind].pincount = 1;
  frames[ind].loved = false;
  frames[ind].dirty = false;

  return OK;
}//end pinPage

//*************************************************************
//** This is the implementation of unpinPage
//************************************************************
Status BufMgr::unpinPage(PageId page_num, int dirty=FALSE, int hate = FALSE){
  int ind = (a * page_num + b) % HTSIZE;
  
  while(hashTable[ind].nextFrame != -1) {
    if(frames[ind].pageId == page_num) {
      frames[ind].loved = !hate;
      frames[ind].pincount--;
      if(dirty) {
        frames[ind].dirty = true;  
      }
      return OK;
    }
    ind = hashTable[ind].nextFrame;
  }

  if(frames[ind].pageId == page_num) {
    frames[ind].loved |= !hate;
    frames[ind].pincount--;
    if(dirty) {
      frames[ind].dirty = true;   
    }
    return OK;
  }

  return FAIL;
}

//*************************************************************
//** This is the implementation of newPage
//************************************************************
Status BufMgr::newPage(PageId& firstPageId, Page*& firstpage, int howmany) {
  Status rc = MINIBASE_DB->allocate_page(firstPageId, howmany);
  assert(rc == OK);

  return pinPage(firstPageId, firstpage, true);  
}

//*************************************************************
//** This is the implementation of freePage
//************************************************************
Status BufMgr::freePage(PageId globalPageId){
  int ind = (a * globalPageId + b) % HTSIZE;
  
  while(hashTable[ind].nextFrame != -1) {
    if(frames[ind].pageId == globalPageId) {
      if(frames[ind].pincount > 0) {
        return FAIL;
      }
      frames[ind].pageId = INVALID_PAGE;
      frames[ind].loved = false;
      frames[ind].dirty = false;
      return MINIBASE_DB->deallocate_page(globalPageId);
    }
    ind = hashTable[ind].nextFrame;
  }

  if(frames[ind].pageId == globalPageId) {
      if(frames[ind].pincount > 0) {
        return FAIL;
      }
      frames[ind].pageId = INVALID_PAGE;
      frames[ind].loved = false;
      frames[ind].dirty = false;
      return MINIBASE_DB->deallocate_page(globalPageId);
  }

  return FAIL;  
}

//*************************************************************
//** This is the implementation of flushPage
//************************************************************
Status BufMgr::flushPage(PageId pageid) {
  for(unsigned int i = 0; i < numBuffers; ++i) {
    if(pageid == frames[i].pageId) {
      frames[i].pageId = INVALID_PAGE;
      frames[i].loved = false;
      if(frames[i].dirty) {
        frames[i].dirty = false;
        return MINIBASE_DB->write_page(pageid, &bufPool[i]);
      }

      return OK;
    }
  }
  return FAIL;
}
    
//*************************************************************
//** This is the implementation of flushAllPages
//************************************************************
Status BufMgr::flushAllPages(){
  Status rc = OK;
  for(unsigned int i = 0; i < numBuffers; ++i) {
    if(frames[i].pageId != INVALID_PAGE) {
      if(frames[i].dirty)
        rc = MINIBASE_DB->write_page(frames[i].pageId, &bufPool[i]);
      if(rc != OK)
        return rc;
      frames[i].loved = false;
      frames[i].dirty = false;
      frames[i].pageId = INVALID_PAGE;
      frames[i].pincount = 0;
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
    if(frames[i].pageId != INVALID_PAGE) {
      count++;
    }
  }
  return count;
}


void BufMgr::debugFrames() {
  for(unsigned int i = 0; i < numBuffers; ++i) {
    cout << "Frame " << i << " prevFrame, nextFrame: " << hashTable[i].prevFrame << " " << hashTable[i].nextFrame << " pageId: " << frames[i].pageId << endl;
  }
}



int BufMgr::locateReplacee()
{
  int lru_loved = -1;
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

void BufMgr::swapUsed(int frame1, int frame2) {
  int loc1 = -1, loc2 = -1;
  for(int i = 0; i < (int) numBuffers; ++i) {
    if(whenUsed[i] == frame1) {
      loc1 = i;
    } else if(whenUsed[i] == frame2) {
      loc2 = i;
    }
  }
  if(loc1 == -1 || loc2 == -1)
    cout << "In swapUsed, could not find the specified frame" << endl;
  int tmp = whenUsed[loc1];
  whenUsed[loc1] = whenUsed[loc2];
  whenUsed[loc2] = tmp;
}

int  BufMgr::deleteFrameId(int numBuffers, int *&whenUsed, int id)
{
  for (int i = 0; i < numBuffers; i++)
  {
    if (whenUsed[i] == id)
    {
      // left shift
      for (int j = i; j < numBuffers - 1; j++)
      {
        whenUsed[j] = whenUsed[j + 1];
      }
      whenUsed[numBuffers - 1] = -1;
      return 1;
    }
  }
  return 0;
}

// insert + update
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