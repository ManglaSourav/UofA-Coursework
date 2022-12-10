
## Design Overview and Implementation Details

Heapfile class 
Have a look at the file heapfile.h in ~/proj2/HeapFile/include. It contains the interfaces for the HeapFile class. This class implements a "heapfile" object. Note that you should implement the public and private member functions, as well as constructor and destructor. You should put all your code into the file heapfile.C.

Constructor and destructor

```HeapFile::HeapFile( const char *name, Status& returnStatus )``` 
Constructor. If the name already denotes a file, the file is opened; otherwise, a new empty file is created.

```HeapFile::~HeapFile()```
Deconstructor.

Public methods

```int HeapFile::getRecCnt()```
This routine returns the number of records in this heapfile.

```Status HeapFile::insertRecord(char *recPtr, int recLen, RID& outRid)```
This routine inserts a record into the heapfile.

```Status HeapFile::deleteRecord (const RID& rid)```
This routine deletes the specified record from the heapfile.

```Status HeapFile::updateRecord (const RID& rid, char *recPtr, int recLen)```
This routine updates the specified record in the heapfile.

```Status HeapFile::getRecord (const RID& rid, char *recPtr, int& recLen)```
This routine reads record from the heapfile, returning pointer and length.

```class Scan *HeapFile::openScan(Status& status)```
This routine initiates and returns a sequential scan.

```Status HeapFile::deleteFile()```
This routine wipes out the heapfile from the database permanently.

Private methods

```Status HeapFile::newDataPage(DataPageInfo *dpinfop)```
This routine gets a new datapage from the buffer manager and initializes dpinfo.

```Status HeapFile::findDataPage(const RID& rid, PageId &rpDirPageId, HFPage *&rpdirpage, PageId &rpDataPageId)``` 
This routine, as an internal HeapFile function used in getRecord and updateRecord, returns a pinned directory page and a pinned data page of the specified user record (rid).

```Status allocateDirSpace(struct DataPageInfo * dpinfop, PageId &allocDirPageId, RID &allocDataPageRid)``` 
This routine allocates directory space for a heapfile page.

Scan class

Have a look at the file scan.h in ~/proj2/HeapFile/include. It contains the interfaces for the Scan class. This class implements a "scan" object. Note that you should implement the public and private member functions, as well as constructor and destructor. You should put all your code into the file scan.C.
Constructor and destructor

```Scan::Scan (HeapFile *hf, Status& status)```
The constructor pins the first page in the file and initializes its private data members with the given hf.

```Scan::~Scan()```
The destructor unpins all the pages in the file.

Public methods
```Status Scan::getNext(RID& rid, char *recPtr, int& recLen)```
This routine retrieves the next record in a sequential scan and returns the RID of the retrieved record.

Private methods

```Status Scan::init(HeapFile *hf)```
This routine does all the constructor work.

```Status Scan::reset()```
This routine resets everything and unpin all pages.

```Status Scan::firstDataPage()```
This routine gets the first data page in the file. (It copies over data about first page.)

```Status Scan::nextDataPage()```
This routine retrieves the next data page in the file.

```Status Scan::nextDirPage()```
This routine retrieves the next directory page in the file.
