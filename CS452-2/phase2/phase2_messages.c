
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

#define PHASE2_BLOCKED_MSG_RECV 11
#define PHASE2_BLOCKED_MSG_SEND 12
#define PHASE2_BLOCKED_MSG_ZERO_SLOT 13

typedef struct ProcQueue ProcQueue;
typedef struct SlotQueue SlotQueue;

typedef struct Mailbox Mailbox;
typedef struct MailSlot MailSlot;

typedef struct Phase2_ProcTableEntry Phase2_ProcTableEntry;

struct ProcQueue
{
    Phase2_ProcTableEntry *head;
    Phase2_ProcTableEntry *tail;

    /* we need to be able to wakeup the head process in a queue.  But our
     * semantics require (for the sake of ordering) that a process stay on
     * the queue until it wakes and removes itself.  So, if a wakeup is already
     * pending on our head but it hasn't removed itself from the queue yet,
     * another wakeup is illegal.
     */
    int wakeupPending;
};

struct MailSlot
{
    int len; // -1 means "not in use;" zero-length messages are possible!
    char buf[MAX_MESSAGE];

    MailSlot *next;
};

struct SlotQueue
{
    MailSlot *head;
    MailSlot *tail;
};

struct Mailbox
{
    int refcount; // free to use if refcount == 0
    int zombie;   // set to 1 in MboxRelease(); tells waiters to go away

    int numSlots;
    int slotSize;

    int pendingMsgCount;

    ProcQueue blockedProducers;
    ProcQueue blockedConsumers;

    SlotQueue pendingMessages;
};


struct Phase2_ProcTableEntry
{
    int pid;                     // 0 if this process is not waiting for any reason.
    Phase2_ProcTableEntry *next; // if this process is blocked as a producer or consumer
};

static Mailbox boxes[MAXMBOX];
static MailSlot slots[MAXSLOTS];
static Phase2_ProcTableEntry phase2_procTable[MAXPROC];

static void procQ_append(ProcQueue *queue)
{
    Phase2_ProcTableEntry *proc = &phase2_procTable[getpid() % MAXPROC];
    assert(proc->pid == 0);
    assert(proc->next == NULL);

    proc->pid = getpid();

    if (queue->head == NULL)
        queue->head = queue->tail = proc;
    else
        queue->tail = queue->tail->next = proc;
}

static Phase2_ProcTableEntry *procQ_peekHead(ProcQueue *queue)
{
    /* it's OK if head is NULL.  But if it's not, it must have a valid PID */
    if (queue->head != NULL)
        assert(queue->head->pid != 0);

    return queue->head;
}

static void procQ_wakeupHead(ProcQueue *queue)
{
    /* this is called when we want to make sure that the head process (if any)
     * is in the process of waking up.  It's OK if the process has already been
     * woken up (then this is a NOP), but it's required that there *be* a head.
     */
    assert(queue->head != NULL);

    if (queue->wakeupPending)
        return;

    queue->wakeupPending = 1;
    unblockProc(queue->head->pid);
}

static void procQ_removeSelfFromHead(ProcQueue *queue)
{
    assert(queue->wakeupPending);
    queue->wakeupPending = 0;

    assert(queue->head != NULL);
    Phase2_ProcTableEntry *head = queue->head;

    if (queue->head->next == NULL)
        queue->head = queue->tail = NULL;
    else
        queue->head = queue->head->next;

    assert(head->pid == getpid());
    memset(head, 0, sizeof(*head));
}

static void slotQ_append(SlotQueue *queue, MailSlot *slot)
{
    assert(slot->next == NULL);

    if (queue->head == NULL)
        queue->head = queue->tail = slot;
    else
        queue->tail = queue->tail->next = slot;
}

static MailSlot *slotQ_peekHead(SlotQueue *queue)
{
    return queue->head; // could be NULL, that's OK
}

static MailSlot *slotQ_removeHead(SlotQueue *queue)
{
    assert(queue->head != NULL);
    MailSlot *head = queue->head;

    if (queue->head->next == NULL)
        queue->head = queue->tail = NULL;
    else
        queue->head = queue->head->next;

    head->next = NULL;

    return head;
}

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

