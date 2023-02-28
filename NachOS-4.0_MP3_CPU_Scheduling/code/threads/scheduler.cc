// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

static int
L1SchedulingComp (Thread *x, Thread *y)
{
    if (x->approx_burst_time < y->approx_burst_time) {
        return -1;
    } else if (x->approx_burst_time > y->approx_burst_time) {
        return 1;
    } else {
        return x->getID() < y->getID() ? -1 : 1;
    }
}

static int
L2SchedulingComp (Thread *x, Thread *y)
{
    if (x->priority < y->priority) {
        return 1;
    } else if (x->priority > y->priority) {
        return -1;
    } else {
        return x->getID() < y->getID() ? -1 : 1;
    }
}

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    readyList = new List<Thread *>; 
    toBeDestroyed = NULL;

    // Initial 3-levels ready-queues
    L1List = new SortedList<Thread *>(L1SchedulingComp);
    L2List = new SortedList<Thread *>(L2SchedulingComp);
    L3List = new List<Thread *>;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList; 

    delete L1List;
    delete L2List;
    delete L3List;
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    readyList->Append(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (readyList->IsEmpty()) {
		return NULL;
    } else {
    	return readyList->RemoveFront();
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}


/* MP3 CPU Scheduler */

//----------------------------------------------------------------------
// Scheduler::AddToQueue
// 	
//	
//----------------------------------------------------------------------

void
Scheduler::AddToQueue (Thread *thread, int priority)
{
    // Set start time of thread for aging mechanism
    thread->set_wait_start_time(kernel->stats->totalTicks);

    thread->setStatus(READY);
    if (priority >= 100) {
        DEBUG(dbgSch, "[AddToQueue] Tick ["<< kernel->stats->totalTicks<<"]: " << \
                                  "Thread ["<< thread->ID <<"] is inserted into queueL[1]" );
        L1List->Insert(thread);
    } else if (priority >= 50) {
        DEBUG(dbgSch, "[AddToQueue] Tick ["<< kernel->stats->totalTicks<<"]: " << \
                                  "Thread ["<< thread->ID <<"] is inserted into queueL[2]" );
        L2List->Insert(thread);
    } else {
        DEBUG(dbgSch, "[AddToQueue] Tick ["<< kernel->stats->totalTicks<<"]: " << \
                                  "Thread ["<< thread->ID <<"] is inserted into queueL[3]" );
        L3List->Append(thread);
    }
}

//----------------------------------------------------------------------
// Scheduler::Scheduling
// 	
//	
//----------------------------------------------------------------------

Thread*
Scheduler::Scheduling()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    Thread *nextThread;

    #ifdef DEBUG_QUEUES
    ListAllThread();
    #endif
    
    if (!L1List->IsEmpty()) {
        // Pick a thread from L1 ready queue (SJF)
        DEBUG(dbgSch,  "[Scheduling] !L1List->IsEmpty()");

        nextThread = L1List->RemoveFront();
        nextThread->record_start_ticks(kernel->stats->totalTicks);

        DEBUG(dbgSch,  "[Scheduling] totalTicks ["<<kernel->stats->totalTicks<<"]: nextThread(L1) [" << nextThread->ID <<"], " << \ 
                       "currThread [" << kernel->currentThread->ID << "] " << \
                       "and it has executed [" << kernel->stats->totalTicks - kernel->currentThread->cpu_start_ticks << "] ticks ");
        return nextThread;
    } else {
        if (!L2List->IsEmpty()) {
            // Pick a thread from L2 ready queue (non-preemptive priority)
            DEBUG(dbgSch,  "[Scheduling] !L2List->IsEmpty()");

            nextThread = L2List->RemoveFront();
            nextThread->record_start_ticks(kernel->stats->totalTicks);

            DEBUG(dbgSch,  "[Scheduling] totalTicks ["<<kernel->stats->totalTicks<<"]: nextThread(L2) [" << nextThread->ID <<"], " << \ 
                        "currThread [" << kernel->currentThread->ID << "] " << \
                        "and it has executed [" << kernel->stats->totalTicks - kernel->currentThread->cpu_start_ticks << "] ticks ");
            return nextThread;
        } else {
            if (!L3List->IsEmpty()) {
                // Pick a thread from L3 ready queue (Round-Robin)
                DEBUG(dbgSch,  "[Scheduling] !L3List->IsEmpty()");

                nextThread = L3List->RemoveFront();
                nextThread->record_start_ticks(kernel->stats->totalTicks);

                DEBUG(dbgSch,  "[Scheduling] totalTicks ["<<kernel->stats->totalTicks<<"]: nextThread(L3) [" << nextThread->ID <<"], " << \ 
                            "currThread [" << kernel->currentThread->ID << "] " << \
                            "and it has executed [" << kernel->stats->totalTicks - kernel->currentThread->cpu_start_ticks << "] ticks ");
                return nextThread;
            } else {
                // There is no thread in the 3-levels ready queues
                // DEBUG(dbgSch, "[Scheduling] There is no thread in the 3-levels ready queues");
                return NULL;
            }
        }
    }

    return nextThread;
}

