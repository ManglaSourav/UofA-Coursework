/*
 * btreefilescan.C - function members of class BTreeFileScan
 *
 * Spring 14 CS560 Database Systems Implementation
 * Edited by Young-K. Suh (yksuh@cs.arizona.edu) 
 */

#include "minirel.h"
#include "buf.h"
#include "db.h"
#include "new_error.h"
#include "btfile.h"
#include "btreefilescan.h"

/*
 * Note: BTreeFileScan uses the same errors as BTREE since its code basically 
 * BTREE things (traversing trees).
 */

BTreeFileScan::~BTreeFileScan()
{
  free(curr_key);
  if(curPid != INVALID_PAGE) {
    Status rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
    assert(rc == OK);
  }
}


Status BTreeFileScan::get_next(RID & rid, void* keyptr) {
  // return DONE if we are past our bound
  if(high_key != NULL && keyCompare(curr_key, high_key, keyType) > 0) {
    return DONE;
  }
  if(scanComplete) {
    return DONE;
  }
  memcpy(keyptr, curr_key, keysize());
  rid = dataRid; 

  memset(curr_key, 0, keySize);
  Status rc = curPage->get_next(curRid, curr_key, dataRid); 
  assert(rc != FAIL);

  // if there are a bunch of empty pages in-between, need to skip
  while(rc != OK) {
    PageId nextPid = curPage->getNextPage();
    rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
    assert(rc == OK);
    if(nextPid == INVALID_PAGE) {
      curPid = INVALID_PAGE;
      scanComplete = true;
      return OK;
    }
    curPid = nextPid;
    rc = MINIBASE_BM->pinPage(curPid, (Page *&)curPage, FALSE);
    assert(rc == OK);
    rc = curPage->get_first(curRid, curr_key, dataRid);
  }

  return OK;
}

Status BTreeFileScan::delete_current() {
  return curPage->deleteRecord(curRid);
}


int BTreeFileScan::keysize() {
  return keySize;
}