extern void phase2_devices_init();
extern void phase2_syscall_init();

void phase2_init()
{
    memset(boxes, 0, sizeof(boxes));
    memset(slots, 0, sizeof(slots));
    memset(phase2_procTable, 0, sizeof(phase2_procTable));

    for (int i = 0; i < MAXSLOTS; i++)
        slots[i].len = -1;

    phase2_devices_init();
    phase2_syscall_init();
}

void phase2_start_service_processes()
{
    /* NOP */
}

/* helper function */
void decrement_mailbox_refcount(Mailbox *mb)
{
    assert(mb->refcount > 0);
    mb->refcount--;

    if (mb->refcount == 0)
        memset(mb, 0, sizeof(*mb));
}

int MboxCreate(int numSlots, int slotSize)
{
    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    if (numSlots < 0 || slotSize < 0 || slotSize > MAX_MESSAGE || numSlots > MAXSLOTS)
        return -1;

    // are there any free mailboxes to use?
    int mb_id = 0;

    while (boxes[mb_id].refcount > 0)
    {
        mb_id++;
        if (mb_id == MAXMBOX)
        {
            mb_id = -1;
            goto DONE;
        }
    }

    Mailbox *mb = &boxes[mb_id];

    mb->refcount = 1;
    mb->numSlots = numSlots;
    mb->slotSize = slotSize;

DONE:
    restore_interrupts(psr_save);
    return mb_id;
}

int MboxRelease(int mb_id)
{
    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    int rc = -100;
    Mailbox *mb = &boxes[mb_id];

    if (mb->refcount == 0 || mb->zombie != 0)
    {
        rc = -1;
        goto DONE;
    }

    /* free any pending messages that are tying up slots */
    while (slotQ_peekHead(&mb->pendingMessages) != NULL)
    {
        MailSlot *slot = slotQ_removeHead(&mb->pendingMessages);
        memset(slot, 0, sizeof(*slot));
    }

    /* let all other threads know that this is in the process of being
     * released.  Then, wake up the head producer and/or consumer.  Note that
     * we *only* wake up the head, because each queue must wake itself in
     * order.
     *
     * Also note that it might seem like it would be impossible to have both
     * producers and consumers.  But if a consumer is starved of CPU, it can
     * happen; the consumer is starved but hasn't been able to remove itself
     * from the queue yet, so consumers and messages back up as more producers
     * send messages; eventually, we start queueing producer processes as well.
     */
    mb->zombie = 1;

    if (procQ_peekHead(&mb->blockedProducers) != NULL)
        procQ_wakeupHead(&mb->blockedProducers);
    if (procQ_peekHead(&mb->blockedConsumers) != NULL)
        procQ_wakeupHead(&mb->blockedConsumers);

    /* there is a race.  Is this the last process to know about the mailbox
     * slot, or do one of those producers or consumers that I just woke up
     * know about it last?  We'll let the last to decrement the refcount be
     * the one to free the memory.
     */
    decrement_mailbox_refcount(mb);

    /* all done! */
    rc = 0;

DONE:
    restore_interrupts(psr_save);
    return rc;
}

static int Mbox_zeroSlot(Mailbox *mb, int side, int conditional);
static int MboxSend_helper(int mb_id, void *msgPtr, int msgSize, int conditional);

int MboxSend(int mb_id, void *msgPtr, int msgSize)
{
    return MboxSend_helper(mb_id, msgPtr, msgSize, 0);
}
int MboxCondSend(int mb_id, void *msgPtr, int msgSize)
{
    return MboxSend_helper(mb_id, msgPtr, msgSize, 1);
}

