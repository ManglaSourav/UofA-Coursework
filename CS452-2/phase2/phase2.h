/*
 * These are the definitions for phase 2 of the project
 */

#ifndef _PHASE2_H
#define _PHASE2_H

#include <usyscall.h>

// Maximum line length. Used by terminal read and write.
#define MAXLINE 80

#define MAXMBOX 2000
#define MAXSLOTS 2500
#define MAX_MESSAGE 150 // largest possible message in a single slot

/*
1. set, boxes, slots, phase2_procTable to 0 using memeset
2. Initialised every slot length to -1
3. call phase2_devices_init
4. call phase2_syscall_init
*/
extern void phase2_init(void);

/* returns id of mailbox, or -1 if no more mailboxes, or -1 if invalid args
1. Disable interrupts
2. check for empty mailbox
3. After finding empty mailbox assign refcount=1, numSlots and slotSize
4. restore_interrupts and return the mb_id
 */
extern int MboxCreate(int slots, int slot_size);

/* returns 0 if successful, -1 if invalid arg
1. Disable restore_interrupts
2. get the mailbox using given mb_id in the args
3. if refcount=0 or zombie != 0 for the mailbox then return -1
4. set zombie=1 and free any pending messages that are tying up slots using slotQ_peekHead and slotQ_removeHead
5. check for head of blockedProducers and blockedConsumers and wake up the head of both the queues
6. decrement_mailbox_refcount
7. restore interrupts
*/
extern int MboxRelease(int mbox_id);

// returns 0 if successful, -1 if invalid args
/*
1. Disable interrupts
2. get the mailbox using given mb_id in the args
3. if refcount=0 or zombie != 0 for the mailbox then return -1
4. if msgSize > mb->slotSize then return -1
5. handle zero-slot mailboxes if any
6. if head of blockProducer is not null or pendingMsgCount == numSlots then: (block if (a) there are already blocked producers; or (b) we have too
     many mail slots consumed so far)
    a. conditional can not block return -2
    b. increment the refcount of the mailbox
    c. append mb->blockedProducers, block process and then unblock it
    d. if mailbox is zombie then check head of blockedProducers and wake it up
    e. decrement_mailbox_refcount
    f. check zombie if yes then return -3
7. do we have any available message slots? check all slot's len
8. If we are here means we have slot available
9. slot->len = msgSize;
10. memcpy(slot->buf, msgPtr, msgSize)
11. mb->pendingMsgCount++ (add the buffer to the queue of waiting messages)
12. if head of blockedConsumers is not null then unblock it
13. producers must wake up in order.  Now that we've completed our work, is there somebody else who might need to be woken up next
14. restore interrupts
*/
extern int MboxSend(int mbox_id, void *msg_ptr, int msg_size);

// returns size of received msg if successful, -1 if invalid args
/*
1. Disable interrupts
2. get the mailbox using given mb_id in the args
3. if refcount=0 or zombie != 0 for the mailbox then return -1
4. handle zero-slot mailboxes if any
5. if head of blockConsumer is not null or pendingMsgCount == 0 then: (block if (a) there are already blocked consumers; or (b) we have no mail slots consumed so far)
    a. conditional can not block return -2
    b. increment the refcount of the mailbox
    c. append mb->blockedConsumers, block process and then unblock it
    d. if mailbox is zombie then check head of blockedConsumers and wake it up
    e. decrement_mailbox_refcount
    f. check zombie if yes then return -3
6. if we get here, we know that this isn't a weird mailbox: numSlots and
     * slotSize are both positive.
     remove the first message from the queue of messages waiting to be consumed from mailbox mb
7.  mb->pendingMsgCount--;
8.  is the message too large to fit into the destination buffer? return -1
9.  memcpy(msgPtr, slot->buf, slot->len); (copy the message into the destination buffer)
10. memset(slot, 0, sizeof(*slot)); and slot->len=-1 // clear the slot
11. are there any producers waiting to post their messages?  Wake one up
    mb->blockedProducers is not null then unblock it
12. are there other consumers in line, waiting for messages and some messages for them to consume?
    mb->blockedConsumers is not null then unblock it
13. restore interrupts
*/
extern int MboxRecv(int mbox_id, void *msg_ptr, int msg_max_size);

// returns 0 if successful, 1 if mailbox full, -1 if illegal args
// do not block
extern int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size);

// returns 0 if successful, 1 if no msg available, -1 if illegal args
// do not block
extern int MboxCondRecv(int mbox_id, void *msg_ptr, int msg_max_size);

// type = interrupt device type, unit = # of device (when more than one),
// status = where interrupt handler puts device's status register.
extern void waitDevice(int type, int unit, int *status);

extern void wakeupByDevice(int type, int unit, int status);

//
extern void (*systemCallVec[])(USLOSS_Sysargs *args);

#endif
