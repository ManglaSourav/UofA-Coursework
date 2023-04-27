/*
 * These are the definitions for phase1 of the project (the kernel).
 */

#ifndef _PHASE1_H
#define _PHASE1_H

#include <usloss.h>

/*
 * Maximum number of processes. 
 */

#define MAXPROC      50

/*
 * Maximum length of a process name
 */

#define MAXNAME      50

/*
 * Maximum length of string argument passed to a newly created process
 */

#define MAXARG       100

/*
 * Maximum number of syscalls.
 */

#define MAXSYSCALLS  50

/*
Initialize your data structures, including setting up the process table entry for the starting process, init.
Steps:
1. Make a process entry with name "init"
2. assign time to  USLOSS_IntVec[USLOSS_CLOCK_INT]
*/
extern void phase1_init(void);

/*
Steps:
1. do not call phase1_init : testcase startup calling it
2. Make a new context and use USLOSS_ContextInit to initialise that context and assign that context to init process
3. Insert init process entry in process table at position 1
4. Incease any count you're maintaining
5. call dispatcher
*/
extern void startProcesses(void); // never returns!

/*
This function creates a new process, which is a child of the currently running process.
Steps:
1. Validation:- Usermode, stackSize is less than USLOSS_MIN_STACK,
no empty slots in the process table, priority out of range, startFunc or name are NULL, name too long
2. disable_interrupts
3. create new process entry and populate every variable and insert that entry to new process table(aftr finding the empty slot)
4. Make a wrapper of child main func like this:
                    void dummy(){}
                        //should not be current process we can pass child process as argument or
                        currProc->status = 5;
                        int result = currProc->startFunc(currProc->startArg);
                        quit(result);
                    }
5. Now create new context and call USLOSS_ContextInit (pass wrapper method) and assign that context to child process
6. Assign current node's child to this newly created entry
7. call mmu_init_proc(child->pid); // for phase 5
8. put the child process in queue according to its priority and call dispatcher
9. return pid of child process
*/

extern int  fork1(char *name, int(*func)(char *), char *arg,
                  int stacksize, int priority);

/*
this function blocks the current process until one of its children has terminated;
Blocks the current process, until one of its children terminates
Steps:
1. disable_interrupts
2. Validation: the process does not have any children or all children joined
3. search for the zombie_child(which is dead) of current process
4. if you dont find dead child, call dispatcher and wait for a child to die(mean block current process)
5. if you found a dead child, start cleaning
6. store child pid to return and assign child status to out status pointer and free child stack and memset 0 at child process location
7. restore_interrupts and return child pid
*/
extern int  join(int *status);

/*
1. disable_interrupts
2. validation
3. mmu_quit(current->pid)
4. assign incoming status to current.status
5. see if current(child)'s parent is in WAITING_FOR_CHILD_TO_QUIT state then
wake up(means meake parent in runnable state and put in queue accordingly) the child
6. see if there is any WAITING_FOR_ZAP_TARGET_TO_QUIT process of current process, wake them up
7. do current = null and call dispatcher

*/

extern void quit(int status) __attribute__((__noreturn__));

/*
The Zap() is used to send a discontinuation signal to a process, urging it to exit.
points to Remember : zap does not unblock a blocked target process.

Steps:
1. assert kernel mode and do Validation: check incoming pid is not the same as current process pid,
    check pid exist in process table, trying to halt pid 1 which is init process :- call USLOSS_Halt(1)
2. disable_interrupts
3. pull out the pid matching entry from process table and do basic validation on that entry
4. We will insert other->zapper_first at the head of current process zapper_next
4. Now set current->runnable_status = WAITING_FOR_ZAP_TARGET_TO_QUIT;
5. call dispatcher
6. restore_interrupts
*/
extern void zap(int pid);

/*
 * Returns 1 if any process has attempted to zap the current process.
Steps:
check: current->zapper_first if this is not null mean 1, current process has been attempted
*/
extern int  isZapped(void);

/*
Steps: Returns the PID of the current process.
*/
extern int  getpid(void);

/*
Steps:
run a loop till MAXPROC and print every index entry data from processTable
*/
extern void dumpProcesses(void);

/*
Used heavily by the other Phases to block processes for a wide variety of reasons.

Steps:
1. Validation: check block_status greater than 10, :- use USLOSS_Console() and USLOSS_Halt(1);
2. disable_interrupts
3.  current->runnable_status = newStatus
4. call dispatcher
5. restore_interrupts
*/
extern void blockMe(int block_status);

