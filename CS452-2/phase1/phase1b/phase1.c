#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "usloss.h"
#include "phase1.h"

typedef struct ProcTableEntry
{
    /* DESIGN CHANGE re: refcount
     *
     * Originally, we had a refcount on the process table entries, so that we
     * would not clean one up until join() had seen the result *AND* all of the
     * zappers had seen it, too.  But we eventually realized that the zappers
     * had no need to see the process table entry after they wake up; quit()
     * walks the list, and they can simply ignore the PTE when they awake.
     * Thus, the *ONLY* process that needs to look at another process's PTE
     * *AFTER* the latter has ended is join(), and so we don't need the
     * refcount after all.
     *
     * So now, we don't have a refcount, and join() does the cleanup (of the
     * process stack) which was previously performed by the decrement_refcount()
     * function.
     */

    int pid; // 0 means "free for allocation"

    char name[MAXNAME + 1];

    char *stack;

    int (*startFunc)(char *);
    char *start_arg;

    /* child tree.  parent is the pointer to the parent structure.  Each
     * parent has a pointer to its first child, and a singly-linked list
     * through the 'sibling' pointers.
     */
    struct ProcTableEntry *parent;
    struct ProcTableEntry *children;
    struct ProcTableEntry *sibling;

    /* set to -1 on init, set to non-negative when the child calls
     * quit().
     */
    int status;

    /* this works exactly like the child/sibling fields above, except that it
     * is a list of processes which have attempted to zap the current process,
     * and thus want to be unblocked when the process dies.
     */
    struct ProcTableEntry *zapper_first;
    struct ProcTableEntry *zapper_next;

    /* this value is 0 to indicate the RUNNABLE state, and nonzero for
     * BLOCKED.  Note that this variable doesn't differentiate between
     * runnable and running; that much is handled by the dispatcher.
     *
     * Block states >10 are used by the user (arguments to blockMe());
     * the lesser values are used by the phase 1 internal mechanisms
     */
    int runnable_status;
#define WAITING_FOR_CHILD_TO_QUIT 1
#define WAITING_FOR_ZAP_TARGET_TO_QUIT 2

    /* range: 1-7 inclusive */
    int priority;
    struct ProcTableEntry *queue_next;

    /* this is for USLOSS's context-switching feature */
    USLOSS_Context context;

    /* clock variables */
    int timeSliceStart; // -1 if not the current process
    int totalTimeConsumed;
} ProcTableEntry;

static ProcTableEntry procTable[MAXPROC];
static int next_pid = 1;

static ProcTableEntry *current = NULL;

static int debug = 0;

typedef struct ProcPriorityQueue
{
    ProcTableEntry *head;
    ProcTableEntry **tail_next; // must point to 'head' if 'head==NULL'
} ProcPriorityQueue;

static ProcPriorityQueue proc_queues[8]; // [0] will never be used

static void require_kernel_mode(const char *func, int require_ints_disabled);
static unsigned int disable_interrupts();
static void restore_interrupts(unsigned int old_psr);

static ProcTableEntry *procTable_find_empty_slot(void);
static void init_new_process_table_entry(ProcTableEntry *);
static void remove_child_from_parent(ProcTableEntry *parent, ProcTableEntry *child);
static ProcTableEntry *get_zombie_child(void);
static void wake_up(int pid, int curStatus);

static void clockHandler(int dev, void *arg);

static void dispatcher(void);
static void dispatcher_removeCurrent(void);

static int init_main(char *);
static int testcase_main_wrapper(char *);
static int sentinel_main(char *);

static void launch(void); // main function, passed to USLOSS_ContextInit.  Will call the startFunc for the current process.

static char init_stack[USLOSS_MIN_STACK];
static char sentinel_stack[USLOSS_MIN_STACK];

/* phase1_init()
 *
 * Called by: Testcase
 * Context:   Before any processes dispatched
 * Mode:      Kernel
 *
 * Initializes the data structures for the OS, at least for this phase.  Creates
 * init (PID 1) and sentinel (PID 2), but neither is the "current" process yet,
 * because we haven't done any dispatch.
 */
