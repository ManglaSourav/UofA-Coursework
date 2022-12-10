/*
 * btleaf_page.C - implementation of class BTLeafPage
 *
 * Johannes Gehrke & Gideon Glass  951016  CS564  UW-Madison
 * Edited by Young-K. Suh (yksuh@cs.arizona.edu) 03/27/14 CS560 Database Systems Implementation 
 */
#include <iostream>
#include <stdlib.h>
#include <memory.h>
#include "btleaf_page.h"
const char *BTLeafErrorMsgs[] = {
    // OK,
    // Insert Record Failed,
};
static error_string_table btree_table(BTLEAFPAGE, BTLeafErrorMsgs);

/*
 * Status BTLeafPage::insertRec(const void *key,
 *                             AttrType key_type,
 *                             RID dataRid,
 *                             RID& rid)
 *
 * Inserts a key, rid value into the leaf node. This is
 * accomplished by a call to SortedPage::insertRecord()
 * The function also sets up the recPtr field for the call
 * to SortedPage::insertRecord() 
 * 
 * Parameters:
 *   o key - the key value of the data record.
 *
 *   o key_type - the type of the key.
 * 
 *   o dataRid - the rid of the data record. This is
 *               stored on the leaf page along with the
 *               corresponding key value.
 *
 *   o rid - the rid of the inserted leaf record data entry.
 */

Status BTLeafPage::insertRec(const void *key,
                             AttrType key_type,
                             RID dataRid,
                             RID &rid)
{
  KeyDataEntry target;
  Datatype data;
  data.rid = dataRid;
  int target_length;

  make_entry(&target, key_type, key, (nodetype)type, data, &target_length);
  Status rc = SortedPage::insertRecord(key_type, (char *)&target, target_length, rid);

  return rc;
}

/*
 *
 * Status BTLeafPage::get_data_rid(const void *key,
 *                                 AttrType key_type,
 *                                 RID & dataRid)
 *
 * This function performs a binary search to look for the
 * rid of the data record. (dataRid contains the RID of
 * the DATA record, NOT the rid of the data entry!)
 */

Status BTLeafPage::get_data_rid(void *key,
                                AttrType key_type,
                                RID &dataRid)
{
  int difference;
  Keytype k;
  //int u = slotCnt - 1;
  //int m;
  //int l = 0;
  /*
  while (l <= u)
  {
    m = (l + u) / 2;

    difference = keyCompare(key, (void *)(data + slot[m].offset), key_type);

    if (difference == 0)
    {
      get_key_data((void *)&k, (Datatype *)&dataRid, (KeyDataEntry *)(data + slot[m].offset), slot[m].length, (nodetype)type);
      return OK;
    }
    else if (difference > 0)
      u = m - 1;
    else if (difference < 0)
      u = m + 1;
  }
  */
  for(int i = 0; i < slotCnt; ++i) {
    if(slot[i].length != EMPTY_SLOT) {
      difference = keyCompare(key, (void *)(&data[slot[i].offset]), key_type);
      if (difference == 0) {
        get_key_data((void *)&k, (Datatype *)&dataRid, (KeyDataEntry *)(&data[slot[i].offset]), slot[i].length, (nodetype)type);
        return OK;
      }
    }
  }
  

  return FAIL;
}

/* 
 * Status BTLeafPage::get_first (const void *key, RID & dataRid)
 * Status BTLeafPage::get_next (const void *key, RID & dataRid)
 * 
 * These functions provide an
 * iterator interface to the records on a BTLeafPage.
 * get_first returns the first key, RID from the page,
 * while get_next returns the next key on the page.
 * These functions make calls to RecordPage::get_first() and
 * RecordPage::get_next(), and break the flat record into its
 * two components: namely, the key and datarid. 
 */
Status BTLeafPage::get_first(RID &rid,
                             void *key,
                             RID &dataRid)
{
  Datatype data;
  char *record = (char *)malloc(sizeof(KeyDataEntry));

  int record_length;

  Status rc = firstRecord(rid);
  assert(rc != FAIL);
  if(rc != OK) {
    free(record);
    return rc;
  }

  rc = getRecord(rid, record, record_length);
  assert(rc != FAIL);

  if(rc != OK) {
    free(record);
    return rc;
  }

  get_key_data(key, &data, (KeyDataEntry *)record, record_length, (nodetype)type);
  dataRid = data.rid;

  free(record);

  return OK;
}

Status BTLeafPage::get_next(RID &rid,
                            void *key,
                            RID &dataRid)
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

  if (rc != OK) {
    free(record);
    return rc;
  }

  get_key_data(key, &data, (KeyDataEntry *)record, record_length, (nodetype)type);
  dataRid = data.rid;

  free(record);

  return OK;
}




/*
 *
 * Status BTLeafPage::get_data_rid(const void *key,
 *                                 AttrType key_type,
 *                                 RID & entryRid)
 *
 * This function performs a binary search to look for the
 * rid of the data entry.
 */

Status BTLeafPage::get_entry_rid(void *key,
                                AttrType key_type,
                                RID &entryRid)
{
  int difference = 1;

  for(int i = 0; i < slotCnt; ++i) {
    if(slot[i].length != EMPTY_SLOT) {
      difference = keyCompare(key, (void *)(&data[slot[i].offset]), key_type);
      if (difference == 0) {
        entryRid.pageNo = curPage;
        entryRid.slotNo = i;
        return OK;
      }
    }
  }

  return FAIL;
}
