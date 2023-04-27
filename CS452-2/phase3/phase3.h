/*
 * These are the definitions for phase 3 of the project
 */

#ifndef _PHASE3_H
#define _PHASE3_H

#define MAXSEMS 200

/*
1.Initialize the phase3_processTable and the phase3_semaphores to 0 using memset
2. Initialize two locks using MboxCreate(1,0) phase3_processTable_lock_mbID and phase3_semaphores_lock_mbID
3. Initialize the systemCallVec to all methods
*/
extern void phase3_init(void);

/*
1. Basic checks like userMain is null, new process name is not null, check stack_size and priority is between 1 to 5
2. acquire phase3_processTable_lock() (locking the process table not global lock)
3. call fork1() to create a new process (use userProcTrampoline function). Fork1 will return the pid of the new process
4. get the child process entry using phase3_processTable[child_pid % MAXPROC]
5. copy the name, args, pid of the new process to the child process entry
6. release phase3_processTable_lock()
*/
// kernSpawn







#endif /* _PHASE3_H */

//