/*
Steps:
1. disable_interrupts and check kernel mode
2. get the process table entry using pid (for example : procTable[pid % MAXPROC])
3. validation: the indicated process was not blocked, does not exist, or is blocked on a status ≤ 10.:- return -2
4. wake_up the process entry and call dispatcher
5. restore_interrupts
6. return 0
*/
extern int  unblockProc(int pid);

/*
returns the wall-clock time (in microseconds) when the current process started its current timeslice
steps:
    return current->timeSliceStart;
*/
extern int  readCurStartTime(void);

/*
Checks to see if the currently executing process has exceeded its time slice;
steps:  if (currentTime() - current->timeSliceStart > 80 * 1000)
        call dispatcher();
*/
extern void timeSlice(void);

/*
Returns the total amount of CPU time (in microseconds) used by the current process, since it was created.
steps :
1. disable_interrupts : to protect access to the readCurStartTime()
2. val = current->totalTimeConsumed + (currentTime() - readCurStartTime());
3. restore_interrupts
4. return val
*/
extern int  readtime(void);

/* simply reads the current wall-clock time (in microseconds) and returns it.
Steps:
int result = USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &now); // unit = 0 and this returns the time of a device.
if (result != USLOSS_DEV_OK)
{
    USLOSS_Console("diskHandler(): Could not get input from device.\n");
    USLOSS_Halt(1);
}
return now
*/
extern int  currentTime(void);


/*
 * These functions are called *BY* Phase 1 code, and are implemented in
 * Phase 5. If we are testing code before Phase 5 is written, then the
 * testcase must provide a NOP implementation of each.
 */

extern void mmu_init_proc(int pid);
extern void mmu_quit(int pid);
extern void mmu_flush(void);

/* these functions are also called by the phase 1 code, from inside
 * init_main().  They are called first; after they return, init()
 * enters an infinite loop, just join()ing with children forever.
 *
 * In early phases, these are provided (as NOPs) by the testcase.
 */
extern void phase2_start_service_processes(void);
extern void phase3_start_service_processes(void);
extern void phase4_start_service_processes(void);
extern void phase5_start_service_processes(void);

/* this function is called by the init process, after the service
 * processes are running, to start whatever processes the testcase
 * wants to run.  This may call fork() or spawn() many times, and
 * block as long as you want.  When it returns, Halt() will be
 * called by the Phase 1 code (nonzero means error).
 */
extern int testcase_main(void);

/* this is called by the clock handler (which is in Phase 1), so that Phase 2
 * can add new clock features
 */
extern void phase2_clockHandler(void);

/* this is called by sentinel to ask if there are any ongoing I/O operations */
extern int phase2_check_io(void);



#endif /* _PHASE1_H */

/*
priorities: init must run at priority 6, and sentinel must run at priority 7; 
for all other processes, the range is limited to 1-5 (inclusive)



*/

/*
dispatcher:
seteps:
1. create a old_process = current
2. curTime = currentTime();

3. blocking case:the current process doesn't want to continue running
     if current->runnable_status != 0 then remove current do following while removing the current:
                                            current->totalTimeConsumed += currentTime() - current->timeSliceStart;
                                            current->timeSliceStart = -1;
                                            current = NULL
4. preempted case:
    the current process must yield for a higher-priority process (preempted case)
5. NOP case: if a current process is running, and it's happy to continue running, and if its timeslice hasn't expired,
then just let it keep going.
6. timeslice-expired case, has two sub-cases:
     *     Nothing else runnable in the same priority - keep running, but reset the timeslice counter.
     *     Else, put this at the end of the run queue and dispatch as if we
     *           didn't have a current process.
7. Now if we get here means we have to find new process to run,
    run a loop with till 7 (total priority): find a process with runnable state and remove that from list
8. mmu_flush() : must be called *BEFORE* we update 'current'.
9. assign current = new found process;
10. Now do the USLOSS_ContextSwitch with old process to current process or new process
11. keep track of clock variables: current->timeSliceStart = currentTime


#define UNUSED 0
#define READY 1
#define RUNNING 2
#define QUIT 3
#define BLOCKED  4
#define JOIN_BLOCKED  5
#define ZAP_BLOCKED  6

#define BlOCKMEBLOCKED 11

#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)
#define AMOUNTPRIORITIES 7

*/