void phase1_init()
{
    memset(procTable, 0, sizeof(procTable));
    memset(init_stack, 0, sizeof(init_stack));
    memset(sentinel_stack, 0, sizeof(sentinel_stack));

    /* create the init process (max priority) and the sentinel process (minimum
     * priority)
     */
    procTable[1].pid = 1;
    strcpy(procTable[1].name, "init");
    procTable[1].startFunc = init_main;
    procTable[1].start_arg = NULL;
    procTable[1].parent = NULL;
    procTable[1].children = NULL;
    procTable[1].sibling = NULL;
    procTable[1].status = -1;
    procTable[1].zapper_first = NULL;
    procTable[1].zapper_next = NULL;
    procTable[1].runnable_status = 0;
    procTable[1].priority = 6; // one better than minimum
    procTable[1].queue_next = NULL;

    // to install an interrupt handler for the clock interrupt, the following statement could be used:
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
}

/* startProcesses()
 *
 * Called by: Testcase
 * Context:   Before any processes dispatched
 * Mode:      Kernel
 *
 * Called after all of the other phases are initialized; this should switch to
 * 'init'.  This *NEVER* returns.
 */
void startProcesses()
{
    USLOSS_ContextInit(&procTable[1].context,
                       init_stack, sizeof(init_stack),
                       NULL,
                       launch);

    /* initailize the run queues to empty.  We'll add two procs in just a moment. */
    proc_queues[0].head = NULL;
    proc_queues[0].tail_next = NULL;
    for (int i = 1; i <= 7; i++)
    {
        proc_queues[i].head = NULL;
        proc_queues[i].tail_next = &proc_queues[i].head;
    }

    /* although we haven't run the dispatcher yet, it's OK to add the two
     * processes to the run queue.
     */
    wake_up(1, 0); // init

    dispatcher(); // this will *NEVER* return
}

/* helper function */
static void require_kernel_mode(const char *func, int require_ints_disabled)
{
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_Console("ERROR: Someone attempted to call %s while in user mode!\n", func);
        USLOSS_Halt(1);
    }

    if (require_ints_disabled)
        if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT)
        {
            USLOSS_Console("ERROR: Someone attempted to call %s with interrupts enabled!\n", func);
            USLOSS_Halt(1);
        }
}

/* helper function */
static unsigned int disable_interrupts()
{
    unsigned int old_psr = USLOSS_PsrGet();

    unsigned int old_flags = old_psr & 0x3;
    unsigned int new_flags = USLOSS_PSR_CURRENT_MODE; // CURRENT_INT bit set to 0
    unsigned int new_psr = (old_flags << 2) | new_flags;

    int ok = USLOSS_PsrSet(new_psr);
    assert(ok == USLOSS_DEV_OK);

    return old_psr;
}

/* helper function */
static void restore_interrupts(unsigned int old_psr)
{
    int ok = USLOSS_PsrSet(old_psr);
    assert(ok == USLOSS_DEV_OK);
}

/* fork1()
 *
 * Context:   Process context
 * Mode:      Kernel
 *
 * Creates a new process.  Does *NOT* actually fork the current process;
 * instead, it spawns a new one (thus all the arguments).
 */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stackSize, int priority)
{
    if (current == NULL)
    {
        USLOSS_Console("ERROR: fork1() was called when there was no current process.  The most likely cause for this is calling fork1() inside one of the phaseX_init() functions, which run before startProcesses().  Process name: '%s'\n", name);
        USLOSS_Halt(1);
    }

    if (strlen(name) > MAXNAME)
        return -1;
    if (stackSize < USLOSS_MIN_STACK)
        return -2;

    /* priorities 6,7 are valid, but reserved for init,sentinel; they
     * cannot be used by any fork()ed process (except "sentinel")
     */
    if (strcmp(name, "sentinel") == 0 && priority == 7)
    { /* NOP */
    }
    else if (priority < 1 || priority > 5)
        return -1;

    if (startFunc == NULL || name == NULL)
        return -1;

    /* ---- BASIC SANITY CHECKS DONE, FUNCTION MAIN BODY BEGINS ---- */

    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    ProcTableEntry *child = procTable_find_empty_slot();
    if (child == NULL)
    {
        restore_interrupts(psr_save);
        return -1;
    }
    assert(child->pid == 0);

    init_new_process_table_entry(child);
    assert(child->pid != 0);             // slot allocated
    assert(child->status == -1);         // not quit() yet
    assert(child->runnable_status == 0); // runnable

    assert(strlen(name) < sizeof(child->name));
    strcpy(child->name, name);

    child->stack = malloc(stackSize);
    assert(child->stack != NULL);

    child->startFunc = startFunc;
    child->start_arg = arg;

    child->priority = priority;

    mmu_init_proc(child->pid);
    USLOSS_ContextInit(&child->context,
                       child->stack, stackSize,
                       NULL,
                       launch);

    wake_up(child->pid, 0); // put the new proc on the runqueue

    /* do we need to switch to the child process? */
    dispatcher();

    restore_interrupts(psr_save);
    return child->pid;
}

