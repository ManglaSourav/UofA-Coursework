#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define NUMBER_OF_PAGES 524288 // (2^32)/(2^13)
//8KB = 2^3 + 2^10 = 2^13
int NUMFRAMES; // number of spaces in RAM array
char *ALGORITHM,*TRACEFILE;

/**
 * @brief represents a PTE that will be stored in a Reference Table
 * int distance - value represnting how far from the beginning of the program
 * the instruction was found
 * PageTableEntryReference *next - pointer to the next reference in a linked list
 * can be NULL
 */

typedef struct PageTableEntryReference{
    int distance; // how far away this instruction is from 0
    struct PageTableEntryReference *next;
    
} PageTableEntryReference;

/**
 * Wrapper for linked lists that store PTE Reference nodes that contains the 
 * page number this linked list represents (also the index in an array).
 * Each node attached in this list represents a reference in which a certain
 * page number was found
 */
typedef struct PageTableEntryReferenceList{
    long pageNumber;
    int size;
    struct PageTableEntryReference *head; // beginning of list
    struct PageTableEntryReference *tail; // end of list
} PageTableEntryReferenceList;

/**
 * Structure that contains all pertinent information on a Page Table Entry(PTE)
 * A PTE is a range of address space theorized to condense a virtual address space
 * 
 */
typedef struct PageTableEntry{
    long pageNumber; // identifier for the PTE
    int  frameNumber; // Where the PTE is in RAM (if it is); should be only considered if valid=1
    int  valid;      // bit that specifies if page is physically preseny in memory
    int  dirty;      // 0 = clean/not modified || 1 = dirty/modified; when a page is WRITTEN to 
    int  referenced;  // 0 = not referenced || 1 = referenced; set when a page is read or written to
    int  lastUse;   // when it was used in execution
} PageTableEntry;

PageTableEntry *pageTable;
PageTableEntryReferenceList referenceTableLists[NUMBER_OF_PAGES];

int *RAM; // Array for our simulated RAM
int RAMsize;
long memoryAccesses; // count of how many times memory has been accessed
int clockPointer, pageFaults,writesToDisk; // various other counts for algos

void readAndParseFile(FILE *fp);
void buildReferenceTable(FILE *fp);
int clockReplacement(long addInPTE);
int notFrequentlyUsed(long addInPTE);
int leastRecentlyUsed(long addInPTE);
int optimalPageReplacement(long addInPTE);
int getFurthestReferencedPage(PageTableEntryReferenceList *list);
int wasEvictionDirty(PageTableEntry *pte);
long counter = 0; // used for LRU

int main(int argc ,char *argv[]){
    if (argc  != 6){
        printf("NOT ENOUGH ARGUMENTS");
        return -1;
    }
    // Get the arguments
    NUMFRAMES = atoi(argv[2]);
    ALGORITHM = argv[4];
    TRACEFILE = argv[5];
    RAM = malloc(NUMFRAMES * sizeof(int));
    RAMsize = 0;
    //[0-524288]
    pageTable = malloc(NUMBER_OF_PAGES * sizeof(PageTableEntry)); // 1 index per chunk
    if(!RAM || !pageTable){
        fprintf(stderr,"Malloc error\n");
        return -1;
    }
    FILE *fp; // File pointer
    if((fp = fopen(TRACEFILE,"r")) == NULL){
        printf("FILE NOT FOUND\n");
        return -1;
    }
    if(strcmp(ALGORITHM,"opt") == 0){
        PageTableEntryReferenceList *ptrl;
        for(int i = 0; i < NUMBER_OF_PAGES; i++){
            ptrl = malloc(sizeof(PageTableEntryReferenceList));
            referenceTableLists[i] = *ptrl;
        }
        FILE *refFp;
        if((refFp = fopen(TRACEFILE,"r")) == NULL){
            fprintf(stderr,"Error reading file\n");
            return -1;
        }
        // builds an array of linked lists that contain all distances
        // the instructions are found in the future
        buildReferenceTable(refFp); 
    }
    // Read the file, line by line and do work
    readAndParseFile(fp);
    printf(
    "Algorithm: %s\n"
    "Number of frames:          %d\n"
    "Total memory accesses:     %ld\n"
    "Total page faults:         %d\n"
    "Total writes to disk:      %d\n"
    "Total size of page table:  %d bytes\n",
    ALGORITHM,NUMFRAMES,memoryAccesses,pageFaults,writesToDisk,NUMBER_OF_PAGES*4);
    
} 

