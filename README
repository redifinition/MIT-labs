# MIT labs 2020--lab1(Utilities) experiment documentation

| Student ID | Student name |
| ---------- | ------------ |
| 1853572    | Qiao Liang   |

## 1. Lab environment

### 1.1 Operating environment

- Lab environment: **Ubuntu 20.04** supported by Alibaba Cloud Server
- Operation management software: **Final shell**

### 2.2 Construction of the experimental environment

​	Firstly, enter ubuntu by using final shell, then install the tools we need: **QEMU 5.2, GDB 8.3, GCC and Binutils**. We use the following command to complete the installation:

```shell
sudo apt-get install git build-essential gdb-multiarch qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu 
```

​	Secondly, install the toolchain into `/usr/local` on our Ubuntu installation. Use the following commands to clone the repository for the RISC-V GNU Compiler Toolchain:

```shell
$ git clone --recursive https://github.com/riscv/riscv-gnu-toolchain
$ sudo apt-get install autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev
$ cd riscv-gnu-toolchain
$ ./configure --prefix=/usr/local
$ sudo make
$ cd ..
$ wget https://download.qemu.org/qemu-5.1.0.tar.xz
$ tar xf qemu-5.1.0.tar.xz
```

​	Then, build QEMU for riscv64-softmmu:

```shell
$ cd qemu-5.1.0
$ ./configure --disable-kvm --disable-werror --prefix=/usr/local --target-list="riscv64-softmmu"
$ make
$ sudo make install
$ cd ..
```

At this point, the environment required for the experiment has been set up.

Finally, clone the experiment file from github named **xv6-labs-2020**, create a branch named util, we can start lab1.

## 2 Problem1--Sleep

> Implement the UNIX program `sleep` for xv6; your `sleep` should pause for a user-specified number of ticks. A tick is a notion of time defined by the xv6 kernel, namely the time between two interrupts from the timer chip. Your solution should be in the file `user/sleep.c`.

#### 2.1 Goal restatement

​	This question requires us to edit a C file under the `user` file, which requires the use of the `sleep` system call, given the number of pauses, the system pauses for the corresponding time.

#### 2.2 Problem analysis

​	Firstly we should know how to use `argc` and `argv[]` to pass arguments.  `argc` means the total number of parameters in the command line, and `argv[]` means the command has `argc` parameters, the first parameter is the name of the procedure , and the following parameters are the parameters entered by the user on the command line. So in this procedure we need to **test the number of the parameters user has input**.

​	Then we need to understand how to call the system call `sleep` given by xv6. In `user.h`, we can get:

```c
int sleep(int);
```

​	Since the parameter passed in this function is of  type `int`, we need to convert the parameter of the string type input by the user through the function `atoi`。

#### 2.3 Solutions

```c
#include "user/user.h"
#include "kernel/types.h"
int main(int argc, char *argv[])
{
    if (argc != 2) //Indicates that the user did not pass parameters
    {
        fprintf(2, "Error:User doesn't input any argument\n");
        exit(1);
    }
    else
        sleep(atoi(argv[1])); //The first parameter of the command line, which is the specified number of ticks
    exit(0);
}
```

​	Firstly , examine the input of users. It the number of parameters is not equal to 2, we can infer that the user did not pass parameters, only input "sleep". Then output the error point. On the contrary, call the system call `sleep`, end the program.

## 3 Problem 2--pingpong

> Write a program that uses UNIX system calls to ''ping-pong'' a byte between two processes over a pair of pipes, one for each direction. The parent should send a byte to the child; the child should print "<pid>: received ping", where <pid> is its process ID, write the byte on the pipe to the parent, and exit; the parent should read the byte from the child, print "<pid>: received pong", and exit. Your solution should be in the file `user/pingpong.c`.

#### 3.1 Goal restatement

​	Use `pipes` to realize: the parent process sends messages to the child process, and the child process prints the information after receiving the message; at the same time, sends a message to the parent process through another pipe, and the parent process prints the information after receiving the message.

#### 3.2 Problem analysis

##### System Call `read` and `write`

