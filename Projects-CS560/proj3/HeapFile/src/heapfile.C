/*=============================================================================
 |   Assignment:  Project 3
 |       Author:  Kinsleigh Wong, Sourav Mangla
 |       NetIDs:  kinsleighwong, souravmangla
 |
 |       Course:  CSC 560
 |   Instructor:  Richard Snodgrass
 |     Due Date:  10/1/2021, 11:59pm
 *===========================================================================*/

#include "heapfile.h"

Status insertIntoPage(char *fileName, PageId pageId, char *recPtr, int recLen, RID &rid, PageId prev = INVALID_PAGE, DataPageInfo *dpi = NULL)
{
    Status rec_rc = FAIL;
    Page *page = NULL;
    HFPage *hfp = NULL;

    //pin data page
    rec_rc = MINIBASE_BM->pinPage(pageId, page, FALSE, fileName);
    assert(rec_rc == OK);
    hfp = (HFPage *)page;
    rec_rc = hfp->insertRecord(recPtr, recLen, rid);
    assert(rec_rc == OK);

    if (prev != INVALID_PAGE)
    {
        hfp->setPrevPage(prev);
    }
    if (dpi != NULL)
    {
        //cout << "Modifying available space on page " << pageId << " from " << dpi->availspace << " to " << hfp->available_space() << endl;
        dpi->availspace = hfp->available_space();
    }

    rec_rc = MINIBASE_BM->unpinPage(pageId, TRUE, fileName); // pin data page
    assert(rec_rc == OK);

    if (prev != INVALID_PAGE)
    {
        //cout << "Spaghetti" << prev << endl;
        rec_rc = MINIBASE_BM->pinPage(prev, page, FALSE, fileName);
        assert(rec_rc == OK);
        hfp = (HFPage *)page;
        hfp->setNextPage(pageId);
        rec_rc = MINIBASE_BM->unpinPage(prev, TRUE, fileName); // pin data page
        assert(rec_rc == OK);
    }

    return OK;
}

// ******************************************************
// Error messages for the heapfile layer

static const char *hfErrMsgs[] = {
    "bad record id",
    "bad record pointer",
    "end of file encountered",
    "invalid update operation",
    "no space on page for record",
    "page is empty - no records",
    "last record on page",
    "invalid slot number",
    "file has already been deleted",
};

static error_string_table hfTable(HEAPFILE, hfErrMsgs);

// ********************************************************
// Constructor
HeapFile::HeapFile(const char *name, Status &returnStatus)
{
    PageId val = INVALID_PAGE;
    Status rc = FAIL;
    DataPageInfo dpi = {.availspace = -1, .recct = -1, .pageId = INVALID_PAGE};
    RID dataPageRid = {.pageNo = INVALID_PAGE, .slotNo = -1};
    //printf("%s, %d\n", name, strlen(name));
    //printf("Construction:  %d\n", MINIBASE_DB->get_file_entry(name, val));

    fileName = (char *)malloc(sizeof(name));
    strcpy(fileName, name);

    if (MINIBASE_DB->get_file_entry(name, val) == FAIL)
    {
        rc = newDataPage(&dpi);
        assert(rc == OK);
        rc = allocateDirSpace(&dpi, firstDirPageId, dataPageRid);
        assert(rc == OK);
        rc = MINIBASE_DB->add_file_entry(fileName, firstDirPageId);
        assert(rc == OK);
    }
    else
    {
        firstDirPageId = val;
    }

    file_deleted = false;

    returnStatus = OK;
}

// ******************
// Destructor
HeapFile::~HeapFile()
{
}

// *************************************
// Return number of records in heap file
int HeapFile::getRecCnt()
{
    Status page_rc = FAIL, rec_rc = FAIL;
    Page *page = NULL;
    int rec_cnt = 0, rec_len;
    HFPage *hfp = NULL;
    PageId nextPage = -1, curPage = firstDirPageId;
    RID curRid;
    struct DataPageInfo curInfo;

    page_rc = MINIBASE_BM->pinPage(firstDirPageId, page, false, fileName);
    hfp = (HFPage *)page;

    //cout << hfp->page_no() << " getRecCnt, next page: " << hfp->getNextPage() << endl;

    while (page_rc == OK)
    { //not sure what this is supposed to return
        hfp = (HFPage *)page;

        rec_rc = hfp->firstRecord(curRid);

        while (rec_rc == OK)
        {
            rec_rc = hfp->getRecord(curRid, (char *)&curInfo, rec_len);
            assert(rec_rc == OK);
            assert(rec_len == sizeof(struct DataPageInfo));

            rec_cnt += curInfo.recct;
            rec_rc = hfp->nextRecord(curRid, curRid);
        }
        nextPage = hfp->getNextPage();
        // we unpin the current page then pin the next page
        page_rc = MINIBASE_BM->unpinPage(curPage, FALSE, fileName);
        assert(page_rc == OK);

        if (nextPage == INVALID_PAGE)
            break;

        page_rc = MINIBASE_BM->pinPage(nextPage, page, false, fileName);
        assert(page_rc == OK);

        curPage = nextPage;
    }

    return rec_cnt;
}

