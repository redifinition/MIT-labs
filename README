# MIT labs 2020--lab4(traps) experiment documentation

| Student id | student name |
| ---------- | ------------ |
| 1853572    | Qiao Liang   |

## Purpose of this Experiment

​	The main purpose of this experiment is to let us understand the **assembly code** of RISC-V and the **stack frame** structure of function calls. On the basis of understanding these two mechanisms, we have a simple understanding of the **trap** mechanism, and then implement a user-level Trap handler.

## Pre-knowledge of  this lab

##### 1.Assembly Language

​	Before proceeding with the experiment, we first need to understand the assembly language in RISC-V. In order to have a better understanding of the experiment, I watched the online video of MIT course S.6081 in advance.

​	In RISC-V, C language is translated into assembly language after being compiled. The computer's processor RISC-V can understand the RISC-V instruction set well. After the C language is compiled into assembly language, the assembly language will be translated into a binary file, which is the familiar `obj` file or `.o` file.

​	Assembly language is similar to our pipeline instruction in the Computer Architecture course. In GDB, we can view the currently executing assembly code by opening the execution window of the assembler under the running mode of GDB:

```shell
(gdb)tui enable
```

##### 2. Stack mechanism

​	The stack starts from the high address and consists of **stack frames** one by one. The function stack frame includes registers and local variables. Different stack frame sizes are not necessarily the same. Each stack frame includes four parts: return address, pointer to the previous stack frame, saved registers, and local variables.

##### 3.Traps

​	 In the xv6 book, we can know that when three subordinate situations occur, the control flow of the CPU can be transferred，Which is the trap discussed in this experiment: 

1. The user mode enters the kernel mode through the `ecall` instruction;
2. An exception occurs, such as division by zero and access to an illegal address; 
3. Interruption, such as the hard disk completes a read and write request. A very important feature of traps is transparency, that is, the trap program being processed has no effect on the current program state. We need to understand the trap from the following aspects.

​     The rest of the knowledge is described in detail in Chapter 5 of the xv6 book, so I won’t elaborate on it here.

## Problem 1-- RISC-V Assembly

#### 1.1 Goal Restatement

> It will be important to understand a bit of RISC-V assembly, which you were exposed to in 6.004. There is a file `user/call.c` in your xv6 repo. `make fs.img`compiles it and also produces a readable assembly version of the program in `user/call.asm`.

​	This experiment requires us to understand some assembly instructions of RISC-V, and learn to use the console window to view the execution of the assembly code.

#### 1.2 Problem Analysis

> (1) Which registers contain arguments to functions? For example, which register holds 13 in main's call to `printf`?

​	According to the content described in the MIT course, we can know that the parameters of the RISC-V function call process preferentially use registers to pass, that is, a0~a7 total 8 registers. Therefore, the return value can be placed in the a0 and a1 registers. The parameter 13 of `printf` is stored in the a2 register.

> Where is the call to function `f` in the assembly code for main? Where is the call tog?

​	As can be seen from the code, both of these are handled by **inline optimization**. The `f ` call in main uses the result 12 directly, and the function `g` call in `f` is directly inlined in f.

> At what address is the function `printf` located?

At position 0x630.

> What value is in the register `ra` just after the `jalr` to `printf` in `main`?

​	To answer this question, we should first understand that the jump and link instruction (jal) has a dual function. If you save the address of the next instruction PC + 4 to the target register, usually the return address register ra, you can use it to implement procedure calls. If you use the zero register (x0) to replace ra as the target register, you can achieve an unconditional jump because x0 cannot be changed. Like a branch, jal multiplies its 20-bit branch address by 2, sign-extends it and adds it to the PC to get the jump address.

The register version (jalr) of the jump and link instructions is also multi-purpose. It can call a function whose address is calculated dynamically, or it can return from a call (only need ra as the source register and the zero register (x0) as the destination register). The address jump of Switch and case statements can also use the jalr instruction, and the destination register is set to x0.

​	Therefore, the value should be 0x38, which is the return address of the function.