static int MboxSend_helper(int mb_id, void *msgPtr, int msgSize, int conditional)
{
    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    int rc = -100;
    Mailbox *mb = &boxes[mb_id];

    if (mb->refcount == 0 || mb->zombie != 0)
    {
        rc = -1;
        goto DONE;
    }

    if (msgSize > mb->slotSize)
    {
        rc = -1;
        goto DONE;
    }

    /* zero-slot mailboxes are weird!  I've put them in their own function. */
    if (mb->numSlots == 0)
    {
        assert(msgSize == 0);
        rc = Mbox_zeroSlot(mb, 0, conditional);
        goto DONE;
    }

    /* if we get here, we know that this isn't a weird mailbox: numSlots and
     * slotSize are both positive.  While it's still possible to have a
     * zero-length *message*, we will treat it the same as a non-zero-length,
     * including using a slot to hold it.
     */

    /* block if (a) there are already blocked producers; or (b) we have too
     * many mail slots consumed so far.
     */
    assert(mb->pendingMsgCount >= 0);
    assert(mb->pendingMsgCount <= mb->numSlots);
    if (procQ_peekHead(&mb->blockedProducers) != NULL || mb->pendingMsgCount == mb->numSlots)
    {
        /* MboxCondSend() cannot block */
        if (conditional)
        {
            rc = -2;
            goto DONE;
        }

        /* see comments in a very similar block in MboxRecieve() */
        mb->refcount++;

        procQ_append(&mb->blockedProducers);
        blockMe(PHASE2_BLOCKED_MSG_SEND);
        procQ_removeSelfFromHead(&mb->blockedProducers);

        int zombie = mb->zombie;
        if (zombie)
        {
            /* must perform this if statement before decrementing the
             * refcount, since if the refcount was 0, the struct would be
             * invalid.
             */
            if (procQ_peekHead(&mb->blockedProducers) != NULL)
                procQ_wakeupHead(&mb->blockedProducers);
        }

        decrement_mailbox_refcount(mb);

        if (zombie)
        {
            rc = -3;
            goto DONE;
        }

        /* when we get here, we are now (rather, just were) the head of the
         * queue (there might or might not be additional processes in the queue
         * after us), but we have the budget to queue up another message.  We
         * will not block again, in this function.
         */
        assert(mb->pendingMsgCount < mb->numSlots);
    }

    /* do we have any available message slots? */
    int slot_id = 0;
    while (slots[slot_id].len != -1)
    {
        slot_id++;
        if (slot_id == MAXSLOTS)
        {
            USLOSS_Console("%s: Could not send, the system is out of mailbox slots.\n", __func__);
            rc = -2;
            goto DONE;
        }
    }
    MailSlot *slot = &slots[slot_id];

    /* copy the message into the buffer */
    slot->len = msgSize;
    memcpy(slot->buf, msgPtr, msgSize);

    /* add the buffer to the queue of waiting messages */
    mb->pendingMsgCount++;

    /* wakeup the first pending consumer (if it's not already runnable and
     * waiting for a turn on the CPU).
     */
    if (procQ_peekHead(&mb->blockedConsumers) != NULL)
        procQ_wakeupHead(&mb->blockedConsumers);

    /* producers must wake up in order.  Now that we've completed our work,
     * is there somebody else who might need to be woken up next?
     */
    if (mb->pendingMsgCount < mb->numSlots && procQ_peekHead(&mb->blockedProducers) != NULL)
        procQ_wakeupHead(&mb->blockedProducers);

    /* all good! */
    rc = 0;

DONE:
    restore_interrupts(psr_save);
    return rc;
}

static int MboxRecv_helper(int mb_id, void *msgPtr, int msgMaxSize, int conditional);

int MboxRecv(int mb_id, void *msgPtr, int msgMaxSize)
{
    return MboxRecv_helper(mb_id, msgPtr, msgMaxSize, 0);
}
int MboxCondRecv(int mb_id, void *msgPtr, int msgMaxSize)
{
    return MboxRecv_helper(mb_id, msgPtr, msgMaxSize, 1);
}

