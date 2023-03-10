# MP1 System Call

## Implement four I/O system calls in NachOS

* Working items
  1. OpenFileId Open(char *name);
  Open a file with the name, and returns its corresponding OpenFileId.
  `Return -1 if fail to open the file`.

  2. int Write(char *buffer, int size, OpenFileId id);
  Write “size” characters from the buffer into the file, and return the
  number of characters actually written to the file.
  `Return -1, if fail to write the file`.

  3. int Read(char *buffer, int size, OpenFileId id);
  Read “size” characters from the file to the buffer, and return the
  number of characters actually read from the file.
  `Return -1, if fail to read the file`.

  4. int Close(OpenFileId id);
  Close the file with id.
  `Return 1 if successfully close the file. Otherwise, return -1`.

* `syscall.h`
  * define four system call macro
```cc
...
#define SC_Open		6
#define SC_Read		7
#define SC_Write	8
#define SC_Seek     9
#define SC_Close	10
...
```

* `start.S`
  * Implement the four `system call` of assembly

```cc
	.globl Open
	.ent    Open
Open:
	addiu $2, $0, SC_Open
	syscall
	j 	$31
	.end Open

	.globl Write
	.ent    Write
Write:
	addiu $2, $0, SC_Write
	syscall
	j 	$31
	.end Write

	.globl Read
	.ent    Read
Read:
	addiu $2, $0, SC_Read
	syscall
	j 	$31
	.end Read

	.globl Close
	.ent    Close
Close:
	addiu $2, $0, SC_Close
	syscall
	j 	$31
	.end Close
```

* `exception.cc`

```cc
void
ExceptionHandler(ExceptionType which)
{
    int type = kernel->machine->ReadRegister(2);
	int val;
    int status, exit, threadID, programID;
	int size;
	int id;
	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");
    switch (which) {
    case SyscallException:
      	switch(type) {
        ...

		case SC_Open:
		{
			val = kernel->machine->ReadRegister(4);

			char *filename = &(kernel->machine->mainMemory[val]);
			DEBUG(dbgSys, "Open filename: " << filename << "\n");

			status = SysOpen(filename);

			kernel->machine->WriteRegister(2, (int)status);

			DEBUG(dbgSys, "Open fileID: " << status << "\n");

			kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
			kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
			kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);

			return;
			ASSERTNOTREACHED();
			break;
		}
			
		case SC_Write: 
		{
			DEBUG(dbgSys, "Write\n");
			val = kernel->machine->ReadRegister(4); 
			{
			char *buffer = &(kernel->machine->mainMemory[val]);
			size = kernel->machine->ReadRegister(5); 
			id = kernel->machine->ReadRegister(6); 

			DEBUG(dbgSys, "Write val: " << val << ", size: " << size << ", fileID: " << id << "\n");

			status = SysWrite(buffer, size, id);
		
			kernel->machine->WriteRegister(2, (int) status);
			}
				
			kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg)); // set previous programm counter (debugging only)
			kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
			kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
			return;
			ASSERTNOTREACHED();
			break;
		}
			
		case SC_Read:
		{
			val = kernel->machine->ReadRegister(4);
			{
			char *buffer = &(kernel->machine->mainMemory[val]);
			size = kernel->machine->ReadRegister(5); 
			id = kernel->machine->ReadRegister(6); 
			
			// return value
			// 1: success
			// 0: failed
			status = SysRead(buffer, size, id);

			kernel->machine->WriteRegister(2, (int) status);
			}

			kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
			kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
			kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);

			return;	
			ASSERTNOTREACHED();
            break;
		}
			
		case SC_Close:
		{
			val = kernel->machine->ReadRegister(4); // mean fileId

			DEBUG(dbgSys, "Close fileID: " << val << "\n");

			status = SysClose(val);
			if(status != -1) status = 1; // successfully close file if status not equal -1

			kernel->machine->WriteRegister(2, (int)status);
			
			kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
			kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
			kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);

			return;	
			ASSERTNOTREACHED();
            break;
		}
        ...
    }
```

* `ksyscall.h`
  * Call the fileSystem API(function)

```cc
int SysOpen(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->OpenFile(filename);
}

int SysWrite(char *buffer, int size, int id)
{
	return kernel->WriteFile(buffer, size, id);
}

int SysRead(char *buffer, int size, int id)
{
	return kernel->ReadFile(buffer, size, id);
}

int SysClose(int id)
{
	return kernel->CloseFile(id);
}
```

```cc
int Kernel::OpenFile(char *filename)
{
	return fileSystem->OpenF(filename);
}

int Kernel::WriteFile(char *buffer, int size, int id)
{
	return fileSystem->WriteF(buffer, size, id);
}

int Kernel::ReadFile(char *buffer, int size, int id)
{
	return fileSystem->ReadF(buffer, size, id);
}

int Kernel::CloseFile(int id)
{
	return fileSystem->CloseF(id);
}
```

## Trace code

*** 

### SC_Halt

![SC_Halt](image/SC_Halt.png)

*  There is one stub per system call, that defined in `start.S`

* [start.S](code/test/start.S)
  * Store System call type into register2
  * Execute `system call`
  * Return to register31 that user program counter

