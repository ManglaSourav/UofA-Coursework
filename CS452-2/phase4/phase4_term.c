
#include <memory.h>
#include <assert.h>

#include <usloss.h>
#include <usyscall.h> // for SYS_* constants

#include "phase1.h"
#include "phase2.h"
#include "phase3.h"
#include "phase4.h"

#define TODO()                                                   \
    do                                                           \
    {                                                            \
        USLOSS_Console("TODO() at %s:%d\n", __func__, __LINE__); \
        *(char *)7 = 0;                                          \
    } while (0)

static int alloc_mutex()
{
    return MboxCreate(1, 0);
}

static void lock(int mutex_mb) { MboxSend(mutex_mb, NULL, 0); }
static void unlock(int mutex_mb) { MboxRecv(mutex_mb, NULL, 0); }

/* for each terminal, we have two structures, one for reads and one for writes,
 * because the two are entirely independent.
 */
typedef struct TermRead TermRead;
struct TermRead
{
    /* handling reading is TRIVIAL if we do it in terms of a mailbox!  Each
     * terminal has enough space to buffer a *SINGLE LINE* of input - but the
     * moment that it is complete, it will be saved using MboxCondSend().  If
     * the mailbox has space for another buffer, then it is saved; if not, then
     * it is discarded.
     */

    /* this is the mailbox we use for storing completed lines */
    int buffer_mb;

    /* this is where we collect the next line-in-progress */
    char working[MAXLINE];
    int soFar;
};

typedef struct TermWrite TermWrite;
struct TermWrite
{
    /* each terminal has its own lock, which is a long-term lock (that is, a
     * lock that you hold while you are flushing out a user's buffer).  The
     * output is spread across many seconds, but the lock is not released until
     * all of the data is flushed.
     *
     * Note that this lock is used for *SEQUENCING* of the various TermWrite()
     * operations on a single terminal; it is not used to protect the actual
     * data structues.
     *
     * Also note that, since this field is initialized in term_init() and never
     * changed, you can read it without a lock (and likewise call Mbox...()
     * functions using it) without a lock.
     */
    int seq_mutex;

    /* this is used to wakeup writing processes when the terminal is ready to
     * accept a write.  It only allows a single message; when we notice that
     * the terminal is ready to accept a write, we do MboxCondSend().  Anyone
     * who wants to write a character must do MboxRecv() before each
     * character.
     */
    int avail_mb;
};

struct Term
{
    TermRead read;
    TermWrite write;
} terms[4];

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

void phase4_term_init()
{
    memset(terms, 0, sizeof(terms));
    /* turn on I/O for all terminals.  TODO: should we intelligently turn it on and off? */
    for (int i = 0; i < 4; i++)
    {
        int term_cr = 0x6;

        // Characters are sent by writing them to the terminal’s control register via
        // where "i" is the unit number of the terminal to be written, and control is the value to write to the control register
        int rc = USLOSS_DeviceOutput(USLOSS_TERM_(void *)(long)term_cr)DEV, i, ;
        if (rc != USLOSS_DEV_OK)
        {
            USLOSS_Console("%s(): Could not set initial state for terminal %d term_cr 0x%04x rc %d\n", __func__, i, term_cr, rc);
            USLOSS_Halt(1);
        }
    }

    for (int i = 0; i < 4; i++)
    {
        terms[i].read.buffer_mb = MboxCreate(10, MAXLINE);
        terms[i].write.seq_mutex = alloc_mutex();
        terms[i].write.avail_mb = MboxCreate(1, 0);

        /* BUGFIX: To get the first I/O started, we have to mark the terminal
         *         as available!  Otherwise, the first writer will block
         *         forever.
         */
        MboxSend(terms[i].write.avail_mb, NULL, 0);
    }
}

static void handleIncomingChar(int unit, char c);
static void handleReadyToWrite(int unit);

