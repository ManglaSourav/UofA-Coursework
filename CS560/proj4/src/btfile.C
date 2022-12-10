/*
 * btfile.C - function members of class BTreeFile 
 * 
 * Johannes Gehrke & Gideon Glass  951022  CS564  UW-Madison
 * Edited by Young-K. Suh (yksuh@cs.arizona.edu) 03/27/14 CS560 Database Systems Implementation 
 */

#include "minirel.h"
#include "buf.h"
#include "db.h"
#include "new_error.h"
#include "btfile.h"
#include "btreefilescan.h"

// Define your error message here
const char* BtreeErrorMsgs[] = {};

static error_string_table btree_table( BTREE, BtreeErrorMsgs);

BTreeFile::BTreeFile (Status& returnStatus, const char *filename) {
  fileName = (char *) malloc(sizeof(char) * strlen(filename));
  strncpy(fileName, filename, strlen(filename));
  PageId tempPid;
  returnStatus = MINIBASE_DB->get_file_entry(filename, tempPid);

  HFPage *page = NULL;
  Status rc = MINIBASE_BM->pinPage(tempPid, (Page *&)page, FALSE);
  assert(rc == OK);

  int headerLength = 0;
  RID tempRid;
  tempRid.pageNo = tempPid;
  tempRid.slotNo = 0;
  rc = page->returnRecord(tempRid, (char*&)header, headerLength);
  assert(rc == OK);
  assert(headerLength == sizeof(headerInfo));

  rc = MINIBASE_BM->pinPage(header->rootPid, (Page *&)rootPage, FALSE);
  assert(rc == OK);
  returnStatus = OK;
}


BTreeFile::BTreeFile (Status& returnStatus, const char *filename, 
                      const AttrType keytype,
                      const int keysize) {
  // note down the name for when we want to destroy the file
  fileName = (char *) malloc(sizeof(char) * strlen(filename));
  strncpy(fileName, filename, strlen(filename));

  // first, we create the header page + the file entry,
  PageId tempPid; 
  Status rc = MINIBASE_DB->allocate_page(tempPid);
  assert(rc == OK);
  rc = MINIBASE_DB->add_file_entry(filename, tempPid);
  assert(rc == OK);
  // then pin the header page.
  HFPage *page;
  rc = MINIBASE_BM->pinPage(tempPid, (Page *&)page, FALSE);
  assert(rc == OK);
  page->init(tempPid);

  // We write out the header to the page & then grab that header,
  headerInfo tempHeader;
  tempHeader.headerPid = tempPid;
  RID tempRid;
  page->insertRecord((char*)&tempHeader, sizeof(headerInfo), tempRid);
  int headerLength = 0;
  rc = page->returnRecord(tempRid, (char*&)header, headerLength);
  assert(rc == OK);
  assert(headerLength == sizeof(headerInfo));

  //create the root page then pin it,
  rc = MINIBASE_DB->allocate_page(header->rootPid);
  assert(rc == OK);
  rc = MINIBASE_BM->pinPage(header->rootPid, (Page *&)rootPage, FALSE);
  assert(rc == OK);
  rootPage->init(header->rootPid);
  rootPage->set_type(INDEX);

  // then fill out the rest of the header.
  header->keyType = keytype;
  header->keySize = keysize;
  header->height = 1; 
  returnStatus = OK;
}

BTreeFile::~BTreeFile () {
  Status rc = MINIBASE_BM->unpinPage(header->headerPid, TRUE, TRUE);
  assert(rc == OK);
  rc = MINIBASE_BM->unpinPage(header->rootPid, TRUE, TRUE);
  assert(rc == OK);
  free(fileName);
}