```assembly
	.globl Halt
	.ent   Halt
Halt:
	addiu $2,$0,SC_Halt
	syscall
	j	$31
	.end Halt
```

* Machine::Run()
  * Execute `syscall` by this API
  * The actual important execution instruction is `OneInstruction(...)`
```cc
class Instruction {
  public:
    void Decode();	// decode the binary representation of the instruction

    unsigned int value; // binary representation of the instruction

    char opCode;     // Type of instruction.  This is NOT the same as the
    		     // opcode field from the instruction: see defs in mips.h
    char rs, rt, rd; // Three registers from instruction.
    int extra;       // Immediate or target or shamt field or offset.
                     // Immediates are sign-extended.
};

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

* Machine::OneInstruction()
  * Execute one instruction from a user-level program
  * Raise `SyscallException` exception by function `RaiseException(SyscallException, .)`
```cc
void
Machine::OneInstruction(Instruction *instr)
{
    ...
    switch (instr->opCode) {
        ...
        case OP_SYSCALL:
	RaiseException(SyscallException, 0);
	return; 
        ...
    }
    ...
}
```

* RaiseException()
  * `ExceptionType` will be further passed to `ExceptionHandler(...)` for processing
  * `kernel->interrupt->setStatus(SystemMode)` Change to `System mode` before execute `ExceptionHandler()`

```cc
void
Machine::RaiseException(ExceptionType which, int badVAddr)
{
    DEBUG(dbgMach, "Exception: " << exceptionNames[which]);
    registers[BadVAddrReg] = badVAddr;
    DelayedLoad(0, 0);			// finish anything in progress
    kernel->interrupt->setStatus(SystemMode);
    ExceptionHandler(which);		// interrupts are enabled at this point
    kernel->interrupt->setStatus(UserMode);
}
```

* ExceptionHandler()
  * `kernel->machine->ReadRegister(2)` read the register2 value
  * Goto the switch case corresponding to the `type`
  * Specifically execute `SysHalt()`

```cc
// machine.h
enum ExceptionType { NoException,           // Everything ok!
		     SyscallException,      // A program executed a system call.
		     PageFaultException,    // No valid translation found
		     ReadOnlyException,     // Write attempted to page marked 
					    // "read-only"
		     BusErrorException,     // Translation resulted in an 
					    // invalid physical address
		     AddressErrorException, // Unaligned reference or one that
					    // was beyond the end of the
					    // address space
		     OverflowException,     // Integer overflow in add or sub.
		     IllegalInstrException, // Unimplemented or reserved instr.
		     
		     NumExceptionTypes
};

ExceptionHandler(ExceptionType which)
{
    int type = kernel->machine->ReadRegister(2);
	int val;
    int status, exit, threadID, programID;
	int size;
	int id;
	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");
    switch (which) {
    case SyscallException:
      	switch(type) {
      	case SC_Halt:
			DEBUG(dbgSys, "Shutdown, initiated by user program.\n");
			SysHalt();
			ASSERTNOTREACHED();
			break;
        ...
        }
    ...
}
```

* SysHalt()
  * Kernel be deleted in the function final(So OS shutdown because `kernel` link all components)
```cc
void SysHalt()
{
  kernel->interrupt->Halt();
}


// interrupt.cc
//----------------------------------------------------------------------
// Interrupt::Halt
// 	Shut down Nachos cleanly, printing out performance statistics.
//----------------------------------------------------------------------
void
Interrupt::Halt()
{
    cout << "Machine halting!\n\n";
    cout << "This is halt\n";
    kernel->stats->Print();
    delete kernel;	// Never returns.
}
```

***

### SC_Create

![SC_Create](image/SC_Create.png)

* ExceptionHandler()
  * Part of steps are the same as above
  * `kernel->machine->ReadRegister(4)` read register4 system call args1 (filename)
  * `kernel->machine->WriteRegister(2, (int) status)` The result of the system call, if any, must be put back into r2
  * `WriteRegister` after `SysCreate`
    * Update `PrevPCReg`, `PCReg`, `NextPCReg`
    * set previous programm counter (debugging only)
    * set programm counter to next instruction (all Instructions are 4 byte wide)
    * set next programm counter for brach execution
```cc
ExceptionHandler(ExceptionType which)
{
    int type = kernel->machine->ReadRegister(2);
	int val;
    int status, exit, threadID, programID;
	int size;
	int id;
	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");
    switch (which) {
    case SyscallException:
      	switch(type) {
      	...
		case SC_Create:
			val = kernel->machine->ReadRegister(4);
			{
			char *filename = &(kernel->machine->mainMemory[val]);

			// return value
			// 1: success
			// 0: failed
			status = SysCreate(filename);

			if(status != -1) status = 1;
			kernel->machine->WriteRegister(2, (int) status);
			}
			kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));
			kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);
			kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg)+4);
			return;
			ASSERTNOTREACHED();
            break;
        ...
        }
}


// machine.cc
void 
Machine::WriteRegister(int num, int value)
{
    ASSERT((num >= 0) && (num < NumTotalRegs));
    registers[num] = value;
}
```

* SC_Create()

```cc
int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename);
}