/* helper for fork1() */
static ProcTableEntry *procTable_find_empty_slot()
{
    // we will search by progressively incrementing the pid until we find an
    // available slot.  We stop if we've looped all the way around.

    int stop_at = next_pid + MAXPROC;
    while (procTable[next_pid % MAXPROC].pid != 0)
    {
        next_pid++;
        if (next_pid == stop_at)
            return NULL;
    }

    return &procTable[next_pid % MAXPROC];
}

/* helper for fork1() */
static void init_new_process_table_entry(ProcTableEntry *slot)
{
    int pid = next_pid;
    next_pid++;

    assert(slot == &procTable[pid % MAXPROC]);

    memset(slot, 0, sizeof(*slot));

    slot->pid = pid;

    ProcTableEntry *parent = current;

    // add 'slot' at the head of the child list.  tail would be more
    // elegant, but head is O(1)
    slot->parent = parent;
    if (parent != NULL)
    {
        slot->sibling = parent->children;
        parent->children = slot;
    }

    slot->children = NULL;

    slot->status = -1;
    // runnable has been memset(0)
}

/* join()
 *
 * Context:   Process context
 * Mode:      Kernel
 *
 * Blocks the current process until one child process has quit().
 */
int join(int *status)
{
    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    if (status == NULL)
    {
        restore_interrupts(psr_save);
        return -3; // not specified in the spec
    }

    if (current->children == NULL)
    {
        restore_interrupts(psr_save);
        return -2;
    }

    ProcTableEntry *child = NULL;
    while (child == NULL)
    {
        child = get_zombie_child();
        if (child == NULL)
        {
            current->runnable_status = WAITING_FOR_CHILD_TO_QUIT;
            dispatcher();
        }
    }
    assert(child != NULL);
    assert(child->parent == current);
    assert(child->status != -1);

    int child_pid = child->pid;

    remove_child_from_parent(current, child);
    *status = child->status;

    /* clean up the stack.  We can do that here because the process is
     * certainly; look at quit(), and you will see that it disables
     * interrupts, and thus races aren't a problem.
     *
     * Once we've done that, then we can free the PTE for real.
     */
    free(child->stack);
    memset(child, 0, sizeof(*child));

    restore_interrupts(psr_save);
    return child_pid;
}

/* helper for join() */
static ProcTableEntry *get_zombie_child()
{
    assert(current->children != NULL);

    ProcTableEntry *cur = current->children;
    while (cur != NULL && cur->status == -1)
        cur = cur->sibling;

    // it's entirely possible that we might end up with cur=NULL; this happens
    // if none of the children have finished yet.

    return cur;
}

/* helper for join() */
static void remove_child_from_parent(ProcTableEntry *parent, ProcTableEntry *child)
{
    if (debug)
    {
        USLOSS_Console("remove_child_from_parent: parent: %d child: %d\n", parent->pid, child->pid);
        dumpProcesses();
    }

    if (parent->children == child)
        parent->children = child->sibling;
    else
    {
        if (parent->children == child)
            parent->children = child->sibling;
        else
        {
            ProcTableEntry *cur = parent->children;
            while (cur->sibling != NULL && cur->sibling != child)
                cur = cur->sibling;
            assert(cur->sibling != NULL); // we must never call this if we're not in the child list

            cur->sibling = cur->sibling->sibling;
        }
    }

    child->parent = NULL;
    child->sibling = NULL;
}