// *****************************
// Insert a record into the file
Status HeapFile::insertRecord(char *recPtr, int recLen, RID &outRid)
{
    if (recLen >= MINIBASE_PAGESIZE)
    {
        return MINIBASE_FIRST_ERROR(HEAPFILE, NO_SPACE);
    }

    Status rec_rc = FAIL, dir_rc = FAIL;
    Page *page = NULL;
    PageId curDirPid = firstDirPageId, nextDirPid = INVALID_PAGE, pastDataPid = INVALID_PAGE;
    RID curDataRid;
    DataPageInfo curInfo;
    int entry_len = -1;

    // read first directory page in
    dir_rc = MINIBASE_BM->pinPage(curDirPid, page, false, fileName);
    if (dir_rc != OK)
    {
        MINIBASE_BM->unpinPage(curDirPid, false, fileName);
        return FAIL;
    }

    HFPage *dir_page = (HFPage *)page;

    while (dir_rc == OK)
    {
        rec_rc = dir_page->firstRecord(curDataRid);

        // check every existing record in dir_page
        while (rec_rc == OK)
        {
            rec_rc = dir_page->getRecord(curDataRid, (char *)&curInfo, entry_len);
            assert(entry_len == sizeof(DataPageInfo));

            if (curInfo.availspace >= recLen)
            {
                // write out to data page
                dir_rc = insertIntoPage(fileName, curInfo.pageId, recPtr, recLen, outRid, INVALID_PAGE, &curInfo);
                assert(dir_rc == OK);

                //update and unpin directory page
                curInfo.recct += 1;

                dir_rc = dir_page->deleteRecord(curDataRid);
                assert(dir_rc == OK); //might not always be OK
                dir_rc = dir_page->insertRecord((char *)&curInfo, sizeof(curInfo), curDataRid);
                assert(dir_rc == OK); //told it was okay to assume that everything would fit

                //unpin directory page and write it out
                dir_rc = MINIBASE_BM->unpinPage(curDirPid, TRUE, fileName);
                assert(dir_rc == OK);
                return OK;
            }
            pastDataPid = curInfo.pageId;
            rec_rc = dir_page->nextRecord(curDataRid, curDataRid);
        }

        // if we have room to add a data page,
        if ((long unsigned int)dir_page->available_space() >= sizeof(DataPageInfo) && dir_page->getNextPage() == INVALID_PAGE)
        {
            // create new DataPage and insert data into date page
            //cout << "Total records: " << curInfo.recct << endl;
            //printf("\n*********************** CREATING NEW DATA PAGE ***********************\n");
            rec_rc = newDataPage(&curInfo);
            assert(rec_rc == OK);
            curInfo.recct = 1;

            dir_rc = insertIntoPage(fileName, curInfo.pageId, recPtr, recLen, outRid, pastDataPid, &curInfo);
            //pastDataRid we need the curInfo equivalent
            assert(dir_rc == OK);

            // inserts into directory page
            rec_rc = dir_page->insertRecord((char *)&curInfo, sizeof(DataPageInfo), curDataRid);
            assert(rec_rc == OK);

            dir_rc = MINIBASE_BM->unpinPage(curDataRid.pageNo, TRUE, fileName);
            assert(dir_rc == OK);

            return OK;
        }

        nextDirPid = dir_page->getNextPage();

        if (nextDirPid == INVALID_PAGE)
            break;

        //cout << curDirPid << " has a next page of " << nextDirPid << endl;
        // we unpin the current page then pin the next page
        dir_rc = MINIBASE_BM->unpinPage(curDirPid, FALSE, fileName);
        assert(dir_rc == OK);

        dir_rc = MINIBASE_BM->pinPage(nextDirPid, page, FALSE, fileName);
        assert(dir_rc == OK);
        dir_page = (HFPage *)page;
        curDirPid = nextDirPid;
        //ON TO NEXT DIR PAGE " << curDirPid << endl;
    }

    // need to insert a new directory page
    //dir_page is the last page of the directory pages
    //cout << "Total records: " << curInfo.recct << endl;
    //cout << endl <<"******** NEW DIRECTORY PAGE ********" << endl;

    dir_rc = newDataPage(&curInfo);
    assert(dir_rc == OK);

    dir_rc = insertIntoPage(fileName, curInfo.pageId, recPtr, recLen, outRid, pastDataPid, &curInfo);
    assert(dir_rc == OK);
    curInfo.recct = 1;

    RID tempRid;
    dir_rc = allocateDirSpace(&curInfo, nextDirPid, tempRid);
    assert(dir_rc == OK);

    // we unpin the current page then pin the next page
    dir_page->setNextPage(nextDirPid);
    dir_rc = MINIBASE_BM->unpinPage(curDirPid, TRUE, fileName);
    assert(dir_rc == OK);

    dir_rc = MINIBASE_BM->pinPage(nextDirPid, page, FALSE, fileName);
    assert(dir_rc == OK);
    dir_page = (HFPage *)page;
    dir_page->setPrevPage(curDirPid);
    dir_rc = MINIBASE_BM->unpinPage(nextDirPid, TRUE, fileName);
    assert(dir_rc == OK);

    return OK;
}