Status BTreeFile::destroy_helper(int curr_level, PageId curPid) {
    // HANDLING BTLEAF_PAGE --------------------------------------
  if(curr_level == 0) 
    return MINIBASE_DB->deallocate_page(curPid);

  // HANDLING BTINDEX_PAGE --------------------------------------
  RID curRid; // not used
  void *curKey = NULL; // not used
  PageId nextPid;
  BTIndexPage *page;

  Status rc = MINIBASE_BM->pinPage(curPid, (Page *&)page, FALSE);
  assert(rc == OK);

  nextPid = page->getLeftLink();
  if(nextPid != INVALID_PAGE) {
    rc = destroy_helper(curr_level - 1, nextPid);
    assert(rc == OK);
  }

  rc = page->get_first(curRid, curKey, nextPid);
  while(rc == OK) {
    rc = destroy_helper(curr_level - 1, nextPid);
    assert(rc != FAIL);
    rc = page->get_next(curRid, curKey, nextPid);
  }
  rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
  assert(rc == OK);

  return MINIBASE_DB->deallocate_page(curPid);
}

Status BTreeFile::destroyFile() {
  headerInfo head = *header;

  //unpin the header page,
  PageId temp = head.headerPid;
  Status rc = MINIBASE_BM->unpinPage(head.headerPid, TRUE, TRUE);
  assert(rc == OK);
  //remove file entry,
  rc = MINIBASE_DB->delete_file_entry(fileName);
  assert(rc == OK);

  //then deallocate it.
  rc = MINIBASE_DB->deallocate_page(temp);
  assert(rc == OK);

  //unpin the root page as well since we are about to delete it
  rc = MINIBASE_BM->unpinPage(head.rootPid, TRUE, TRUE);

  //then, we recursively delete the tree
  destroy_helper(head.height, head.rootPid);

  return rc;
}

// grows the root
Status BTreeFile::grow() {
  PageId nextPid = INVALID_PAGE, leftPid = INVALID_PAGE, rightPid = INVALID_PAGE;
  RID curRid; // not used for anything
  void *key = malloc(keysize()), *median_key = malloc(keysize());
  BTIndexPage *leftPage = NULL, *rightPage = NULL;
  int i = 0, median = rootPage->numberOfRecords() / 2;


  // create a new page that will be the left sub-tree,
  Status rc = MINIBASE_DB->allocate_page(leftPid);
  assert(rc == OK);
  rc = MINIBASE_BM->pinPage(leftPid, (Page *&)leftPage, FALSE);
  assert(rc == OK);  
  leftPage->init(leftPid);
  leftPage->set_type(INDEX);
  leftPage->setLeftLink(rootPage->getLeftLink());
  // copy over the first half. 
  rc = rootPage->get_first(curRid, key, nextPid);
  for(i = 0; i < median; ++i) {
    rc = leftPage->insertKey(key, header->keyType, nextPid, curRid);
    assert(rc != FAIL);
    rc = rootPage->get_next(curRid, key, nextPid);
    assert(rc != FAIL);
  }

  //copy over our median key,
  memcpy(median_key, key, keysize());

  //create a new page that will be the right sub-tree,
  rc = MINIBASE_DB->allocate_page(rightPid);
  assert(rc == OK);
  rc = MINIBASE_BM->pinPage(rightPid, (Page *&)rightPage, FALSE);
  assert(rc == OK);
  rightPage->init(rightPid);
  rightPage->set_type(INDEX);
  rightPage->setLeftLink(nextPid);
  rc = rootPage->get_next(curRid, key, nextPid);
  assert(rc != FAIL);
  // move over the second half,
  for(i = median + 1; i < rootPage->numberOfRecords(); ++i) {
    rc = rightPage->insertKey(key, header->keyType, nextPid, curRid);
    assert(rc != FAIL);
    rc = rootPage->get_next(curRid, key, nextPid);
    assert(rc != FAIL);
  }

  // then we go destroy everything
  rc = leftPage->get_first(curRid, key, nextPid);
  assert(rc != FAIL);
  for(i = 0; i < leftPage->numberOfRecords(); ++i) {
    rc = rootPage->deleteKey(key, header->keyType, curRid);
    assert(rc != FAIL);
    rc = leftPage->get_next(curRid, key, nextPid);
    assert(rc != FAIL);
  }
  rc = rightPage->get_first(curRid, key, nextPid);
  assert(rc != FAIL);
  for(i = 0; i < leftPage->numberOfRecords(); ++i) {
    rc = rootPage->deleteKey(key, header->keyType, curRid);
    assert(rc != FAIL);
    rc = rightPage->get_next(curRid, key, nextPid);
    assert(rc != FAIL);
  }

  rc = MINIBASE_BM->unpinPage(leftPid, TRUE, TRUE);
  assert(rc == OK);
  rc = MINIBASE_BM->unpinPage(rightPid, TRUE, TRUE);
  assert(rc == OK);

  // and finally add some things back
  rootPage->setLeftLink(leftPid);
  rootPage->insertKey(median_key, header->keyType, rightPid, curRid);

  header->height++; // root grew so increase height
  free(key);
  free(median_key);
  return OK;
} 

