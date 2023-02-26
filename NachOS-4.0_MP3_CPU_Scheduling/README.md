# MP3 CPU Scheduling

## Implement a multilevel feedback queue scheduler  with aging mechanism

### New additions in `kernel.cc / .h`

* `kernel.h`
    * Overload `Exec(...)` to bring in `priority`
    * Add a intger array for record initial priority of thread
    
    ```diff
    class Kernel {
    public:
        ...

        int Exec(char* name);
    +    int Exec(char* name, int initial_priority);

    private:
        Thread* t[10];
        char*   execfile[10];
    +    int     threadPriority[10];
        ...
    ```

* `kernel.c`

    * Add `-ep` flag, Allow the user to create threads and bring in `priority`

    ```diff
    Kernel::Kernel(int argc, char **argv)
    {
        ...
        for (int i = 1; i < argc; i++) {
            ...
            } else if (strcmp(argv[i], "-e") == 0) {
                execfile[++execfileNum]= argv[++i];
                cout << execfile[execfileNum] << "\n";
    +        } else if (strcmp(argv[i], "-ep") == 0) {
    +            execfile[++execfileNum]= argv[++i];
    +            threadPriority[execfileNum] = atoi(argv[++i]);
    +            cout << "Execute <<" << execfile[execfileNum] << " with priority" << threadPriority[execfileNum] << "\n";
    +        } 
            ...
        }
    }
    ```

    * Overload `Exec(...)` to bring in `priority`

    ```diff
    int Kernel::Exec(char* name, int initial_priority)
    {
    -    t[threadNum] = new Thread(name, threadNum);
    +    t[threadNum] = new Thread(name, threadNum, initial_priority);
        t[threadNum]->space = new AddrSpace();
        t[threadNum]->Fork((VoidFunctionPtr) &ForkExecute, (void *)t[threadNum]);
        threadNum++;

        return threadNum-1;
    }
    ```

## Trace Code

### New -> Ready

* `Kernel::ExecAll()`
  * `Exec` the files(threads) to be executed sequentially (`execfile` stores the name of files that waited executing)
  * `currentThread` is `main` thread (Called by ThreadRoot when a thread is done executing the forked procedure)

* `Kernel::Exec()`
  * Create a `thread` object
  * Allocate an address space to the newly created `thread`


```cc
void Kernel::ExecAll()
{
	for (int i=1;i<=execfileNum;i++) {
		int a = Exec(execfile[i]);
	}
	currentThread->Finish();
}

int Kernel::Exec(char* name)
{
	t[threadNum] = new Thread(name, threadNum);
	t[threadNum]->space = new AddrSpace();
	t[threadNum]->Fork((VoidFunctionPtr) &ForkExecute, (void *)t[threadNum]);
	threadNum++;

	return threadNum-1;
}

void ForkExecute(Thread *t)
{
	if ( !t->space->Load(t->getName()) ) {
    	return;             // executable not found
    }
	
    t->space->Execute(t->getName());
}
```

* `Thread::Fork()`
  * `t[threadNum]->Fork` to allocate and initial `stack`
  * Send this thread to the ready queue

* `Thread StackAllocate()`
  * Allocate a stack space (size is `StackSize`)
  * `ThreadRoot` is called from the SWITCH() routine to start a thread for the first time.
  * The setting of `PCState` is based on the definition in `switch.h`/`switch.S`
    
    ```asm
    _ThreadRoot:
    lwz	r20, 16(r4)	/* StartupPCState - ThreadBegin		*/
    lwz	r21, 8(r4)	/* InitialArgState - arg		*/
    lwz	r22, 4(r4)	/* InitialPCState - func		*/
    lwz	r23, 12(r4)	/* WhenDonePCState - ThreadFinish	*/
    ```

```cc
void 
Thread::Fork(VoidFunctionPtr func, void *arg)
{
    Interrupt *interrupt = kernel->interrupt;
    Scheduler *scheduler = kernel->scheduler;
    IntStatus oldLevel;
    
    DEBUG(dbgThread, "Forking thread: " << name << " f(a): " << (int) func << " " << arg);
    StackAllocate(func, arg);

    oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);	// ReadyToRun assumes that interrupts 
					// are disabled!
    (void) interrupt->SetLevel(oldLevel);
}   

void
Thread::StackAllocate(VoidFunctionPtr func, void *arg)
{
  // 1.
  stack = (int *) AllocBoundedArray(StackSize * sizeof(int));
  // 2.
  stackTop = stack + StackSize - 4; // -4 to be on the safe side!
  // 3.
  *(--stackTop) = (int) ThreadRoot;

  *stack = STACK_FENCEPOST;
  // 4.
  machineState[PCState] = (void*)ThreadRoot;
  machineState[StartupPCState] = (void*)ThreadBegin;
  machineState[InitialPCState] = (void*)func;
  machineState[InitialArgState] = (void*)arg;
  machineState[WhenDonePCState] = (void*)ThreadFinish;
}
```