/**
 * @brief Function that reads the file line by line, parses the data and records
 * data necessary as per the instruction.
 * According to passed in command arguments, an algorithm will be specified when 
 * a page fault occurs. These algorithms are Clock, NFU, OPT and LRU.
 * 
 * @param fp file pointer
 */
void readAndParseFile(FILE *fp){
    // read file line by line
    char line[256],specifier[10]; // buffer to load in the line -> ok size?
    long VA; // Virtual Address
    int size,len;
    while(fgets(line,sizeof(line),fp)){
        // ignores the header text of the files; 2 total compares
        if(strncmp(line,"--",2) != 0 && strncmp(line,"==",2) != 0){
            len = strlen(line);
            // strips newline from string
            if(line[len-1] == '\n'){
                line[len-1] = 0; // NULL TERMINATOR
            }
            sscanf(line,"%s %lx,%d",specifier,&VA,&size);
            long pageNum = VA >> 13; // page size is 8KB -> 2^3 * 2^10 = 2^13
            PageTableEntry *pte = &pageTable[pageNum]; // represents the PTE in the table
            // this is a brand new page added to the Page Table
            if(pte->pageNumber == 0){
                pte->pageNumber = pageNum;
            }
            memoryAccesses++;
            pte->referenced++; // should happen given the pulled in page is now being referenced
            pte->lastUse = counter++;
            if(strcmp(specifier,"S") == 0){
                pte->dirty = 1; // write instruction
            }else if(strcmp(specifier,"M") == 0){
                pte->dirty = 1; // write instruction
                memoryAccesses++; //performs an additional access: load/store
            }
            // the page table entry is already in RAM;
            if(pte->valid){
                printf("hit\n");
                continue;
            }
            // At this point, pte is NOT in RAM
            // if the PTE is not in RAM and RAM has free frames; no eviction
            if(!pte->valid && RAMsize < NUMFRAMES){
                pageFaults++;
                printf("page fault - no eviction\n");
                pte->valid = 1; // set the valid bit, now in RAM
                pte->frameNumber = RAMsize; // Where page lives in RAM == current open index
                RAM[RAMsize] = pte->pageNumber; // store the pageNumber in index at RAM
                RAMsize++; // increase how many pages are in RAM
            }
            // pte is not in RAM and RAM is full, need to evict using an algo.
            else if(!pte->valid && RAMsize == NUMFRAMES){
                pageFaults++; // page fault occured
                int dirty; //value used to tell if the eviced page has dirty or not
                // The Optimal Page Replace Algorithm 
                // take out the page in RAM that is to be used LAST
                if(strcmp(ALGORITHM,"opt") == 0){
                    dirty = optimalPageReplacement(pte->pageNumber);  
                }
                // The Clock Replacement Algorithm
                else if(strcmp(ALGORITHM,"clock") == 0){
                    dirty = clockReplacement(pte->pageNumber);
                }
                // The Least Recently Used Page Replacement Algorithm 
                else if(strcmp(ALGORITHM, "lru") == 0){
                    dirty = leastRecentlyUsed(pte->pageNumber);
                }
                // Not Frequently Used
                else if(strcmp(ALGORITHM,"nfu") == 0){
                    dirty = notFrequentlyUsed(pte->pageNumber);

                }else{
                    fprintf(stderr,"Algorithm specifier is missing or improperly formatted");
                    return;
                }
                if(dirty){
                    writesToDisk++;
                    printf("page fault - evict dirty\n");
                }else{
                    printf("page fault - evict clean\n");
                }
                pte->valid = 1;
            }
        }
    }

}

