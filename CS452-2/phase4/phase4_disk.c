
#include <memory.h>
#include <assert.h>

#include <usloss.h>
#include <usyscall.h> // for SYS_* constants

#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include "phase4.h"

#define TODO()                                                   \
    do                                                           \
    {                                                            \
        USLOSS_Console("TODO() at %s:%d\n", __func__, __LINE__); \
        *(char *)7 = 0;                                          \
    } while (0)

int disk_mutex;

static int alloc_mutex()
{
    return MboxCreate(1, 0);
}

static void lock(int mutex_mb) { MboxSend(mutex_mb, NULL, 0); }
static void unlock(int mutex_mb) { MboxRecv(mutex_mb, NULL, 0); }

static void require_kernel_mode(const char *caller, int require_ints_disabled)
{
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", caller);
        USLOSS_Halt(1);
    }

    if (require_ints_disabled)
        if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT)
        {
            USLOSS_Console("ERROR: Someone attempted to call %s with interrupts enabled!\n", caller);
            USLOSS_Halt(1);
        }
}

/* this models a single operation that is pending on one disk; like the clock
 * queue structures (phase4_clock.c) these will be allocated on the stack of
 * processes that are performing syscalls, and so we don't have to deal with
 * dynamic memory.
 *
 * Each op has a next field, which is used for building queues of operations.
 * We have three queues: (1) size queue; (2) cur queue; (3) next queue.
 * The size queue is unsorted (add at the head), and is entirely made up of
 * DISKSIZE ops; add new ones at the head.  Any time that we are staring a new
 * operation, we will prefer DISKSIZE operations if they exist (but we assume
 * that they will be rare).
 *
 * The cur queue and next queue will both be sorted by track (we don't bother
 * with sorting within a track); the cur queue is made up entirely of ops which
 * are on the current track (or after), and the next queue is made up of the
 * rest.  So we always pull from the cur queue until we exhaust it; then we
 * return to the head of the disk and promote the next queue to the cur.
 *
 * Each disk needs to keep track of these three queue heads, the current track
 * position, and the current request.  (The current request requires two
 * variables - the DeviceRequest struct that we give to the disk, and a
 * pointer to the current DiskOp that is running.)
 *
 * Note that the current running operation will *NOT* be on any queue; we pull
 * it off when we start it (since it would be annoying to figure out which
 * queue to remove it from, if we did it late).
 */
typedef struct DiskOp DiskOp; // represents one syscall
struct DiskOp
{
    DiskOp *next;

    int op; // SYS_DISKSIZE, SYS_DISKREAD, SYS_DISKWRITE

    // these fields only mean anything for READ/WRITE
    int track;
    int startSector, sectorLen;
    char *buf;

    // these fields only mean anything for SIZE
    int retval_numTracks;

    // all pending operations will have a mailbox assigned, for wakeup.  The
    // syscall allocates it and frees it when the op completes.
    int wakeup_mb;
};

typedef struct Disk Disk; // represents one disk
struct Disk
{
    int unit;

    int curTrack; // my belief about the disk's actual state.  If a SEEK is
                  // currently running, then this is the track that we are going
                  // towards, even if we're not there yet.

    // normally, this is NULL iff busy==0 - but there's a transient state,
    // during read/write operations, where an existing request has finished
    // but we haven't yet set up the next one.  In that case, curOp != NULL
    // but busy==0
    DiskOp *curOp;

    int busy;
    USLOSS_DeviceRequest req; // valid only if busy=1

    DiskOp *size_queue;
    DiskOp *cur_queue;
    DiskOp *next_queue;

    /* NOTICE - not a pointer, a real struct! */
    DiskOp seekOp;
} disks[2];

void phase4_disk_init()
{
    disk_mutex = alloc_mutex();

    memset(disks, 0, sizeof(disks));
    for (int i = 0; i < 2; i++)
        disks[i].unit = i;
}

static void process_one_disk_interrupt(Disk *disk, int status);
static void schedule_disk_request(Disk *disk); // unconditional, *MUST* schedule
static void disk_dispatcher(Disk *disk);       // only schedule if not already busy

int diskDriver(char *arg)
{
    int unit = (int)(long)arg;
    assert(0 <= unit && unit < 2);
    Disk *disk = &disks[unit];

    while (1)
    {
        int status;
        waitDevice(USLOSS_DISK_DEV, unit, &status);

        lock(disk_mutex);

        // status gathered during an interrupt should never be BUSY - I think?
        assert(status != USLOSS_DEV_BUSY);

        process_one_disk_interrupt(disk, status);
        disk_dispatcher(disk); // will add a new request if needed

        unlock(disk_mutex);
    }

    assert(0); // we'll never get here
    return 0;
}

