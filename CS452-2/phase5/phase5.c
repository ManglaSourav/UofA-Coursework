
#include <memory.h>
#include <assert.h>

#include <usloss.h>
#include <usyscall.h> // for SYS_* constants

#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include "phase4.h"
#include "phase5.h"

#define TODO()                                                   \
    do                                                           \
    {                                                            \
        USLOSS_Console("TODO() at %s:%d\n", __func__, __LINE__); \
        *(char *)7 = 0;                                          \
    } while (0)

/* these are used by the testcases, we have to share them. */
VmStats vmStats;
void *vmRegion = NULL;

/* this is *NOT* shared with the testcase, we don't want them to see it */
static void *physMemRegion = NULL;

/* there are three key arrays which we must maintain:
 *
 * - processes.  Process-specific information; includes the page table pointer,
 *               and also per-page information about which pages are in the
 *               swap disk (if any), so that we can draw them back from disk if
 *               needed.
 *
 * - frames.     Frame-specific information; useful for finding a new, free
 *               frame, and also for flushing a frame to disk if needed.
 *
 * - swap.       Swap-page specific informant; indicates which blocks of the
 *               swap disk are currently in use.  Unlike the frames, we don't
 *               have to identify what information is in which page, since we
 *               never "flush swap;" we always draw pages back from swap when
 *               driven by a certain process.
 */

typedef struct Phase5_Proc
{
    /* all fields set in phase5_mmu_pageTable_alloc(), cleared in *_free() */
    int pid;
    USLOSS_PTE *page_table;
    struct
    {
        int swapPage; // -1 if not in swap, page (not sector) address otherwise
    } virtPages[VM_MAX_VIRT_PAGES];
} Phase5_Proc;

typedef struct Frame
{
    int state;
#define FRAME_AVAIL 0
#define FRAME_INUSE 1
#define FRAME_IO_IN_PROG 2

    /* these fields only valid if INUSE or IO_IN_PROG */
    int pid;
    int virtPage;
    int ref;

    /* this is only used when state==IO_IN_PROG; it exists because the process
     * that owns this memory might have a page fault while we're swapping out;
     * this blocks the page fault process until the frame has been flushed to
     * disk.  Then it will do an ordinary page fault to bring the page back in.
     */
    int wakeup_on_io_complete_mb;
} Frame;

typedef struct SwapPage
{
    int inUse;
} SwapPage;

/* -------------------- Now, declare the arrays -------------------- */

static Phase5_Proc phase5_procTbl[MAXPROC];
static Frame frames[VM_MAX_PHYS_PAGES];
static SwapPage swapPages[VM_MAX_SWAP_PAGES];

/* ------------ Various components have their runtime globals. ------------ */

static struct
{
    int requests_mb; // this field is *NOT* protected by the lock

    /* everything below this is controled by 'lock_mb' below. */
    int lock_mb;

    /* where we are in the frame scan */
    int frame_clock;
} pagers;

static struct
{
    int size_pages;
    int clock;
} swapDisk;

/* ------- Used for communiating from the interrupt handler to pagers ------- */
typedef struct PagerRequest
{
    int pid;      // pid of the
    int virtPage; // VM page that needs space

    int reply_mb;
} PagerRequest;

static inline void lock(int mb) { MboxRecv(mb, NULL, 0); }
static inline void unlock(int mb) { MboxSend(mb, NULL, 0); }

static void pageFaultHandler(int type, void *offset);
static int pagerProc(char *);