// ***********************
// delete record from file
Status HeapFile::deleteRecord(const RID &rid)
{
    Status page_rc = FAIL, rec_rc = FAIL;
    Page *page = NULL;
    HFPage *dir_page = NULL, *data_page = NULL;
    PageId nextDirPid = INVALID_PAGE, curDirPid = firstDirPageId;
    RID curDirRid = {.pageNo = INVALID_PAGE, .slotNo = -1};
    DataPageInfo curInfo;
    int rec_len = -1;

    page_rc = MINIBASE_BM->pinPage(curDirPid, page); //, FALSE, fileName);
    assert(page_rc == OK);

    //dir_page = (HFPage *) page;
    //cout << "Deleting rid " << rid.pageNo << " " << rid.slotNo << endl;
    //cout << dir_page->page_no() << " Delete, next page: " << dir_page->getNextPage() << endl;

    while (page_rc == OK)
    {
        dir_page = (HFPage *)page;
        rec_rc = dir_page->firstRecord(curDirRid);

        while (rec_rc == OK)
        {
            rec_rc = dir_page->getRecord(curDirRid, (char *)&curInfo, rec_len);
            assert(rec_rc == OK);
            //cout << rec_len << " vs " << sizeof(DataPageInfo) << endl;
            assert(rec_len == sizeof(DataPageInfo));
            // we found the page with the record we want to delete
            if (rid.pageNo == curInfo.pageId)
            {
                // pin the data page to edit
                page_rc = MINIBASE_BM->pinPage(rid.pageNo, page, false, fileName);
                assert(page_rc == OK);
                data_page = (HFPage *)page;
                rec_rc = data_page->deleteRecord(rid);
                if (rec_rc == OK)
                {
                    curInfo.recct -= 1;
                    curInfo.availspace = data_page->available_space();
                }

                //cleanup pages
                page_rc = MINIBASE_BM->unpinPage(rid.pageNo, TRUE, fileName);
                assert(page_rc == OK);

                // need to update curInfo in the page.
                page_rc = MINIBASE_BM->unpinPage(curDirPid, TRUE, fileName);
                assert(page_rc == OK);

                return rec_rc;
            }
            rec_rc = dir_page->nextRecord(curDirRid, curDirRid);
        }
        nextDirPid = dir_page->getNextPage();
        if (nextDirPid == INVALID_PAGE)
            break;

        // we unpin the current page then pin the next page
        page_rc = MINIBASE_BM->unpinPage(curDirPid, FALSE, fileName);
        assert(page_rc == OK);
        page_rc = MINIBASE_BM->pinPage(nextDirPid, page, false, fileName);
        assert(page_rc == OK);
        curDirPid = nextDirPid;
    }

    page_rc = MINIBASE_BM->unpinPage(curDirPid, false, fileName);
    assert(page_rc == OK);

    return FAIL;
}