static void process_one_disk_interrupt(Disk *disk, int status)
{
    assert(status == USLOSS_DEV_READY || status == USLOSS_DEV_ERROR);

    // we should only get interrupts when we have an operation in progress
    assert(disk->busy);

    // USLOSS_Console("%s(): disk %d   opr %d   status %d\n", __func__, disk->unit, disk->req.opr, status);

    // if the current operation was an error, then we should report status
    // immediately, no matter what.
    if (status == USLOSS_DEV_ERROR)
    {
        MboxSend(disk->curOp->wakeup_mb, &status, sizeof(status));

        disk->busy = 0;
        disk->curOp = NULL;
        return;
    }

    // the request should be DISK_TRACKS iff curOp is DISKSIZE
    assert((disk->req.opr == USLOSS_DISK_TRACKS) ==
           (disk->curOp->op == USLOSS_DISK_TRACKS));

    if (disk->req.opr == USLOSS_DISK_TRACKS)
    {
        /* remember that, when we set up the request, we pointed the
         * request at the 'retval_numTracks' field of the op.  So we
         * don't need to take any action other than alerting the
         * requesting process that their operation completed OK.
         */
        int mb_rc = MboxSend(disk->curOp->wakeup_mb, &status, sizeof(status));
        assert(mb_rc == 0);

        disk->busy = 0;
        disk->curOp = NULL;
        return;
    }

    /* handling read/write operations are more complex, because they
     * can require a chain of requests (and a seek might be first!).
     * Note that, in the case of a seek, 'curOp' will be READ/WRITE
     * but the request will be SEEK.
     */
    assert(disk->curOp->op == USLOSS_DISK_READ || disk->curOp->op == USLOSS_DISK_WRITE);

    /* the simplest case, for READ/WRITE, is that the operation has
     * just completed.  But obviously that isn't true of the request
     * was SEEK, or if multiple blocks remain in the Op.
     */
    if (disk->req.opr != USLOSS_DISK_SEEK && disk->curOp->sectorLen == 1)
    {
        if (disk->curOp->op == USLOSS_DISK_READ)
            assert(disk->req.opr == USLOSS_DISK_READ);
        else if (disk->curOp->op == USLOSS_DISK_WRITE)
            assert(disk->req.opr == USLOSS_DISK_WRITE);
        else
            assert(0);

        /* we decrement sectorLen over time, so we don't know if this
         * was actually a 1-sector op, or if it was the last sector
         * of many.  But who cares?
         */
        // USLOSS_Console("%s(): Sending wakeup at completion of a READ/WRITE   mbID %d\n", __func__, disk->curOp->wakeup_mb);
        int mb_rc = MboxSend(disk->curOp->wakeup_mb, &status, sizeof(status));
        // USLOSS_Console("      %d\n", mb_rc);
        assert(mb_rc == 0);

        disk->busy = 0;
        disk->curOp = NULL;
        return;
    }

    /* if we just completed a seek, then update our belief about what the
     * current track is.  Otherwise, we just completed a READ/WRITE request
     * in a multi-block op, and we want to advance the current request to
     * the next block.
     *
     * BUGFIX: Backwards seeks don't have to go to 0; they can go anywhere.
     *         We'll trust that the dispatcher made a good decision.
     *
     * BUGFIX: Testcases actually *do* perform read/write ops that cross
     *         track boundaries.
     */
    if (disk->req.opr == USLOSS_DISK_SEEK)
    {
        int destTrack = (int)(long)disk->req.reg1;
        assert(disk->curTrack == destTrack); // we should have updated it before

        /* seeks should *ONLY* happen to enable READ/WRITE */
        assert(disk->curOp->op == USLOSS_DISK_READ || disk->curOp->op == USLOSS_DISK_WRITE);
        assert(disk->curTrack == disk->curOp->track);
    }
    else
    {
        assert(disk->curOp->op == USLOSS_DISK_READ || disk->curOp->op == USLOSS_DISK_WRITE);
        assert(disk->curOp->sectorLen > 1);
        disk->curOp->sectorLen--;
        disk->curOp->startSector++;
        disk->curOp->buf += 512;
        /* testcases *DO* perform read/write ops that cross track boundaries.
         * So if we have just hit the end of the track but there's more to do,
         * then advance the target track; this means that the next operation
         * that we perform will (quite naturally) be a SEEK
         */
        assert(disk->curOp->startSector <= 16);
        if (disk->curOp->startSector == 16)
        {
            disk->curOp->track++;
            disk->curOp->startSector = 0;
        }
    }

    /* we just completed a SEEK or READ/WRITE, and we have another request to
     * schedule for this op.  We do this immediately.  But we have to let the
     * function know that we're clear that the old request actually *IS* done.
     */
    disk->busy = 0;
    schedule_disk_request(disk);
}