void phase5_init()
{
    assert(vm_num_virtPages <= VM_MAX_VIRT_PAGES);
    assert(vm_num_physPages <= VM_MAX_PHYS_PAGES);

    /* ------------ set up the MMU ----------- */
    int rc = USLOSS_MmuInit(vm_num_virtPages, vm_num_virtPages,
                            vm_num_physPages,
                            USLOSS_MMU_MODE_PAGETABLE);
    assert(rc == USLOSS_MMU_OK);

    /* ----- VARIABLES SHARED WITH USERS ----- */
    memset(&vmStats, 0, sizeof(vmStats));

    rc = USLOSS_MmuGetConfig(&vmRegion, &physMemRegion,
                             NULL, &vmStats.pages, &vmStats.frames, NULL);
    assert(rc == USLOSS_MMU_OK);
    assert(vmRegion != NULL);
    assert(physMemRegion != NULL);
    assert(vmStats.pages == vm_num_virtPages);
    assert(vmStats.frames == vm_num_physPages);

    vmStats.freeFrames = vmStats.frames;

    /* ----- INTERRUPT HANDLER ----- */
    USLOSS_IntVec[USLOSS_MMU_INT] = pageFaultHandler;

    /* ----- PAGER LOGIC ----- */

    /* this is the pager process mutex.  We use Recv() for locking, so we
     * must initialize it with a message.
     */
    pagers.lock_mb = MboxCreate(1, 0);
    assert(pagers.lock_mb > 0);
    MboxSend(pagers.lock_mb, NULL, 0);

    /* this is the queue of requests that ordinary processes send to the
     * pagers.  Each has all of the information we need about the request,
     * including the MB to use to wake up the process when we're done.
     *
     * Note that there could be any number of processes blocked, waiting for
     * work to be done on the queue.  But since they are blocked, there's no
     * point in allowing them to queue a bunch of messages; they can wait in
     * the Producer Queue of the mailbox.
     */
    pagers.requests_mb = MboxCreate(1, sizeof(PagerRequest));
    assert(pagers.requests_mb > 0);

    pagers.frame_clock = 0;

    /* ----- OTHER MISC GLOBALS ----- */
    swapDisk.size_pages = -1; // need to read it from disk

    /* ----- fill in the various arrays, described above ----- */
    memset(phase5_procTbl, -1, sizeof(phase5_procTbl));
    memset(frames, 0, sizeof(frames));
    memset(swapPages, 0, sizeof(swapPages));
}

void phase5_start_service_processes()
{
    for (int i = 0; i < vm_num_pagerDaemons; i++)
        fork1("pager", pagerProc, NULL, 2 * USLOSS_MIN_STACK, 1);
}

USLOSS_PTE *phase5_mmu_pageTable_alloc(int pid)
{
    USLOSS_PTE *retval = calloc(vm_num_virtPages, sizeof(USLOSS_PTE));
    assert(retval != NULL);

    Phase5_Proc *proc = &phase5_procTbl[pid % MAXPROC];
    assert(proc->pid == -1);

    proc->pid = pid;
    proc->page_table = retval;

    return retval;
}

void phase5_mmu_pageTable_free(int pid, USLOSS_PTE *page_table)
{
    assert(pid == getpid());

    Phase5_Proc *proc = &phase5_procTbl[pid % MAXPROC];
    assert(proc->pid == pid);
    assert(page_table == proc->page_table);

    /* scan through the virtual memory, and release any frames that
     * we were using.  Also free any swap pages consumed.
     */
    for (int i = 0; i < vm_num_virtPages; i++)
    {
        USLOSS_PTE *pte = &page_table[i];

        /* is this virtual page consuming a frame?  If so, then free that frame */
        if (pte->incore == 1)
        {
            int frameID = pte->frame;
            assert(frameID >= 0 && frameID < vm_num_physPages);

            Frame *frame = &frames[frameID];
            assert(frame->state == FRAME_INUSE);
            assert(frame->pid == pid);
            assert(frame->virtPage == i);

            /* free the frame */
            frame->state = FRAME_AVAIL;

            vmStats.freeFrames++;
            assert(vmStats.freeFrames <= vm_num_physPages);
            // USLOSS_Console("%s(): pid %d virtPage %d   vmStats.freeFrames %d\n", __func__, pid,i, vmStats.freeFrames);

            /* we don't really care what the page table has, because it's about
             * to get destroyed.  But nonetheless, it feels right to remove the
             * page table entry!
             */
            pte->incore = 0;
        }

        /* is this virtual page reflected by something in swap?  If so, then
         * free that swap space.
         */
        int swap = proc->virtPages[i].swapPage;
        if (swap != -1)
        {
            assert(swapPages[swap].inUse == 1);
            swapPages[swap].inUse = 0;
            vmStats.freeDiskBlocks++;
        }
    }

    /* finally, free the memory for the page table, and reset our
     * shadow process table info
     */
    free(page_table);
    memset(proc, -1, sizeof(*proc));
}