/**
 * @brief Build the Reference Table to be used in OPT to look into the
 * future to predict which page would be best to evict * 
 * @param fp file pointer
 */
void buildReferenceTable(FILE *fp){
    char line[256],specifier[10]; // buffer to load in the line -> ok size?
    long VA; // Virtual Address
    int size,len,lineNo;
    while(fgets(line,sizeof(line),fp)){
        // ignores the header text of the files; 2 total compares
        if(strncmp(line,"--",2) != 0 && strncmp(line,"==",2) != 0){
            len = strlen(line);
            // strips newline from string
            if(line[len-1] == '\n'){
                line[len-1] = 0; // NULL TERMINATOR
            }
            sscanf(line,"%s %lx,%d",specifier,&VA,&size);
            long pageNum = VA >> 13;
            // this gets the linked list wrapper that contains all references
            PageTableEntryReferenceList *referenceList = &referenceTableLists[pageNum];
            referenceList->pageNumber = pageNum;
            PageTableEntryReference *newRef = malloc(sizeof(PageTableEntryReference));
            newRef->distance = lineNo;
            newRef->next = NULL;
            if(!referenceList->head){
                referenceList->head = newRef;
                referenceList->tail = newRef;
                referenceList->size = 1;
            }
            // there is atleast one element in the reference list
            else {
                referenceList->tail->next = newRef;
                referenceList->tail = newRef;
                referenceList->size++;
            }
        lineNo++;
        }
    }
}

/**
 * @brief 
 * 
 * @param addInPTE long value of the page number to be added into RAM
 * @return int value of the function that decides if the passed in page number value
 * representing an index in the Page Table if evicted was dirty or not
 */
int clockReplacement(long addInPTE){
    int pageNum = RAM[(clockPointer % NUMFRAMES)]; // gets the PTE at the last left off rotation
    PageTableEntry *pte = &pageTable[ pageNum ];
    // keep going until you find a PTE with referenced bit == 0
    while(pte->referenced != 0){
        pte->referenced = 0; // set it to zero to avoid infinite loop
        clockPointer++; //keep track of where we are in the clock
        // reset values
        pageNum = RAM[(clockPointer % NUMFRAMES)];
        pte = &pageTable[ pageNum ];
    }
    // at this point, pte will be the page to evict.
    pageTable[pageNum].valid = 0; // "evicts" the page by saying its not in RAM in pageTable
    pageTable[addInPTE].valid = 1; // "add page" to pageTable by setting valid to 1
    pageTable[addInPTE].frameNumber = pageTable[pte->pageNumber].frameNumber;
    RAM[(clockPointer % NUMFRAMES)] = addInPTE; // add the page number of new PTE to current pointed at index in RAM
    clockPointer++;
    return wasEvictionDirty(&pageTable[pageNum]);
}

/**
 * @brief Not Requently Used - remove the page that has the smallest value for the 
 * number of time it was referenced in the past. 
 * 
 * @param addInPTE 
 * @return int 
 */
int notFrequentlyUsed(long addInPTE){
    PageTableEntry *pte = &pageTable[ RAM[0] ]; //get the first PTE in RAM
    int min = pte->referenced; // set min
    int pageNum = pte->pageNumber; // will be the page to evicit after loop
    for(int x = 1; x < NUMFRAMES; x++){
        pte = &pageTable[RAM[x]];
        if(pte->referenced < min){
            min = pte->referenced;
            pageNum = pte->pageNumber;
        }
    }
    pageTable[pageNum].valid = 0; // evicted page, set to zero
    pageTable[addInPTE].valid = 1; // new page added to RAM
    pageTable[pageNum].referenced = 0; //reset the evicted page's ref to 0
    pageTable[addInPTE].frameNumber = pageTable[pageNum].frameNumber; // swap frame numbers
    RAM[pageTable[pageNum].frameNumber] = addInPTE;
    return wasEvictionDirty(&pageTable[pageNum]);
}

