
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

int clock_mutex;

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

/* we chain up all of the sleeping processes, ordered by their wakeup time.
 * Each struct in the queue actually resides in the stack of the process
 * that uses it; this means that we don't have to do any dynamic allocation
 * or cleanup.  Each process has also allocated a mailbox, which we will
 * use to wake it up, when the time comes.
 */
typedef struct ClockQueueEntry ClockQueueEntry;
struct ClockQueueEntry
{
    int wakeup_at;
    ClockQueueEntry *next;

    int wakeup_mailbox;
};

int curClock = 0;
ClockQueueEntry *clockQueue = NULL;

void phase4_clock_init()
{
    clock_mutex = alloc_mutex();
}

static void dumpClockQueue()
{
    USLOSS_Console("%s(): curClock %d   queue", __func__, curClock);

    ClockQueueEntry *cur = clockQueue;
    while (cur != NULL)
    {
        USLOSS_Console(" -> %d", cur->wakeup_at);
        cur = cur->next;
    }

    USLOSS_Console("\n");
}

int clockDriver(char *ignored)
{
    while (1)
    {
        int status_ignored;
        // calling wait device and say, I' m waiting on disc zero or disc one And
        //  when an interrupt shows up from that disk, then this will wake up and then decide what to do with it.
        //  Then it will interpret what the interrupt means, and then it will act.
        waitDevice(USLOSS_CLOCK_DEV, 0, &status_ignored); // Daemon will on something that blocks it

        lock(clock_mutex);
        // dumpClockQueue();

        curClock++;

        while (clockQueue != NULL && curClock == clockQueue->wakeup_at)
        {

            // USLOSS_Console("here in while before mboxsend %d %d\n", clockQueue->wakeup_mailbox, curClock);
            MboxSend(clockQueue->wakeup_mailbox, NULL, 0);
            // USLOSS_Console("here in while After mboxsend %d %d\n", clockQueue->wakeup_mailbox);

            /* the sleeping process cleans up the memory, which is actually on
             * its own stack.  But to prevent races, it will attempt to get
             * the mutex first - so it's save for us to follow to the 'next'
             * pointer even as the other process starts to wake up.
             */
            clockQueue = clockQueue->next;
        }
        assert(clockQueue == NULL || curClock < clockQueue->wakeup_at);

        unlock(clock_mutex);
    }

    assert(0); // we'll never get here
    return 0;
}

void kernSleep(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_SLEEP);
    require_kernel_mode(__func__, 0);

    int secs = (int)(long)args->arg1;
    if (secs < 0) // 0 is OK
    {
        args->arg4 = (void *)(long)-1;
        return;
    }

    /* we'll need a mailbox to go to sleep! */
    int my_mb = MboxCreate(1, 0);
    if (my_mb < 0)
    {
        USLOSS_Console("ERROR: Inside the SLEEP system call, failed to allocate a new mailbox.\n");
        USLOSS_Halt(1);
    }

    /* since this struct is local, we don't need the mutex until we try to add
     * it to the global queue.
     */
    ClockQueueEntry my_entry;

    my_entry.wakeup_at = curClock + 10 * secs;
    // USLOSS_Console("wake up at %d %d\n", curClock, my_entry.wakeup_at);
    my_entry.next = NULL;
    my_entry.wakeup_mailbox = my_mb;

    /* grab the mutex, and then put the current process on the queue */

    lock(clock_mutex);

    ClockQueueEntry *cur = clockQueue;
    ClockQueueEntry **pPrev = &clockQueue;

    while (cur != NULL && cur->wakeup_at < my_entry.wakeup_at)
    {
        pPrev = &cur->next;
        cur = cur->next;
    }

    my_entry.next = cur;
    *pPrev = &my_entry;
    // USLOSS_Console("before unlock \n");

    unlock(clock_mutex);

    /* this actually performs the sleep.  Note that we don't need to do any
     * cleanup when we wakeup, because we've already been removed from the
     * queue, and the memory that we used was on the stack.
     */
    // USLOSS_Console("after unlock \n");

    MboxRecv(my_mb, NULL, 0);
    MboxRelease(my_mb);

    args->arg4 = (void *)(int)0;
}