//----------------------------------------------------------------------
// Scheduler::Aging
// 	
//	
//----------------------------------------------------------------------

void
Scheduler::Aging()
{
    Thread *thread;
    int totalTicks = kernel->stats->totalTicks;

    if (!L1List->IsEmpty()) {
        ListIterator<Thread*> *it;
        it = new ListIterator<Thread*> (L1List);
        for (; !it->IsDone(); it->Next()) {
            thread = it->Item();

            thread->accu_wait_ticks += totalTicks - thread->start_wait_ticks;
            thread->start_wait_ticks = totalTicks;

            if (thread->accu_wait_ticks >= 1500) {
                DEBUG(dbgSch, "[Aging] L1 update priority, Thread: " << thread->ID <<  \
                              " , old priority: " << thread->priority);

                // L1 queue limit of priority is 149
                thread->priority = min(149, thread->priority + 10); 
                thread->accu_wait_ticks -= 1500;
            } 
        }
    }

    if (!L2List->IsEmpty()) {
        ListIterator<Thread*> *it;
        it = new ListIterator<Thread*> (L2List);
        for (; !it->IsDone(); it->Next()) {
            thread = it->Item();

            thread->accu_wait_ticks += totalTicks - thread->start_wait_ticks;
            thread->start_wait_ticks = totalTicks;

            if (thread->accu_wait_ticks >= 1500) {
                DEBUG(dbgSch, "[Aging] L2 update priority, Thread: " << thread->ID <<  \
                              " , old priority: " << thread->priority);

                thread->priority += 10;
                thread->accu_wait_ticks -= 1500;
            }
        }
    }

    if (!L3List->IsEmpty()) {
        ListIterator<Thread*> *it;
        it = new ListIterator<Thread*> (L3List);
        for (; !it->IsDone(); it->Next()) {
            thread = it->Item();

            thread->accu_wait_ticks += totalTicks - thread->start_wait_ticks;
            thread->start_wait_ticks = totalTicks;

            if (thread->accu_wait_ticks >= 1500) {
                DEBUG(dbgSch, "[Aging] L3 update priority, Thread: " << thread->ID <<  \
                              " , old priority: " << thread->priority);

                thread->priority += 10;
                thread->accu_wait_ticks -= 1500;
            }
        }
    }
}

//----------------------------------------------------------------------
// Scheduler::ReArrangeThreads
// 	
//	
//----------------------------------------------------------------------

