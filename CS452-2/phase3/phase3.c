
#include <memory.h>
#include <assert.h>

#include <usloss.h>
#include <usyscall.h>    // for SYS_* constants

#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include "phase3_usermode.h"

#define TODO() do { USLOSS_Console("TODO() at %s:%d\n", __func__,__LINE__); *(char*)7 = 0; } while(0)


/* the Phase 3 ProcessTable will have entries *ONLY* for user processes, but
 * for those processes, it will mirror the Phase 1 table.
 */

typedef struct Phase3_ProcTableEntry Phase3_ProcTableEntry;
struct Phase3_ProcTableEntry
{
    /* this is 0 except for processes that have been created with Spawn().
     *
     * TODO: does this serve any purpose, other than simple debugging?  Should
     *       I remove this?
     */
    int pid;

    /* these are set, by the parent, once it has been told what the process ID
     * of the child is.  But the parent will be holding the process table lock
     * through this window, meaning that the trampoline won't read these fields
     * until they are set.
     */
    int (*userMain)(char*);
    void *userArg;
};
Phase3_ProcTableEntry phase3_processTable[MAXPROC];

/* this mailbox (max 1 slot) is used as a semaphore, functioning as a lock
 * in on the entire proces table.
 */
int phase3_processTable_lock_mbID;

static void phase3_processTable_lock(void)
{
    MboxRecv(phase3_processTable_lock_mbID, NULL,0);
}
static void phase3_processTable_unlock(void)
{
    MboxSend(phase3_processTable_lock_mbID, NULL,0);
}


typedef struct Phase3_Semaphore Phase3_Semaphore;

struct Phase3_Semaphore
{
    int allocated;
    int zombie;

    int curVal;

    /* the textbook definition of a semaphore says that there ought to be a
     * per-semaphore mutex.  But we are using a big global lock below, which
     * (I think) will work pretty well - especially to serialize create
     * operations with others.  So I'm not (yet?) defining a per-sem mutex.
     *
     * Instead, the only mailbox I'm using is a P-waiting mailbox, along with
     * a "pending P" count.  Never use either of these fields without holding
     * the global semaphore lock, and only ever send a message on P-waiting
     * if you actually see a nonzero pending-P count.
     *
     * Future thoughts: A P() process cannot block while holding the global
     *                  lock, but once we implement SemFree(), it will be
     *                  confirm that SemFree() cannot race with a P going to
     *                  sleep.  For this reason, we will *NOT* clean up the
     *                  semaphore, in SemFree(), if pending-P is nonzero; we'll
     *                  leave that to the last P that wakes up.
     */
    int p_waiting_mbID;
    int p_pending_count;
};
Phase3_Semaphore phase3_semaphores[MAXSEMS];

int phase3_sems_overall_lock_mbID;

static void phase3_sems_lock()
{
    MboxRecv(phase3_sems_overall_lock_mbID, NULL,0);
}
static void phase3_sems_unlock()
{
    MboxSend(phase3_sems_overall_lock_mbID, NULL,0);
}


/* helper function */
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



static void kernSpawn       (USLOSS_Sysargs*);
static void kernWait        (USLOSS_Sysargs*);
static void kernTerminate   (USLOSS_Sysargs*);

static void kernSemCreate   (USLOSS_Sysargs*);
static void kernSemP        (USLOSS_Sysargs*);
static void kernSemV        (USLOSS_Sysargs*);
static void kernSemFree     (USLOSS_Sysargs*);

static void kernGetTimeOfDay(USLOSS_Sysargs*);
static void kernGetProcInfo (USLOSS_Sysargs*);
static void kernGetPID      (USLOSS_Sysargs*);

static void nullsys_phase3  (USLOSS_Sysargs*);


