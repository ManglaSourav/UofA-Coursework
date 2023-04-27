
#include <memory.h>
#include <assert.h>

#include <usloss.h>

#include "phase1.h"
#include "phase2.h"

#define TODO()                                                   \
    do                                                           \
    {                                                            \
        USLOSS_Console("TODO() at %s:%d\n", __func__, __LINE__); \
        *(char *)7 = 0;                                          \
    } while (0)

static void diskHandler(int dev, void *arg);
static void termHandler(int dev, void *arg);

typedef struct Device Device;

struct Device
{
    int type, unit; /* sanity checking */
    int mb_id;
};

static Device devs[] = {
    {USLOSS_CLOCK_DEV, 0, -1},

    {USLOSS_DISK_DEV, 0, -1},
    {USLOSS_DISK_DEV, 1, -1},

    {USLOSS_TERM_DEV, 0, -1},
    {USLOSS_TERM_DEV, 1, -1},
    {USLOSS_TERM_DEV, 2, -1},
    {USLOSS_TERM_DEV, 3, -1},

    {-1, -1, -1}};

static int blocked_on_io_count = 0;
static int lastClockIntDelivered = 0;

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

void phase2_devices_init()
{
    assert(sizeof(devs) / sizeof(devs[0]) == 8);

    for (int i = 0; i < 7; i++)
    {
        devs[i].mb_id = MboxCreate(1, sizeof(int));
        assert(devs[i].mb_id >= 0);
    }

    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;
}

static Device *getDev(int type, int unit)
{
    Device *dev;

    if (type == USLOSS_CLOCK_DEV)
    {
        assert(unit == 0);
        dev = &devs[0];
    }
    else if (type == USLOSS_DISK_DEV)
    {
        assert(unit == 0 || unit == 1);
        dev = &devs[1 + unit];
    }
    else if (type == USLOSS_TERM_DEV)
    {
        assert(unit >= 0 && unit < 4);
        dev = &devs[3 + unit];
    }
    else
    {
        USLOSS_Console("%s(): Unsupported device type %d\n", type);
        USLOSS_Halt(1);
    }

    assert(dev->type == type);
    assert(dev->unit == unit);
    return dev;
}

void waitDevice(int type, int unit, int *status)
{
    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();
    assert(status != NULL);
    Device *dev = getDev(type, unit);
    // USLOSS_Console("%s():   DEBUG   Beginning wait on device %d:%d pid %d\n", __func__, type,unit, getpid());
    blocked_on_io_count++;

    MboxRecv(dev->mb_id, status, sizeof(*status));
    blocked_on_io_count--;

    // USLOSS_Console("%s():   DEBUG   Ending wait on device %d:%d pid %d\n", __func__, type,unit, getpid());

    restore_interrupts(psr_save);
}

int phase2_check_io()
{
    return blocked_on_io_count > 0;
}

void wakeupByDevice(int type, int unit, int status)
{
    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    Device *dev = getDev(type, unit);

    //    USLOSS_Console("%s():   DEBUG   Artificially waking up any thread sleeping on device %d:%d pid %d\n", __func__, type,unit, getpid());

    MboxCondSend(dev->mb_id, &status, sizeof(status));

    restore_interrupts(psr_save);
}

void phase2_clockHandler()
{
    Device *clock_dev = getDev(USLOSS_CLOCK_DEV, 0);

    int diff_us = currentTime() - lastClockIntDelivered;
    if (diff_us < 100 * 1000)
        return;
    lastClockIntDelivered += diff_us; // resolves to the currentTime() value above!

    int status;

    int dev_rc = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &status);
    if (dev_rc != USLOSS_DEV_OK)
    {
        USLOSS_Console("%s(): Failed to read the clock device!  rc = %d\n", __func__, dev_rc);
        USLOSS_Halt(1);
    }

    //    USLOSS_Console("%s():   DEBUG   clock tick!  clockCount = %d\n", __func__, clockCount);
    MboxCondSend(clock_dev->mb_id, &status, sizeof(status));
}

static void diskHandler(int dev, void *arg)
{
    assert(dev == USLOSS_DISK_INT);
    int unit = (int)(long)arg;
    // 4 different terminal and 2 devices => finding which terminal or device  interrupt arrives
    Device *term_dev = getDev(dev, unit);

    int status;

    int dev_rc = USLOSS_DeviceInput(dev, unit, &status);
    if (dev_rc != USLOSS_DEV_OK)
    {
        USLOSS_Console("%s(): Failed to read the disk device!  rc = %d\n", __func__, dev_rc);
        USLOSS_Halt(1);
    }

    //    USLOSS_Console("%s(): arg %d    status 0x%08x   char 0x%02x   xmit %d   recv %d\n", __func__, arg, term_dev->status, USLOSS_TERM_STAT_CHAR(term_dev->status), USLOSS_TERM_STAT_XMIT(term_dev->status), USLOSS_TERM_STAT_RECV(term_dev->status));

    MboxCondSend(term_dev->mb_id, &status, sizeof(status));
    //    USLOSS_Console("%s(): MboxCondSend() returned %d\n", __func__, mb_rc);
}

static void termHandler(int dev, void *arg)
{
    assert(dev == USLOSS_TERM_INT);
    int unit = (int)(long)arg;
    Device *term_dev = getDev(dev, unit);

    int status;

    int dev_rc = USLOSS_DeviceInput(dev, unit, &status);
    if (dev_rc != USLOSS_DEV_OK)
    {
        USLOSS_Console("%s(): Failed to read the terminal device!  rc = %d\n", __func__, dev_rc);
        USLOSS_Halt(1);
    }

    //    USLOSS_Console("%s(): arg %d    status 0x%08x   char 0x%02x   xmit %d   recv %d\n", __func__, arg, term_dev->status, USLOSS_TERM_STAT_CHAR(term_dev->status), USLOSS_TERM_STAT_XMIT(term_dev->status), USLOSS_TERM_STAT_RECV(term_dev->status));

    MboxCondSend(term_dev->mb_id, &status, sizeof(status));
    //    USLOSS_Console("%s(): MboxCondSend() returned %d\n", __func__, mb_rc);
}
