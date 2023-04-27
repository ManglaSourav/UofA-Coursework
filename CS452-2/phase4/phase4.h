/*
 * These are the definitions for phase 4 of the project (support level, part 2).
 */

#ifndef _PHASE4_H
#define _PHASE4_H

#define MAXLINE 80

extern void phase4_init(void);

#endif /* _PHASE4_H */

// calling wait device and say, I' m waiting on disc zero or disc one And
//  when an interrupt shows up from that disk, then this will wake up and then decide what to do with it.
//  Then it will interpret what the interrupt means, and then it will act.
// waitdevice

/*
1. while true
2. waitdevice
3. lock
4. while loop with condition current clock is equal to clockQueue->wakeup_at if yes wake up the sleeping process
5 unlock
int clockDriver(char *ignored)
*/

/*
1. Parse the args to seconds
2. create a mailbox to go to sleep
3. make a ClockQueueEntry (linkedlist) entry
4. lock on mutex
5. find the right position in the linkedlist according to process wakeup_at time and insert entry to that location
6 unlock the mutex
7. cleanup: MboxRecv and MboxRelease

void kernSleep(USLOSS_Sysargs *args)
*/

/*
1. turn on I/O for all terminals using USLOSS_DeviceOutput on all terminals
2. create read and write terminal's mailboxes or allocate_mutex
3. To get the first I/O started, we have to mark the terminal
         *         as available!  Otherwise, the first writer will block forever (using mboxsend()).
phase4_term_init
*/

/*
1. Cast the arguments: buf, maxLen, unit
2. basic checks for all 3 arugments
3. pre-allocate a buffer (bounce) that is to be able to receive the whole line
void kernTermRead(USLOSS_Sysargs *args)
4. call MboxRecv to get the message in bounce buffer
5. memcpy or send data to buf buffer from bouce buffer
void kernTermRead(USLOSS_Sysargs *args)

*/

/*
1. Cast the arguments: buf, len, unit
2. basic checks for all 3 arugments
3. take lock on terms[unit].write.seq_mutex
4. loop till the len
5. call MboxRecv on terms[unit].write.avail_mb
6. send control register using USLOSS_DeviceOutput
7. loop end unlock the seq_mutex
void kernTermWrite(USLOSS_Sysargs *args)

*/