static void pageFault_recordAccessFault(int virtPage);

static void pageFaultHandler(int type, void *offset)
{
    // USLOSS_Console("%s(): BEGIN pid %d offset %p\n", __func__, getpid(), offset);

    assert(type == USLOSS_MMU_INT);

    /* is the offset within the range of pages that we expect? */
    if ((long)offset < 0 || (long)offset >= 4096 * vm_num_virtPages)
    {
        USLOSS_Console("%s(): The offset is outside of the VM region.  This is an unrecoverable fault.  vm_num_virtPages %d   offset %p\n", __func__, vm_num_virtPages, offset);
        USLOSS_Halt(1);
    }

    /* ok, which page contains the fault? */
    int page = ((long)offset) / 4096;

    /* why did the page fault occur?  ACCESS faults are handled inside
     * the interrupt handler, since paging is never required.  Once the
     * function finishes, we will ask USLOSS to reload the page table, because
     * we know that *something* will have changed.  And by doing it in the
     * common location here, we won't need to do it in multiple places
     * elsewhere.
     */
    int cause = USLOSS_MmuGetCause();
    if (cause == USLOSS_MMU_ACCESS)
    {
        pageFault_recordAccessFault(page);

        Phase5_Proc *proc = &phase5_procTbl[getpid() % MAXPROC];
        assert(proc->pid == getpid());

        int rc = USLOSS_MmuSetPageTable(proc->page_table);
        assert(rc == USLOSS_MMU_OK);

        return;
    }

    // USLOSS_Console("%s(): pid %d virtPage %d\n", __func__, getpid(), page);

    assert(cause == USLOSS_MMU_FAULT);
    vmStats.faults++;

    /* allocate a mailbox for waking myself up; store that, plus the rest of our
     * information, into a pager request.
     */
    PagerRequest req;
    memset(&req, 0, sizeof(req));

    req.pid = getpid();
    req.virtPage = page;
    req.reply_mb = MboxCreate(1, sizeof(int));
    assert(req.reply_mb >= 0);

    /* send the pager request through the shared mailbox.  It will eventually
     * reply (on our process-specific mailbox) with the # of the physical page
     * that was allocated for us.
     */
    MboxSend(pagers.requests_mb, &req, sizeof(req));

    /* wait until the pager has completed our work.  The pager was responsible
     * for updating our page table, so all of the changes should have already
     * occurred, before we context switched back - so we don't need to do any
     * of the updates ourselves.
     */
    MboxRecv(req.reply_mb, NULL, 0);
    MboxRelease(req.reply_mb);

    /* the pager should have altered our page table *before* they woke us up.
     * Therefore, when the context switch happened to get us running again, the
     * new page table was restored - therefore, we don't have to do anything on
     * this side, at all!
     */
    Phase5_Proc *proc = &phase5_procTbl[getpid() % MAXPROC];
    assert(proc->pid == getpid());

    assert(proc->page_table[page].incore == 1);

    // USLOSS_Console("%s(): END   pid %d offset %p    -- pte: incore %d frame %d\n", __func__, getpid(), offset, pte->incore, pte->frame);
}

