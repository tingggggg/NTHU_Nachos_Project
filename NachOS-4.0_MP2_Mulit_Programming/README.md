# MP2 Multi Programming

## Implement page table in NachOS

### Modify pageTable building

* `AddrSpace`

  * `AddrSpace::AddrSpace()` load a thread to use whole physical memort in default

  ```cc
  AddrSpace::AddrSpace()
  {
      pageTable = new TranslationEntry[NumPhysPages];
      for (int i = 0; i < NumPhysPages; i++) {
        pageTable[i].virtualPage = i;	// for now, virt page # = phys page #
        pageTable[i].physicalPage = i;
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;  
      }
      
      // zero out the entire address space
      bzero(kernel->machine->mainMemory, MemorySize);
  }
  ```

  * Load at runtime stage instead of load at initial stage
    * `numPages = divRoundUp(size, PageSize)` calculate the actual number of pages needed and check `ASSERT(numPages < kernel->usedPhyPage->numUnused())`
    * Set up page table
    * Add a new member(`class UsedPhyPage`) in `Kernel` to manage the state of the page table(physical memory)

  ```cc
  bool AddrSpace::Load(char *fileName)
  {
    ...

    #ifdef RDATA
    // how big is address space?
        size = noffH.code.size + noffH.readonlyData.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;	
        // we need to increase the size to leave room for the stack
    #else
    // how big is address space?
        size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;	
        // we need to increase the size to leave room for the stack
    #endif

    numPages = divRoundUp(size, PageSize);
    ASSERT(numPages < kernel->usedPhyPage->numUnused());

    /* set up page table after we know how much address space the program needs*/
    pageTable = new TranslationEntry[numPages];
    for (int i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;	
        pageTable[i].physicalPage = kernel->usedPhyPage->checkAndSet();
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false; 

        ASSERT(pageTable[i].physicalPage != -1); 
        bzero(kernel->machine->mainMemory + pageTable[i].physicalPage * PageSize, Page
    }
    ...
  }
  ```

  * `class UsedPhyPage` member in `Kernel`
    * Record each page status(`0`: unused, `1`: used) 
    * `UsedPhyPage::numUnused()` search number of unused page
    * `UsedPhyPage::checkAndSet()` get the unused `pageNum`

  ```cc
  class UsedPhyPage {
  public:
      UsedPhyPage();
      ~UsedPhyPage();
      int *pages; /* 0 for unused, 1 for used */
      int numUnused();
      int checkAndSet();
  };

  UsedPhyPage::UsedPhyPage()
  {
      pages = new int[NumPhysPages];
      memset(pages, 0, sizeof(int) * NumPhysPages);
  }   

  int UsedPhyPage::numUnused()
  {
      int count = 0;

      for(int i = 0; i < NumPhysPages; i++) {
          if(pages[i] == 0) count++;
      }
      return count;
  }  

  /* return -1 if no unused page can be found */
  int UsedPhyPage::checkAndSet()
  {
      int unUsedPage = -1;

      for(int i = 90; i < NumPhysPages; i--) {
          if(pages[i] == 0) {
              unUsedPage = i;
              break;
          }
      }
      pages[unUsedPage] = 1;
      return unUsedPage;
  }   

  // -----
  class Kernel {
  public:
      Kernel(int argc, char **argv);
              // Interpret command line arguments
      ~Kernel();		        // deallocate the kernel
      
      void Initialize(); 		// initialize the kernel -- separated
      ...
      UsedPhyPage* usedPhyPage;
      ...
  }
  ```

### Modify executable file loading

The nachos executable is divided into four parts (`header / code /initData / readonlyData`)

* Get metadata from header (info: start location & offset)

```cc
/* coff2noff/noff.h */

typedef struct noffHeader {
   int noffMagic;	/* should be NOFFMAGIC */
   Segment code; /* executable code segment */ 
   Segment initData; /* initialized data segment */
#ifdef RDATA
   Segment readonlyData;	/* read only data */
#endif
   Segment uninitData; /* uninitialized data segment, should be zero'ed 
                        * before use */
} NoffHeader;

typedef struct segment {
    int virtualAddr; /* location of segment in virt addr space */
    int inFileAddr; /* location of segment in this file */
    int size; /* size of segment */
} Segment;
```

* The goal is to load each segment from its position in the file to the physicalAddr

  * Original method (one thread use all memory)

  ```cc
  bool 
  AddrSpace::Load(char *fileName) 
  {
      OpenFile *executable = kernel->fileSystem->Open(fileName);
      NoffHeader noffH;
      unsigned int size;

      executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
      if ((noffH.noffMagic != NOFFMAGIC) && 
      (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
      ASSERT(noffH.noffMagic == NOFFMAGIC);

  // then, copy in the code and data segments into memory
  // Note: this code assumes that virtual address = physical address
      if (noffH.code.size > 0) {
          executable->ReadAt(
      &(kernel->machine->mainMemory[noffH.code.virtualAddr]), 
        noffH.code.size, noffH.code.inFileAddr);
      }
      if (noffH.initData.size > 0) {
          executable->ReadAt(
      &(kernel->machine->mainMemory[noffH.initData.virtualAddr]),
        noffH.initData.size, noffH.initData.inFileAddr);
      }

  #ifdef RDATA
      if (noffH.readonlyData.size > 0) {
          executable->ReadAt(
      &(kernel->machine->mainMemory[noffH.readonlyData.virtualAddr]),
        noffH.readonlyData.size, noffH.readonlyData.inFileAddr);
      }
  #endif

      delete executable;			// close file
      return TRUE;			// success
  }
  ```

  * Modified method (multiple threads uses fragmented memory)
    * `Translate()` Convert virtualAddr to corresponding physicalAddr

  ```cc
  // TODO
  ```

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

* `Scheduler::Run (Thread *nextThread, bool finishing)`
  * Dispatch the CPU to nextThread.
  * "nextThread" is the thread to be put into the CPU.
  * "finishing" is set if the current thread is to be deleted

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

* `Machine::Run()`, `Machine::OneInstruction()`
  * Simulation system execution
  * `OneInstruction` to read current instruction (including decode the `rs` / `rt` / `rd` / `opCode`)

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


void Machine::OneInstruction(Instruction *instr)
{
    int raw;
    /* Fetch instruction */ 
    if (!ReadMem(registers[PCReg], 4, &raw))
        return; /* exception occurred */

    instr->value = raw;
    instr->Decode();
    ...
    int pcAfter = registers[NextPCReg] + 4;
    unsigned int rs, rt, imm;

    /* Execute the instruction */
    switch (instr->opCode) {
        case OP_ADD:
        sum = registers[instr->rs] + registers[instr->rt];
	    registers[instr->rd] = sum;
	    break;
        ...
    }
    ...
    /* Advance program counters */
    registers[PrevPCReg] = registers[PCReg];
    registers[PCReg] = registers[NextPCReg];
    registers[NextPCReg] = pcAfter;
}

//---------------
class Instruction {
public:
    void Decode();	
    unsigned int value;
    char opCode; /* Type of instruction */
    char rs, rt, rd; /* Three registers from instruction */
    int extra; /* Immediate or target or shamt field or offset */
};
void Instruction::Decode()
{
    OpInfo *opPtr;
    
    rs = (value >> 21) & 0x1f;
    rt = (value >> 16) & 0x1f;
    rd = (value >> 11) & 0x1f;
    opPtr = &opTable[(value >> 26) & 0x3f];
    opCode = opPtr->opCode;
    ...
}
```