Status BTreeFile::split(int curr_height, PageId parentPid, PageId childPid) {
  PageId  newPid = INVALID_PAGE, nextPid = INVALID_PAGE;
  RID curRid, dataRid, medianRid; // curRid not used for anything, dataRid used for leaves
  RID childRid, newRid;
  BTIndexPage *parentPage = NULL;
  Status rc = OK, final_rc = OK;
  int i = 0;
  void *key = (void *) malloc(keysize());
  void *median_key = (void *) malloc(keysize());
  memset(key, 0, keysize());
  memset(median_key, 0, keysize());

  // pin the index page 
  rc = MINIBASE_BM->pinPage(parentPid, (Page *&)parentPage, FALSE);
  assert(rc == OK);
  // create a new page for the median.
  rc = MINIBASE_DB->allocate_page(newPid);
  assert(rc == OK);

  // CASE: CHILDREN ARE BTINDEXPAGES ****************************
  if(curr_height > 1) {
    BTIndexPage *childPage = NULL, *newPage = NULL;
    // pin the children,
    rc = MINIBASE_BM->pinPage(childPid, (Page *&)childPage, FALSE);
    assert(rc == OK); 
    rc = MINIBASE_BM->pinPage(newPid, (Page *&)newPage, FALSE);
    assert(rc == OK);
    newPage->init(newPid);
    newPage->set_type(INDEX);

    // then get to the median of the child that should be split
    int median = childPage->numberOfRecords() / 2;
    rc = childPage->get_first(curRid, key, nextPid);
    for(i = 0; i < median; ++i) {
      memset(key, 0, keysize());
      rc = childPage->get_next(curRid, key, nextPid);
      assert(rc != FAIL);
    }
    // once we've reached the median,
    memcpy(median_key, key, keysize()); // note down to delete later
    rc = parentPage->insertKey(key, header->keyType, newPid, curRid);
    assert(rc == OK);
    newPage->setLeftLink(nextPid);
    // move second half over,
    for(i = median + 1; i < childPage->numberOfRecords(); ++i) {
      rc = newPage->insertKey(key, header->keyType, nextPid, curRid);
      assert(rc != FAIL);
      memset(key, 0, keysize());
      rc = childPage->get_next(curRid, key, nextPid);
      assert(rc != FAIL);
    }

    // then move out the data from child page
    rc = newPage->deleteKey(median_key, header->keyType, curRid);
    memset(key, 0, keysize());
    rc = newPage->get_first(curRid, key, nextPid);
    while(rc == OK) {
      rc = newPage->deleteKey(key, header->keyType, curRid);
      assert(rc == OK);
      memset(key, 0, keysize());
      rc = newPage->get_next(curRid, key, nextPid);
    }

    rc = MINIBASE_BM->unpinPage(childPid, TRUE, TRUE);
    assert(rc == OK); 
    rc = MINIBASE_BM->unpinPage(newPid, TRUE, TRUE);
    assert(rc == OK); 

  // CASE: CHILDREN ARE BTLEAFPAGES ****************************
  } else if(curr_height == 1) { 
    BTLeafPage *childPage = NULL, *newPage = NULL;
    // child we keep the first half, new we have the median + second half
    rc = MINIBASE_BM->pinPage(childPid, (Page *&)childPage, FALSE);
    assert(rc == OK); 
    rc = MINIBASE_BM->pinPage(newPid, (Page *&)newPage, FALSE);
    assert(rc == OK);
    newPage->init(newPid);
    newPage->set_type(LEAF);
    // make sure that leaves are connected
    newPage->setNextPage(childPage->getNextPage());
    newPage->setPrevPage(childPid);
    childPage->setNextPage(newPid);

    // then get to the median of the child that should be split
    int median = childPage->numberOfRecords() / 2;
    rc = childPage->get_first(childRid, key, dataRid);
    for(i = 0; i < median; ++i) {
      memset(key, 0, keysize());
      rc = childPage->get_next(childRid, key, dataRid);
      assert(rc != FAIL);
    }

    // once we've reached the median,
    memcpy((char *)median_key, (char *)key, keysize()); // note down to delete later
    medianRid = childRid;
    rc = parentPage->insertKey(key, header->keyType, newPid, curRid);
    assert(rc == OK);
    // move second half over,
    RID *rids = (RID *) malloc(sizeof(RID) * childPage->numberOfRecords() - median);
    for(i = median; i < childPage->numberOfRecords(); ++i) {
      memcpy(&rids[i - median], &childRid, sizeof(RID));
      rc = newPage->insertRec(key, header->keyType, dataRid, newRid);
      assert(rc == OK);
      memset(key, 0, keysize());
      rc = childPage->get_next(childRid, key, dataRid);
      assert(rc != FAIL);
    }

    // then delete
    int lim = childPage->numberOfRecords() - median;
    for(int ind = 0; ind < lim; ++ind) {
      rc = childPage->deleteRecord(rids[ind]);
      assert(rc == OK);
    }

    rc = MINIBASE_BM->unpinPage(childPid, TRUE, TRUE);
    assert(rc == OK); 
    rc = MINIBASE_BM->unpinPage(newPid, TRUE, TRUE);
    assert(rc == OK); 
    free(rids);
  // should never happen ****************************
  } else { 
    final_rc = FAIL;
  }

  rc = MINIBASE_BM->unpinPage(parentPid, TRUE, TRUE);
  assert(rc == OK);
  //free(key);
  //free(median_key);

  return final_rc;
}

