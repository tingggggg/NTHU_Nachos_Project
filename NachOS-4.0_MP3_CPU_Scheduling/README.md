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