* `AllocBoundedArray()`
  * Apply for two additional spaces of size `pgSize` before and after the address space, and then protect(`mprotect`) the memory before and after the address space to avoid access illegal address

```cc
char * 
AllocBoundedArray(int size)
{
#ifdef NO_MPROT
    return new char[size];
#else
    int pgSize = getpagesize();
    char *ptr = new char[pgSize * 2 + size];

    mprotect(ptr, pgSize, 0);
    mprotect(ptr + pgSize + size, pgSize, 0);
    return ptr + pgSize;
#endif
}
```

* `Scheduler::ReadyToRun()`
  * Check the interrupt is disabled or not
  * Set input `thread` to `READY` status
  * Add `thread` to read queue
  
```cc
void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    readyList->Append(thread);
}
```

***

### Running -> Ready

`AddrSpace::Execute` will call `kernel_machine->Run()` (jump to the user program)

```cc
void 
AddrSpace::Execute(char* fileName) 
{

    kernel->currentThread->space = this;

    this->InitRegisters();		// set the initial register values
    this->RestoreState();		// load page table register

    kernel->machine->Run();		// jump to the user progam

    ASSERTNOTREACHED();			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}
```

* `Machine::Run()`
  * Run user program in `user mode`
  * Infinite loop
    * Decode the user program (instruction)
    * `OneTick` to simulate system/user clock time

```cc
void
Machine::Run()
{
    Instruction *instr = new Instruction;  // storage for decoded instruction

    if (debug->IsEnabled('m')) {
        cout << "Starting program in thread: " << kernel->currentThread->getName();
		cout << ", at time: " << kernel->stats->totalTicks << "\n";
    }
    kernel->interrupt->setStatus(UserMode);
    for (;;) {
        OneInstruction(instr);
		kernel->interrupt->OneTick();
		if (singleStep && (runUntilTime <= kernel->stats->totalTicks))
	  		Debugger();
    }
}
```

* `Interrupt::OneTick()`
  * `CheckIfDue` check any pending interrupts are now ready to fire
  * `yieldOnReturn` if the timer device handler asked for a context switch, ok to do it now
  * `kernel->currentThread->Yield()` makes currentThread executed `Yield` and give up CPU resources
```cc
void
Interrupt::OneTick()
{
    MachineStatus oldStatus = status;
    Statistics *stats = kernel->stats;

// advance simulated time
    if (status == SystemMode) {
        stats->totalTicks += SystemTick;
	stats->systemTicks += SystemTick;
    } else {
	stats->totalTicks += UserTick;
	stats->userTicks += UserTick;
    }
    DEBUG(dbgInt, "== Tick " << stats->totalTicks << " ==");

// check any pending interrupts are now ready to fire
    ChangeLevel(IntOn, IntOff);	// first, turn off interrupts
				// (interrupt handlers run with
				// interrupts disabled)
    CheckIfDue(FALSE);		// check for pending interrupts
    ChangeLevel(IntOff, IntOn);	// re-enable interrupts
    if (yieldOnReturn) {	// if the timer device handler asked 
    				// for a context switch, ok to do it now
	yieldOnReturn = FALSE;
 	status = SystemMode;		// yield is a kernel routine
	kernel->currentThread->Yield();
	status = oldStatus;
    }
}
```

* `Thread::Yield()`

  The purpose is to switch threads

  * Disable interrupt
  * `Scheduler`
    * Find next thread in ready list
    * Add current thread to the ready list
    * Run the next thread
  * Enable interrupt

```cc
void
Thread::Yield ()
{
    Thread *nextThread;
    IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
    
    ASSERT(this == kernel->currentThread);
    
    DEBUG(dbgThread, "Yielding thread: " << name);
    
    nextThread = kernel->scheduler->FindNextToRun();
    if (nextThread != NULL) {
	kernel->scheduler->ReadyToRun(this);
	kernel->scheduler->Run(nextThread, FALSE);
    }
    (void) kernel->interrupt->SetLevel(oldLevel);
}
```