/* quit()
 *
 * Context:   Process context
 * Mode:      Kernel
 *
 * Terminates the current process.  If a parent is waiting in a join(), then it
 * is awoken - although if multiple children quit() in rapid succession, it's
 * the parent will notice them in unpredictable order.  Also wakes up any
 * process(es) that are blocked, waiting to zap the current process.
 */
void quit(int status)
{
    require_kernel_mode(__func__, 0);
    disable_interrupts(); // we don't have to save the old PSR, because we're not going to restore it

    if (current->children != NULL)
    {
        USLOSS_Console("ERROR: Process pid %d called quit() while it still had children.\n", getpid());
        USLOSS_Halt(1);
    }

    /* this is special-case code, which I don't require students to implement.
     * But it exists because we want the quit() path, and the return path,
     * from testcase_main to do the same thing - namely, to terminate the
     * simulation.
     *
     * I can't rely on this to be in place for Phase 1 testcases (where they
     * run against student code), but I can rely on it from Phase 2 and
     * following.
     */
    if (strcmp(current->name, "testcase_main") == 0)
    {
        if (status != 0)
            USLOSS_Console("ERROR: testcase_main() returned nonzero: status=%d\n", status);
        USLOSS_Halt(status);
        // Halt() never returns.  TODO: set attribute no-return on that function in usloss.h?
    }

    assert(0 <= status && status <= 255);

    mmu_quit(current->pid);

    current->status = status;

    /* the parent certainly must join() on this child.  But maybe it hasn't
     * started that, yet.
     */
    if (current->parent->runnable_status == WAITING_FOR_CHILD_TO_QUIT)
        wake_up(current->parent->pid, WAITING_FOR_CHILD_TO_QUIT);

    while (current->zapper_first != NULL)
    {
        ProcTableEntry *zapper = current->zapper_first;
        current->zapper_first = zapper->zapper_next;
        zapper->zapper_next = NULL;

        assert(zapper->runnable_status == WAITING_FOR_ZAP_TARGET_TO_QUIT);
        wake_up(zapper->pid, WAITING_FOR_ZAP_TARGET_TO_QUIT);
    }

    // NOTE: We do *not* free the stack here!  We cannot do that here because
    //       we are still using it!  But we'll free it when join() notices that
    //       the process is dead.  (Such a process might be runnable right now,
    //       because we woke it up, above, but because we hold interrupts it
    //       won't run yet.)

    // there is no longer any current process, at least until the dispatcher runs.
    current = NULL;

    /* ---- dispatcher() will never return, since we're calling it with no
     *      current process.
     */
    dispatcher();

    /* this solves a gcc warning; gcc doesn't realize that, when we call
     * dispatcher(), the function will never return.  Adding this failed
     * assert makes that clear.
     */
    assert(0);
}

/* zap()
 *
 * Context:   Process context
 * Mode:      Kernel
 *
 * Marks the target process as 'zapped' - meaning that we request that it
 * die as soon as is practical.  This does *NOT* automatically cause anything
 * to happen - not even waking up the process if it is blocked inside join()
 * or zap().  (This makes it unlike UNIX kill, which will wake up the target
 * process unless it's blocked on I/O.)  zap() blocks the calling process
 * until the target dies, so it can be used like a join() that crosses process
 * tree boundaries.  Any number of processes can be blocked, waiting for a
 * common target to die.
 */