static void pageFault_recordAccessFault(int virtPage)
{
    // USLOSS_Console("%s(): pid %d virtPage %d\n", __func__, getpid(),virtPage);

    Phase5_Proc *proc = &phase5_procTbl[getpid() % MAXPROC];
    assert(proc->pid == getpid());

    assert(virtPage >= 0 && virtPage < vm_num_virtPages);
    USLOSS_PTE *pte = &proc->page_table[virtPage];
    assert(pte->incore);

    Frame *frame = &frames[pte->frame];
    assert(frame->pid == getpid());
    assert(frame->virtPage == virtPage);

    // USLOSS_Console("%s(): pid %d virtPage %d frame %d  --  r/w %d/%d  --  ref %d\n", __func__, getpid(), virtPage, pte->frame, pte->read,pte->write, frame->ref);

    if (frame->state == FRAME_IO_IN_PROG)
    {
        /* we get here if the user tries to access a page while we're already
         * doing I/O to swap it in or out.  Or rather, IO_IN_PROG means that
         * we're doing swap in or swap out - but if it was swap in, then it
         * would have been driven by this process.  So the only way that this
         * can happen is for a swap out to be in progress - triggered by memory
         * pressure in some other process, and then initiated by the pager
         * daemon.
         *
         * We're going to lose this frame, and then we're going to need to
         * restore it from swap.  Annoying.  But we don't want to simply copy
         * the page sideways (seems dangerous and complex), and also we can't
         * start reading the page back until it is fully on disk.
         *
         * So we're going to wait for a mail message (that tells us that the
         * page has been flushed to disk), and then *RETURN*.  Since the
         * swap-out code would have cleared our 'incore' flag in the PTE, we
         * know that the next fault we hit on this page will take us down the
         * swap-in path.
         */

        int mb = MboxCreate(1, 0);
        assert(mb > 0);

        assert(frame->wakeup_on_io_complete_mb == 0);
        frame->wakeup_on_io_complete_mb = mb;

        MboxRecv(mb, NULL, 0);
        MboxRelease(mb);

        /* we no longer have a map of the virtual page.  But it should be on
         * the swapdisk, somewhere
         */
        assert(pte->incore == 0);
        assert(proc->virtPages[virtPage].swapPage != -1);

        /* start the page fault over from scratch, with the new PTE state */
        return;
    }
    assert(frame->state == FRAME_INUSE);

    if (pte->read == 0)
    {
        assert(pte->write == 0);
        assert((frame->ref & USLOSS_MMU_REF) == 0);
        pte->read = 1;
        frame->ref |= USLOSS_MMU_REF;
    }
    else if (pte->write == 0)
    {
        pte->write = 1;
        frame->ref |= USLOSS_MMU_DIRTY;

        int old_swap_page = proc->virtPages[virtPage].swapPage;
        proc->virtPages[virtPage].swapPage = -1;

        if (old_swap_page != -1)
        {
            assert(old_swap_page >= 0 && old_swap_page < swapDisk.size_pages);
            assert(swapPages[old_swap_page].inUse == 1);
            swapPages[old_swap_page].inUse = 0;

            vmStats.freeDiskBlocks++;
        }
    }
    else
        assert(0); // invalid PTE state

    // USLOSS_Console("%s(): pid %d virtPage %d frame->ref %d \n", __func__, getpid(), virtPage,frame->ref);
}

static void pager_readDiskSize(void);
static void pager_handleOne(PagerRequest req);
static int pager_findEmptyFrame_orFlush(void);
static void pager_flushToSwap(int frameID);
static void pager_writeZeroesToFrame(int frameID);
static void pager_readPageFromSwap(int frameID, int swapPage);

static int pagerProc(char *ignored)
{
    pager_readDiskSize();

    while (1)
    {
        PagerRequest req;
        memset(&req, 0, sizeof(req));

        int mb_rc = MboxRecv(pagers.requests_mb, &req, sizeof(req));
        assert(mb_rc == sizeof(req));

        // USLOSS_Console("%s: New request received.   pid %d   vm page %d\n", __func__, req.pid, req.virtPage);

        /* we use a helper function, because it's handy to be able to return
         * from the function, to indicate that we've completed the fault.
         */
        pager_handleOne(req);

        /* let the requesting process know that we're done */
        int rc = MboxSend(req.reply_mb, NULL, 0);
        assert(rc == 0);
    }

    return 0; // we'll never get here
}