> Run the following code.
>
> ```c
> 	unsigned int i = 0x00646c72;
> 	printf("H%x Wo%s", 57616, &i);   
> ```
>
> What is the output? [Here's an ASCII table](http://web.cs.mun.ca/~michael/c/ascii-table.html) that maps bytes to characters.

​	The result is: He110 World; do not modify it to 0x726c6400; 57616 does not need to be changed, the compiler will convert it.

> In the following code, what is going to be printed after `'y='`? (note: the answer is not a specific value.) Why does this happen?
>
> ```c
> 	printf("x=%d y=%d", 3);
> ```

​	Produce undefined behavior, y will be equal to a random number.

## Problem 2--Backtrace

> Implement a `backtrace()` function in `kernel/printf.c`. Insert a call to this function in `sys_sleep`, and then run bttest, which calls `sys_sleep`. Your output should be as follows:
>
> ```c
> backtrace:
> 0x0000000080002cda
> 0x0000000080002bb6
> 0x0000000080002898
> ```
>
> After `bttest` exit qemu. In your terminal: the addresses may be slightly different but if you run `addr2line -e kernel/kernel` (or `riscv64-unknown-elf-addr2line -e kernel/kernel`) and cut-and-paste the above addresses as follows:
>
> ```c
>  $ addr2line -e kernel/kernel
>  0x0000000080002de2
>  0x0000000080002f4a
>  0x0000000080002bfc
>  Ctrl-D
> ```
>
> You should see something like this:
>
> ```c
>  kernel/sysproc.c:74
>  kernel/syscall.c:224
>  kernel/trap.c:85
> ```

#### 2.1 Goal Restatement

​	The purpose of this question is for us to complete a `backtrace` function in `printf.c`, which is used when an error occurs, and we can use this function to see which system calls are in the current function stack.

#### 2.2 Problem Analysis

​	This question mentioned the structure of the stack frame. Since this term did not appear in the operating system courses we learned in our class, I watched the MIT lec5 video during the experiment, and one of the slides clearly showed The structure of the function stack frame:

<img src="https://pic1.zhimg.com/v2-a618c2b55ea2d78d2b8522ccc548b3f8_r.jpg" alt="preview" style="zoom: 25%;" />

​	The addresses of the stack are arranged in order **from high to low.** Each stack frame stores the return address of the function, the pointer to the previous stack frame, the register for storing the value, and some local variables. Among them, `fp `address is the address of the return value of the stack frame at the top of the stack;`sp` is the lowest address. We can calculate the pointer address of each stack frame separately according to this slide and a series of functions provided by the experiment to complete the function of backtracking.

Below we can complete the writing of the code.

First, we declare the `backtrace` function in `defs.h` so that this function can be called in other files.

```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0": "=r"(x));
  return x;
}
```

​	We denote the address of the current stack frame pointer as fp. Then the address of the next stack frame pointer is: $$fp-8$$; the address of the return value is $$fp-16$$.

​	Then according to the prompt, we know that xv6 allocates a page size for each stack to store these stack frames, and provides two functions,` PGROUNDDOWN(fp)` and `PGROUNDUP(fp)`, to get the top and bottom of the stack. This we can implement the `backtrace` function:

```c
//Print the corresponding stack frame
void backtrace(void)
{
  uint64 fp = r_fp();
  // Find the end of the stack
  uint64 bottom = PGROUNDUP(fp);
  uint64 return_addr;
  printf("backtrace:\n");
  while (fp < bottom)
  {
    return_addr = *(uint64 *)(fp - 8);
    fp = *(uint64 *)(fp - 16);
    printf("%p\n", return_addr);
  }
}
```

​	The last step is to insert the `backtrace` call in the right place. Here we can insert it anywhere in the `sys_sleep` and panic functions and see the result of the operation.

#### 2.3 Result

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab4-1.png" style="zoom:50%;" />

## Problem 3 -- Alarm

> In this exercise you'll add a feature to xv6 that periodically alerts a process as it uses CPU time. This might be useful for compute-bound processes that want to limit how much CPU time they chew up, or for processes that want to compute but also want to take some periodic action. More generally, you'll be implementing a primitive form of user-level interrupt/fault handlers; you could use something similar to handle page faults in the application, for example. Your solution is correct if it passes `alarmtest` and `usertests`.

#### 3.1 Goal Restatement

​	This question requires us to implement a function that can be called after a certain ticks on the CPU.

#### 3.2 Problem Analysis

​	The two functions we need to implement are system calls, so we need to use the knowledge of **lab2** to complete the corresponding system call declarations in `user.h`, `syscall.h`, and `syscall.c` respectively.

First of all, for the first function, its declaration is:

```c
int sigalarm(int ticks, void (*handler)());
```

​	This function is used to set the time interval of the acquisition. Therefore, for each process, we need two attributes: the time interval and the total number of ticks performed by the process. We add these two attributes to `proc.h`.

```c
// Per-process state
struct proc {
  /////////////////////////////////////////
  int alarm_interval;          // alarm interval
  uint64 alarm_handler;        // alarm handler pointer
  int ticks;                   // ticks passed since the last call to alarm handler
  struct user_context alarm_context;
  int in_alarm_handler;
    //////////////////////////////////////
};
```

For the new attributes, we all need to initialize in `allocproc`.

```c
static struct proc*
allocproc(void)
{
/////////////////////////////////////////
found:
  p->pid = allocpid();
  p->ticks = 0;
  p->alarm_interval = 0;
  p->alarm_handler = 0;
  p->in_alarm_handler = 0;
  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }
    //////////////////////////////////////
}
```

Then we can start to implement the `sys_alarm` function.

```c
uint64
sys_sigalarm(void)
{
  int interval;
  uint64 handler;

  if (argint(0, &interval) < 0)
    return -1;
  if (argaddr(1, &handler) < 0)
    return -1;

  struct proc *proc = myproc();
  proc->alarm_interval = interval;
  proc->alarm_handler = handler;
  return 0;
}
```

​	The last is the acquisition of CPU time. At each tick, the hardware will generate an interrupt and then enter the `usertrap` of `kernel/trap.c` for processing. Therefore, we add a count to the `usertrap()` function to change the total number of ticks in the process.

```c
void
usertrap(void)
{
   ///////////////////////////////////////
  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
  {
    p->ticks++;
  }
  yield();
  usertrapret();
}
```

​	Then there is how to call our custom function when it reaches a certain ticks. When the user enters the kernel, the current program counter PC will be stored in the `spec`, and then load in when returning from the kernel. At this time, if we put the `spec` (program Here is `trapframe->epc`) replaced by the address of the custom function we got, and then it will enter the address of the custom function when it returns, and then in order to return the correct address after the function is called, you need to reset `trapframe`- in `sys_sigreturn` value of `epc` is assigned to the original value, so it is necessary to save the value of historical `trapframe->epc`. At the same time, not only `epc` will be lost due to clock interruption, but also other register values need to be saved, so the value in `proc.h` struct proc needs to add historical register attributes. Then, to prevent the custom function can be executed before the custom function is executed, the proc structure also needs to add a flag to prevent the above situation.

​	Therefore, we add a new structure called user_context in `proc.h` and use this structure as an attribute of `proc.h`.

```c
struct user_context
{
  uint64 epc;
  uint64 ra;
  uint64 sp;
  uint64 gp;
  uint64 tp;
  uint64 t0;
  uint64 t1;
  uint64 t2;
  uint64 s0;
  uint64 s1;
  uint64 a0;
  uint64 a1;
  uint64 a2;
  uint64 a3;
  uint64 a4;
  uint64 a5;
  uint64 a6;
  uint64 a7;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
  uint64 t3;
  uint64 t4;
  uint64 t5;
  uint64 t6;
};
```

Finally implement the `sys_sigreturn` function:

```c
uint64
sys_sigreturn(void)
{
  struct proc *p = myproc();

  p->trapframe->epc = p->alarm_context.epc;
  p->trapframe->ra = p->alarm_context.ra;
  p->trapframe->sp = p->alarm_context.sp;
  p->trapframe->gp = p->alarm_context.gp;
  p->trapframe->tp = p->alarm_context.tp;

  p->trapframe->s0 = p->alarm_context.s0;
  p->trapframe->s1 = p->alarm_context.s1;
  p->trapframe->s2 = p->alarm_context.s2;
  p->trapframe->s3 = p->alarm_context.s3;
  p->trapframe->s4 = p->alarm_context.s4;
  p->trapframe->s5 = p->alarm_context.s5;
  p->trapframe->s6 = p->alarm_context.s6;
  p->trapframe->s7 = p->alarm_context.s7;
  p->trapframe->s8 = p->alarm_context.s8;
  p->trapframe->s9 = p->alarm_context.s9;
  p->trapframe->s10 = p->alarm_context.s10;
  p->trapframe->s11 = p->alarm_context.s11;

  p->trapframe->t0 = p->alarm_context.t0;
  p->trapframe->t1 = p->alarm_context.t1;
  p->trapframe->t2 = p->alarm_context.t2;
  p->trapframe->t3 = p->alarm_context.t3;
  p->trapframe->t4 = p->alarm_context.t4;
  p->trapframe->t5 = p->alarm_context.t5;
  p->trapframe->t6 = p->alarm_context.t6;

  p->trapframe->a0 = p->alarm_context.a0;
  p->trapframe->a1 = p->alarm_context.a1;
  p->trapframe->a2 = p->alarm_context.a2;
  p->trapframe->a3 = p->alarm_context.a3;
  p->trapframe->a4 = p->alarm_context.a4;
  p->trapframe->a5 = p->alarm_context.a5;
  p->trapframe->a6 = p->alarm_context.a6;
  p->trapframe->a7 = p->alarm_context.a6;

  p->in_alarm_handler = 0;
  return 0;
}
```

Then make the corresponding modification in the `usertrap `function to complete.

```c
void
usertrap(void)
{
///////////////////////////////////////////
  if(which_dev == 2)
  {
    p->ticks++;
    if (p->alarm_interval > 0 && p->ticks >= p->alarm_interval && !p->in_alarm_handler)
    {
      // save enough state in struct proc context when the timer goes off
      // that sigreturn can correctly return to the interrupted user code.
      p->alarm_context.epc = p->trapframe->epc;
      p->alarm_context.ra = p->trapframe->ra;
      p->alarm_context.sp = p->trapframe->sp;
      p->alarm_context.gp = p->trapframe->gp;
      p->alarm_context.tp = p->trapframe->tp;

      p->alarm_context.s0 = p->trapframe->s0;
      p->alarm_context.s1 = p->trapframe->s1;
      p->alarm_context.s2 = p->trapframe->s2;
      p->alarm_context.s3 = p->trapframe->s3;
      p->alarm_context.s4 = p->trapframe->s4;
      p->alarm_context.s5 = p->trapframe->s5;
      p->alarm_context.s6 = p->trapframe->s6;
      p->alarm_context.s7 = p->trapframe->s7;
      p->alarm_context.s8 = p->trapframe->s8;
      p->alarm_context.s9 = p->trapframe->s9;
      p->alarm_context.s10 = p->trapframe->s10;
      p->alarm_context.s11 = p->trapframe->s11;

      p->alarm_context.t0 = p->trapframe->t0;
      p->alarm_context.t1 = p->trapframe->t1;
      p->alarm_context.t2 = p->trapframe->t2;
      p->alarm_context.t3 = p->trapframe->t3;
      p->alarm_context.t4 = p->trapframe->t4;
      p->alarm_context.t5 = p->trapframe->t5;
      p->alarm_context.t6 = p->trapframe->t6;

      p->alarm_context.a0 = p->trapframe->a0;
      p->alarm_context.a1 = p->trapframe->a1;
      p->alarm_context.a2 = p->trapframe->a2;
      p->alarm_context.a3 = p->trapframe->a3;
      p->alarm_context.a4 = p->trapframe->a4;
      p->alarm_context.a5 = p->trapframe->a5;
      p->alarm_context.a6 = p->trapframe->a6;
      p->alarm_context.a7 = p->trapframe->a7;
      // return to alarm handler
      p->trapframe->epc = p->alarm_handler;
      p->ticks = 0;
      p->in_alarm_handler = 1;
    }
  }
  yield();
  usertrapret();
}
```

#### 3.3 Result

<img src="C:\Users\86199\Desktop\lab4-2.png" style="zoom:50%;" />

## Screenshot of the experiment process

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab4-3.png" style="zoom:50%;" />

## Experimental experience of Lab2

​	The purpose of this experiment is to let us understand the meaning and function of **traps**. During the completion of this experiment, I also learned the important role of **assembly** language as a higher-level language of machine language in the operating system, and also learned the method of obtaining the tick count of the process clock, and further practiced the implementation of system calls. . At the same time, in the debugging process of this experiment, I also learned to use the **assembly command window** to view the storage status of each register, which improved the efficiency of correcting errors.