// *******************************************
// updates the specified record in the heapfile.
Status HeapFile::updateRecord(const RID &rid, char *recPtr, int recLen)
{
    PageId dirPageId = INVALID_PAGE, dataPageId = INVALID_PAGE;
    RID dataPageRid = {.pageNo = INVALID_PAGE, .slotNo = -1};
    HFPage *dirPage = NULL, *dataPage = NULL;
    Status rc = FAIL;
    char *writeLocation = (char *)malloc(recLen);
    int writeLocLength = -1;

    if (recLen >= MINIBASE_PAGESIZE)
    {
        return MINIBASE_FIRST_ERROR(HEAPFILE, INVALID_UPDATE);
    }
    //cout << "Before findDataPage: " << rid.pageNo << " " << rid.slotNo << endl;
    rc = findDataPage(rid, dirPageId, dirPage, dataPageId, dataPage, dataPageRid);
    assert(rc == OK);
    //printf("In updateRecord, before delete: %d %d\n", dataPageRid.pageNo, dataPageRid.slotNo);

    rc = dataPage->returnRecord(rid, writeLocation, writeLocLength);
    assert(rc == OK);

    if (recLen != writeLocLength)
    {
        rc = MINIBASE_BM->unpinPage(dirPageId, FALSE, fileName);
        assert(rc == OK);
        //write out modified data page
        rc = MINIBASE_BM->unpinPage(dataPageId, FALSE, fileName);
        assert(rc == OK);
        return MINIBASE_FIRST_ERROR(HEAPFILE, INVALID_UPDATE);
        /*
        //don't want to deal with this case now
        rc = dataPage->deleteRecord(rid);
        assert(rc == OK); //might not always be OK
        rc = dataPage->insertRecord(recPtr, recLen, dataPageRid);
        assert(rc == OK); //told it was okay to assume that everything would fit
    
        rc = MINIBASE_BM->unpinPage(dirPageId, TRUE, fileName);
        assert(rc == OK);    
        */
    }
    else
    {
        memcpy(writeLocation, recPtr, recLen);

        rc = MINIBASE_BM->unpinPage(dirPageId, FALSE, fileName);
        assert(rc == OK);
    }
    //write out modified data page
    rc = MINIBASE_BM->unpinPage(dataPageId, TRUE, fileName);
    assert(rc == OK);

    return OK;
}

// ***************************************************
// read record from file, returning pointer and length
Status HeapFile::getRecord(const RID &rid, char *recPtr, int &recLen)
{
    PageId dirPageId = INVALID_PAGE, dataPageId = INVALID_PAGE;
    RID dataPageRid = {.pageNo = INVALID_PAGE, .slotNo = -1};
    HFPage *dirPage = NULL, *dataPage = NULL;
    Status rc = FAIL;

    rc = findDataPage(rid, dirPageId, dirPage, dataPageId, dataPage, dataPageRid);
    assert(rc == OK);

    rc = dataPage->getRecord(rid, recPtr, recLen);
    assert(rc == OK); //might not always be OK

    rc = MINIBASE_BM->unpinPage(dirPageId, FALSE, fileName);
    assert(rc == OK);

    //write out modified data page
    rc = MINIBASE_BM->unpinPage(dataPageId, FALSE, fileName);
    assert(rc == OK);

    return OK;
}

// **************************
// initiate a sequential scan
Scan *HeapFile::openScan(Status &status)
{
    return new Scan(this, status);
}

// ****************************************************
// Wipes out the heapfile from the database permanently.
Status HeapFile::deleteFile()
{
    // fill in the body
    Status page_rc = FAIL, rec_rc = FAIL;

    page_rc = MINIBASE_DB->delete_file_entry(fileName);
    assert(page_rc == OK);

    Page *page = NULL;
    HFPage *hfp = NULL;
    int rec_len = -1;
    PageId nextDirPid = -1, curDirPid = firstDirPageId;
    RID curRid;
    struct DataPageInfo curInfo;

    page_rc = MINIBASE_BM->pinPage(curDirPid, page, false, fileName);

    while (page_rc == OK)
    {
        hfp = (HFPage *)page;

        rec_rc = hfp->firstRecord(curRid);

        while (rec_rc == OK)
        {
            rec_rc = hfp->getRecord(curRid, (char *)&curInfo, rec_len);
            assert(rec_rc == OK);
            assert(rec_len == sizeof(struct DataPageInfo));

            rec_rc = MINIBASE_DB->deallocate_page(curInfo.pageId);
            assert(rec_rc == OK);

            rec_rc = hfp->nextRecord(curRid, curRid);
        }
        nextDirPid = hfp->getNextPage();

        // we unpin the current page then deallocate it
        page_rc = MINIBASE_BM->unpinPage(curDirPid, FALSE, fileName);
        assert(page_rc == OK);
        rec_rc = MINIBASE_DB->deallocate_page(curDirPid);
        assert(rec_rc == OK);

        if (nextDirPid == INVALID_PAGE)
            break;

        page_rc = MINIBASE_BM->pinPage(nextDirPid, page, false, fileName);
        assert(page_rc == OK);

        curDirPid = nextDirPid;
    }

    return OK;
}

void printDPI(DataPageInfo *dpinfop)
{
    printf("availspace: %d\n", dpinfop->availspace);
    printf("recct: %d\n", dpinfop->recct);
    printf("pageId: %d\n", dpinfop->pageId);
}