* `Scheduler::FindNextToRun()`
  * Return the next thread to be scheduled onto the CPU
  * If there are no ready threads, return NULL

```cc
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
```

* `Scheduler::ReadyToRun()`
  * Mark a thread as ready, but not running, then put it on the ready list, for later scheduling onto the CPU.
```cc
void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    readyList->Append(thread);
}
```

* `Scheduler::Run()`
  * Dispatch the CPU to nextThread.  
  * Save the state of the old thread, and load the state of the new thread, by calling the machine dependent context switch routine, `SWITCH`.

```cc
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
```

***

### Running -> Waiting

* `SynchConsoleOutput::PutChar()`
  * `Acquire()`, `Release()` wraps critical section
  * `ConsoleOutput::PutChar()` put `this` into interrupt pending list (in order to wait to be able to executed `ConsoleOutput::CallBack()`)
  * `callWhenDone` interrupt handler to call when the next char can be put
    * `SynchConsoleInput::CallBack()` increment semaphore value, waking up a waiter if necessary


```cc
void
SynchConsoleOutput::PutChar(char ch)
{
    lock->Acquire();
    consoleOutput->PutChar(ch);
    waitFor->P();
    lock->Release();
}

void
ConsoleOutput::PutChar(char ch)
{
    ASSERT(putBusy == FALSE);
    WriteFile(writeFileNo, &ch, sizeof(char));
    putBusy = TRUE;
    kernel->interrupt->Schedule(this, ConsoleTime, ConsoleWriteInt);
}

void
Interrupt::Schedule(CallBackObj *toCall, int fromNow, IntType type)
{
    int when = kernel->stats->totalTicks + fromNow;
    PendingInterrupt *toOccur = new PendingInterrupt(toCall, when, type);

    DEBUG(dbgInt, "Scheduling interrupt handler the " << intTypeNames[type] << " at time = " << when);
    ASSERT(fromNow > 0);

    pending->Insert(toOccur);
}

void
ConsoleOutput::CallBack()
{
    putBusy = FALSE;
    kernel->stats->numConsoleCharsWritten++;
    callWhenDone->CallBack();
}
void
SynchConsoleInput::CallBack()
{
    waitFor->V();
}
```

* `Semaphore::P()`
  * Wait until semaphore value > 0, then decrement Checking the value and decrementing must be done atomically
  * We need to disable interrupts before checking the value

```cc
void
Semaphore::P()
{
    Interrupt *interrupt = kernel->interrupt;
    Thread *currentThread = kernel->currentThread;
    
    // disable interrupts
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	
    
    while (value == 0) { 		// semaphore not available
	queue->Append(currentThread);	// so go to sleep
	currentThread->Sleep(FALSE);
    } 
    value--; 			// semaphore available, consume its value
   
    // re-enable interrupts
    (void) interrupt->SetLevel(oldLevel);	
}
```

* `SyncList<T>::Append()`
  * Singly linked-list
  * 

```cc
template <class T>
class ListElement {
  public:
    ListElement(T itm); 	// initialize a list element
    ListElement *next;	     	// next element on list, NULL if this is last
    T item; 	   	     	// item on the list
};

template <class T>
void
List<T>::Append(T item)
{
    ListElement<T> *element = new ListElement<T>(item);

    ASSERT(!this->IsInList(item));
    if (IsEmpty()) {		// list is empty
	first = element;
	last = element;
    } else {			// else put it after last
	last->next = element;
	last = element;
    }
    numInList++;
    ASSERT(this->IsInList(item));
}
```

* `Thread::Sleep`
  * Let the thread status waiting for `semaphore` value be changed to `block`
    * If there is a next thread, find it and execute
    * If there is no next thread, goto `Idle`
      * Check for any pending interrupts. If there are no pending interrupts, stop
```cc
void
Thread::Sleep (bool finishing)
{
    Thread *nextThread;
    
    ASSERT(this == kernel->currentThread);
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    
    DEBUG(dbgThread, "Sleeping thread: " << name);

    status = BLOCKED;
	//cout << "debug Thread::Sleep " << name << "wait for Idle\n";
    while ((nextThread = kernel->scheduler->FindNextToRun()) == NULL) {
		kernel->interrupt->Idle();	// no one to run, wait for an interrupt
	}    
    // returns when it's time for us to run
    kernel->scheduler->Run(nextThread, finishing); 
}
```

* `Scheduler::FindNextToRun()`
  
  See previous chapters
  
* `Scheduler::Run()`

  See previous chapters