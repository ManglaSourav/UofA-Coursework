/*
Phase 1b
*/

#include "phase1Int.h"
#include "usloss.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef struct PCB
{
    int cid;                   // context's ID
    int cpuTime;               // process's running time
    char name[P1_MAXNAME + 1]; // process's name
    int priority;              // process's priority
    P1_State state;            // state of the PCB
    // more fields here
    int sid;                  // semaphore on which process is blocked, if any
    int parent;               // parent PID
    int tag;                  // process's tag
    int children[P1_MAXPROC]; // children PIDs
    int numChildren;          // # of children
    int quitChildren[P1_MAXPROC];
    int numQuitChildren;
    int status;
} PCB;

static PCB processTable[P1_MAXPROC]; // the process table
static int readyQueue[P1_MAXPROC];

void P1ProcInit(void)
{
    P1ContextInit();
    // check Kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_IllegalInstruction();
    }
    // initialize everything including the processTable
    for (int i = 0; i < P1_MAXPROC; i++)
    {
        processTable[i].cid = -1;
        processTable[i].cpuTime = -1;
        processTable[i].name[0] = '\0';
        processTable[i].priority = -1;
        processTable[i].state = P1_STATE_FREE;
        for (int j = 0; j < P1_MAXPROC; ++j)
        {
            processTable[i].children[j] = -1;
            processTable[i].quitChildren[j] = -1;
        }
        processTable[i].numChildren = 0;
        processTable[i].numQuitChildren = 0;
        processTable[i].parent = -1;
        processTable[i].sid = -1;
        readyQueue[i] = -1;
    }
}

int P1_GetPid(void)
{
    // check Kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_IllegalInstruction();
    }
    // find the running pid
    for (int pid = 0; pid < P1_MAXPROC; pid++)
    {
        if (processTable[pid].state == P1_STATE_RUNNING)
        {
            return pid;
        }
    }
    return -1;
}

// a wrapper function to do quit
int (*currentFunc)(void *);
void launch(void *arg)
{
    P1_Quit(currentFunc(arg));
}

int P1_Fork(char *name, int (*func)(void *), void *arg, int stacksize, int priority, int tag, int *pid)
{
    int result = P1_SUCCESS;
    // check for kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_IllegalInstruction();
    }
    // disable interrupts
    int prvInterrupt = P1DisableInterrupts();
    // check all parameters
    if (tag != 1 && tag != 0)
    {
        return P1_INVALID_TAG;
    }

    if (priority > 6 || priority < 1)
    {
        return P1_INVALID_PRIORITY;
    }
    // only the first process can have priority of 6
    if (priority == 6 && processTable[0].state != P1_STATE_FREE)
    {
        return P1_INVALID_PRIORITY;
    }
    if (stacksize < USLOSS_MIN_STACK)
    {
        return P1_INVALID_STACK;
    }
    if (name == NULL)
    {
        return P1_NAME_IS_NULL;
    }
    if (strlen(name) > P1_MAXNAME)
    {
        return P1_NAME_TOO_LONG;
    }
    for (int i = 0; i < P1_MAXPROC; ++i)
    {
        if (processTable[i].state != P1_STATE_FREE && strcmp(processTable[i].name, name) == 0)
        {
            return P1_DUPLICATE_NAME;
        }
    }
    // check too many process
    for (*pid = 0; *pid < P1_MAXPROC; (*pid)++)
    {
        if (processTable[*pid].cid == -1)
        {
            break;
        }
    }
    if (*pid >= P1_MAXPROC)
    {
        return P1_TOO_MANY_PROCESSES;
    }
    // create a context using P1ContextCreate
    int cid;
    int rc;
    currentFunc = func;
    rc = P1ContextCreate(launch, arg, stacksize, &cid);
    // allocate and initialize PCB
    processTable[*pid].cid = cid;
    strcpy(processTable[*pid].name, name);
    processTable[*pid].priority = priority;
    processTable[*pid].tag = tag;
    processTable[*pid].state = P1_STATE_READY;
    // add pid into readyQueue
    int index;
    for (index = 0; readyQueue[index] != -1 && index < P1_MAXPROC; ++index)
    {
    }
    readyQueue[index] = *pid;

    // if this is the first process or this process's priority is higher than the
    //    currently running process call P1Dispatch(FALSE)
    int runningPid = P1_GetPid();
    if (runningPid == -1)
    {
        P1Dispatch(FALSE);
    }
    else if (processTable[*pid].priority < processTable[runningPid].priority)
    {
        processTable[*pid].parent = runningPid;
        processTable[runningPid].children[processTable[runningPid].numChildren++] = *pid;
        P1Dispatch(FALSE);
    }
    else
    {
        processTable[*pid].parent = runningPid;
        processTable[runningPid].children[processTable[runningPid].numChildren++] = *pid;
    }
    // re-enable interrupts if they were previously enabled
    if (prvInterrupt == 1)
    {
        P1EnableInterrupts();
    }
    return result;
}