static void schedule_disk_request(Disk *disk)
{
    /* this function is only called when we are *CERTAIN* that it's time to
     * schedule a new request, and somebody has already decided which op it's
     * part of.
     */
    assert(disk->busy == 0);
    assert(disk->curOp != NULL);

    memset(&disk->req, 0, sizeof(disk->req));

    /* SIZE operations are pretty easy to do */
    if (disk->curOp->op == USLOSS_DISK_TRACKS)
    {
        disk->req.opr = USLOSS_DISK_TRACKS;
        disk->req.reg1 = &disk->curOp->retval_numTracks;
        goto tell_device;
    }

    assert(disk->curOp->op == USLOSS_DISK_READ || disk->curOp->op == USLOSS_DISK_WRITE);

    /* SEEK operations are automatically triggered */
    if (disk->curOp->track != disk->curTrack)
    {
        /* backward seeks, to the head of the disk, are only allowed in very
         * limited circumstances.  These only happen when the dispatcher
         * chooses to cycle us back to the head of the disk.
         */
        if (disk->curOp->track < disk->curTrack)
        {
            assert(disk->next_queue == NULL);
            assert(disk->cur_queue == NULL || disk->cur_queue->track >= disk->curOp->track);
        }

        /* BUGFIX: We need to update the current track here.  Imagine a scenario
         *         where an operation is queued up, and we hit it, and we notice
         *         that we must seek ahead a bit, to handle it.  We schedule that
         *         SEEK operation.  But while we are waiting for it to complete,
         *         we get *another* incoming operation, which is between the old and
         *         new locations; we would put it on the "current" list, instead of
         *         the "next" list, and thus end up doing a backwards seek.  By
         *         updating the seek location now, we know where to put new,
         *         incoming operations.
         */
        disk->curTrack = disk->curOp->track;

        disk->req.opr = USLOSS_DISK_SEEK;
        disk->req.reg1 = (void *)(long)disk->curOp->track;
        goto tell_device;
    }

    /* if we get here, we actually need to set READ or WRITE */
    if (disk->curOp->op == USLOSS_DISK_READ)
        disk->req.opr = USLOSS_DISK_READ;
    else
        disk->req.opr = USLOSS_DISK_WRITE;

    disk->req.reg1 = (void *)(long)disk->curOp->startSector;
    disk->req.reg2 = disk->curOp->buf;

tell_device:
    disk->busy = 1;

    // USLOSS_Console("%s(): Sending a disk request   unit %d   op %d   reg1 %p   reg2 %p\n", __func__, disk->unit, disk->req.opr, disk->req.reg1, disk->req.reg2);
    int dev_rc = USLOSS_DeviceOutput(USLOSS_DISK_DEV, disk->unit, &disk->req);
    if (dev_rc != USLOSS_DEV_OK)
    {
        USLOSS_Console("ERROR: Failed to send a disk operation to unit %d.\n", disk->unit);
        USLOSS_Console("       req.opr  = %d\n", disk->req.opr);
        USLOSS_Console("       req.reg1 = %p\n", disk->req.reg1);
        USLOSS_Console("       req.reg2 = %p\n", disk->req.reg2);
        USLOSS_Halt(1);
    }
}

static void disk_dispatcher(Disk *disk)
{
    /* this function sets up a *NEW* operation, if there isn't one defined
     * already.
     */
    if (disk->busy)
        return; // nothing to do
    assert(disk->curOp == NULL);

    /* if there are any pending size operations, then we'll schedule them first */
    if (disk->size_queue != NULL)
    {
        disk->curOp = disk->size_queue;
        disk->size_queue = disk->size_queue->next;
        schedule_disk_request(disk);
        return;
    }

    /* if there are any operations in the current queue, then get started with
     * the next of them; we'll get to the 'next' queue when this has been
     * flushed out
     *
     * UPDATED: include the next queue as well.
     */
    if (disk->cur_queue != NULL || disk->next_queue != NULL)
    {
        if (disk->cur_queue == NULL)
        {
            /* the request schedule will cause a backwards seek */
            disk->cur_queue = disk->next_queue;
            disk->next_queue = NULL;
        }

        disk->curOp = disk->cur_queue;
        disk->cur_queue = disk->cur_queue->next;
        schedule_disk_request(disk);
        return;
    }

    // USLOSS_Console("NOTE: Disk %d has gone idle.\n", disk->unit);
}