| system call                         | dESCRIPTION                                                  |
| ----------------------------------- | ------------------------------------------------------------ |
| int read(int fd, char *buf,int n)   | Read up to n characters from the file descriptor `fd` to `buf`, and then return the number of characters read |
| int write(int fd, char *buf, int n) | Write up to n bytes from `buf` to `fd`, and return the number of bytes written |

​	We use these two system calls to write bytes to pipes and receive bytes from pipes. Notice that if the return number of system call `write` is not equal to number `n`, this write operation failed.

##### pipes and file descriptors

> ​	A *pipe* is a small kernel buffer exposed to processes as a pair of file descriptors, one for reading and one for writing. Writing data to one end of the pipe makes that data available for reading from the other end of the pipe. Pipes provide a way for processes to communicate.

> ​	A *file descriptor* is a small integer representing a kernel-managed object that a process may read from or write to. A process may obtain a file descriptor by opening a file, directory, or device, or by creating a pipe, or by duplicating an existing descriptor. For simplicity we’ll often refer to the object a file descriptor refers to as a “file”; the file descriptor interface abstracts away the differences between files, pipes, and devices, making them all look like streams of bytes. We’ll refer to input and output as *I/O*.

​	As far as I am  understand, file descriptors are like channels connecting various resources such as files and processes. Through file descriptors, processes can perform operations such as **reading and writing** on resources such as files. Therefore, the same file can have one or more file descriptors. Since file descriptors usually have read file descriptors and write file descriptors, we can use **an array of two integers** to create a pipe. Once the pipe is created, one end of the pipe is connected to the two file descriptors. Associated, the process can perform write and read operations on the pipeline through these two file descriptors.

​	 In general, the value of the file descriptor is the following three:

| File descriptor | description            |
| --------------- | ---------------------- |
| 0               | stdin standard input   |
| 1               | stdout standard output |
| 2               | stderr standard error  |



#####  The process of pingpong

​	First, we need to define the pipe that the parent process sends bytes to the child process. The input parameter is an array of size 2, defined as `parent_pipe[2]`. The pipe used for the child process defines the array in the same way and passes in the parameters. . Then use the fork function to create a child process of the parent process. When the process at this time is a child process, we send bytes to the pipe for the parent process to send bytes to the child process, and the child process prints the corresponding information, Use the write system call to send bytes to the pipe, and then exit the program; on the contrary, when the process at this time is the parent process, the parent process pipe is written, and then the child process pipe is read.

#### 3.3 Solutions

```c
#include "user/user.h"
#include "kernel/types.h"

int main(void)
{
    //First create a pipeline using a two-dimensional array of size 2
    int parent_pipefd[2];//Used by the parent process to send messages
    int child_pipefd[2];//Used for child processes to send messages
    pipe(parent_pipefd);//create pipes
    pipe(child_pipefd);//create pipes

    char msg='0';//The message returned by the child process
    char* buf=(char*)malloc(sizeof(char));//Write buffer

    int pid=fork();//Create child process
    
    if(!pid)//Child process
    {
        close(parent_pipefd[1]); //Close the write file descriptor of the parent process
        close(child_pipefd[0]);  //Close the read file descriptor of the child process
        read(parent_pipefd[0], &buf, 1);
        printf("%d: received ping\n", getpid());
        write(child_pipefd[1], &msg, 1);
    }
    else{
        close(parent_pipefd[0]); //Close the read file descriptor of the parent process
        close(child_pipefd[1]);  //Close the write file descriptor of the child process
        write(parent_pipefd[1], &msg, 1);
        read(child_pipefd[0], &buf, 1);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}
```

The idea of the code is as follows:

​	Since not only the parent process needs to send messages to the child process, but the child process also needs to reply to the parent process, we need to define two arrays of size two to **create two pipelines**.

​	Then, we define the message to be sent (assuming it is the character '0'), and the bufffer character used to store the message.

​	After that, we use the system call `fork()` to create a new process.  If the return value of the function is 0, it means that the **child process is being executed**; otherwise, the parent process is being executed. Different operations need to be performed in different processes.  As for the parent process, it is necessary to write to the pipe through which the parent process sends messages. In order to save resources, we **release the read file descriptor of the pipe**, and then perform the **write operation**. The content of the write is the message named `msg` that will occur in the parent process. In addition, we also need to read the message returned by the child process, so in the same way, we release the write file descriptor of the pipe used by the child process to reply to the message, and then read the pipe. **This read operation succeeds if and only if the child process writes to this pipe**. After that , the parent process can print the reply message `received pong`.As for the child process, the situation is just the opposite. If the child process successfully reads the message from the pipe `parent_pipefd` and saves it to `buf`, it can print the information that the message was successfully received.

