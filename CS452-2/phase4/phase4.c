
#include <memory.h>
#include <assert.h>

#include <usloss.h>
#include <usyscall.h>    // for SYS_* constants

#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include "phase4.h"



void phase4_clock_init(void);
void phase4_term_init (void);
void phase4_disk_init (void);

int clockDriver(char*);
int  diskDriver(char*);
int  termDriver(char*);

void kernSleep    (USLOSS_Sysargs*);
void kernDiskIO   (USLOSS_Sysargs*);
void kernDiskSize (USLOSS_Sysargs*);
void kernTermRead (USLOSS_Sysargs*);
void kernTermWrite(USLOSS_Sysargs*);



void phase4_init()
{
    phase4_clock_init();
    phase4_term_init();
    phase4_disk_init();

    systemCallVec[SYS_SLEEP] = kernSleep;

    systemCallVec[SYS_DISKREAD ] = kernDiskIO;
    systemCallVec[SYS_DISKWRITE] = kernDiskIO;
    systemCallVec[SYS_DISKSIZE ] = kernDiskSize;

    systemCallVec[SYS_TERMREAD ] = kernTermRead;
    systemCallVec[SYS_TERMWRITE] = kernTermWrite;
}

void phase4_start_service_processes()
{
    fork1("clockDriver",  clockDriver, NULL,     USLOSS_MIN_STACK, 2);

    fork1( "diskDriver_0", diskDriver, (void*)0, USLOSS_MIN_STACK, 2);
    fork1( "diskDriver_1", diskDriver, (void*)1, USLOSS_MIN_STACK, 2);

    fork1( "termDriver_0", termDriver, (void*)0, USLOSS_MIN_STACK, 2);
    fork1( "termDriver_1", termDriver, (void*)1, USLOSS_MIN_STACK, 2);
    fork1( "termDriver_2", termDriver, (void*)2, USLOSS_MIN_STACK, 2);
    fork1( "termDriver_3", termDriver, (void*)3, USLOSS_MIN_STACK, 2);
}

