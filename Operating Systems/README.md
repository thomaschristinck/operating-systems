# Operating Systems Assignments

This repository is a collection of Operating Systems Assignments I wrote for a class at McGill. Will run on Linux x86_64 machines 

### Assignment 1

In this assignment I was required to create a C program that implements a shell interface that accepts user commands and executes each command in a separate process. The shell program provides a command prompt, where the user inputs a line of command. The shell is responsible for executing the command.
Commands I've included are cd (change directory), pwd (present working directory), exit (leave shell), fg (send a background job to the foreground), and jobs (list background jobs). Output redirection via > and command piping via | are also implemented here.

1. Clone this repository.
```git clone https://github.com/thomaschristinck/Operating-Systems```

2. Go to "/Operating-Systems/Assignment 1" 

3. Compile as ```gcc -g jobs.c -o executable```

4. Run as ```./executable``` or, with valgrind, ```valgrind - -leak-check=full ./executable```

### Assignment 2

As part of this assignment, I was expected to implement an in-memory key-value store. This key- value store is setup in shared memory so that the data that is written to the store persists even after the process that writes it terminates. Because the shared memory is in RAM, the reads and writes are very fast. The amount of storage, however, is limited. Also, the store is not durable. If the machine reboots, the data is lost.

1. Clone this repository.
```git clone https://github.com/thomaschristinck/Operating-Systems```

2. Go to "/Operating-Systems/Assignment 2" 

3. Compile with -lrt and -lpthread as ```gcc -g a2_lib.c -o executable -lrt -lpthread```

4. Run as ```./executable``` or, with valgrind, ```valgrind - -leak-check=full ./executable```

### Assignment 3 

In this assignment, I was expected to design and implement a Simple File System. I was able to demonstrate a working file system working in Linux, and got 100% on this assignment. The SFS introduces many limitations such as restricted filename lengths, no user concept, no protection among files, no support for concurrent access, etc.

1. Clone this repository.
```git clone https://github.com/thomaschristinck/Operating-Systems```

2. Go to "/Operating-Systems/Assignment 3" 

3. Compile as ```gcc -g sfs_api.c -o executable```

4. Run as ```./executable``` or, with valgrind, ```valgrind - -leak-check=full ./executable```

### Author

[thomaschristinck](https://github.com/thomaschristinck/), 2018.