static int MboxRecv_helper(int mb_id, void *msgPtr, int msgMaxSize, int conditional)
{
    require_kernel_mode(__func__, 0);
    unsigned int psr_save = disable_interrupts();

    int rc = -100;
    Mailbox *mb = &boxes[mb_id];

    if (mb->refcount == 0 || mb->zombie != 0)
    {
        rc = -1;
        goto DONE;
    }

    /* zero-slot mailboxes are weird!  I've put them in their own function. */
    if (mb->numSlots == 0)
    {
        assert(msgMaxSize == 0);
        rc = Mbox_zeroSlot(mb, 1, conditional);
        goto DONE;
    }

    /* a consumer *always* blocks if there not any pending messages in the
     * mailbox (including the case of zero-length messages).  So while we
     * check for zero-length first in the case of Send(), in the case of
     * Recv(), we must check for pending messages first.
     *
     * Note that, just like in Send(), if there are other consumers ahead
     * of us, then we'll queue up no matter what the message state it.
     */
    if (mb->pendingMsgCount == 0 || procQ_peekHead(&mb->blockedConsumers) != NULL)
    {
        /* MboxCondRecv() cannot block */
        if (conditional)
        {
            rc = -2;
            goto DONE;
        }

        /* we are about to go to sleep.  Increment the refcount so that the
         * mailbox isn't freed (or even re-used!) while we're asleep.
         */
        mb->refcount++;

        procQ_append(&mb->blockedConsumers);
        blockMe(PHASE2_BLOCKED_MSG_RECV);
        procQ_removeSelfFromHead(&mb->blockedConsumers);

        /* the mailbox may have been destroyed while we were waiting.  Reak
         * that flag before we decrement refcount, or the struct might be
         * wiped.
         */
        int zombie = mb->zombie;
        if (zombie)
        {
            /* must perform this if statement before decrementing the
             * refcount, since if the refcount was 0, the struct would be
             * invalid.
             */
            if (procQ_peekHead(&mb->blockedConsumers) != NULL)
                procQ_wakeupHead(&mb->blockedConsumers);
        }

        decrement_mailbox_refcount(mb);

        if (zombie)
        {
            rc = -3;
            goto DONE;
        }

        /* we must not have been woken up (assuming the queue wasn't destroyed)
         * until there was at least one pending message for us
         */
        assert(mb->pendingMsgCount > 0);
    }

    /* if we get here, we know that this isn't a weird mailbox: numSlots and
     * slotSize are both positive.  While it's still possible to have a
     * zero-length *message* in the queue (or, that our receive buffer is
     * zero-length), we will treat this message the same as a non-zero-length,
     * including reading it from a slot.
     */

    /* if we get here, then the message that we're trying to receive has
     * nonzero length.  We will have to read it from a mail slot, somewhere.
     *
     * remove the first message from the queue of messages
     */
    MailSlot *slot = slotQ_removeHead(&mb->pendingMessages);
    mb->pendingMsgCount--;

    /* is the message too large to fit into the destination buffer? */
    if (slot->len > msgMaxSize)
    {
        rc = -1;
        goto DONE;
    }

    /* copy the message from the slot, and then free the slot */
    int len = slot->len;
    memcpy(msgPtr, slot->buf, len);

    memset(slot, 0, sizeof(*slot));
    slot->len = -1;

    /* are there any producers waiting to post their messages?  Wake one up
     * if so.
     *
     * Are you worried that a race might occur here?  What if we wake up a
     * producer, but in the meantime another thread adds a message to a slot?
     * That's not possible because, if there are *any* producers on the queue,
     * other producers will refuse to post their messages; instead, they put
     * themselves on the end of the queue.
     */
    assert(mb->pendingMsgCount < mb->numSlots);
    if (procQ_peekHead(&mb->blockedProducers) != NULL)
        procQ_wakeupHead(&mb->blockedProducers);

    /* are there other consumers in line, waiting for messages - and some
     * messages for them to consume?
     */
    if (mb->pendingMsgCount > 0 && procQ_peekHead(&mb->blockedConsumers) != NULL)
        procQ_wakeupHead(&mb->blockedConsumers);

    /* all good! */
    rc = len;

DONE:
    restore_interrupts(psr_save);
    return rc;
}