static void pager_handleOne(PagerRequest req)
{
    Phase5_Proc *proc = &phase5_procTbl[req.pid % MAXPROC];
    assert(proc->pid == req.pid);

    USLOSS_PTE *pte = &proc->page_table[req.virtPage];
    assert(pte->incore == 0);

    /* the page doesn't exist in core.  This is the meat of the algorithm;
     * we have to search for a free Frame (perhaps flushing one out to disk),
     * and then fill that frame with the proper data (perhaps reading from
     * disk)
     */

    lock(pagers.lock_mb);

    int frameID = pager_findEmptyFrame_orFlush();
    assert(frameID >= 0 && frameID < VM_MAX_PHYS_PAGES);
    Frame *frame = &frames[frameID];

    /* grab control of the frame.  Update vmStats to match */
    assert(frame->state == FRAME_AVAIL);
    frame->state = FRAME_IO_IN_PROG;
    frame->pid = req.pid;
    frame->virtPage = req.virtPage;
    frame->ref = USLOSS_MMU_REF;

    assert(vmStats.freeFrames > 0);
    vmStats.freeFrames--;

    /* now, what do we want to fill the frame with?  If the virtual page has
     * never been touched (or, more precisely, if it's never been *written to*,
     * then the contents should be all zeroes.  Otherwise, we must read from
     * the swap disk.
     */
    if (proc->virtPages[req.virtPage].swapPage == -1)
        pager_writeZeroesToFrame(frameID);
    else
    {
        /* if we get here, then:
         *    - It was a FAULT
         *    - We found (or created) a free page
         *    - But the contents are sitting on disk, gotta read them
         *
         * BTW, we still hold the "pager lock" at this point.  But we don't
         * want to keep it while we're doing disk I/O.  We don't have to worry
         * about our frame, however - it should be held as "inaccessible," to
         * all other code, by the fact that we have it in the IO_IN_PROG state.
         */

        unlock(pagers.lock_mb);
        pager_readPageFromSwap(frameID, proc->virtPages[req.virtPage].swapPage);
        lock(pagers.lock_mb);
    }

    /* update the PTE to point to the newly-allocated frame.  Then update the
     * frame to mark it as in use.
     */
    pte->frame = frameID;
    pte->read = 1;
    pte->write = 0;
    pte->incore = 1;

    frame->state = FRAME_INUSE;

    // USLOSS_Console("%s(): pid %d virtPage %d -- frameID %d\n", __func__, req.pid, req.virtPage, frameID);

    /* we don't need the lock anymore, since we're done updating the global
     * variables.
     */
    unlock(pagers.lock_mb);

    /* NOTE: We have altered the page table of one (or many) processes, but we
     *       do *NOT* need to tell USLOSS about it.  Since USLOSS is strictly
     *       single-CPU, and the running process is not us, then there
     *       definitely will be a context switch (eventually) and the updated
     *       page table will be enacted at that time.
     */
    return;
}

static void pager_readDiskSize(void)
{
    // USLOSS_Console("%s(): START pid %d\n", __func__, getpid());

    /* all of the pager processes do this check; the first to win the race for
     * the lock will read from the disk to get its size.  The rest will notice
     * that the size is known, and skip it (but they are serialized behind the
     * lock, so they can't make progress until the disk op completes).
     */
    lock(pagers.lock_mb);

    if (swapDisk.size_pages == -1)
    {
        int blockSize_bytes,
            trackSize_blocks,
            diskSize_tracks;

        int rc = kernDiskSize(1, &blockSize_bytes,
                              &trackSize_blocks,
                              &diskSize_tracks);
        assert(rc == 0);
        assert(blockSize_bytes == 512);
        assert(trackSize_blocks == 16);
        assert(diskSize_tracks > 0);

        // each track has 8K of data, but a page is 4K in size
        int diskSize_pages = diskSize_tracks * 2;
        assert(diskSize_pages <= VM_MAX_SWAP_PAGES);

        swapDisk.size_pages = diskSize_pages;

        // copy the same value into vmStats, for the testcase to look at
        vmStats.diskBlocks = diskSize_pages;
        vmStats.freeDiskBlocks = diskSize_pages;
    }

    unlock(pagers.lock_mb);
    return;
}

