# MIT labs 2020--lab5(Lazy Allocation) experiment documentation

| Student id | student name |
| ---------- | ------------ |
| 1853572    | Qiao Liang   |

## Purpose of this Experiment

​	The purpose of this experiment is to understand one of the ingenious mechanisms of page table hardware: **lazy allocation of user-space heap memory**. Before using the lazy allocation mechanism, the xv6 program used the `sbrk()` system utility to request heap memory from the kernel. The kernel allocates and maps memory for a large request, which may take a long time to modify. This experiment requires us to modify the kernel code to implement the mechanism of lazy memory allocation.

## Pre-knowledge of  this lab

​	Before completing this part of the experiment, we need to understand the page fault exception handling mechanism in xv6 in advance. The knowledge of this part of the mechanism is explained in detail **in section 4.6 of the xv6 book.** The following is a summary I made when I **read this part of the textbook and watched the replay of MIT's online course**.

##### 1.Virtual Memory Address

The benefits of using virtual memory:

- **Isolation**. Each address has its own address space, so that there will be no malicious modification of the address space of other programs.
- **level off indirection**. Program instructions only use virtual addresses, and the kernel defines the mapping from virtual addresses to physical addresses.

After using page faults in virtual memory, you can let the kernel dynamically change the mapping.

##### 2.System Call `sbrk()`

​	This system call is an **eager allocation**. That is, once `sbrk` is called, the kernel will immediately allocate the physical memory required by the application. Under normal circumstances, it is difficult for an application to predict how much memory it needs. Therefore,an application will request more memory than it needs. In order to cope with the large space allocation requirements of various applications, we have introduced a page fault mechanism and lazy allocation.

##### 3. Lazy allocation

​	After calling `sbrk()`, the only thing we need to do is to add the corresponding memory allocation to the `p->sz` field, but the kernel has not actually allocated the corresponding physical space. When the application needs to use the memory, it will cause a page fault, that is, when: $$va<p->sz$$ and $$p>stack$$, the kernel will use the `kalloc` function to allocate a page, Zero the page and map it to the table.

##### 4.Page-fault Exceptions

​	In the first experiment, we came into contact with the `fork` system call. This system call spawns a child process for the parent process, and the child process shares physical space with the parent process. But when the child process and the parent process write to the shared stack and heap, they will interrupt each other's operation. So we have a page fault mechanism. RISC-V has three different types of page faults:

- **Load page error** (when the load instruction cannot convert its virtual address)
- **Storage page fault** (when the storage instruction cannot translate its virtual address)
- **Command page error** (when the command address cannot be converted)

When a page fault occurs, the **`scause` register records the type of page fault; and the `stval` register contains an address that cannot be converted correctly**.

​	Therefore, COW fork realizes that the parent and child nodes initially share all physical pages, but only map these pages. In addition, COW fork is transparent: no modification of the heap application is required.

​	Another widely used feature, which is the subject of our experiment, is called lazy allocation. First, we need to understand the system call `sbrk`. When the program calls `sbrk`, the kernel increases the address space, but marks the new address as invalid in the page table. Since the program always requests more space than it actually uses, lazy allocation can be achieved: **The kernel allocates memory only when the program actually uses the memory**.

​	In addition, the xv6 book also introduced us to another page fault feature: **Paging from Disk**. If a program requires more physical space than is currently free, the kernel will write some pages to storage devices such as disks and mark `pte` as invalid. When the program reads or writes the removed page, A page fault will occur.

​	After understanding these pre-knowledges, we can complete the following experiments.

## Problem 1--Eliminate Allocation from sbrk()