int termDriver(char *arg)
{
    int unit = (int)(long)arg;
    assert(0 <= unit && unit < 4);

    while (1)
    {
        int status;
        waitDevice(USLOSS_TERM_DEV, unit, &status);

        // USLOSS_Console("%s(): unit %d status 0x%04x\n", __func__, unit, status);

        /* BUGFIX: We use *different* statuses for read and write.
         *         On the READ side, BUSY means "character available."
         *         On the WRITE side, READY means "gone idle, write another."
         */
        if (USLOSS_TERM_STAT_RECV(status) == USLOSS_DEV_BUSY)
            handleIncomingChar(unit, USLOSS_TERM_STAT_CHAR(status));
        if (USLOSS_TERM_STAT_XMIT(status) == USLOSS_DEV_READY)
            handleReadyToWrite(unit);
    }

    assert(0); // we'll never get here
    return 0;
}

static void handleIncomingChar(int unit, char c)
{
    // USLOSS_Console("%s(): unit %d    c 0x%02x\n", __func__, unit,c);
    assert(0 <= unit && unit < 4);
    TermRead *rd = &terms[unit].read;

    rd->working[rd->soFar] = c;
    rd->soFar++;

    if (c == '\n' || rd->soFar == MAXLINE)
    {
        MboxCondSend(rd->buffer_mb, rd->working, rd->soFar);
        rd->soFar = 0;
    }
}

static void handleReadyToWrite(int unit)
{
    // USLOSS_Console("%s(): unit %d\n", __func__, unit);
    assert(0 <= unit && unit < 4);
    MboxCondSend(terms[unit].write.avail_mb, NULL, 0);
}

void kernTermRead(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_TERMREAD);
    require_kernel_mode(__func__, 0);

    char *buf = args->arg1;
    int maxLen = (int)(long)args->arg2;
    int unit = (int)(long)args->arg3;

    if (unit < 0 || unit >= 4 || buf == NULL || maxLen < 1)
    {
        args->arg4 = (void *)(long)-1;
        return;
    }

    /* we pre-allocate a buffer that is *GUARANTEED* to be able to receive
     * the whole line; we'll copy over partial messages as a second step.
     */
    char bounce[MAXLINE];
    memset(bounce, 0, sizeof(bounce));

    /* NOTE: Since the buffer_mb field never changes outside of term_init(),
     *       we don't need a lock to access it.
     */
    int rc = MboxRecv(terms[unit].read.buffer_mb, bounce, sizeof(bounce));
    assert(rc > 0);
    assert(rc <= MAXLINE);

    //    USLOSS_Console("%s(): line read!\n", __func__);
    //    USLOSS_Console("         buf: '%s'\n", bounce);
    //    USLOSS_Console("         rc %d   maxLen %d\n", rc,maxLen);

    /* if the user asked for less data than is available, only send them
     * as much as they want.
     */
    if (rc > maxLen)
        rc = maxLen;

    /* send the data and we're done */
    memcpy(buf, bounce, maxLen);

    assert(rc > 0);
    args->arg2 = (void *)(long)rc;
    args->arg4 = (void *)(long)0;
}

void kernTermWrite(USLOSS_Sysargs *args)
{
    assert(args->number == SYS_TERMWRITE);
    require_kernel_mode(__func__, 0);

    char *buf = args->arg1;
    int len = (int)(long)args->arg2;
    int unit = (int)(long)args->arg3;

    if (unit < 0 || unit >= 4 || buf == NULL || len < 1)
    {
        args->arg4 = (void *)(long)-1;
        return;
    }

    // USLOSS_Console("%s(): BEFORE lock()    unit %d    pid %d\n", __func__, unit, getpid());
    lock(terms[unit].write.seq_mutex);
    // USLOSS_Console("%s(): AFTER  lock()    unit %d    pid %d\n", __func__, unit, getpid());

    // USLOSS_Console("    buf = '%s'   len=%d    unit=%d\n", buf,len,unit);

    for (int i = 0; i < len; i++)
    {
        // USLOSS_Console("%s(): waiting to be awoken ... unit %d\n", __func__, unit);
        MboxRecv(terms[unit].write.avail_mb, NULL, 0);

        int term_cr = (buf[i] << 8) | 0x7;
        // USLOSS_Console("%s(): Sending Control Register 0x%04x\n", __func__, term_cr);
        int rc = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *)(long)term_cr);
        assert(rc == USLOSS_DEV_OK);
    }

    unlock(terms[unit].write.seq_mutex);

    args->arg2 = (void *)(long)len;
    args->arg4 = (void *)(long)0;
}
