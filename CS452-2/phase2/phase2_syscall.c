
#include <memory.h>
#include <assert.h>

#include <usloss.h>
#include <usyscall.h>

#include "phase1.h"
#include "phase2.h"

#define TODO()                                                   \
    do                                                           \
    {                                                            \
        USLOSS_Console("TODO() at %s:%d\n", __func__, __LINE__); \
        *(char *)7 = 0;                                          \
    } while (0)

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);

static void syscallHandler(int dev, void *arg);

static void nullsys(USLOSS_Sysargs *args);
static void kernDumpProcesses(USLOSS_Sysargs *args);

void phase2_syscall_init()
{
    for (int i = 0; i < MAXSYSCALLS; i++)
        systemCallVec[i] = nullsys;
    systemCallVec[SYS_DUMPPROCESSES] = kernDumpProcesses;

    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;
}

static void syscallHandler(int dev, void *arg_void)
{
    assert(dev == USLOSS_SYSCALL_INT);
    USLOSS_Sysargs *args = (USLOSS_Sysargs *)arg_void;

    if (args->number < 0 || args->number >= MAXSYSCALLS)
    {
        USLOSS_Console("%s(): Invalid syscall number %d\n", __func__, args->number);
        USLOSS_Halt(1);
    }

    systemCallVec[args->number](args);
}

static void nullsys(USLOSS_Sysargs *args)
{
    USLOSS_Console("%s(): Program called an unimplemented syscall.  syscall no: %d   PSR: 0x%02x\n", __func__, args->number, USLOSS_PsrGet());
    USLOSS_Halt(1);
}

static void kernDumpProcesses(USLOSS_Sysargs *args)
{
    dumpProcesses();
}