static int Mbox_zeroSlot(Mailbox *mb, int side, int conditional)
{
    /* handle zero-slot mailboxes.  zero-slot mailboxes can only wake up
     * a sleeping process; they either wake up a blocked consumer (on Send())
     * or a blocked producer (on Recv()).  They support Cond operations, too.
     *
     * We implement this logic by "send"ing messages in *either* direction -
     * not just from Send() to Recv(), but instead (sometimes) from
     * Recv() to Send().  We never send messages *both* directions for a
     * given linkup; instead the 2nd-to-arrive sends a message to the first.
     * We'll use the "pendingMsgCount" field (either 0 or 1) to indicate
     * whether one of these wakeups is in process; if it is, then we'll queue
     * all future incoming operations.
     *
     * In truth, we'll sometimes set the pendingMsgCount to 2!  Each call that
     * "sends" messages does not block; instead, it sets pendingMsgCount and
     * wakes up the receiver.  Thus, every process that blocks will (after
     * checking for zombie) decrements the pendingMsgCount as they "receive"
     * the message.  But if, by the time that they do this, there are now a
     * blocked consumer *and* producer, then the current thread can unblock
     * *both* of them.  So it sets pendingMsgCount to 2, since the current
     * process is essentially sending *two* messages.
     *
     * Of course, this means that the wakeup logic can't execute when
     * pendingMsgCount is nonzero, since you don't know whether the "leftover"
     * process is a producer or consumer.
     */

    /* the 'side' tells us which queue to add ourselves to */
    ProcQueue *myQ, *theirQ;
    if (side == 0)
    {
        myQ = &mb->blockedProducers;
        theirQ = &mb->blockedConsumers;
    }
    else
    {
        myQ = &mb->blockedConsumers;
        theirQ = &mb->blockedProducers;
    }

    /* we block if any of the following are true:
     *   1) a message is still in flight, to *some* process
     *   2) we already have blocked processes on the queue that applies to us
     *   3) the *other* side has no blocked processes
     */
    if (mb->pendingMsgCount > 0 ||
        procQ_peekHead(myQ) != NULL || procQ_peekHead(theirQ) == NULL)
    {
        /* we need to block.  But if we're conditional, then that's
         * not allowed.
         */
        if (conditional)
            return -2;

        /* we are about to go to sleep.  Increment the refcount so that the
         * mailbox isn't freed (or even re-used!) while we're asleep.
         */
        mb->refcount++;

        procQ_append(myQ);
        blockMe(PHASE2_BLOCKED_MSG_ZERO_SLOT);
        procQ_removeSelfFromHead(myQ);

        int zombie = mb->zombie;
        if (zombie)
        {
            /* must perform this if statement before decrementing the
             * refcount, since if the refcount was 0, the struct would be
             * invalid.
             */
            if (procQ_peekHead(myQ) != NULL)
                procQ_wakeupHead(myQ);
        }

        decrement_mailbox_refcount(mb);

        if (zombie)
            return -3;

        assert(mb->pendingMsgCount > 0);
        mb->pendingMsgCount--;

        /* if we've consumed the last pending message, then see if we have
         * reason to wake up another pair of processes.
         */
        if (mb->pendingMsgCount == 0 &&
            procQ_peekHead(myQ) != NULL && procQ_peekHead(theirQ) != NULL)
        {
            mb->pendingMsgCount += 2;
            procQ_wakeupHead(myQ);
            procQ_wakeupHead(theirQ);
        }

        return 0;
    }

    /* if we get here, then we're in a perfect situation to send a wakeup
     * to the other side: nothing is in flight, nothing is blocked on our
     * side, but something is blocked on the opposite.
     */
    assert(mb->pendingMsgCount == 0);
    assert(procQ_peekHead(myQ) == NULL);
    assert(procQ_peekHead(theirQ) != NULL);

    mb->pendingMsgCount++;
    procQ_wakeupHead(theirQ);

    return 0;
}