void
Scheduler::ReArrangeThreads()
{   
    Thread *migrated_thread;

    ListIterator<Thread*> *it3;

    it3 = new ListIterator<Thread*> (L3List);
    for (; !it3->IsDone(); it3->Next()) {
        migrated_thread = L3List->RemoveFront();

        if (migrated_thread->priority >= 100) {
            DEBUG(dbgSch, "[ReArrangeThreads] Remove L3 Thread: " << migrated_thread->ID << " to L1");
            L1List->Insert(migrated_thread);
            
            int statusCheckPreempt = this->CheckPreempt(migrated_thread);
            if (statusCheckPreempt) break;
        } else if (migrated_thread->priority >= 50) {
            DEBUG(dbgSch, "[ReArrangeThreads] Remove L3 Thread: " << migrated_thread->ID << " to L2");
            L2List->Insert(migrated_thread);

            int statusCheckPreempt = this->CheckPreempt(migrated_thread);
            if (statusCheckPreempt) break;
        } else {
            DEBUG(dbgSch, "[ReArrangeThreads] Move L3 Thread: " << migrated_thread->ID << " to L3 tail");
            L3List->Append(migrated_thread);
        }
    }

    ListIterator<Thread*> *it2;
    it2 = new ListIterator<Thread*> (L2List);
    for (; !it2->IsDone(); it2->Next()) {
        if (it2->Item()->priority >= 100) {
            migrated_thread = L2List->RemoveFront();
            DEBUG(dbgSch, "[ReArrangeThreads] Remove L2 Thread: " << migrated_thread->ID << " to L1");
            L1List->Insert(migrated_thread);

            int statusCheckPreempt = this->CheckPreempt(migrated_thread);
            if (statusCheckPreempt) break;
        }
    }

    #ifdef DEBUG_QUEUES
    DEBUG(dbgSch, "[DEBUG_QUEUES][ReArrangeThreads] end");
    ListAllThread();
    #endif
}


//----------------------------------------------------------------------
// Scheduler::CheckPreempt
// 	
//	
//----------------------------------------------------------------------

int
Scheduler::CheckPreempt(Thread *thread)
{
    if (kernel->currentThread->get_level_of_queue() == 3 && \
        (thread->get_level_of_queue() == 2 || thread->get_level_of_queue() == 1)) {
        DEBUG(dbgSch, "[CheckPreempt] case 1");

        kernel->currentThread->true_ticks += kernel->stats->totalTicks - kernel->currentThread->cpu_start_ticks;

        Thread *nextThread = this->Scheduling();

        this->AddToQueue(kernel->currentThread, kernel->currentThread->priority);
        this->Run(nextThread, FALSE);
        return 1;
    } else if (kernel->currentThread->get_level_of_queue() == 2 && \
               thread->get_level_of_queue() == 1) {
        DEBUG(dbgSch, "[CheckPreempt] case 2");

        kernel->currentThread->true_ticks += kernel->stats->totalTicks - kernel->currentThread->cpu_start_ticks;

        Thread *nextThread = this->Scheduling();

        this->AddToQueue(kernel->currentThread, kernel->currentThread->priority);
        this->Run(nextThread, FALSE);
        return 1;
    } else if (kernel->currentThread->get_level_of_queue() == 1 && \
               thread->get_level_of_queue() == 1 && \
               (thread->approx_burst_time < kernel->currentThread->approx_burst_time)) {
        DEBUG(dbgSch, "[CheckPreempt] case 3");

        kernel->currentThread->true_ticks += kernel->stats->totalTicks - kernel->currentThread->cpu_start_ticks;

        Thread *nextThread = this->Scheduling();

        this->AddToQueue(kernel->currentThread, kernel->currentThread->priority);
        this->Run(nextThread, FALSE);
        return 1;
    } else {
        return 0;
    }
}


#ifdef DEBUG_QUEUES

void
Scheduler::ListAllThread()
{
    if (L1List->IsEmpty() && L2List->IsEmpty() && L3List->IsEmpty())
        return;

    printf("L1 queue:\n");
    if (!L1List->IsEmpty()) {
        ListIterator<Thread*> *it;
        it = new ListIterator<Thread*> (L1List);
        for (; !it->IsDone(); it->Next()) {
            printf("\t%s(status: %d, priority: %d)\n", it->Item()->getName(), it->Item()->getStatus(), it->Item()->priority);
        }
    }

    printf("L2 queue:\n");
    if (!L2List->IsEmpty()) {
        ListIterator<Thread*> *it;
        it = new ListIterator<Thread*> (L2List);
        for (; !it->IsDone(); it->Next()) {
            printf("\t%s(status: %d, priority: %d)\n", it->Item()->getName(), it->Item()->getStatus(), it->Item()->priority);
        }
    }

    printf("L3 queue:\n");
    if (!L3List->IsEmpty()) {
        ListIterator<Thread*> *it;
        it = new ListIterator<Thread*> (L3List);
        for (; !it->IsDone(); it->Next()) {
            printf("\t%s(status: %d, priority: %d)\n", it->Item()->getName(), it->Item()->getStatus(), it->Item()->priority);
        }
    }
}

#endif