// interrupt.cc
int
Interrupt::CreateFile(char *filename)
{
    return kernel->CreateFile(filename);
}
```

* FileSystem::Create()
    * Specifically call the `open` in `stdlib` (so call stub file system)
```cc
// makefile
DEFINES =  -DFILESYS_STUB -DRDATA -DSIM_FIX


#ifdef FILESYS_STUB 		// Temporarily implement file system calls as 
				// calls to UNIX, until the real file system
				// implementation is available
class FileSystem {
  public:
    FileSystem() { for (int i = 0; i < 20; i++) fileDescriptorTable[i] = NULL; }

    bool Create(char *name) 
	{
        int fileDescriptor = OpenForWrite(name);

        if (fileDescriptor == -1) return FALSE;
        Close(fileDescriptor); 
        return TRUE; 
	}
}

int
OpenForWrite(char *name)
{
    int fd = open(name, O_RDWR|O_CREAT|O_TRUNC, 0666);
    ASSERT(fd >= 0); 
    return fd;
}
```

***

### SC_PrintInt

![SC_PrintInt](image/SC_PrintInt.png)

* ExceptionHandler() same as `SC_Create` above

* SysPrintInt()
  * Specifically call the `SyncConsoleOutput::PutInt()`
  * Use `sprintf()` to store int into string `str`
  * `lock->Acquire()`, `lock->Release()` achieve synchronization (make sure a string can be printed completely without interruption, other thread can not get the writer resources)
  * `waitFor->P()` waiting if `semaphore` resource not available.
```cc
void SysPrintInt(int number)
{
	kernel->synchConsoleOut->PutInt(number);
}

void
SynchConsoleOutput::PutInt(int value)
{
    char str[15];
    int idx=0;
    //sprintf(str, "%d\n\0", value);  the true one
    sprintf(str, "%d\n\0", value); //simply for trace code
    lock->Acquire();
    do{	
        consoleOutput->PutChar(str[idx]);
	    idx++;
        waitFor->P();
	
    } while(str[idx] != '\0');
    lock->Release();
}

class SynchConsoleOutput : public CallBackObj {
  ...
  private:
    Lock *lock;			// only one writer at a time
    ...
};
```

* Semaphore::P()
  * It will diable interrupts before acquiring resources

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

* ConsoleOutput::PutChar()
  * `WriteFile` write data into file
  * Set `putBusy` `True` to avoid others instructions

```cc
//----------------------------------------------------------------------
// ConsoleOutput::PutChar()
// 	Write a character to the simulated display, schedule an interrupt 
//	to occur in the future, and return.
//----------------------------------------------------------------------
void
ConsoleOutput::PutChar(char ch)
{
    ASSERT(putBusy == FALSE);
    WriteFile(writeFileNo, &ch, sizeof(char));
    putBusy = TRUE;
    kernel->interrupt->Schedule(this, ConsoleTime, ConsoleWriteInt);
}

int
WriteFile(int fd, char *buffer, int nBytes)
{
    int retVal = write(fd, buffer, nBytes);
    ASSERT(retVal == nBytes);
    return retVal;
}
```

* Interrupt::Schedule()
  * Arrange for the CPU to be interrupted when simulated time reaches "now + when"
  * `toCall` is the object to call when the interrupt occurs
  * `fromNow` is how far in the future (in simulated time) the interrupt is to occur
  * `type` is the hardware device that generated the interrupt
```cc
void
Interrupt::Schedule(CallBackObj *toCall, int fromNow, IntType type)
{
    int when = kernel->stats->totalTicks + fromNow;
    PendingInterrupt *toOccur = new PendingInterrupt(toCall, when, type);

    DEBUG(dbgInt, "Scheduling interrupt handler the " << intTypeNames[type] << " at time = " << when);
    ASSERT(fromNow > 0);

    pending->Insert(toOccur);
}
```

* Machine::Run()
  * Running until simulated time reaches target time("now + when" above), then do one instruction that be inserted before
  * `OneInstruction` will fetch instruction by `(!ReadMem(registers[PCReg], 4, &raw))`
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

achine::OneInstruction(Instruction *instr)
{
#ifdef SIM_FIX
    int byte;       // described in Kane for LWL,LWR,...
#endif

    int raw;
    int nextLoadReg = 0; 	
    int nextLoadValue = 0; 	// record delayed load operation, to apply in the future

    /* fetch instruction */ 
    if (!ReadMem(registers[PCReg], 4, &raw)) return;	// exception occurred 
    instr->value = raw;
    instr->Decode();

    ...

    switch (instr->opCode) {
      ...
    }
}
```

* Interrupt::OneTick()
  * Advance simulated time and check if there are any pending interrupts to be called

* Interrupt::CheckIfDue()
  * Check if any interrupts are scheduled to occur, and if so, fire them off

* ConsoleOutput::CallBack()
  * Set `putBusy` to `FALSE` so that can do print out next item
  
  * `kernel->stats->numConsoleCharsWritten++` count `numConsoleCharsWritten`

* SynchConsoleInput::CallBack()
  * `waitFor->V()` release a `semaphore` resource