Status BTreeFile::insert(const void *key, const RID rid) {
  RID curRid;
  PageId nextPid = INVALID_PAGE;
  Status rc = rootPage->get_page_no(key, header->keyType, nextPid);

  if(rc == FAIL) { // shouldn't return this
    cout << "Unable to get_page_no of root" << endl;
    return FAIL;
  }

  // EDGE CASE: we have an empty index page
  if(rc != OK) {
    PageId leftPid = INVALID_PAGE, rightPid = INVALID_PAGE, newPid = INVALID_PAGE;
    // we create our first two leaf pages, 
    rc = MINIBASE_DB->allocate_page(leftPid);
    assert(rc == OK);
    rootPage->setLeftLink(leftPid);
    rc = MINIBASE_DB->allocate_page(rightPid);
    assert(rc == OK);

    // then, we connect the children. 
    BTLeafPage *page = NULL;
    rc = MINIBASE_BM->pinPage(leftPid, (Page *&)page, FALSE);
    assert(rc == OK);
    page->init(leftPid);
    page->set_type(LEAF);
    page->setNextPage(rightPid);
    page->setPrevPage(INVALID_PAGE);

    rc = MINIBASE_BM->unpinPage(leftPid, TRUE, TRUE);
    assert(rc == OK);
    rc = MINIBASE_BM->pinPage(rightPid, (Page *&)page, FALSE);
    assert(rc == OK);
    page->init(rightPid);
    page->set_type(LEAF);
    page->setPrevPage(leftPid);
    page->setNextPage(INVALID_PAGE);

    rc = MINIBASE_BM->unpinPage(rightPid, TRUE, TRUE);
    assert(rc == OK);

    // add it to the root page,
    rc = rootPage->insertKey(key, header->keyType, rightPid, curRid);
    assert(rc == OK);
    rc = rootPage->get_page_no(key, header->keyType, newPid);
    assert(rc == OK);
    // then insert the actual data into the leaf
    rc = insert_helper(header->height - 1, newPid, key, rid);
    assert(rc == OK);
    return rc;
  } 

  rc = insert_helper(header->height - 1, nextPid, key, rid);

  // if we were not able to insert, 
  if(rc != OK) {
    // if we do not have enough space,
    if(rootPage->available_space() < keysize()) {
      // we split the root and then re-insert. 
      rc = grow();
      assert(rc == OK);
    } else { // otherwise, we split nodes
      rc = split(header->height, header->rootPid, nextPid);
      assert(rc == OK);
      return insert(key, rid);
    }
  }
  return OK;
}

