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
#include "list.h"

#define QUANTUM 3


int SJFCompare(Thread *a, Thread *b) {
    if(a->getBurstTime() == b->getBurstTime())
        return 0;
    return a->getBurstTime() > b->getBurstTime() ? 1 : -1;
}
int PriorityCompare(Thread *a, Thread *b) {
    if(a->getPriority() == b->getPriority())
        return 0;
    return a->getPriority() > b->getPriority() ? 1 : -1;
}

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler(SchedulerType type) : schedulerType(type), current(0)
{
	switch(schedulerType) {
    case FCFS:
        readyList = new List<Thread *>;
        break;
    case SJF:
        readyList = new SortedList<Thread *>(SJFCompare);
        break;
    case Priority:
        readyList = new SortedList<Thread *>(PriorityCompare);
        break;
    } 
	toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete readyList; 
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
 
	// cout << "Current Thread" <<oldThread->getName() << "    Next Thread"<<nextThread->getName()<<endl;
   
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
#ifdef USER_PROGRAM			// ignore until running user programs 
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
#endif
    
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
    
#ifdef USER_PROGRAM
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
#endif
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

void Scheduler::FallAsleep(Thread *t, int val){
    DEBUG(dbgThread, "Thread " << t->getName() << " waits until " << val << "(ms)");

    sleepingList.push_back(thread_clk(t, current + val));
    t->Sleep(false);
}

bool Scheduler::WakeUp(){
    ++current;
    bool woken = false;

    for(unsigned i = 0; i < sleepingList.size(); ){
        thread_clk it = sleepingList[i];

        if(it.second < current){
            woken = true;
            DEBUG(dbgThread, "Thread "<<kernel->currentThread->getName() << " is Called back");
            ReadyToRun(it.first);
            sleepingList.erase(sleepingList.begin() + i);
        } else ++i;
    }

    return woken;
}

bool Scheduler:: needYield() {
    Thread *now = kernel->currentThread;
    Thread *next = readyList->GetFront();
    if(!next) return false;
    switch (schedulerType) {
        case FCFS      : return false;
        case SJF       : return SJFCompare(now, next) > 0;
        case Priority  : return PriorityCompare(now, next) > 0;
    }
}

void threadBody() {
    Thread *thread = kernel->currentThread;
    while (thread->getBurstTime() > 0) {
        thread->setBurstTime(thread->getBurstTime() - 1);
        kernel->interrupt->OneTick();
        printf("%s: remaining %d\n", kernel->currentThread->getName(), kernel->currentThread->getBurstTime());
    }
}

void Scheduler::SelfTest(int testcase) {
    cout << "Using Testcase: " << testcase << endl;
    cout << "Using scheduler: ";
    switch (schedulerType) {
    case FCFS:
        cout << "FCFS";
        break;

    case SJF:
        cout << "SJF";
        break;

    case Priority:
        cout << "Priority";
        break;
    }
    cout << endl;
    
    const int thread_num = 4;
    char *name[thread_num] = {"A", "B", "C", "D"};
    int thread_priority[thread_num];
    int thread_burst[thread_num];

    switch(testcase){
        case 0:
            thread_priority[0] = 5;
            thread_priority[1] = 1;
            thread_priority[2] = 3;
            thread_priority[3] = 2;

            thread_burst[0] = 3;
            thread_burst[1] = 9;
            thread_burst[2] = 7;
            thread_burst[3] = 3;

            break;

        case 1:
            thread_priority[0] = 5;
            thread_priority[1] = 1;
            thread_priority[2] = 3;
            thread_priority[3] = 2;

            thread_burst[0] = 1;
            thread_burst[1] = 9;
            thread_burst[2] = 2;
            thread_burst[3] = 3;

            break;

        case 2:
            thread_priority[0] = 10;
            thread_priority[1] = 1;
            thread_priority[2] = 2;
            thread_priority[3] = 3;

            thread_burst[0] = 50;
            thread_burst[1] = 10;
            thread_burst[2] = 5;
            thread_burst[3] = 10;

            break;

        default :
            cerr << "No such testcase : " << testcase << endl;
            ASSERTNOTREACHED();
            
    }
    
    Thread *t;
    for (int i = 0; i < thread_num; i ++) {
        t = new Thread(name[i]);
        t->setPriority(thread_priority[i]);
        t->setBurstTime(thread_burst[i]);
        t->Fork((VoidFunctionPtr) threadBody, (void *)NULL);
    }
    kernel->currentThread->Yield();
}