void zap(int other_pid)
{
    require_kernel_mode(__func__, 0);

    if (other_pid <= 0)
    {
        USLOSS_Console("ERROR: Attempt to zap() a PID which is <=0.  other_pid = %d\n", other_pid);
        USLOSS_Halt(1);
    }

    if (other_pid == 1)
    {
        USLOSS_Console("ERROR: Attempt to zap() init.\n");
        USLOSS_Halt(1);
    }
    if (other_pid == getpid())
    {
        USLOSS_Console("ERROR: Attempt to zap() itself.\n");
        USLOSS_Halt(1);
    }

    unsigned int psr_save = disable_interrupts();

    ProcTableEntry *other = &procTable[other_pid % MAXPROC];

    if (other->pid != other_pid)
    {
        USLOSS_Console("ERROR: Attempt to zap() a non-existent process.\n");
        USLOSS_Halt(1);
    }

    if (other->status != -1)
    {
        USLOSS_Console("ERROR: Attempt to zap() a process that is already in the process of dying.\n");
        USLOSS_Halt(1);
    }

    /* record that we are waiting for the process to die.  But remember, it's a
     * linked list, so we will insert at the head.
     */
    current->zapper_next = other->zapper_first;
    other->zapper_first = current;

    /* we need to block.  We will only wake up when the target process dies, so
     * we don't need a loop here.
     */
    current->runnable_status = WAITING_FOR_ZAP_TARGET_TO_QUIT;
    dispatcher();

    restore_interrupts(psr_save);
}

/* isZapped()
 *
 * Context:   Process context
 * Mode:      Kernel
 *
 * Returns 1 if any process has attempted to zap the current process.
 */
int isZapped()
{
    /* see getpid() to understand why interrupts are not required here. */
    if (current->zapper_first == NULL)
        return 0;
    else
        return 1;
}

int getpid()
{
    /* no interrupts are needed.  Yes, it is certainly true that we might get
     * interrupted in between reading 'current' and using it.  And that
     * interrupt might cause a context switch which would end up changing
     * 'current'.  But we would not run again until we had been context
     * switched *back* to this process - meaning that 'current' will be
     * restored to what I saw before.  So actually, this *is* thread-safe,
     * even if it seems at first like it might not be!
     *
     * In fact, this argument holds for nearly all code ... except (maybe)
     * for the dispatcher.  At time of writing, I haven't figured out if
     * it holds for the dispatcher, too...although I'm betting that it does.
     */

    return current->pid;
}

void dumpProcesses()
{
    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    USLOSS_Console(" PID  PPID  NAME              PRIORITY  STATE\n");

    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTableEntry *slot = &procTable[i];
        if (slot->pid == 0)
            continue;

        int ppid = (slot->parent == NULL) ? 0 : slot->parent->pid;
        USLOSS_Console("%4d  %4d  %-17s %-10d", slot->pid, ppid, slot->name, slot->priority);

        if (slot->status != -1)
            USLOSS_Console("Terminated(%d)\n", slot->status);
        else if (slot->runnable_status == WAITING_FOR_CHILD_TO_QUIT)
            USLOSS_Console("Blocked(waiting for child to quit)\n");
        else if (slot->runnable_status == WAITING_FOR_ZAP_TARGET_TO_QUIT)
            USLOSS_Console("Blocked(waiting for zap target to quit)\n");
        else if (slot->runnable_status != 0)
            USLOSS_Console("Blocked(%d)\n", slot->runnable_status);
        else if (slot == current)
            USLOSS_Console("Running\n");
        else
            USLOSS_Console("Runnable\n");
    }

    restore_interrupts(psr_save);
}


void blockMe(int newStatus)
{
    assert(newStatus > 10);

    /* I've had students, in Phase 4, try to block the start_service_processes()
     * function, which kills the whole system because init doesn't get to finish
     * bootstrap.  This code exists to catch that strange case.
     *
     * The only case in which init should block is in a join().
     */
    if (getpid() == 1)
    {
        USLOSS_Console("ERROR: You attempted to perform a blocking operation from inside init.  This is illegal!  If you must block, fork off a child process which will block - never block init itself!!!\n");
        USLOSS_Halt(1);
    }

    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    assert(current->runnable_status == 0);

    current->runnable_status = newStatus;
    dispatcher();

    restore_interrupts(psr_save);
}

