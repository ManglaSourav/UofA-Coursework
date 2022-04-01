/*
 * sorted_page.C - implementation of class SortedPage
 *
 * Johannes Gehrke & Gideon Glass  951016  CS564  UW-Madison
 * Edited by Young-K. Suh (yksuh@cs.arizona.edu) 03/27/14 CS560 Database Systems Implementation 
 */

#include <iostream>
#include <stdlib.h>
#include <memory.h>

#include "sorted_page.h"
#include "btindex_page.h"
#include "btleaf_page.h"

const char* SortedPage::Errors[SortedPage::NR_ERRORS] = {
  //OK,
  //Insert Record Failed (SortedPage::insertRecord),
  //Delete Record Failed (SortedPage::deleteRecord,
};


/*
 *  Status SortedPage::insertRecord(AttrType key_type, 
 *                                  char *recPtr,
 *                                    int recLen, RID& rid)
 *
 * Performs a sorted insertion of a record on an record page. The records are
 * sorted in increasing key order.
 * Only the  slot  directory is  rearranged.  The  data records remain in
 * the same positions on the  page.
 *  Parameters:
 *    o key_type - the type of the key.
 *    o recPtr points to the actual record that will be placed on the page
 *            (So, recPtr is the combination of the key and the other data
 *       value(s)).
 *    o recLen is the length of the record to be inserted.
 *    o rid is the record id of the record inserted.
 */

Status SortedPage::insertRecord (AttrType key_type,
                                 char * recPtr,
                                 int recLen,
                                 RID& rid)
{
    if (recLen > available_space())
        return DONE;

    int i = 0, empty_ind = -1; 

    //we try to find the first empty slot
    for (i = 0; i < slotCnt; ++i) {
      if (slot[i].length == EMPTY_SLOT) {
        empty_ind = i;
      } else if(key_type == attrInteger || key_type == attrString) {
        if(keyCompare(recPtr, (void *) &data[slot[i].offset], key_type) < 0) 
          break;
      } else {
        cout << "Key was not an Integer or String, not sure what to do" << endl;
        return FAIL;
      }
    }

    // edge case for the very beginning
    if(slotCnt == 1 && empty_ind == 0) {
        i--;
    }


    if(empty_ind == -1) {
      for(int j = i + 1; j < slotCnt; ++j) {
        if (slot[j].length == EMPTY_SLOT) {
          empty_ind = j;
          break;
        }
      }
      if(empty_ind == -1) {
        empty_ind = slotCnt;
        if ((int) (recLen + sizeof(slot_t)) > available_space()) {
          return DONE;
        }
        freeSpace -= sizeof(slot_t);
        slotCnt++;        
      }
    }

    if(empty_ind < i) {
        i--;
        for(int j = empty_ind; j < i; ++j) {
            memmove(&slot[j], &slot[j+1], sizeof(slot_t));
        }
    } else if(empty_ind > i) {
        for(int j = empty_ind; j > i; --j) {
            memmove(&slot[j], &slot[j-1], sizeof(slot_t));
        }
    } else { //this means empty_ind == i aka edge case
        freeSpace -= recLen;
        usedPtr -= recLen;
        slot[i].offset = usedPtr;
        slot[i].length = recLen;
        memset(&data[usedPtr], 0, recLen);
        memcpy(&data[usedPtr], recPtr, recLen);
        rid.pageNo = curPage;
        rid.slotNo = i;
        return OK;
    }

    freeSpace -= recLen;
    usedPtr -= recLen;
    slot[i].offset = usedPtr;
    slot[i].length = recLen;

    //insert new record here
    memset(&data[usedPtr], 0, recLen);
    memcpy(&data[usedPtr], recPtr, recLen);
    rid.pageNo = curPage;
    rid.slotNo = i;

    return OK;
}


/*
 * Status SortedPage::deleteRecord (const RID& rid)
 *
 * Deletes a record from a sorted record page. It just calls
 * HFPage::deleteRecord().
 */

Status SortedPage::deleteRecord (const RID& rid)
{
  return HFPage::deleteRecord(rid);
}

int SortedPage::numberOfRecords()
{ 
  int count = 0;
  for(int i = 0; i < slotCnt; ++i) {
    if(slot[i].length != EMPTY_SLOT)
      count++;
  }
  return count;
}