void phase3_init()
{
    memset(phase3_processTable, 0, sizeof(phase3_processTable));
    memset(phase3_semaphores,   0, sizeof(phase3_semaphores));

    /* initialize each "lock" mailbox to have a single message in it */
    phase3_processTable_lock_mbID = MboxCreate(1,0);
    MboxSend(phase3_processTable_lock_mbID, NULL,0);

    phase3_sems_overall_lock_mbID = MboxCreate(1,0);
    MboxSend(phase3_sems_overall_lock_mbID, NULL,0);

    for (int i=0; i<USLOSS_MAX_SYSCALLS; i++)
        if (i == SYS_DUMPPROCESSES)
            systemCallVec[i] = nullsys_phase3;

    systemCallVec[SYS_SPAWN    ] = kernSpawn;
    systemCallVec[SYS_WAIT     ] = kernWait;
    systemCallVec[SYS_TERMINATE] = kernTerminate;

    systemCallVec[SYS_SEMCREATE] = kernSemCreate;
    systemCallVec[SYS_SEMP     ] = kernSemP;
    systemCallVec[SYS_SEMV     ] = kernSemV;
    systemCallVec[SYS_SEMFREE  ] = kernSemFree;

    systemCallVec[SYS_GETTIMEOFDAY] = kernGetTimeOfDay;
    systemCallVec[SYS_GETPROCINFO ] = kernGetProcInfo;
    systemCallVec[SYS_GETPID      ] = kernGetPID;
}

void phase3_start_service_processes()
{
}



static int userProcTrampoline(char*);

static void kernSpawn(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_SPAWN);
    require_kernel_mode(__func__, 0);

    int (*userMain)(char*) = (int(*)(char*))args->arg1;
    char *arg              =         (char*)args->arg2;
    int   stack_size       =     (int)(long)args->arg3;
    int   priority         =     (int)(long)args->arg4;
    char *name             =         (char*)args->arg5;

    if (userMain == NULL ||
        name == NULL || strlen(name) > MAXNAME ||
        stack_size < USLOSS_MIN_STACK ||
        priority < 1 || priority > 5)
    {
        args->arg1 = (void*)(long)-1;
        args->arg4 = (void*)(long)-1;
        return;
    }
    /* we have to hold the lock on our process table while we create the child
     * process - because we need to set the userMain pointer, but we cannot set
     * it until we know what PID was assigned to the child.
     *
     * If the child is lower priority than us, then setting the pointer in time
     * is trivial; the child won't execute until we block, and thus we'll
     * definitely get there first.
     *
     * If the child is the same priority as us, then the same argument holds -
     * EXCEPT that we might get time-sliced away.  So no, it *DOESN'T* hold.
     *
     * And if the child is higher priority than us, then the child definitely
     * runs first.
     *
     * But if the child wins the race (because either of the last 2 cases), the
     * lock will prevent them from running until we have set the userMain
     * pointer.
     *
     * COMMENTARY
     *
     * Do I like using a global lock?  No.  If the child is low priority, then
     * we might hold the global lock arbitrarily long.  A *far better*
     * alternative would be to store the *child* main pointer in the *parent*
     * proc table entry (or even in a struct on the stack!) and then have a
     * per-parent blocking mechanism to wake up the parent when the child has
     * done its work.  While this might be an excellent design in general, I
     * think that it's beyond what my students are likely to do.  And since the
     * primary purpose of these solutions is to simulate what my students will
     * need to accomplish, I think I should limit myself to things they might
     * realistically come up with.    TODO: could I give them more hints???
     */
    phase3_processTable_lock();

    int child_pid = fork1(name,
                          userProcTrampoline,NULL,
                          stack_size, priority);
    assert(child_pid > 0);    // we should never fail to create a process (TODO: what about too many processes?)

    Phase3_ProcTableEntry *child_proc = &phase3_processTable[child_pid % MAXPROC];
    assert(child_proc->pid == 0);

    child_proc->pid      = child_pid;
    child_proc->userMain = userMain;
    child_proc->userArg  = arg;

    /* now that the process table has been filled in, we can release the child
     * process's trampoline function to run.  (If it was lower priority than
     * us, then it wasn't going to run anyway - but if it was higher priority,
     * then this lock prevents the trampoline from moving forward until we've
     * filled in the process table entry.
     */
    phase3_processTable_unlock();
    

    args->arg1 = (void*)(long)child_pid;
    args->arg4 = (void*)(long)0;

    return;
}