int unblockProc(int other_pid)
{
    if (other_pid < 0)
        return -2;

    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    ProcTableEntry *other = &procTable[other_pid % MAXPROC];

    if (other->pid != other_pid || other_pid == getpid() || other->status != -1 ||
        (other->runnable_status > 0 && other->runnable_status <= 10))
    {
        restore_interrupts(psr_save);
        return -2;
    }

    if (other->runnable_status == 0)
    {
        USLOSS_Console("ERROR: Attempting to wake up process %d, which is already runnable, from process %d.\n", other_pid, getpid());
        dumpProcesses();
        USLOSS_Halt(1);
    }

    wake_up(other_pid, other->runnable_status);
    dispatcher();

    restore_interrupts(psr_save);
    return 0;
}

/* wake_up(pid, old_status)
 *
 * Called, by other functions in this file, when a process becomes runnable.
 * This adds the process to the proper queue; it does *NOT* actually call
 * the dispatcher.  This will also change the runnable_status field in the
 * process to 0 (RUNNABLE).
 *
 * 2nd parameter is the current status of the process (for sanity checking).
 * This will be nonzero in every case but one: when fork1() creates a new
 * process, its runnable_status is already zero, but we want to get it added
 * to the proper runqueue.
 *
 * Must be called with interrupts disabled.
 */
static void wake_up(int pid, int curStatus)
{
    assert(pid > 0);

    if (debug)
        USLOSS_Console("wake_up(%d, %d)\n", pid, curStatus);

    ProcTableEntry *other = &procTable[pid % MAXPROC];

    if (other->pid != pid)
    {
        USLOSS_Console("The PID %d that you were attempting to wake up (that is, to make runnable) was not a valid PID.  The given slot in the process table instead held PID %d.\n", pid, other->pid);
        USLOSS_Halt(1);
    }

    if (other->status != -1)
    {
        USLOSS_Console("The PID %d that you were attempting to wake up (that is, to make runnable) has already died, but has not yet been join()ed.  Process termination status was %d.\n", pid, other->status);
        USLOSS_Halt(1);
    }

    assert(other->runnable_status == curStatus);

    other->runnable_status = 0;

    assert(other->priority > 0 && other->priority <= 7);
    ProcPriorityQueue *queue = &proc_queues[other->priority];

    assert(*(queue->tail_next) == NULL);
    *(queue->tail_next) = other;
    queue->tail_next = &other->queue_next;
    assert(*(queue->tail_next) == NULL);
}

int readCurStartTime()
{
    /* see getpid() to understand why interrupts are not required here. */
    return current->timeSliceStart;
}

int currentTime()
{
    /* I don't care about interrupts for this process.  If it gets interrupted,
     * then it will either give the correct value for *before* the interrupt,
     * or after.  But if the caller hasn't disabled interrupts, then either one
     * is valid
     */

    int retval;

    int usloss_rc = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &retval);
    assert(usloss_rc == USLOSS_DEV_OK);

    return retval;
}

int readtime()
{
    /* I don't need interrupts to protect the 'current' variable, but I need
     * to protect access to the readCurStartTime(), compared to currentTime().
     */

    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    int retval = current->totalTimeConsumed + (currentTime() - readCurStartTime());

    restore_interrupts(psr_save);
    return retval;
}

void timeSlice()
{
    /* see getpid() to understand why interrupts are not required here. */
    if (currentTime() - current->timeSliceStart > 80 * 1000)
        dispatcher();
}

static void clockHandler(int dev, void *arg)
{
    if (debug)
    {
        USLOSS_Console("clockHandler(): PSR = %d\n", USLOSS_PsrGet());
        USLOSS_Console("clockHandler(): currentTime = %d\n", currentTime());
    }

    /* make sure to call this first, before timeSlice(), since we want to do
     * the Phase 2 related work even if process(es) are chewing up lots of
     * CPU.
     */
    phase2_clockHandler();

    // call the dispatcher if the time slice has expired
    timeSlice();

    /* when we return from the handler, USLOSS automatically re-enables
     * interrupts and disables kernel mode (unless we were previously in
     * kernel code).  Or I think so.  I haven't double-checked yet.  TODO
     */
}