// ****************************************************************
// Get a new datapage from the buffer manager and initialize dpinfo
// (Allocate pages in the db file via buffer manager)
Status HeapFile::newDataPage(DataPageInfo *dpinfop)
{
    Page *page = NULL;
    HFPage *hfp = NULL;

    Status page_rc = MINIBASE_BM->newPage(dpinfop->pageId, page, 1);
    assert(page_rc == OK);
    hfp = (HFPage *)page;

    hfp->init(dpinfop->pageId);

    dpinfop->availspace = hfp->available_space();
    dpinfop->recct = 0;

    page_rc = MINIBASE_BM->unpinPage(dpinfop->pageId, TRUE, fileName);
    assert(page_rc == OK);

    return OK;
}

// ************************************************************************
// Internal HeapFile function (used in getRecord and updateRecord): returns
// pinned directory page and pinned data page of the specified user record
// (rid).
//
// If the user record cannot be found, rpdirpage and rpdatapage are
// returned as NULL pointers.
//
Status HeapFile::findDataPage(const RID &rid,
                              PageId &rpDirPageId, HFPage *&rpdirpage,
                              PageId &rpDataPageId, HFPage *&rpdatapage,
                              RID &rpDataPageRid)
{
    PageId nextPageId = INVALID_PAGE, curPageId = firstDirPageId;
    Page *page = NULL;
    HFPage *dir_page = NULL, *data_page = NULL;
    Status rec_rc = FAIL, page_rc = FAIL;
    // variables to
    RID curRec = {.pageNo = INVALID_PAGE, .slotNo = -1};
    DataPageInfo dpi = {.availspace = -1, .recct = -1, .pageId = INVALID_PAGE};
    int rec_len = 0;

    page_rc = MINIBASE_BM->pinPage(curPageId, page, false, fileName);
    assert(page_rc == OK);
    dir_page = (HFPage *)page;

    // goind thorugh all the directory pages,
    while (page_rc == OK)
    {
        //for a given directory page, going through all records
        rec_rc = dir_page->firstRecord(curRec);
        while (rec_rc == OK)
        {
            rec_rc = dir_page->getRecord(curRec, (char *)&dpi, rec_len);
            //printf("reclen vs sizeof: %d %ld\n", rec_len, sizeof(DataPageInfo));
            assert(rec_len == sizeof(DataPageInfo));

            // try to find a matching page.
            //printf("rid.pageNo vs dpi.pageId: %d %d\n", rid.pageNo, dpi.pageId);
            if (rid.pageNo == dpi.pageId)
            {
                rec_rc = MINIBASE_BM->pinPage(dpi.pageId, page, false, fileName);
                assert(rec_rc == 0);
                data_page = (HFPage *)page;

                // fill out the parameters
                rpdirpage = dir_page;
                rpdatapage = data_page;
                rpDirPageId = curPageId;
                rpDataPageId = dpi.pageId;
                rpDataPageRid.pageNo = curRec.pageNo;
                rpDataPageRid.slotNo = curRec.slotNo;
                return OK;
            }

            rec_rc = dir_page->nextRecord(curRec, curRec);
        }

        nextPageId = dir_page->getNextPage();
        page_rc = MINIBASE_BM->unpinPage(curPageId);
        page_rc = MINIBASE_BM->pinPage(nextPageId, page, false, fileName);
        curPageId = nextPageId;
        dir_page = (HFPage *)page;
    }

    // if we could not find a page, we set all values to be invalid.
    if (page_rc != OK)
    {
        rpdirpage = NULL;
        rpdatapage = NULL;
        rpDirPageId = INVALID_PAGE;
        rpDataPageId = INVALID_PAGE;
        rpDataPageRid.pageNo = INVALID_PAGE;
        rpDataPageRid.slotNo = -1;
    }

    return DONE;
}

// *********************************************************************
// Allocate directory space for a heap file page

Status HeapFile::allocateDirSpace(struct DataPageInfo *dpinfop,
                                  PageId &allocDirPageId,
                                  RID &allocDataPageRid)
{
    Page *page = NULL;
    HFPage *hfp = NULL;

    Status rc = MINIBASE_BM->newPage(allocDirPageId, page, 1);
    assert(rc == OK);

    hfp = (HFPage *)page;
    hfp->init(allocDirPageId);

    rc = hfp->insertRecord((char *)dpinfop, sizeof(DataPageInfo), allocDataPageRid);
    assert(rc == OK);

    rc = MINIBASE_BM->unpinPage(allocDirPageId, TRUE, fileName);
    assert(rc == OK);

    return OK;
}

// *******************************************