/**
 * @brief Least Recently Used - remove the page that was used furthes in the past
 * 
 * @param addInPTE 
 * @return int 
 */
int leastRecentlyUsed(long addInPTE){
    PageTableEntry *pte = &pageTable[ RAM[0] ]; //get the first PTE in RAM
    int min = pte->lastUse; // set min
    int pageNum = pte->pageNumber; // will be the page to evicit after loop
    for(int x = 1; x < NUMFRAMES; x++){
        pte = &pageTable[RAM[x]];
        if(pte->lastUse < min){
            min = pte->lastUse;
            pageNum = pte->pageNumber;
        }
    }
    pageTable[pageNum].valid = 0; // evicted page, set to zero
    pageTable[addInPTE].valid = 1; // new page added to RAM
    pageTable[addInPTE].frameNumber = pageTable[pageNum].frameNumber; // swap frame numbers
    RAM[pageTable[pageNum].frameNumber] = addInPTE;
    return wasEvictionDirty(&pageTable[pageNum]);
}

/**
 * @brief Optimal Page Replacment Algorithm - this algorithm, which is impossible
 * in real life, that looks into the future to determine which page is best to 
 * evict. The optimal page to evict, is the one that will NEXT be used FURTHEST into
 * the future. 
 * 
 * @param addInPTE 
 * @return int 
 */
int optimalPageReplacement(long addInPTE){
    PageTableEntryReferenceList *refList;
    int furthestFound;
    long removeThisPage = 0;
    int max = -1;
    for(int x = 0; x < NUMFRAMES; x++){
        int pageInMem = RAM[x]; // get the page number of RAM at index x
        refList = &referenceTableLists[pageInMem];
        furthestFound = getFurthestReferencedPage(refList);
       // printf("found: %d > %ld (%x)\n",furthestFound,counter,pageInMem);
        // this means that we ran out of refs before finding a reference > counter
        if(furthestFound == -1){
            removeThisPage = x;
            break;
        }
        if(furthestFound >= max){
            max = furthestFound;
            removeThisPage = x;
        }
    }
    removeThisPage = RAM[removeThisPage];
    //printf("OUT: %lx",removeThisPage);
    // now we have found whatever page was chosen to be evicted
    refList = &referenceTableLists[removeThisPage];
    PageTableEntryReference *curr = refList->head;
    while(curr && curr->distance < furthestFound){
        curr = curr->next;
        refList->head = curr;
        refList->size--;
    }
    pageTable[removeThisPage].valid = 0; // "evicts" page
    pageTable[addInPTE].valid = 1; // adds in the new page to RAM
    pageTable[addInPTE].frameNumber = pageTable[removeThisPage].frameNumber;
    RAM[ pageTable[removeThisPage].frameNumber ] = addInPTE;

    return wasEvictionDirty(&pageTable[removeThisPage]);
}

/**
 * @brief Get the Furthest Referenced Page will return the integer that represents the 
 * next time in the future that an instruction will be used
 * 
 * @param list 
 * @return int 
 */
int getFurthestReferencedPage(PageTableEntryReferenceList *list){
    PageTableEntryReference *ref = list->head;
    int retVal;
    //printf("%lx: ",list->pageNumber);
    while(ref){
        if(ref->distance <= counter){
            //printf("%d,",ref->distance);
            ref = ref->next;
        }else{
            retVal = ref->distance;
            break;
        }
    }
    // reached to where the distance was > counter; return the distance
    if(ref){
        return retVal;
    }
    // this will indicate that we reached the end of the linked list
    // before finding the value thats greater than counter
    // this reference should be deleted then
    return -1;
  
}

/**
 * @brief helper function that returns an int (boolean) value indicating
 * if a page was dirty or not
 * 
 * @param pte 
 * @return int 
 */
int wasEvictionDirty(PageTableEntry *pte){
    int temp = pte->dirty;
    pte->dirty = 0;
    return temp == 1;
}
