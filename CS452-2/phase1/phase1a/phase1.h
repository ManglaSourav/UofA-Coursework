/*
 * These are the definitions for phase1 of the project (the kernel).
 */

#ifndef _PHASE1_H
#define _PHASE1_H

#include <usloss.h>

/*
 * Maximum number of processes.
 */

#define MAXPROC 50

/*
 * Maximum length of a process name
 */

#define MAXNAME 50

/*
 * Maximum length of string argument passed to a newly created process
 */

#define MAXARG 100

/*
 * Maximum number of syscalls.
 */

#define MAXSYSCALLS 50

/*
 * These functions must be provided by Phase 1.
 */

/*
1. create init process(process table entry at index 1) and assign all values
*/
extern void phase1_init(void);

/*
1. switch context from null to init using USLOSS_ContextInit (we have to pass main function of the process in context we can use wrapper also)
2. initailize the run queues to empty (7 queues in a form of linked list with head and tail pointer)
3. assign init process from process table to global current variable
4. call context switch from null to current.context which is init process
*/
extern void startProcesses(void); // never returns!

/*
1. validations and disable interrupts
2. find empty slot in process table
3. add child to the new slot after initialising all variable in the slot entry
4. call mmu_init_proc
5. call USLOSS_ContextInit with wrapper method
6. restore interrupts
7. returen child.pid
*

extern int  fork1(char *name, int(*func)(char *), char *arg,
                  int stacksize, int priority);

/*
//  * Blocks the current process until one child process has quit().
1. Disable Interrupts
2. Validations
3. Find a child from current process
4. remove child from current process
5. assign child status to status
6. free child stack
7. memset 0 to child
8. restore interrupts and return child's pid
*/
extern int join(int *status);

/*
Terminates the current process.
1. Disable Interrupts
2. Validation
3. call mmu_quit on the current.pid
4. assign current.status = status
5. do current = null
6. call TEMP_switchTo(switchToPid)
*/
extern void quit(int status, int switchToPid) __attribute__((__noreturn__));

/*
 return current->pid;
*/
extern int getpid(void);

/*
1. disable_interrupts
2. use USLOSS_Console to print all variable in a process table in loop
3. restore_interrupts
*/
extern void dumpProcesses(void);

#if 1 /* ADDED.  This is *NOT* part of Phase 1b, but I'm adding it to 1a */
/*
1. disable_interrupts
2. find newpid or switchTo pid in proc table
3. cal context USLOSS_ContextSwitch (check if current is null or not )
4. restore_interrupts
*/
void TEMP_switchTo(int pid);
#endif

/*
 * These functions are called *BY* Phase 1 code, and are implemented in
 * Phase 5.  If we are testing code before Phase 5 is written, then the
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



init_main(){

}

*/