## 4 Problem 3--primes

> Write a concurrent version of prime sieve using pipes. This idea is due to Doug McIlroy, inventor of Unix pipes. The picture halfway down [this page](http://swtch.com/~rsc/thread/) and the surrounding text explain how to do it. Your solution should be in the file `user/primes.c`.

#### 3.1 Goal restatement

​	The **prime number sieve** is realized through the pipeline. First, write all the numbers 2-35 into the pipeline. In the next **recursive process**: create a child process to filter and read these numbers, the method of filtering is to filter the numbers that cannot divide the first number of these numbers. Repeat these processes until **there are no more numbers left in the pipeline.**

#### 3.2 Problem analysis

##### The basic idea of prime sieve

![](https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/sieve.png)

​	The initial state is all numbers, and the first number must be a prime number 2. Then take out the first number, and remove all the numbers that can be divided evenly, and the first round of screening ends. In these numbers, the first number must also be a prime number, repeat the above process. Until there are no digits left.

​	The above picture clearly shows us the idea of using pipelines and processes to achieve prime sieve. Every read or write process is a process, and every box is a pipe. We can recursively **create a new pipe** to store the filtered batch of numbers, or we can use the `dup` and `close` functions to **reallocate** file descriptors without creating a new pipe. For the sake of simplicity, I used the first method here.

#### 3.3 Solutions

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void newPipe(int fd[2])
{
    int prime;
    int flag;
    int num;                                             //Store the number read out later
    close(fd[1]);                                        //Close the write file descriptor when reading
    if (read(fd[0], &prime, sizeof(int)) != sizeof(int)) //Read the numbers in the file into prime one by one. Obviously, the first number read must be prime
    {
        fprintf(2, "child process failed when reading the number.\n");
        exit(1);
    }
    printf("prime %d\n", prime); //Output filtered prime number

    flag = read(fd[0], &num, sizeof(int));
    if (flag)
    {
        int newpipefd[2]; //Store the file descriptor of the new pipe
        pipe(newpipefd);
        if (fork() == 0)
            newPipe(newpipefd);
        else
        {
            close(newpipefd[0]); //Close the read file descriptor when writing
            if (num % prime != 0)
                write(newpipefd[1], &num, sizeof(num));
            while (read(fd[0], &num, sizeof(int)))
                if (num % prime)
                    write(newpipefd[1], &num, sizeof(int));
            close(fd[0]);
            close(newpipefd[1]);
            wait(0); //Recursively wait for the end of the child process
        }
    }
    exit(0);
}
int main(int argc, char *argv)
{
    int pipefd[2];   //Used to store new read and write file descriptors
    pipe(pipefd);    //Create the first pipeline before filtering
    if (fork() == 0) //Child process
        newPipe(pipefd);
    else
    {
        close(pipefd[0]); //Release the read descriptor when writing
        //Write 2-35 numbers sequentially
        for (int i = 2; i <= 35; i++)
        {
            if (write(pipefd[1], &i, sizeof(int)) != sizeof(int)) //We believe that as long as the number of characters written is not equal to the maximum number of characters that can be written, it is a write failure
            {
                fprintf(2, "writing the number %d to the pipe failed!\n", i);
                exit(1);
            }
        }
        close(pipefd[1]); //Close the write channel
        wait(0);          //Wait for the end of the child process
        exit(0);
    }
    return 0;
}
```

​	Speaking from the main function. First create a pipe and child process for initializing numbers. In the parent process stage, we write a total of 34 numbers from 2-35 to the pipeline. Since the pipeline will no longer perform write operations, the write file descriptor is **released** in time to save resources.

​	Then in the child process stage, we use a  defined function named `newPipe`. In this function, first we output the  first number; Then use a **while** loop in the parent process: as long as the number can be read in the pipeline, each read number is filtered and judged in turn; if it is a child process, the pipeline is recursively created to read the number written by the parent process.