/* dispatcher()
 *
 * Makes scheduling decisions.  If the 'current' is NULL, then this will
 * definitely find a runnable process and execute it; if not, then this
 * will make decisions about whether or not we need to switch to another
 * process.
 */
static void dispatcher()
{
    require_kernel_mode(__func__, 1);
    // since the interrupts are disabled, we don't need to disable them here.

    ProcTableEntry *old_proc = current;

    int curTime = currentTime();

    /* sanity check: the current process must not be part of a queue! */
    if (current != NULL)
        assert(current->queue_next == NULL);

    if (debug)
    {
        USLOSS_Console("---- dispatcher(): QUEUE DEBUG ----\n");
        for (int i = 1; i <= 7; i++)
        {
            USLOSS_Console("%d     &head=%p    tail_next=%p\n", i, &proc_queues[i].head, proc_queues[i].tail_next);

            ProcTableEntry *cur = proc_queues[i].head;
            while (cur != NULL)
            {
                if (cur == current)
                    USLOSS_Console("ERROR: Current is already on the queue!\n");

                USLOSS_Console("  %p   %s\n", cur, cur->name);
                cur = cur->queue_next;
            }
        }
        USLOSS_Console("----\n");
    }

    /* blocking case: the current process doesn't want to continue running */
    if (current != NULL && current->runnable_status != 0)
        dispatcher_removeCurrent(); // will change the (global) current pointer

    /* preempted case: the current process must yield for a higher-priority
     *                 process
     */
    if (current != NULL)
        for (int i = 1; i < current->priority; i++)
            if (proc_queues[i].head != NULL)
            {
                dispatcher_removeCurrent();
                break;
            }

    /* NOP case: if a current process is running, and it's happy to continue
     *           running, and if its timeslice hasn't expired, then just let
     *           it keep going.
     */
    if (current != NULL && curTime - readCurStartTime() < 80 * 1000)
        return;

    /* timeslice-expired case, has two sub-cases:
     *     Nothing else runnable - keep running, but reset the timeslice counter.
     *     Else, put this at the end of the run queue and dispatch as if we
     *           didn't have a current process.
     */
    if (current != NULL)
    {
        if (proc_queues[current->priority].head == NULL)
        {
            /* nothing else runnable at the current priority.
             * Update the time-slice counters, and we're done.
             */
            current->totalTimeConsumed += curTime - current->timeSliceStart;
            current->timeSliceStart = curTime;
            return;
        }

        /* move the current process to the end of its queue */
        dispatcher_removeCurrent();
    }

    /* if we get here, then we have handled all of the current-process cases.
     * If we decided to let it run, we already returned; otherwise, we
     * removed it.
     */
    assert(current == NULL);

    /* hunt through the process tables to see which process we want to run.
     * Since sentinel is always runnable, we're guaranteed to find one
     * *somewhere*.
     */
    ProcTableEntry *new_proc = NULL;
    for (int i = 1; i <= 7; i++)
        if (proc_queues[i].head != NULL)
        {
            new_proc = proc_queues[i].head;

            proc_queues[i].head = new_proc->queue_next;
            if (proc_queues[i].head == NULL)
                proc_queues[i].tail_next = &proc_queues[i].head;

            new_proc->queue_next = NULL;

            break;
        }
    assert(new_proc != NULL);

    /* it should be impossible to switch to a process that's not runnable. */
    assert(new_proc->pid != -1);

    assert(new_proc->runnable_status == 0);

    /* DESIGN CHANGE
     *
     * We replaced mmu_switch(new) with mmu_flush(void).  The new function
     * must be called *BEFORE* we update 'current'.
     */
    mmu_flush();

    /* actually perform the switch
     *
     * BUGFIX: Set 'current' *before* the context switch,
     *         so that it is already configured when the launch() function
     *         starts, the first time.
     */
    current = new_proc;

    if (debug)
    {
        if (old_proc == NULL)
            USLOSS_Console("dispatcher(): switching contexts: old_pid = n/a     new_pid = %d\n", new_proc->pid);
        else
            USLOSS_Console("dispatcher(): switching contexts: old_pid = %d      new_pid = %d\n", old_proc->pid, new_proc->pid);
    }

    USLOSS_Context *old_context = (old_proc == NULL) ? NULL : &old_proc->context;
    USLOSS_ContextSwitch(old_context, &new_proc->context);

    /* no matter what we switch to (with a critical exception!) the result of a
     * ContextSwitch always takes us here, to the point where an old process
     * has been restored.  We immediately return to our caller and pick up from
     * where we left off.
     *
     * What is the exception?  Simple!  It's a newly-created process, which
     * jumps directly to launch().  But it will land here the *second* time we
     * context switch to it.
     */

    /* sanity check that the interrupts are still disabled, like they were when
     * we switched away.  And kernel mode, too.
     */
    assert((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT) == 0); // ints disabled
    assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE);       // kernel mode

    /* keep track of clock variables.  BUGFIX: curTime is out of date, get a new one! */
    curTime = currentTime();
    assert(current->timeSliceStart == -1);
    current->timeSliceStart = curTime;

    /* sanity check: the current process must not be part of a queue! */
    assert(current->queue_next == NULL);

    return;
}

