/*=============================================================================
 |   Assignment:  Project 3 
 |       Author:  Kinsleigh Wong, Sourav Mangla
 |       NetIDs:  kinsleighwong, souravmangla
 |
 |       Course:  CSC 560
 |   Instructor:  Richard Snodgrass
 |     Due Date:  10/23/2021, 11:59pm
 *===========================================================================*/

#include <iostream>
#include <stdlib.h>
#include <memory.h>

#include "hfpage.h"
#include "buf.h"
#include "db.h"

// **********************************************************
// page class constructor

void HFPage::init(PageId pageNo)
{
    slotCnt = 1;
    usedPtr = MAX_SPACE - DPFIXED; //end of data array
    freeSpace = MAX_SPACE - DPFIXED;

    prevPage = INVALID_PAGE;
    nextPage = INVALID_PAGE;
    curPage = pageNo;

    // zero things out to be safe
    slot[0].offset = 0;
    slot[0].length = EMPTY_SLOT;

    memset(data, 0, sizeof(char) * (MAX_SPACE - DPFIXED));
}

// **********************************************************
// dump page utlity
void HFPage::dumpPage()
{
    int i;

    cout << "dumpPage, this: " << this << endl;
    cout << "curPage= " << curPage << ", nextPage=" << nextPage << endl;
    cout << "usedPtr=" << usedPtr << ",  freeSpace=" << freeSpace
         << ", slotCnt=" << slotCnt << endl;

    for (i = 0; i < slotCnt; i++)
    {
        cout << "slot[" << i << "].offset=" << slot[i].offset
             << ", slot[" << i << "].length=" << slot[i].length << endl;
        cout << (char *) &data[slot[i].offset] << endl;
    }
}

// **********************************************************
PageId HFPage::getPrevPage()
{
    return prevPage;
}

// **********************************************************
void HFPage::setPrevPage(PageId pageNo)
{
    prevPage = pageNo;
}

// **********************************************************
PageId HFPage::getNextPage()
{
    return nextPage;
}

// **********************************************************
void HFPage::setNextPage(PageId pageNo)
{
    nextPage = pageNo;
}

// **********************************************************
// Add a new record to the page. Returns OK if everything went OK
// otherwise, returns DONE if sufficient space does not exist
// RID of the new record is returned via rid parameter.
Status HFPage::insertRecord(char *recPtr, int recLen, RID &rid)
{
    if (recLen > available_space())
        return DONE;

    int i = 0;
    //we try to find the first empty slot
    for (i = 0; i < slotCnt; ++i)
    {
        if (slot[i].length == EMPTY_SLOT)
            break;
    }
    if (i == slotCnt)
    {
        if ((int) (recLen + sizeof(slot_t)) > available_space())
            return DONE;
        freeSpace -= sizeof(slot_t);
        slotCnt++;
    }

    freeSpace -= recLen;
    usedPtr -= recLen;

    slot[i].offset = usedPtr;
    slot[i].length = recLen;

    //insert new record here
    memcpy(&data[usedPtr], recPtr, recLen);
    rid.pageNo = curPage;
    rid.slotNo = i;

    return OK;
}

// **********************************************************
// Delete a record from a page. Returns OK if everything went okay.
// Compacts remaining records but leaves a hole in the slot array.
// Use memmove() rather than memcpy() as space may overlap.
Status HFPage::deleteRecord(const RID &rid) {
    if (rid.pageNo != curPage)
        return FAIL;
    if (rid.slotNo >= slotCnt || rid.slotNo < 0)
        return FAIL;
    if (slot[rid.slotNo].length == EMPTY_SLOT)
        return FAIL;

    short r_offset = slot[rid.slotNo].offset;
    short r_length = slot[rid.slotNo].length;

    short num = r_offset - usedPtr;
    short oldPtr = usedPtr;
    usedPtr += r_length;

    memmove(&data[usedPtr], &data[oldPtr], num);
    for (int i = 0; i < slotCnt; ++i)
    {
        if (slot[i].offset < r_offset)
        {
            slot[i].offset += r_length;
        }
    }

    freeSpace += r_length;
    slot[rid.slotNo].length = EMPTY_SLOT;

    if (rid.slotNo == slotCnt - 1) {
        // greater than zero to avoid removing slot 0
        for (int i = slotCnt - 1; i > 0; --i)
        {
            if (slot[i].length == EMPTY_SLOT)
            {
                freeSpace += sizeof(slot_t);
                slotCnt--;
            }
            else
            {
                break;
            }
        }
    }

    return OK;
}

// **********************************************************
// returns RID of first record on page
Status HFPage::firstRecord(RID &firstRid)
{
    int i = 0;
    for (i = 0; i < slotCnt; ++i)
    {
        if (slot[i].length != EMPTY_SLOT)
        {
            firstRid.pageNo = curPage;
            firstRid.slotNo = i;
            return OK;
        }
    }

    return DONE;
}

// **********************************************************
// returns RID of next record on the page
// returns DONE if no more records exist on the page; otherwise OK
Status HFPage::nextRecord(RID curRid, RID &nextRid)
{
    if (curRid.pageNo != curPage)
        return FAIL;
    if (curRid.slotNo >= slotCnt || curRid.slotNo < 0)
        return FAIL;

    int i = curRid.slotNo + 1;
    for (i = curRid.slotNo + 1; i < slotCnt; ++i)
    {
        if (slot[i].length != EMPTY_SLOT)
        {
            nextRid.pageNo = curPage;
            nextRid.slotNo = i;
            return OK;
        }
    }

    return DONE;
}

// **********************************************************
// returns length and copies out record with RID rid
Status HFPage::getRecord(RID rid, char *recPtr, int &recLen)
{
    if (rid.pageNo != curPage)
        return FAIL;
    if (rid.slotNo >= slotCnt || rid.slotNo < 0)
        return FAIL;
    if (slot[rid.slotNo].length == EMPTY_SLOT)
        return FAIL;

    recLen = slot[rid.slotNo].length;
    memcpy(recPtr, &data[slot[rid.slotNo].offset], recLen);

    //need to find when to return FAIL, check memcpy return maybe?

    return OK;
}

// **********************************************************
// returns length and pointer to record with RID rid.  The difference
// between this and getRecord is that getRecord copies out the record
// into recPtr, while this function returns a pointer to the record
// in recPtr.
Status HFPage::returnRecord(RID rid, char *&recPtr, int &recLen)
{
    if (rid.pageNo != curPage)
        return FAIL;
    if (rid.slotNo >= slotCnt || rid.slotNo < 0)
        return FAIL;
    if (slot[rid.slotNo].length == EMPTY_SLOT)
        return FAIL;

    recLen = slot[rid.slotNo].length;
    recPtr = &data[slot[rid.slotNo].offset];

    return OK;
}

// **********************************************************
// Returns the amount of available space on the heap file page
int HFPage::available_space(void)
{
    //we try to find the first empty
    int i = 0;
    for (i = 0; i < slotCnt; ++i)
    {
        if (slot[i].length == EMPTY_SLOT)
        {
            return freeSpace;
        }
    }

    //return freeSpace - sizeof(slot_t) * slotCnt;
    return freeSpace - sizeof(slot_t);
}

// **********************************************************
// Returns 1 if the HFPage is empty, and 0 otherwise.
// It scans the slot directory looking for a non-empty slot.
bool HFPage::empty(void)
{
    int i = 0;
    for (i = 0; i < slotCnt; ++i)
    {
        if (slot[i].length != EMPTY_SLOT)
        {
            return false;
        }
    }

    return true;
}