static int userProcTrampoline(char *arg_unused)
{
    assert(arg_unused == NULL);

    require_kernel_mode(__func__, 0);

    phase3_processTable_lock();

    int pid = getpid();
    assert(pid != 0);

    Phase3_ProcTableEntry *pte = &phase3_processTable[pid % MAXPROC];
    assert(pte->pid == pid);

    int (*userMain)(char*) = pte->userMain;
    void *userArg          = pte->userArg;

    phase3_processTable_unlock();

    /* convert to user mode! */
    if (USLOSS_PsrSet(USLOSS_PSR_CURRENT_INT) != USLOSS_DEV_OK)
    {
        USLOSS_Console("ERROR: Could not disable kernel mode.\n");
        USLOSS_Halt(1);
    }

    int rc = userMain(userArg);

    /* if we get here, then the user returned without calling Terminate().
     * We must simulate that the user had performed a Terminate() system call.
     *
     * BUGFIX: Although this trampoline code is part of our kernel
     * implementation, we must remember that right here, it is *RUNNING* in
     * user mode!  So use the Terminate() function, instead of calling
     * kernTerminate() directly.
     */
    Terminate(rc);   // will never return!
    assert(0);
}



static void kernWait(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_WAIT);
    require_kernel_mode(__func__, 0);


    /* call the Phase1 join() function.  If there are no children,
     * it will return -2
     */
    int status;
    int child_pid = join(&status);


    if (child_pid == -2)
    {
        args->arg4 = (char*)(long)-2;
        goto DONE;
    }

    args->arg1 = (char*)(long)child_pid;
    args->arg2 = (char*)(long)status;
    args->arg4 = (char*)(long)0;

DONE:
    return;
}



static void kernTerminate(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_TERMINATE);
    require_kernel_mode(__func__, 0);

    int my_status = (int)(long)args->arg1;

    while (1)    // we'll break out when join() returns -2
    {
//        USLOSS_Console("Phase 3 kernel mode: In the TERMINATE syscall, PID %d is automatically calling join() until no children remain.\n", getpid());
//        dumpProcesses();

        int child_status;
        int child_pid = join(&child_status);
        assert(child_pid > 0 || child_pid == -2);

        if (child_pid == -2)
        {
//            USLOSS_Console("Phase 3 kernel mode: In the TERMINATE syscall for PID %d, after join() returns.  child pid %d\n", getpid(), child_pid);
            break;
        }
//        else
//            USLOSS_Console("Phase 3 kernel mode: In the TERMINATE syscall for PID %d, after join() returns.  child pid %d    status %d\n", getpid(), child_pid, child_status);
    }


    /* clean up this process.  It might or might not have anything defined */
    phase3_processTable_lock();

    int this_pid = getpid();
    Phase3_ProcTableEntry *this_proc = &phase3_processTable[this_pid % MAXPROC];
    assert(this_proc->pid == 0 || this_proc->pid != 0);

    memset(this_proc, 0, sizeof(*this_proc));

    phase3_processTable_unlock();

    quit(my_status);    // will never return
}



static void kernSemCreate(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_SEMCREATE);
    require_kernel_mode(__func__, 0);

    int init_val = (int)(long)args->arg1;
    int rc = -127;

    phase3_sems_lock();

    if (init_val < 0)
    {
        rc = -1;
        goto DONE;
    }

    int semID = 0;
    while (semID < MAXSEMS && phase3_semaphores[semID].allocated)
        semID++;

    if (semID == MAXSEMS)
    {
        rc = -1;
        goto DONE;
    }


    /* before I actually mark this semaphore as allocated...can I allocate a
     * mailbox for it?
     *
     * DESIGN CHANGE: The original implementation here had a slot-limit of 0,
     *                meaning that V() can wake up a single process, but once
     *                that is accomplished, additional V()s will block until
     *                the low-priority process has run.  I changed this to 10,
     *                so that many V()s can be done before the waker-process
     *                blocks.  But beware that this can still lead to blocking
     *                in crazy cases!
     */
    int mbID = MboxCreate(10,0);
    if (mbID == -1)
    {
        rc = -1;
        goto DONE;
    }


    /* confirmed, we are going to allocate this semaphore! */

    Phase3_Semaphore *sem = &phase3_semaphores[semID];
    args->arg1 = (void*)(long)semID;
    rc = 0;

    sem->allocated = 1;
    assert(sem->zombie == 0);
    sem->curVal = init_val;
    sem->p_waiting_mbID = mbID;
    sem->p_pending_count = 0;