/* helper function for dispatcher() */
void dispatcher_removeCurrent()
{
    assert(current != NULL);

    current->totalTimeConsumed += currentTime() - current->timeSliceStart;
    current->timeSliceStart = -1;

    /* if the process that we removed is still runnable, then put it back on
     * the proper queue.  This happens when a process is preempted by another,
     * either because of priority, or because a timeslice expired and we're
     * doing the round-robin thing.
     */
    if (current->runnable_status == 0)
        wake_up(current->pid, 0); // this is a little inefficient, but the code is clear!

    current = NULL;
}

int init_main(char *ignored)
{
    phase2_start_service_processes();
    phase3_start_service_processes();
    phase4_start_service_processes();
    phase5_start_service_processes();

    int rc = fork1("sentinel", sentinel_main, NULL, USLOSS_MIN_STACK, 7);
    if (rc <= 0)
    {
        USLOSS_Console("ERROR: Could not create the testcase_init process, rc=%d\n", rc);
        USLOSS_Halt(1);
    }

    rc = fork1("testcase_main", testcase_main_wrapper, NULL, USLOSS_MIN_STACK, 3);
    if (rc <= 0)
    {
        USLOSS_Console("ERROR: Could not create the testcase_init process, rc=%d\n", rc);
        USLOSS_Halt(1);
    }

    while (1)
    {
        int join_rc, status_ignored;
        join_rc = join(&status_ignored);

        if (join_rc == -2)
        {
            USLOSS_Console("ERROR: All of the children of init have died.  This should never occcur, since testcase_main() is supposed to Halt() the system!!!\n");
            USLOSS_Halt(1);
        }
    }

    /* never get here */
    return 255;
}

int testcase_main_wrapper(char *ignored)
{
    quit(testcase_main());
}

int sentinel_main(char *ignored)
{
    while (1)
    {
        /* this process only runs if *everything* else blocks.  If there is I/O
         * running, then this is harmless; if there is no I/O ongoing, then it
         * means that there is a deadlock.
         */
        if (phase2_check_io() == 0)
        {
            USLOSS_Console("DEADLOCK DETECTED!  All of the processes have blocked, but I/O is not ongoing.\n");
            USLOSS_Halt(1);
        }

        USLOSS_WaitInt();
    }

    /* never get here */
    return 255;
}

void launch()
{
    if (debug)
        USLOSS_Console("launch(): PSR = %d\n", USLOSS_PsrGet());

    require_kernel_mode(__func__, 1);

    assert(current != NULL);
    assert(current->startFunc != NULL);

    /* enable interrupts.  User must switch to user mode if they wish. */
    int psr_retval = USLOSS_PsrSet(USLOSS_PSR_CURRENT_MODE | USLOSS_PSR_CURRENT_INT);
    assert(psr_retval == USLOSS_DEV_OK);

    /* call the user function.  If it returns, then we must quit() */
    int retval = current->startFunc(current->start_arg);
    quit(retval);
}