/* this function must be called while already holding the pager lock */
static int pager_findEmptyFrame_orFlush(void)
{
    int count = 0; // how many frames have we checked so far?  We stop after
                   // two passes: one to clear the reference bits, and one to
                   // actually drop a page.  It will be *VERY* rare that we
                   // ever hit this limit (it would require that every frame
                   // be in the IO_IN_PROC state), but it's *conceivable*

    while (count < vm_num_physPages * 2)
    {
        int frameID = pagers.frame_clock;
        pagers.frame_clock++;
        if (pagers.frame_clock == vm_num_physPages)
            pagers.frame_clock = 0;
        count++;

        Frame *frame = &frames[frameID];

        // USLOSS_Console("  frameID %d state %d ref %d\n", frameID, frame->state, frame->ref);

        /* if there's an available frame, then this is trivial: just use it.
         * Available frames abound when we first start up, but then become
         * rare - but when processes die, they can open up frames for re-use
         * again.  So this check, while most common early in the simulation
         * runtime, is not *exclusively* for that.
         */
        if (frame->state == FRAME_AVAIL)
            return frameID;

        /* If the frame has I/O going in either direction (swap in or swap
         * out), we must leave it alone, entirely.  The pager that is doing the
         * I/O will update the state when the I/O completes.
         */
        if (frame->state == FRAME_IO_IN_PROG)
            continue; // can't touch this page

        /* we have found a frame which we are going to do some work on.  Note
         * that we have *NOT* yet decided that we are going to use it; we might
         * simply clear the REF flag, and change its permissions in the proper
         * page table.  But whatever we choose to do in the end, it will be
         * necessary to "shoot down" some permissions (or maybe remove
         * entirely) in a PTE.  Note that the PTE *might* be from the same
         * process as what cause the page fault, but often it might not be.
         */
        assert(frame->state == FRAME_INUSE);

        int shootDown_pid = frame->pid;
        int shootDown_virtPage = frame->virtPage;

        Phase5_Proc *sd_proc = &phase5_procTbl[shootDown_pid % MAXPROC];
        assert(sd_proc->pid == shootDown_pid);

        assert(shootDown_virtPage >= 0 &&
               shootDown_virtPage < vm_num_virtPages);
        USLOSS_PTE *sd_pte = &sd_proc->page_table[shootDown_virtPage];

        /* what we do next depends on the 'ref' state.  If the REF bit is set,
         * then we will simply disable read and write permissions, and clear
         * that bit; if the clock makes it all the way around to this page a
         * *second* time, then we would discard it from memory.  But often, we
         * will clear the bit here, and then end up discarding some *other*
         * page.  Maybe the page will be touched before the clock gets back
         * around, and maybe not.
         */

        if (frame->ref & USLOSS_MMU_REF)
        {
            sd_pte->read = 0;
            sd_pte->write = 0;

            /* mask, don't set - because the DIRTY flag might be set, too */
            frame->ref &= ~USLOSS_MMU_REF;

            continue;
        }

        /* if we get here, then we have chosen to take over this page.  No
         * matter what else happens, we must stop the owning proc from using
         * this page table entry anymore.  However, if we need to do a swap to
         * disk, then we do *NOT* want to lose the association between the
         * owning process and the going-away frame - just in case they hit a
         * page fault on it soon!
         */

        sd_pte->read = 0;
        sd_pte->write = 0;

        /* if the page is dirty, then we must flush it to disk.  We mark the
         * frame as busy (so that, if the owning process hits a page fault,
         * they'll know that they shouldn't use this frame) but otherwise
         * keep the virtual->physical association intact.
         */

        int wakeup_mb = -1; // see code in the ACCESS handler
        if (frame->ref & USLOSS_MMU_DIRTY)
        {
            pager_flushToSwap(frameID);

            wakeup_mb = frame->wakeup_on_io_complete_mb;
            frame->wakeup_on_io_complete_mb = -1;
        }

        /* maybe the page was never dirty, and so we can just drop it without
         * any disk I/O.  Maybe the page *was* dirty, and so we did a disk
         * write to make it dirty.  But whatever was true, we have now made it
         * not dirty.  We can drop it now.
         */

        // USLOSS_Console("%s(): frame->ref %d\n", __func__, frame->ref);
        assert(frame->ref == 0);
        assert(frame->state == FRAME_INUSE);

        frame->state = FRAME_AVAIL;
        frame->pid = -1;
        frame->virtPage = -1;

        sd_pte->frame = -1;
        sd_pte->incore = 0;

        vmStats.freeFrames++;
        vmStats.replaced++;

        /* did we find that there was any thread waiting for us, which needed
         * to be awoken?  If so, this is a good time for it - since we've reset
         * the variables
         */
        if (wakeup_mb != -1)
            MboxSend(wakeup_mb, NULL, 0);

        return frameID;
    }

    assert(0); // TODO: what to do if *all* of the pages are in IO_IN_PROG?
}