Status BTreeFile::insert_helper(int curr_level, PageId curPid, 
                                const void *key, const RID rid) {
  Status rc = FAIL, final_rc = FAIL;
  RID curRid;

  // HANDLING BTLEAF_PAGE --------------------------------------
  if(curr_level == 0) {
    BTLeafPage *page;
    // we pin the leaf page,
    rc = MINIBASE_BM->pinPage(curPid, (Page *&)page, FALSE);
    assert(rc == OK);

    final_rc = page->insertRec((void *) key, header->keyType, rid, curRid);
    //then unpin our btleaf_page and return our status

    rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
    assert(rc == OK);
    
    return final_rc;
  }

  // HANDLING BTINDEX_PAGE -------------------------------------
  PageId nextPid;
  BTIndexPage *page = NULL;

  rc = MINIBASE_BM->pinPage(curPid, (Page *&)page, FALSE);
  assert(rc == OK);

  rc = page->get_page_no(key, header->keyType, nextPid);
  assert(rc == OK);
  final_rc = insert_helper(curr_level - 1, nextPid, key, rid);
  // if we were unable to insert aka full, attempt to redistribute
  if(final_rc == DONE) {
    if(page->available_space() >= keysize()) {
      rc = split(curr_level, curPid, nextPid);
      assert(rc == OK);
      final_rc = insert_helper(curr_level, curPid, key, rid);
    } 
  }
  rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
  assert(rc == OK);

  return final_rc;
}


Status BTreeFile::Delete(const void *key, const RID rid) {
  // just delete, no need to recombine (thank goodness!)
  Status rc = delete_helper(header->height, header->rootPid, key, rid);

  return rc;
}

