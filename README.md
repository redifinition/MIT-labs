# MIT labs 2020--lab6(Copy-on-write Fork) experiment documentation

| Student id | student name |
| ---------- | ------------ |
| 1853572    | Qiao Liang   |

## Purpose of this Experiment

The purpose of this experiment is to implement another ingenious mechanism for page faults: copy-on write fork.

## **Pre-knowledge of  this lab**

> The goal of copy-on-write (COW) fork() is to defer allocating and copying physical memory pages for the child until the copies are actually needed, if ever.
>
> COW fork() creates just a pagetable for the child, with PTEs for user memory pointing to the parent's physical pages. COW fork() marks all the user PTEs in both parent and child as not writable. When either process tries to write one of these COW pages, the CPU will force a page fault. The kernel page-fault handler detects this case, allocates a page of physical memory for the faulting process, copies the original page into the new page, and modifies the relevant PTE in the faulting process to refer to the new page, this time with the PTE marked writeable. When the page fault handler returns, the user process will be able to write its copy of the page.
>
> COW fork() makes freeing of the physical pages that implement user memory a little trickier. A given physical page may be referred to by multiple processes' page tables, and should be freed only when the last reference disappears.

​	Regarding the introduction of copying branches while writing, there has been a clearer explanation in the introduction part of the experimental document. And the following is the pre-knowledge about this part that I summarized when I watched the S.6801 course video.

##### Copy on write fork

​	The first is the raising of the problem. When `shell` processes instructions, it creates a process through the `fork` function, and this process creates a copy of the `shell` process. Then the first thing executed by the child process of `shell` is to call `exec` to run some other programs such as `echo`. But before `fork` had a complete copy of the address space, and the `exec` program would discard the copied address space, which caused a waste of space.

​	The optimization given here is: instead of copying all the physical space when creating the child process, but directly set the page table entry (PTE) of the child process to point to the physical page corresponding to the parent process**. In order to ensure strong isolation between the parent process and the child process, we need to set the page table entry flag bits corresponding to the parent process and the child process to **read only**. Later, when the content of this part of the memory needs to be changed to cause a page fault, the corresponding physical page is copied.

## Problem --Implement copy-on-write

> Your task is to implement copy-on-write fork in the xv6 kernel. You are done if your modified kernel executes both the `cowtest` and `usertests` programs successfully.

#### Problem Analysis

1. Select the reserved bit definition in PTE in ***kernel/riscv.h*** to mark whether a page is a flag bit of COW Fork page

   ```c
   // 记录应用了COW策略后fork的页面
   #define PTE_F (1L << 8)
   ```

