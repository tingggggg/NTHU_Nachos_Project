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