static void pager_flushToSwap(int frameID)
{
    // we'll need all these variables eventually, to flush out the frame
    Frame *frame = &frames[frameID];
    assert(frame->state == FRAME_INUSE);
    assert(frame->ref == USLOSS_MMU_DIRTY);

    Phase5_Proc *proc = &phase5_procTbl[frame->pid % MAXPROC];
    assert(proc->pid == frame->pid);

    assert(frame->virtPage >= 0 && frame->virtPage < vm_num_virtPages);
    USLOSS_PTE *pte = &proc->page_table[frame->virtPage];
    assert(pte->incore);
    assert(pte->frame == frameID);

    assert(proc->virtPages[frame->virtPage].swapPage == -1);

    /* now, find an available swap page.  We use a clock for efficiency,
     * even though it really doesn't matter
     */

    int tries = 0;
    while (tries < swapDisk.size_pages)
    {
        int swap_pageID = swapDisk.clock;
        swapDisk.clock++;
        if (swapDisk.clock == swapDisk.size_pages)
            swapDisk.clock = 0;
        tries++;

        SwapPage *swap_page = &swapPages[swap_pageID];
        if (swap_page->inUse)
            continue;

        /* cool, we've found some swap to use!  We must grab ownership of it
         * before we release the pager lock.  Likewise, record the association
         * in the virtPage struct.
         */
        swap_page->inUse = 1;
        proc->virtPages[frame->virtPage].swapPage = swap_pageID;
        vmStats.pageOuts++;
        vmStats.freeDiskBlocks--;

        assert(frame->state == FRAME_INUSE);
        frame->state = FRAME_IO_IN_PROG;

        unlock(pagers.lock_mb);

        void *frame_ptr = physMemRegion + 4096 * frameID;
        int track = swap_pageID / 2;
        int start_block = (swap_pageID % 2) * 8;

        int status = -1;
        int rc = kernDiskWrite(frame_ptr, 1,
                               track, start_block, 8,
                               &status);
        assert(rc == 0);
        assert(status == USLOSS_DEV_OK);

        lock(pagers.lock_mb);

        /* now that we've written the page to disk, we move the frame into a
         * state that looks like a clean, not-recently-referenced disk; the
         * caller will break the association between the virtual and physical
         * pages.
         */
        frame->state = FRAME_INUSE;
        frame->ref &= ~USLOSS_MMU_DIRTY;

        return;
    }

    assert(0); // TODO: how to handle the out-of-swap condition?
}

static void pager_writeZeroesToFrame(int frameID)
{
    assert(frameID >= 0 && frameID < vm_num_physPages);
    void *frame_ptr = physMemRegion + 4096 * frameID;

    memset(frame_ptr, 0, 4096);

    vmStats.new ++;
}

static void pager_readPageFromSwap(int frameID, int swapPage)
{
    /* we don't have to do any of the bookkeeping here ... the calling function
     * sets all of the valid fields in the PTE, Frame, etc.
     */

    assert(frameID >= 0 && frameID < vm_num_physPages);
    void *frame_ptr = physMemRegion + 4096 * frameID;

    int track = swapPage / 2;
    int start_block = (swapPage % 2) * 8;

    // USLOSS_Console("%s(): frameID %d swapPage %d  -- track %d start_block %d\n", __func__, frameID, swapPage, track, start_block);
    int status = -1;
    int rc = kernDiskRead(frame_ptr, 1,
                          track, start_block, 8,
                          &status);
    assert(rc == 0);
    assert(status == USLOSS_DEV_OK);

    vmStats.pageIns++;

    return;
}