> Your first task is to delete page allocation from the `sbrk(n)` system call implementation, which is the function `sys_sbrk()` in `sysproc.c`. The `sbrk(n)` system call grows the process's memory size by n bytes, and then returns the start of the newly allocated region (i.e., the old size). Your new `sbrk(n)` should just increment the process's size (`myproc()->sz`) by n and return the old size. It should not allocate memory -- so you should delete the call to `growproc()` (but you still need to increase the process's size!).

#### 1.1 Goal Restatement

​	The purpose of this experiment is to complete the preparations for lazy allocation, delete the part of memory allocation in `sys_sbrk()`, and only modify the size of `myproc->sz`.

#### 1.2 Problem Analysis

​	We only need to delete the code that allocates physical memory in `sysproc.c/sys_sbrk()`, and then add an increase in the address space.

```c
uint64
sys_sbrk(void)
{
  int addr;
  int n;
  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  myproc()->sz=myproc()->sz+n;//只需要考虑地址空间的增加
  //if(growproc(n) < 0)//增加应用程序的地址空间
    //return -1;
  return addr;
}
```

Then `make qemu`, after entering `echo hi`, the following exception will occur:

```shell
$ echo hi
usertrap(): unexpected scause 0x000000000000000f pid=3
            sepc=0x00000000000012a6 stval=0x0000000000004008
panic: uvmunmap: not mapped
```

​	The reason for the following page fault is that the shell needs `fork` `echo`, and then the child process executes `echo`, and the shell actually allocates some memory. In the output, we see that the value of the `scause` register is 15, the process id is 3, the value stored in the exception program counter is `12a6`, and the virtual address where the page fault occurs is `4008`.

 	And in the assembly code of the shell, we can find that the instruction corresponding to the address `12a6` is completing the writing work, but the kernel has not actually allocated this part of the memory space.

## Problem 2--Lazy Allocation

> Modify the code in `trap.c` to respond to a page fault from user space by mapping a newly-allocated page of physical memory at the faulting address, and then returning back to user space to let the process continue executing. You should add your code just before the `printf` call that produced the "`usertrap()`: ..." message. Modify whatever other xv6 kernel code you need to in order to get `echo hi` to work.

#### 2.1 Goal Restatement

​	This question requires us to implement a basic lazy allocation scheme in the `usertrap()` function in `trap.c`, allocate specific physical pages in this function, and then switch to user mode to continue running subsequent programs.

#### 2.2 Problem Analysis

​	In this experiment, we need to accept the page error that occurred in the previous experiment, intercept the error in the right place and deal with it correctly. Therefore, we check in the `usertrap()` function whether the return value of the `r_scause()` function is 15 or 13, and then use the `r_stval()` function to obtain the virtual address of the page fault.

```c
//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
//////////////////////////////////////////////////////
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if (r_scause() == 15||r_scause() == 13)
  {
    uint64 va=r_stval();//获取产生页面错误的虚拟地址
    uint64 ka=(uint64) kalloc();
    if(ka==0) p->killed=-1;//如果没有可以分配的内存，则终止该进程
    else{
      memset((void*)ka,0,PGSIZE);//将页面清零
      va=PGROUNDDOWN(va);//获取该物理页面的底部
      if(mappages(p->pagetable,va,PGSIZE,ka,PTE_U|PTE_W|PTE_R)!=0)//将页面映射到用户地址空间中适当的地址
      {
        kfree((void*)ka);
        p->killed=-1;
      }
    }
  }
  else{
    //////////////////////////////////////////////////////
  }
///////////////////////////////////////////////////////////
}
```

​	In the code, we can imitate the writing of `uvmalloc()`, first obtain the virtual address that caused the page fault, and then allocate it. If there is not enough memory space to allocate, the process is terminated directly. Otherwise, we initialize the newly allocated page to zero, then obtain the low address of the physical page, and map the page to the appropriate address in the user space to complete the modification.

​	At this time, if you run `echo hi`, an error of `uvmunmap:not mapped` will be reported, so we find the `uvmunmap()` function of `vm.c` to find the reason. You can see that the function's panic information shows that we are releasing pages that have not yet been mapped, but we only moved the address when allocating memory space for the program. If the application has never used this part of the address space, then the mapping is actually still It does not exist because it has not been allocated yet, and is only allocated when needed, so it is reasonable for this error to occur here.

​	So we only need to comment out this part of the `panic` error report, and then add `continue`.

```c
// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
/////////////////////////////////////////////////
    if((pte = walk(pagetable, a, 0)) == 0)
      // panic("uvmunmap: walk");
      continue;
    if((*pte & PTE_V) == 0)
      // panic("uvmunmap: not mapped");
      continue;
/////////////////////////////////////////////////
}
```

​	At this point, `echo hi` can run correctly.

## Problem 3-- Lazytests and UserTests

> We've supplied you with `lazytests`, an xv6 user program that tests some specific situations that may stress your lazy memory allocator. Modify your kernel code so that all of both `lazytests` and `usertests` pass.
>
> - Handle negative `sbrk()` arguments.
> - Kill a process if it page-faults on a virtual memory address higher than any allocated with `sbrk()`.
> - Handle the parent-to-child memory copy in `fork()` correctly.
> - Handle the case in which a process passes a valid address from `sbrk()` to a system call such as read or write, but the memory for that address has not yet been allocated.
> - Handle out-of-memory correctly: if `kalloc()` fails in the page fault handler, kill the current process.
> - Handle faults on the invalid page below the user stack.

#### 3.1 Goal Restatement

​	This question requires us to improve the function of lazy allocation in specific situations. In different situations, the lazy allocation mechanism can correctly handle page errors.

#### 3.2 Problem Analysis

- **Handle negative `sbrk()` arguments.**

  Add the case where the processing parameter is negative to `sbrk()`, which is the memory `n` corresponding to `dealloc`. Make the following modifications in the `sys_sbrk()` function.

  ```c
  uint64
  sys_sbrk(void)
  {
    int addr;
    int n;
  
    if(argint(0, &n) < 0)
      return -1;
    addr = myproc()->sz;
    if(n<0)
    {
      if(myproc()->sz+n<0)return -1;
      else uvmdealloc(myproc()->pagetable,myproc()->sz,myproc()->sz+n);
    }
    myproc()->sz=myproc()->sz+n;//只需要考虑地址空间的增加
    //if(growproc(n) < 0)//增加应用程序的地址空间
      //return -1;
    return addr;
  }
  ```

- **Kill a process if it page-faults on a virtual memory address higher than any allocated with `sbrk()`.**

  The meaning of this question is: when a page fault is found, the virtual address read is larger than `myproc()->sz`, or the virtual address is smaller than the user stack of the process, or the requested space is not enough Time to terminate the process. So what we have to do is to add a big prerequisite to the trap processing function, which is `usertrap()`, that is, the minimum virtual address is not less than the bottom address of the user stack of the process, and the highest does not exceed the maximum address of the process.

  ```c
  ////////////////////////////////////////////////
  else if (r_scause() == 15||r_scause() == 13)
    {
      uint64 va=r_stval();//获取产生页面错误的虚拟地址
      if(va<p->sz&&va>PGROUNDDOWN(p->trapframe->sp))
      {
        uint64 ka = (uint64)kalloc();
        if (ka == 0)
          p->killed = -1; //如果没有可以分配的内存，则终止该进程
        else
        {
          memset((void *)ka, 0, PGSIZE);                                          //将页面清零
          va = PGROUNDDOWN(va);                                                   //获取该物理页面的底部
          if (mappages(p->pagetable, va, PGSIZE, ka, PTE_U | PTE_W | PTE_R) != 0) //将页面映射到用户地址空间中适当的地址
          {
            kfree((void *)ka);
            p->killed = -1;
          }
        }
      }
      else{
        p->killed=-1;
      }
      /////////////////////////////////////////////
  ```

- **Handle the parent-to-child memory copy in `fork()` correctly.**

  This question requires us to modify the `fork()` function to realize the correct handling when the parent process copies the address space to the child process.

  ```c
  int
  uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
  {
  //////////////////////////////////////////////////
    for(i = 0; i < sz; i += PGSIZE){
      if((pte = walk(old, i, 0)) == 0)
        //panic("uvmcopy: pte should exist");
        continue;
      if((*pte & PTE_V) == 0)
        //panic("uvmcopy: page not present");
        continue;
  //////////////////////////////////////////////////
  }
  ```

  Finally, when a system call is made, if the address is not mapped to the physical space, the corresponding physical address needs to be added to the page table. We know that the code to convert virtual addresses to physical addresses is in `exec.c`, so we modify the `walkaddr()` function accordingly.

  ```c
  uint64walkaddr(pagetable_t pagetable, uint64 va){  pte_t *pte;  uint64 pa;  struct proc *p = myproc();  if (va >= MAXVA)    return 0;  pte = walk(pagetable, va, 0);  // sbrk increase sz but have not alloc memory yet.  if (pte == 0 || (*pte & PTE_V) == 0)  {    if (va >= p->sz || va < PGROUNDUP(p->trapframe->sp))      return 0;    char *mem = kalloc();    if (mem == 0)    {      printf("walkaddr: kalloc failed\n");      return 0;    }    if (mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)mem, PTE_W | PTE_X | PTE_R | PTE_U) != 0)    {      printf("walkaddr: mappages failed\n");      kfree(mem);      return 0;    }    return (uint64)mem;  }  if ((*pte & PTE_U) == 0)    return 0;  pa = PTE2PA(*pte);  return pa;}
  ```

## Experience of this lab

​	Through this experiment, I learned the specific mechanism of the operating system to delay page allocation in actual situations. In the operating system courses we learned in class, we only understood the page allocation mechanism in a short answer, and in order to have a more in-depth understanding of this aspect.

## Screenshot of lab process

**1.Result of  problem 1**:

<img src="C:\Users\86199\Desktop\lab5-1.png" style="zoom:50%;" />

**2. Result of problem 2**:

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab5-2.png" style="zoom: 67%;" />

**3. Result of problem 3：**

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab5-3.png" style="zoom:50%;" />

**4. Make Grade:**

![](https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab5-4.png)
