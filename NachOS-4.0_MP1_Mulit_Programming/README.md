# MP2 Multi Programming

## Trace Code

Starting from `threads/kernel.cc ​Kernel::ExecAll()​`, `threads/thread.cc thread::Sleep`​ ​until `machine/mipssim.cc ​Machine::Run()` is called
for executing the first instruction from the user program.

### `threads/thread.h`

Check thread struct

* A thread running a user program actually has *two* sets of CPU registers
    * one for its state while executing user code
    * one for its state while executing kernel code
* `AddrSpace` implement memory translation
* `thread`
    * `InitRegisters()` set the initial register values
    * `RestoreState()` load page table register
    * `kernel->machine->Run()` jump to the user program

```cc
class Thread {
  private:
    // NOTE: DO NOT CHANGE the order of these first two members.
    // THEY MUST be in this position for SWITCH to work.
    int *stackTop;			 // the current stack pointer
    void *machineState[MachineStateSize];  // all registers except for stackTop

  public:
    Thread(char* debugName, int threadID);		// initialize a Thread 
    ~Thread(); 				// deallocate a Thread
					// NOTE -- thread being deleted
					// must not be running when delete 
					// is called

    // basic thread operations

    void Fork(VoidFunctionPtr func, void *arg); 
    				// Make thread run (*func)(arg)
    void Yield();  		// Relinquish the CPU if any 
				// other thread is runnable
    void Sleep(bool finishing); // Put the thread to sleep and 
				// relinquish the processor
    void Begin();		// Startup code for the thread	
    void Finish();  		// The thread is done executing
    
    void CheckOverflow();   	// Check if thread stack has overflowed
    void setStatus(ThreadStatus st) { status = st; }
    ThreadStatus getStatus() { return (status); }
	char* getName() { return (name); }
    
	int getID() { return (ID); }
    void Print() { cout << name; }
    void SelfTest();		// test whether thread impl is working

  private:
    // some of the private data for this class is listed above
    
    int *stack; 	 	// Bottom of the stack 
				// NULL if this is the main thread
				// (If NULL, don't deallocate stack)
    ThreadStatus status;	// ready, running or blocked
    char* name;
	int   ID;
    void StackAllocate(VoidFunctionPtr func, void *arg);
    				// Allocate a stack for thread.
				// Used internally by Fork()

// A thread running a user program actually has *two* sets of CPU registers -- 
// one for its state while executing user code, one for its state 
// while executing kernel code.

    int userRegisters[NumTotalRegs];	// user-level CPU register state

  public:
    void SaveUserState();		// save user-level register state
    void RestoreUserState();		// restore user-level register state

    AddrSpace *space;			// User code this thread is running.
};
```

### `threads/kernel.cc​ Kernel::ExecAll()​`

* Call `Kernel::Exec()` for each threads waiting executed
* 

```cc
void Kernel::ExecAll()
{
	for (int i=1;i<=execfileNum;i++) {
		int a = Exec(execfile[i]);
	}
	currentThread->Finish();
}
```

* `Kernel::Exec()`

  For each program to be executed

  * Create a thread
  * Give thread a address space(AddrSpace)
  * Excute the actual program through `Fork()`
  * Increment `threadNum` by 1

```cc
int Kernel::Exec(char* name)
{
	t[threadNum] = new Thread(name, threadNum);
	t[threadNum]->space = new AddrSpace();
	t[threadNum]->Fork((VoidFunctionPtr) &ForkExecute, (void *)t[threadNum]);
	threadNum++;

	return threadNum - 1;
}
```

* `threads/thread.cc Thread::Fork()`
  * Allocate `stack`
  * Put the thread on the read queue `scheduler->ReadyToRun(this)` (Mark a thread as ready, but not running. Put it on the ready list, for later scheduling onto the CPU.)
  
```cc
int Kernel::Exec(char* name)
{
  ...
	t[threadNum]->Fork((VoidFunctionPtr) &ForkExecute, (void *)t[threadNum]);
  ...
}

void 
Thread::Fork(VoidFunctionPtr func, void *arg)
{
    Interrupt *interrupt = kernel->interrupt;
    Scheduler *scheduler = kernel->scheduler;
    IntStatus oldLevel;
    
    DEBUG(dbgThread, "Forking thread: " << name << " f(a): " << (int) func << " " << arg);
    StackAllocate(func, arg);

    oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);	// ReadyToRun assumes that interrupts are disabled!

    (void) interrupt->SetLevel(oldLevel);
}    

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

* `ForkExecute()` be used by previous part `t[threadNum]->Fork((VoidFunctionPtr) &ForkExecute, ..)`
  * `AddrSpace::Load()` load the program to memory
  * If the previous step is successful, `AddrSpace::Execute()` will be executed

```cc
void ForkExecute(Thread *t)
{
	if ( !t->space->Load(t->getName()) ) {
        /* allocate pageTable for this process */
    	return;             // executable not found
    }
	
    t->space->Execute(t->getName());
}
```

* `AddrSpace::Execute()`
  * Initial the user registers
  * Load the page table
  * `kernel->machine->Run()` simulation system execution

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

* `Thread::Finish()`
  * Called by ThreadRoot when a thread is done executing the forked procedure
```diff
void Kernel::ExecAll()
{
	for (int i=1;i<=execfileNum;i++) {
		int a = Exec(execfile[i]);
	}
+	currentThread->Finish();
    //Kernel::Exec();	
}

void
Thread::Finish()
{
    (void) kernel->interrupt->SetLevel(IntOff);		
    ASSERT(this == kernel->currentThread);
    
    DEBUG(dbgThread, "Finishing thread: " << name);
    Sleep(TRUE);				// invokes SWITCH
    // not reached
}
```

* `Thread::Sleep ()`
  * Only currentThread can call `Sleep()`
  * `while ((nextThread = kernel->scheduler->FindNextToRun()) == NULL) {...}` checks if there is another thread to run
    * if yes, `kernel->scheduler->Run(nextThread, finishing)` got to next
    * if not, `kernel->interrupt->Idle()`. (no one to run, wait for an interrupt)

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