void P1_Quit(int status)
{
    // check for kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_IllegalInstruction();
    }
    // disable interrupts
    int prvInterrupt;
    prvInterrupt = P1DisableInterrupts();

    int runningPid = P1_GetPid();

    // remove from ready queue, set status to P1_STATE_QUIT
    int i = 0;
    for (i = 0; readyQueue[i + 1] != -1 && (i + 1) < P1_MAXPROC; ++i)
    {
        if (readyQueue[i] == runningPid)
        {
            readyQueue[i] = readyQueue[i + 1];
            readyQueue[i + 1] = runningPid;
        }
    }
    readyQueue[i] = -1;
    processTable[runningPid].state = P1_STATE_QUIT;
    // if first process verify it doesn't have children, otherwise give children to first process
    if (runningPid == 0 && processTable[0].numChildren > 0)
    {
        USLOSS_Console("First process quitting with children, halting.\n");
        USLOSS_Halt(1);
    }
    processTable[runningPid].status = status;
    processTable[runningPid].state = P1_STATE_QUIT;
    for (int i = 0; i < processTable[runningPid].numChildren; ++i)
    {
        processTable[0].children[processTable[0].numChildren + i] = processTable[runningPid].children[i];
        processTable[processTable[runningPid].children[i]].parent = 0;
        processTable[0].numChildren++;
    }

    // add ourself to list of our parent's children that have quit
    int parent = processTable[runningPid].parent;
    if (parent >= 0)
    {
        processTable[parent].quitChildren[processTable[parent].numQuitChildren++] = runningPid;
        // remove quit child from its parent
        int i = 0;
        for (i = 0; i < processTable[parent].numChildren; i++)
        {
            if (processTable[parent].children[i] == runningPid)
            {
                processTable[parent].children[i] = -1;
                processTable[parent].numChildren--;
                break;
            }
        }
        // update the children list for parent
        for (; i < P1_MAXPROC - 1; i++)
        {
            processTable[parent].children[i] = processTable[parent].children[i + 1];
            processTable[parent].children[i + 1] = -1;
        }
        // if parent is in state P1_STATE_JOINING set its state to P1_STATE_READY
        if (processTable[parent].state == P1_STATE_JOINING)
        {
            processTable[parent].state = P1_STATE_READY;
        }
    }
    P1Dispatch(FALSE);
    // should never get here
    assert(0);
}

int P1GetChildStatus(int tag, int *cpid, int *status)
{
    // check for kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_IllegalInstruction();
    }
    int result = P1_SUCCESS;
    // invalid tag
    if (tag != 0 && tag != 1)
    {
        return P1_INVALID_TAG;
    }

    int runningPid = P1_GetPid();
    // no children
    int nextChildPid = -1;
    // find the next quit child
    for (int i = 0; i < processTable[runningPid].numQuitChildren; i++)
    {
        int tempPid = processTable[runningPid].quitChildren[i];
        if (processTable[tempPid].tag == tag)
        {
            nextChildPid = tempPid;
            break;
        }
    }
    // USLOSS_Console("%d\n",nextChildPid);
    //  if no children
    if (processTable[runningPid].numChildren == 0 && nextChildPid == -1)
    {
        return P1_NO_CHILDREN;
    }
    // no qiuit
    if (processTable[runningPid].numChildren > 0)
    {
        return P1_NO_QUIT;
    }
    for (int i = 0; i < processTable[runningPid].numQuitChildren; i++)
    {
        if (processTable[runningPid].quitChildren[i] == processTable[runningPid].quitChildren[nextChildPid])
        {
            int tmp = processTable[runningPid].quitChildren[i];
            processTable[runningPid].quitChildren[i] = processTable[runningPid].quitChildren[i + 1];
            processTable[runningPid].quitChildren[i + 1] = tmp;
        }
    }
    processTable[runningPid].quitChildren[processTable[runningPid].numQuitChildren--] = -1;

    // update messages
    *cpid = nextChildPid;
    *status = processTable[*cpid].status;
    int rc;
    rc = P1ContextFree(processTable[*cpid].cid);
    processTable[nextChildPid].cid = -1;
    return result;
}

