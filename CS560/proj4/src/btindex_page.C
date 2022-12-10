/*
 * btindex_page.C - implementation of class BTIndexPage
 *
 * Johannes Gehrke & Gideon Glass  951016  CS564  UW-Madison
 * Edited by Young-K. Suh (yksuh@cs.arizona.edu) 03/27/14 CS560 Database Systems Implementation 
 */

#include "btindex_page.h"

// Define your Error Messge here
const char* BTIndexErrorMsgs[] = {
  //Possbile error messages,
  //OK,
  //Record Insertion Failure,
};

static error_string_table btree_table(BTINDEXPAGE, BTIndexErrorMsgs);

Status BTIndexPage::insertKey (const void *key,
                               AttrType key_type,
                               PageId pageNo,
                               RID& rid)
{
  KeyDataEntry target;
  Datatype data;
  data.pageNo = pageNo;
  int target_length;

  make_entry(&target, key_type, key, (nodetype)type, data, &target_length);
  Status rc = SortedPage::insertRecord(key_type, (char *)&target, target_length, rid);
  return rc;
}

Status BTIndexPage::deleteKey (const void *key, AttrType key_type, RID& curRid)
{
  PageId curPage;
  Keytype *curkey = new Keytype;
  Status rc = get_first(curRid, curkey, curPage);
  assert(rc == OK);

  while (keyCompare(key, curkey, key_type) > 0)
  {
    rc = get_next(curRid, curkey, curPage);
    if (rc != OK)
      break;
  }
  return deleteRecord(curRid);
}

Status BTIndexPage::get_page_no(const void *key,
                                AttrType key_type,
                                PageId & pageNo)
{
  pageNo = getLeftLink();
  Keytype *ckey = new Keytype;
  PageId pageId;
  RID rid;
  Status rc = get_first(rid, ckey, pageId);
  if(rc != OK)
    return rc;
  while (rc == OK && keyCompare(key, ckey, key_type) >= 0) {
    pageNo = pageId;
    rc = get_next(rid, ckey, pageId);
  }

  return OK;
}

    
Status BTIndexPage::get_first(RID& rid,
                              void *key,
                              PageId & pageNo)
{
  Status rc = HFPage::firstRecord(rid);
  if(rc != OK)
    return rc;
  int record_length;
  Datatype data;

  char *record = (char *)malloc(sizeof(KeyDataEntry));
  rc = HFPage::getRecord(rid, record, record_length);


  get_key_data(key, &data, (KeyDataEntry *)record, record_length, (nodetype)type);
  pageNo = data.pageNo;

  return OK;
}

Status BTIndexPage::get_next(RID& rid, void *key, PageId & pageNo)
{
  
	RID next_rid;
	Datatype data;

	Status rc = nextRecord(rid, next_rid);
	if (rc != OK)
		return NOMORERECS;

	rid = next_rid;
	char *record = (char *)malloc(sizeof(KeyDataEntry));
	int record_length;
	rc = getRecord(rid, record, record_length);
	if (rc != OK)
		return rc;

    get_key_data(key, &data, (KeyDataEntry *)record, record_length, (nodetype)type);
    pageNo = data.pageNo;

	return OK;
}
