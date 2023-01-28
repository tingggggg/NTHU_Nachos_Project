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

