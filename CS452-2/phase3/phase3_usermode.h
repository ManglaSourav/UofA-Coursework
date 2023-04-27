/*
 * This file contains the function definitions for the library interfaces
 * to the USLOSS system calls.
 */
#ifndef _PHASE3_USERMODE_H
#define _PHASE3_USERMODE_H

// Phase 3 -- User Function Prototypes
extern int  Spawn(char *name, int (*func)(char*), char *arg, int stack_size,
                  int priority, int *pid);
extern int  Wait(int *pid, int *status);
extern void Terminate(int status) __attribute__((__noreturn__));
extern void GetTimeofDay(int *tod);
extern void CPUTime(int *cpu);
extern void GetPID(int *pid);
extern int  SemCreate(int value, int *semaphore);
extern int  SemP(int semaphore); //this is similar to lock
extern int  SemV(int semaphore); // this is similar to unlock

   // NOTE: No SemFree() call, it was removed

#endif