Status BTreeFile::delete_helper(int curr_level, PageId curPid, const void *key, const RID rid) {
  Status rc = FAIL, final_rc = DONE;
  RID curRid;

  // HANDLING BTLEAF_PAGE --------------------------------------
  if(curr_level == 0) {
    BTLeafPage *page;
    // we pin the leaf page,
    rc = MINIBASE_BM->pinPage(curPid, (Page *&)page, FALSE);
    assert(rc == OK);

    //then attempt to find the record & delete it
    if(header->keyType == attrInteger) {
      rc = page->get_entry_rid((void *)key, header->keyType, curRid);
    } else {
      RID dataRid;
      void *other_key = (void *)malloc(keysize()); 

      assert(rc == OK);
      rc = page->get_first(curRid, other_key, dataRid);
      while(rc == OK) {
        if(strcmp((char *)key, (char *)other_key) == 0) {
          break;
        } 
        memset(other_key, 0, keysize());
        rc = page->get_next(curRid, other_key, dataRid);
        assert(rc != FAIL);
      }
      free(other_key);
    }
    assert(rc != FAIL);
    if(rc == OK)
      final_rc = page->deleteRecord(curRid);

    //finally, unpin our btleaf_page and return our status
    rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
    assert(rc == OK);

    return final_rc;
  }

  // HANDLING BTINDEX_PAGE -------------------------------------
  PageId nextPid = INVALID_PAGE;
  BTIndexPage *page = NULL;

  rc = MINIBASE_BM->pinPage(curPid, (Page *&)page, FALSE);
  assert(rc == OK);

  rc = page->get_page_no(key, header->keyType, nextPid);
  assert(rc == OK);

  final_rc = delete_helper(curr_level - 1, nextPid, key, rid);

  rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
  assert(rc == OK);

  return final_rc;
}



Status BTreeFile::search(int curr_level, PageId pid, const void *key, void *curr_key, RID &rid, RID &dataRid) {
  Status rc = FAIL, final_rc = DONE;
  memset(curr_key, 0, keysize());

  // HANDLING BTLEAF_PAGE --------------------------------------
  if(curr_level == 0) {
    BTLeafPage *page;
    // we pin the leaf page,
    rc = MINIBASE_BM->pinPage(pid, (Page *&)page, FALSE);
    assert(rc == OK);

    //then attempt to find the record
    rc = page->get_first(rid, curr_key, dataRid);
    if(rc != OK) {
      return rc;
    }

    while(rc == OK && keyCompare((const void*) curr_key, key, header->keyType) < 0) {
      memset(curr_key, 0, keysize());
      rc = page->get_next(rid, curr_key, dataRid);
    }

    if(rc != OK) {
      //finally, unpin our btleaf_page and return our status
      rc = MINIBASE_BM->unpinPage(pid, TRUE, TRUE);
      assert(rc == OK);
      return DONE;
    }


    //finally, unpin our btleaf_page and return our status
    rc = MINIBASE_BM->unpinPage(pid, TRUE, TRUE);
    assert(rc == OK);

    return rc;
  }

  // HANDLING BTINDEX_PAGE -------------------------------------
  PageId nextPid;
  BTIndexPage *page = NULL;

  rc = MINIBASE_BM->pinPage(pid, (Page *&)page, FALSE);
  assert(rc == OK);

  rc = page->get_page_no(key, header->keyType, nextPid);
  assert(rc == OK);
  final_rc = search(curr_level - 1, nextPid, key, curr_key, rid, dataRid);

  rc = MINIBASE_BM->unpinPage(pid, TRUE, TRUE);
  assert(rc == OK);

  return final_rc;
}

    
IndexFileScan *BTreeFile::new_scan(const void *lo_key, const void *hi_key) {
  BTreeFileScan *scan = new BTreeFileScan();
  scan->keySize = keysize();
  scan->scanComplete = FALSE;
  scan->keyType = header->keyType;
  Status rc = FAIL;
  BTLeafPage *leafPage = NULL;
  BTIndexPage *indPage = rootPage;
  scan->curr_key = (void *)malloc(sizeof(keysize()));
  memset(scan->curr_key, 0, keysize());


  // do i need to malloc and memcpy these?
  scan->low_key = (void *) lo_key;
  scan->high_key = (void *) hi_key;

  // if null, start at leftmost vertex
  if(lo_key == NULL) {
    int curr_level = header->height - 1;
    PageId curPid = rootPage->getLeftLink(), nextPid = INVALID_PAGE; 

    while(curr_level > 0) {
      rc = MINIBASE_BM->pinPage(curPid, (Page *&) indPage, FALSE);
      assert(rc == OK);
      nextPid = indPage->getLeftLink();
      rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
      assert(rc == OK);
      curPid = nextPid;
      curr_level--;
    }

    // then we grab the first record.
    Status final_rc = DONE;
    while(final_rc != OK) {
      rc = MINIBASE_BM->pinPage(curPid, (Page *&) leafPage, FALSE);
      assert(rc == OK);
      final_rc = leafPage->get_first(scan->curRid, scan->curr_key, scan->dataRid);
      assert(final_rc != FAIL);
      curPid = leafPage->getNextPage();
      rc = MINIBASE_BM->unpinPage(curPid, TRUE, TRUE);
      assert(rc == OK);
      if(curPid == INVALID_PAGE)
        return NULL;
    }
  } else {
    memset(scan->curr_key, 0, keysize());
    rc = search(header->height, header->rootPid, lo_key, scan->curr_key, scan->curRid, scan->dataRid);
    if(rc != OK) {
      scan->scanComplete = true;
    }
  }

  scan->curPid = (scan->curRid).pageNo;
  rc = MINIBASE_BM->pinPage(scan->curPid, (Page *&) scan->curPage, FALSE);
  assert(rc == OK);

  return scan;
}