void kernDiskIO(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_DISKREAD || args->number == SYS_DISKWRITE);
    require_kernel_mode(__func__, 0);

    char *buf = args->arg1;
    int sectors = (int)(long)args->arg2;
    int track = (int)(long)args->arg3;
    int startSector = (int)(long)args->arg4;
    int unit = (int)(long)args->arg5;
    if (buf == NULL || sectors < 1 || track < 0 ||
        startSector < 0 || startSector >= 16 || unit < 0 || unit >= 2)
    {
        args->arg1 = (void *)(long)-1;
        args->arg4 = (void *)(long)-1;
        return;
    }
    Disk *disk = &disks[unit];

    DiskOp myOp;
    int status;
    memset(&myOp, 0, sizeof(myOp));

    if (args->number == SYS_DISKREAD)
        myOp.op = USLOSS_DISK_READ;
    else
        myOp.op = USLOSS_DISK_WRITE;

    myOp.track = track;
    myOp.startSector = startSector;
    myOp.sectorLen = sectors;
    myOp.buf = buf;

    myOp.wakeup_mb = MboxCreate(1, sizeof(status));
    if (myOp.wakeup_mb < 0)
    {
        args->arg1 = (void *)(long)-1;
        args->arg4 = (void *)(long)-1;
        return;
    }

    /* if we get here, then the parameters look reasonable.  So arg4 will
     * definitely be 0 - even if the operation fails.
     */
    args->arg4 = (void *)(long)0;

    lock(disk_mutex);

    DiskOp *cur;
    DiskOp **pPrev;

    if (myOp.track >= disk->curTrack)
    {
        cur = disk->cur_queue;
        pPrev = &disk->cur_queue;
    }
    else
    {
        cur = disk->next_queue;
        pPrev = &disk->next_queue;
    }

    while (cur != NULL && cur->track < myOp.track)
    {
        pPrev = &cur->next;
        cur = cur->next;
    }

    myOp.next = cur;
    *pPrev = &myOp;

    unlock(disk_mutex);

    /* do we need to start up a request?  This will do something only if the
     * disk is idle.
     */
    disk_dispatcher(disk);

    /* wait until the disk answers our query */
    // USLOSS_Console("%s(): Blocking   mbID %d\n", __func__, myOp.wakeup_mb);
    MboxRecv(myOp.wakeup_mb, &status, sizeof(status));
    // USLOSS_Console("%s(): Awake!     mbID %d\n", __func__, myOp.wakeup_mb);

    /* free up the mailbox, deliver the result into the args, and we're done */
    MboxRelease(myOp.wakeup_mb);

    // USLOSS_Console("%s(): Returning\n", __func__);
    args->arg1 = (void *)(long)status;
}

void kernDiskSize(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_DISKSIZE);
    require_kernel_mode(__func__, 0);

    int unit = (int)(long)args->arg1;
    if (unit < 0 || unit >= 2)
    {
        args->arg1 = (void *)(long)-1;
        args->arg4 = (void *)(long)-1;
        return;
    }
    Disk *disk = &disks[unit];

    DiskOp myOp;
    int status;
    memset(&myOp, 0, sizeof(myOp));
    myOp.op = USLOSS_DISK_TRACKS;

    myOp.wakeup_mb = MboxCreate(1, sizeof(status));
    if (myOp.wakeup_mb < 0)
    {
        args->arg1 = (void *)(long)-1;
        args->arg4 = (void *)(long)-1;
        return;
    }

    lock(disk_mutex);

    myOp.next = disk->size_queue;
    disk->size_queue = &myOp;

    unlock(disk_mutex);

    /* do we need to start up a request?  This will do something only if the
     * disk is idle.
     */
    disk_dispatcher(disk);

    /* wait until the disk answers our query */
    MboxRecv(myOp.wakeup_mb, &status, sizeof(status));
    assert(status == USLOSS_DEV_OK);

    /* free up the mailbox, deliver the result into the args, and we're done */
    MboxRelease(myOp.wakeup_mb);

    args->arg1 = (void *)(long)512;
    args->arg2 = (void *)(long)16;
    args->arg3 = (void *)(long)myOp.retval_numTracks;
    args->arg4 = (void *)(long)0;
}
