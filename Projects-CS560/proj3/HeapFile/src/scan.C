/*=============================================================================
 |   Assignment:  Project 2
 |       Author:  Kinsleigh Wong, Sourav Mangla
 |       NetIDs:  kinsleighwong, souravmangla
 |
 |       Course:  CSC 560
 |   Instructor:  Richard Snodgrass
 |     Due Date:  10/1/2021, 11:59pm
 *===========================================================================*/

#include <stdio.h>
#include <stdlib.h>

#include "heapfile.h"
#include "scan.h"
#include "hfpage.h"
#include "buf.h"
#include "db.h"

int pin = 0;
// *******************************************
// The constructor pins the first page in the file
// and initializes its private data members from the private data members from hf
Scan::Scan(HeapFile *hf, Status &status)
{
  status = init(hf);
}

// *******************************************
// The deconstructor unpin all pages.
Scan::~Scan()
{

  reset();
  // put your code here
}

// *******************************************
// Retrieve the next record in a sequential scan.
// Also returns the RID of the retrieved record.
Status Scan::getNext(RID &rid, char *recPtr, int &recLen)
{
  //cout << "Pin " << pin;
  if (nxtUserStatus != OK)
  {
    Status rc = nextDataPage();
    if (rc != OK)
    {
      //cout << "The end " << pin << endl;
      return rc;
    }
  }

  Status rc = dataPage->getRecord(userRid, recPtr, recLen);
  assert(rc == OK);
  if (rc != OK)
    return MINIBASE_CHAIN_ERROR(HEAPFILE, rc);
  rid = userRid;

  nxtUserStatus = dataPage->nextRecord(userRid, userRid);

  return OK;
}

// *******************************************
// Do all the constructor work.
Status Scan::init(HeapFile *hf)
{
  _hf = hf;

  dirPageId = hf->firstDirPageId;
  if (dirPageId == INVALID_PAGE)
    return FAIL;

  Status rc = MINIBASE_BM->pinPage(dirPageId, (Page *&)dirPage);
  assert(rc == OK);
  pin++;

  Status status = firstDataPage();

  return status;
}

// *******************************************
// Reset everything and unpin all pages.
Status Scan::reset()
{
  Status rc;
  if (dirPage != NULL)
  {
    rc = MINIBASE_BM->unpinPage(dirPageId);
    if (rc != OK)
      return MINIBASE_CHAIN_ERROR(HEAPFILE, rc);
    pin--;
  }

  if (dataPage != NULL)
  {
    rc = MINIBASE_BM->unpinPage(dataPageId);
    if (rc != OK)
      return MINIBASE_CHAIN_ERROR(HEAPFILE, rc);
    pin--;
  }

  dirPage = NULL;
  dataPage = NULL;

  dirPageId = INVALID_PAGE;
  dataPageId = INVALID_PAGE;

  nxtUserStatus = OK;
  return OK;
}

// *******************************************
// Copy data about first page in the file.
Status Scan::firstDataPage()
{
  dataPage = NULL;

  Status rc = dirPage->firstRecord(dataPageRid);
  assert(rc == OK);

  DataPageInfo dpi;
  int recLen = -1;
  rc = dirPage->getRecord(dataPageRid, (char *)&dpi, recLen);
  assert(rc == OK);
  dataPageId = dpi.pageId;

  rc = MINIBASE_BM->pinPage(dataPageId, (Page *&)dataPage);
  assert(rc == OK);
  pin++;

  rc = dataPage->firstRecord(userRid);
  assert(rc == OK);

  if (rc == FAIL)
    return MINIBASE_CHAIN_ERROR(HEAPFILE, rc);
  //cout << "UserRid: "<< userRid.pageNo << " " << userRid.slotNo << endl;

  return OK;
}

// *******************************************
// Retrieve the next data page.
Status Scan::nextDataPage()
{
  Status rc = FAIL;
  PageId nextPid = dataPage->getNextPage();

  rc = MINIBASE_BM->unpinPage(dataPageId);
  assert(rc == OK);
  pin--;
  dataPage = NULL;

  if (nextPid == INVALID_PAGE)
  {
    rc = MINIBASE_BM->unpinPage(dirPageId);
    assert(rc == OK);
    pin--;
    dirPage = NULL;
    return DONE;
  }

  dataPageId = nextPid;
  rc = MINIBASE_BM->pinPage(dataPageId, (Page *&)dataPage);
  assert(rc == OK);
  pin++;
  nxtUserStatus = dataPage->firstRecord(userRid);
  if (nxtUserStatus != OK)
    return MINIBASE_CHAIN_ERROR(HEAPFILE, rc);

  return OK;
}

// *******************************************
// Retrieve the next directory page.
Status Scan::nextDirPage()
{

  PageId nextDirPid = dirPage->getNextPage();
  Status dir_rc = MINIBASE_BM->unpinPage(dirPageId);
  if (dir_rc != OK)
    return MINIBASE_CHAIN_ERROR(HEAPFILE, dir_rc);
  pin--;
  dirPageId = nextDirPid;
  dir_rc = MINIBASE_BM->pinPage(nextDirPid, (Page *&)dirPage);
  pin++;

  return OK;
}