int BTreeFile::keysize(){
  return header->keySize;
}


void BTreeFile::debugPrivateVars() {
  cout << "PRIVATE VARS ------------" << endl;
  cout << "Filename: " << fileName << endl;
  cout << "headerPid: " << header->headerPid << endl;
  cout << "rootPid: " << header->rootPid << endl;
  cout << "keyType: " << header->keyType << endl;
  cout << "keySize: " << header->keySize << endl;
  cout << "height: " << header->height << endl;
  cout << "-------------------------" << endl;
}

void BTreeFile::debugPage(int curr_level, PageId pid) {
  RID curRid, dataRid;
  cout << "DEBUGGING PAGE " << pid << endl;
  if(curr_level == 0) {
    BTLeafPage *page = NULL;
    void *key = malloc(keysize()); 
    // we have a leaf page
    Status rc = MINIBASE_BM->pinPage(pid, (Page *&)page, FALSE);
    assert(rc == OK);
    cout << "page records total to: " << page->numberOfRecords() << endl;
    rc = page->get_first(curRid, key, dataRid);
    while(rc == OK) {
      if(header->keyType == attrInteger) {
        cout << "\tKey: " << *(int *)key  << ", RID page slot:" << dataRid.pageNo << " " << dataRid.slotNo << endl;
      } else {
        cout << "\tKey: " << (char *)key  << ", RID page slot:" << dataRid.pageNo << " " << dataRid.slotNo << endl;
        memset(key, 0, keysize());
      }
      rc = page->get_next(curRid, key, dataRid);
      assert(rc != FAIL);
    }
    rc = MINIBASE_BM->unpinPage(pid, TRUE, TRUE);
    assert(rc == OK);
    free(key);
  } else {
    BTIndexPage *page = NULL;
    PageId curPid = INVALID_PAGE;
    void *key = malloc(keysize()); 
    // we have a leaf page
    Status rc = MINIBASE_BM->pinPage(pid, (Page *&)page, FALSE);
    assert(rc == OK);
    cout << "page records total to: " << page->numberOfRecords() << endl;
    rc = page->get_first(curRid, key, curPid);
    cout << "Left link: " << page->getLeftLink() << endl;
    while(rc == OK) {
      if(header->keyType == attrInteger) {
        cout << "\tKey: " << *(int *)key  << ", next page:" << curPid << endl;
      } else {
        cout << "\tKey: " << (char *)key  << ", nextPage:" << curPid << endl;
        memset(key, 0, keysize());
      }
      rc = page->get_next(curRid, key, curPid);
      assert(rc != FAIL);
    }
    rc = MINIBASE_BM->unpinPage(pid, TRUE, TRUE);
    assert(rc == OK);
    free(key);  
  }
}