2. Make the following changes in `kalloc.c`:

   1. The global variable `ref` that defines the reference count contains a spin lock and a reference count array. Since `ref` is a global variable, it will be initialized to 0. The reason for using a spin lock is: if there are two memories Shared memory M, the reference count of M is 2. At this time, if CPU1 wants to execute `fork` to generate the child process of P1, CPU2 must terminate P2. If two CPUs read the value 2 at the same time, then the reference saved by CPU1 after the execution is complete The count is 3, and the value saved by CPU2 is 1, which will cause an error.

      ```c
      struct ref_stru {
        struct spinlock lock;
        int cnt[PHYSTOP / PGSIZE];  // 引用计数
      } ref;
      ```

   2. Then we initialize the spinlock of `ref` in knit:

      ```c
      void
      kinit()
      {
      //////////////////////////////////////////
        initlock(&ref.lock, "ref");
      ////////////////////////////////////////
      }
      ```

   3. Then we can modify the `kalloc` and `kfree` functions. Initialize the memory reference count in `kalloc` to 1, and subtract 1 from the reference count in the `kfree` function. When the reference count is 0, delete it.

      ```c
      void
      kfree(void *pa)
      {
        struct run *r;
        if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
          panic("kfree");
        //当且仅当引用计数为0时，回收空间
        //否则仅仅是将引用计数减1
        acquire(&ref.lock);
        if(--ref.cnt[(uint64)pa/PGSIZE]==0)
        {
          release(&ref.lock);
          r=(struct run*)pa;
          // Fill with junk to catch dangling refs.
          memset(pa, 1, PGSIZE);
          acquire(&kmem.lock);
          r->next = kmem.freelist;
          kmem.freelist = r;
          release(&kmem.lock);
        }
        else{
          release(&ref.lock);
        }
      }
      ```

      ```c
      void *
      kalloc(void)
      {
        struct run *r;
        acquire(&kmem.lock);
        r = kmem.freelist;
        if(r){
          kmem.freelist = r->next;
          acquire(&ref.lock);
          ref.cnt[(uint64)r / PGSIZE] = 1; // 将引用计数初始化为1
          release(&ref.lock);
        }
        release(&kmem.lock);
      
        if(r)
          memset((char*)r, 5, PGSIZE); // fill with junk
        return (void*)r;
      }
      
      ```

   4. Then we add the following four functions, the functions of these four functions are: determine whether a page is a COW page; copy-on write allocator; get the reference count of the current memory; increase the reference count of the current memory. In `cowalloc`, read the memory reference count. If it is 1, it means that only the current process refers to the physical memory (other processes have been allocated to other physical pages before), and you only need to change the PTE to enable `PTE_W`; otherwise It allocates physical pages and decrements the original memory reference count by one. This function needs to return the physical address, which will be used in `copyout`. The following is a detailed introduction to the implementation ideas of these four functions,

      **First is the function to determine whether the page is a COW page:**

      ```c
      int cowpage(pagetable_t pagetable, uint64 va) {
        if(va >= MAXVA)
          return -1;
        pte_t* pte = walk(pagetable, va, 0);
        if(pte == 0)
          return -1;
        if((*pte & PTE_V) == 0)
          return -1;
        return (*pte & PTE_F ? 0 : -1);
      }
      ```

      The input parameters of this function are to specify the page table to be queried and the virtual address va. Then judge the virtual address and the physical page obtained through the `walk` function.

      **Then the copy-on-write allocator:**

      ```c
      void* cowalloc(pagetable_t pagetable, uint64 va) {
        if(va % PGSIZE != 0)
          return 0;
        uint64 pa = walkaddr(pagetable, va);  // 获取对应的物理地址
        if(pa == 0)
          return 0;
        pte_t* pte = walk(pagetable, va, 0);  // 获取对应的PTE
        if(krefcnt((char*)pa) == 1) {
          // 只剩一个进程对此物理地址存在引用
          // 则直接修改对应的PTE即可
          *pte |= PTE_W;
          *pte &= ~PTE_F;
          return (void*)pa;
        } else {
          // 多个进程对物理内存存在引用
          // 需要分配新的页面，并拷贝旧页面的内容
          char* mem = kalloc();
          if(mem == 0)
            return 0;
          // 复制旧页面内容到新页
          memmove(mem, (char*)pa, PGSIZE);
          // 清除PTE_V，否则在mappagges中会判定为remap
          *pte &= ~PTE_V;
          // 为新页面添加映射
          if(mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_F) != 0) {
            kfree(mem);
            *pte |= PTE_V;
            return 0;
          }
          // 将原来的物理内存引用计数减1
          kfree((char*)PGROUNDDOWN(pa));
          return mem;
        }
      }
      ```

      First obtain the corresponding physical address and page table entry. Then deal with the referenced count of the process accordingly. If there is only one process that has a reference to this physical address, you can directly modify the corresponding PTE; if there are multiple processes that reference the physical process, you need to allocate a new page, copy the content of the employment surface, and then copy the page To the new page and add a mapping for the new page.

      **Then get the reference count of the memory**, this function is relatively simple, so I won't repeat it.

      ```c
      int krefcnt(void* pa) {
        return ref.cnt[(uint64)pa / PGSIZE];
      }
      ```

      **Finally is to increase the reference count of the memory:**

      ```c
      int kaddrefcnt(void* pa) {
        if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
          return -1;
        acquire(&ref.lock);
        ++ref.cnt[(uint64)pa / PGSIZE];
        release(&ref.lock);
        return 0;
      }
      ```