int P1SetState(int pid, P1_State state, int sid)
{
    // check for kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_IllegalInstruction();
    }
    int result = P1_SUCCESS;
    // judgement
    if (pid < 0 || pid >= P1_MAXPROC || processTable[pid].cid == -1)
    {
        return P1_INVALID_PID;
    }
    if (state != P1_STATE_READY && state != P1_STATE_JOINING && state != P1_STATE_BLOCKED && state != P1_STATE_QUIT)
    {
        return P1_INVALID_STATE;
    }

    if (state == P1_STATE_JOINING)
    {
        // a child has quit
        for (int i = 0; i < P1_MAXPROC; ++i)
        {
            int cpid = processTable[pid].quitChildren[i];
            if (cpid != -1 && processTable[cpid].state == P1_STATE_QUIT)
            {
                return P1_CHILD_QUIT;
            }
        }
    }
    // set sid
    if (state == P1_STATE_BLOCKED)
    {
        processTable[pid].sid = sid;
    }
    processTable[pid].state = state;
    return result;
}

void P1Dispatch(int rotate)
{
    // check Kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_IllegalInstruction();
    }
    // no runnable process in readyQueue
    if (readyQueue[0] == -1)
    {
        USLOSS_Console("No runnable processes, halting.\n");
        USLOSS_Halt(0);
    }

    int runningPid = P1_GetPid();
    int highestPriority = 7;
    int highestPid = -1;

    // select the highest-priority runnable process
    for (int i = 0; readyQueue[i] != -1 && i < P1_MAXPROC; ++i)
    {
        if (readyQueue[i] != runningPid && processTable[readyQueue[i]].priority < highestPriority && processTable[readyQueue[i]].state == P1_STATE_READY)
        {
            highestPriority = processTable[readyQueue[i]].priority;
            highestPid = readyQueue[i];
        }
    }
    // call P1ContextSwitch to switch to that process
    int rc;
    if (runningPid < 0)
    {
        processTable[highestPid].state = P1_STATE_RUNNING;
        rc = P1ContextSwitch(processTable[highestPid].cid);
    }
    else if (highestPid != -1 && highestPriority < processTable[runningPid].priority)
    {
        processTable[runningPid].state = P1_STATE_READY;
        processTable[highestPid].state = P1_STATE_RUNNING;
        rc = P1ContextSwitch(processTable[highestPid].cid);
    }
    else if (rotate == TRUE && highestPriority == processTable[runningPid].priority)
    {
        processTable[runningPid].state = P1_STATE_READY;
        processTable[highestPid].state = P1_STATE_RUNNING;
        // move the runningPid to the tail of ready queue
        for (int i = 0; readyQueue[i + 1] != -1 && (i + 1) < P1_MAXPROC; ++i)
        {
            if (readyQueue[i] == runningPid)
            {
                readyQueue[i] = readyQueue[i + 1];
                readyQueue[i + 1] = runningPid;
            }
        }
        rc = P1ContextSwitch(processTable[highestPid].cid);
    }
}

int P1_GetProcInfo(int pid, P1_ProcInfo *info)
{
    // check for kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0)
    {
        USLOSS_IllegalInstruction();
    }
    int result = P1_SUCCESS;
    // judge pid
    if (pid < 0 || pid >= P1_MAXPROC || processTable[pid].cid == -1)
    {
        return P1_INVALID_PID;
    }
    // set process info
    strcpy(info->name, processTable[pid].name);
    info->state = processTable[pid].state;
    info->sid = processTable[pid].sid;
    info->priority = processTable[pid].priority;
    info->tag = processTable[pid].tag;
    info->cpu = processTable[pid].cpuTime;
    info->parent = processTable[pid].parent;
    for (int i = 0; i < P1_MAXPROC; ++i)
    {
        info->children[i] = processTable[pid].children[i];
    }
    info->numChildren = processTable[pid].numChildren;
    return result;
}