DONE:
    phase3_sems_unlock();

    args->arg4 = (void*)(long)rc;
    return;
}



static void kernSemP(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_SEMP);
    require_kernel_mode(__func__, 0);

    int rc = -127;

    phase3_sems_lock();

    int semID = (int)(long)args->arg1;
    if (semID < 0 || semID >= MAXSEMS)
    {
        rc = -1;
        goto DONE;
    }

    Phase3_Semaphore *sem = &phase3_semaphores[semID];
    if (sem->allocated != 1 || sem->zombie)
    {
        rc = -1;
        goto DONE;
    }

    /* if there are available values in the semaphore, then we can
     * return immediately.
     */
    assert(sem->curVal >= 0);
    if (sem->curVal > 0)
    {
        sem->curVal--;
        rc = 0;
        goto DONE;
    }


    /* if we get here, then we have to block waiting for a V to
     * wake us up.  We first mark that we are waiting on this
     * semaphore; then we release the global semaphore lock; then
     * we block on the mailbox.  Of course, this means that there
     * is a race condition, where other threads might break in
     * before we are able to block on the mailbox; if they send a
     * message, then it's possible that we might wake up
     * synchronously - but that's a harmless (and in fact, correct)
     * action.
     *
     * Note that it's save to access the mbID field, even without
     * a lock, because the p_pending_count effectively functions as
     * a "pin" on the created semaphore; the semaphore cannot be
     * freed, and thus the mailbox cannot change.
     */
    sem->p_pending_count++;
    phase3_sems_unlock();
    MboxRecv(sem->p_waiting_mbID, NULL,0);
    /* SemV() decrements the p_pending_count, so I don't need the lock */

    rc = 0;
    goto DONE_NO_UNLOCK;

DONE:
    phase3_sems_unlock();
DONE_NO_UNLOCK:
    args->arg4 = (void*)(long)rc;
}



static void kernSemV(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_SEMV);
    require_kernel_mode(__func__, 0);

    int rc = -127;

    phase3_sems_lock();

    int semID = (int)(long)args->arg1;
    if (semID < 0 || semID >= MAXSEMS)
    {
        rc = -1;
        goto DONE;
    }

    Phase3_Semaphore *sem = &phase3_semaphores[semID];
    if (sem->allocated != 1 || sem->zombie)
    {
        rc = -1;
        goto DONE;
    }


    /* if there are any P() processes that need to wakeup, then we'll do that
     */
    assert(sem->p_pending_count >= 0);
    if (sem->p_pending_count > 0)
    {
        /* it's an unfortunate reality that, if the P() process is higher
         * priority than our current process, then it will interrupt us, and
         * then immediately block on the lock, causing a context switch back
         * to us.  So we don't really have a race to deal with, but we do have
         * a context-switch-happy possibility here.
         */
        MboxSend(sem->p_waiting_mbID, NULL,0);
        sem->p_pending_count--;
    }
    else
    {
        /* if we get here, then there are no pending P() processes, so we store
         * this into the value.
         */
        sem->curVal++;
    }

    rc = 0;


DONE:
    phase3_sems_unlock();
    args->arg4 = (void*)(long)rc;
}



static void kernSemFree(USLOSS_Sysargs *args)
{
    USLOSS_Console("This system call is DISABLED.  It has been removed from the Phase 2 requirements, at least in Fall 22.  I may put it back in, in future semesters.\n");
    TODO();
}



static void kernGetTimeOfDay(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_GETTIMEOFDAY);
    require_kernel_mode(__func__, 0);

    args->arg1 = (void*)(long)currentTime();
}



static void kernGetProcInfo(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_GETPROCINFO);
    require_kernel_mode(__func__, 0);

    args->arg1 = (void*)(long)readtime();
}



static void kernGetPID(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_GETPID);
    require_kernel_mode(__func__, 0);

    args->arg1 = (void*)(long)getpid();
}



static void nullsys_phase3(USLOSS_Sysargs *args)
{
    USLOSS_Console("ERROR: The process %d has called an undefined system call %d\n", getpid(), args->number);
    dumpProcesses();
    Terminate(1);
}