3. Then we need to modify `freerange`:

   ```c
   void
   freerange(void *pa_start, void *pa_end)
   {
     char *p;
     p = (char*)PGROUNDUP((uint64)pa_start);
     for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
       // 在kfree中将会对cnt[]减1，这里要先设为1，否则就会减成负数
       ref.cnt[(uint64)p / PGSIZE] = 1;
       kfree(p);
     }
   }
   ```

4. Finally modify `uvmcopy`. Do not allocate memory for the child process, but make the parent and child process share memory, but disable `PTE_W`, mark `PTE_F` at the same time, and call `kaddrefcnt` to increase the reference count.

   ```c
   pte_t *pte;
     uint64 pa, i;
     uint flags;
   
     for(i = 0; i < sz; i += PGSIZE){
       if((pte = walk(old, i, 0)) == 0)
         panic("uvmcopy: pte should exist");
       if((*pte & PTE_V) == 0)
         panic("uvmcopy: page not present");
       pa = PTE2PA(*pte);
       flags = PTE_FLAGS(*pte);
   
       // 仅对可写页面设置COW标记
       if(flags & PTE_W) {
         // 禁用写并设置COW Fork标记
         flags = (flags | PTE_F) & ~PTE_W;
         *pte = PA2PTE(pa) | flags;
       }
   
       if(mappages(new, i, PGSIZE, pa, flags) != 0) {
         uvmunmap(new, 0, i / PGSIZE, 1);
         return -1;
       }
       // 增加内存的引用计数
       kaddrefcnt((char*)pa);
     }
     return 0;
   ```

5. Then modify `usertrap` to handle page errors

   ```c
   uint64 cause = r_scause();
   if(cause == 8) {
     ...
   } else if((which_dev = devintr()) != 0){
     // ok
   } else if(cause == 13 || cause == 15) {
     uint64 fault_va = r_stval();  // 获取出错的虚拟地址
     if(fault_va >= p->sz
       || cowpage(p->pagetable, fault_va) != 0
       || cowalloc(p->pagetable, PGROUNDDOWN(fault_va)) == 0)
       p->killed = 1;
   } else {
     ...
   }
   ```

6. Finally, we deal with the same situation in `copyout`, if it is a COW page, we need to change the physical address pointed to by `pa0`

   ```c
   while(len > 0){
     va0 = PGROUNDDOWN(dstva);
     pa0 = walkaddr(pagetable, va0);
   
     // 处理COW页面的情况
     if(cowpage(pagetable, va0) == 0) {
       // 更换目标物理地址
       pa0 = (uint64)cowalloc(pagetable, va0);
     }
   
     if(pa0 == 0)
       return -1;
     ...
   }
   ```

   #### Result of the lab

   <img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab6-1.png" style="zoom:50%;" />



## Experience of this lab:

​	This experiment taught me about the copy-on-write technology of the operating system kernel. In the traditional way in the past, the child process copied all the resources of the parent process, which had the disadvantages of high memory consumption, time-consuming copy operation, and low efficiency. This experiment made me understand the specific operation of copy-on-write by changing the code manually, that is, only copy the page table when fork, so that the address space of the parent and child processes point to the same physical memory page, and mark these pages as read-only . When one of the two processes wants to modify the corresponding physical page, a page fault will be triggered. At this time, the kernel will copy the content of the page as a new physical page, set it to a writable state, allocate the new physical page to the writing process, and re-execute the write operation.

## Screenshot of this lab:

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab6-2.png" style="zoom: 50%;" />

