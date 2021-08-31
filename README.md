# MIT labs 2020--lab10(mmap) experiment documentation

| Student id | student name |
| ---------- | ------------ |
| 1853572    | Qiao Liang   |

## Purpose of this Experiment

​	The purpose of this experiment is to allow us to implement a memory-mapped file function, map the file to the memory, thereby reducing disk operations when interacting with the file.

## Problem --mmap

> You should implement enough `mmap` and `munmap` functionality to make the `mmaptest` test program work. If `mmaptest` doesn't use a `mmap` feature, you don't need to implement that feature.

#### 1.1 Problem Analysis

1).First, we follow the prompts to configure the system calls of `mmap` and `munmap` in the way of `lab2-syscall`. Macros are defined in ***kernel/fcntl.h***. These macros only take effect when `LAB_MMAP` is defined, and `LAB_MMAP` is defined by the `-D` parameter of gcc on the command line at compile time of.

```c
void* mmap(void* addr, int length, int prot, int flags, int fd, int offset);
int munmap(void* addr, int length);
```

2).Then we follow Tip 3: Track the content mapped by `mmap` for each process. Define the structure corresponding to the VMA (virtual memory area) described in Lesson 15, and record the address, length, permissions, files, etc. of the virtual memory range created by `mmap`. Since there is no memory allocator in the xv6 kernel, a fixed-size VMA array can be declared and allocated from the array as needed. A size of 16 should be sufficient. Define the VMA structure and add it to the process structure. The added code and comments are as follows:

```c
#define NVMA 16
// 虚拟内存区域结构体
struct vm_area {
  int used;           // 是否已被使用
  uint64 addr;        // 起始地址
  int len;            // 长度
  int prot;           // 权限
  int flags;          // 标志位
  int vfd;            // 对应的文件描述符
  struct file* vfile; // 对应文件
  int offset;         // 文件偏移，本实验中一直为0
};

struct proc {
  ...
  struct vm_area vma[NVMA];    // 虚拟内存区域
}
```

3).Then initialize the `vma` array in `allocproc` to all 0s

```c
static struct proc*
allocproc(void)
{
  ...

found:
  ...

  memset(&p->vma, 0, sizeof(p->vma));
  return p;
}
```

4).Then we refer to the method of lazy allocation, that is, `p->sz` is used as the actual address of the allocated virtual memory, but the physical page is not actually allocated. This function can be written in ***sysfile.c*** and you can use the static function` argfd` parses the file descriptor and `struct file` at the same time.

```c
uint64
sys_mmap(void) {
  uint64 addr;
  int length;
  int prot;
  int flags;
  int vfd;
  struct file* vfile;
  int offset;
  uint64 err = 0xffffffffffffffff;

  // 获取系统调用参数
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 ||
    argint(3, &flags) < 0 || argfd(4, &vfd, &vfile) < 0 || argint(5, &offset) < 0)
    return err;

  // 实验提示中假定addr和offset为0，简化程序可能发生的情况
  if(addr != 0 || offset != 0 || length < 0)
    return err;

  // 文件不可写则不允许拥有PROT_WRITE权限时映射为MAP_SHARED
  if(vfile->writable == 0 && (prot & PROT_WRITE) != 0 && flags == MAP_SHARED)
    return err;

  struct proc* p = myproc();
  // 没有足够的虚拟地址空间
  if(p->sz + length > MAXVA)
    return err;

  // 遍历查找未使用的VMA结构体
  for(int i = 0; i < NVMA; ++i) {
    if(p->vma[i].used == 0) {
      p->vma[i].used = 1;
      p->vma[i].addr = p->sz;
      p->vma[i].len = length;
      p->vma[i].flags = flags;
      p->vma[i].prot = prot;
      p->vma[i].vfile = vfile;
      p->vma[i].vfd = vfd;
      p->vma[i].offset = offset;

      // 增加文件的引用计数
      filedup(vfile);

      p->sz += length;
      return p->vma[i].addr;
    }
  }

  return err;
}
```

5).According to prompt 5, accessing the corresponding page at this time will cause a page error, which needs to be processed in `usertrap`, which mainly completes three tasks: assigning physical pages, reading file content, and adding mapping relationships

```c
void
usertrap(void)
{
  ...
  if(cause == 8) {
    ...
  } else if((which_dev = devintr()) != 0){
    // ok
  } else if(cause == 13 || cause == 15) {
#ifdef LAB_MMAP
    // 读取产生页面故障的虚拟地址，并判断是否位于有效区间
    uint64 fault_va = r_stval();
    if(PGROUNDUP(p->trapframe->sp) - 1 < fault_va && fault_va < p->sz) {
      if(mmap_handler(r_stval(), cause) != 0) p->killed = 1;
    } else
      p->killed = 1;
#endif
  } else {
    ...
  }

  ...
}

/**
 * @brief mmap_handler 处理mmap惰性分配导致的页面错误
 * @param va 页面故障虚拟地址
 * @param cause 页面故障原因
 * @return 0成功，-1失败
 */
int mmap_handler(int va, int cause) {
  int i;
  struct proc* p = myproc();
  // 根据地址查找属于哪一个VMA
  for(i = 0; i < NVMA; ++i) {
    if(p->vma[i].used && p->vma[i].addr <= va && va <= p->vma[i].addr + p->vma[i].len - 1) {
      break;
    }
  }
  if(i == NVMA)
    return -1;

  int pte_flags = PTE_U;
  if(p->vma[i].prot & PROT_READ) pte_flags |= PTE_R;
  if(p->vma[i].prot & PROT_WRITE) pte_flags |= PTE_W;
  if(p->vma[i].prot & PROT_EXEC) pte_flags |= PTE_X;


  struct file* vf = p->vma[i].vfile;
  // 读导致的页面错误
  if(cause == 13 && vf->readable == 0) return -1;
  // 写导致的页面错误
  if(cause == 15 && vf->writable == 0) return -1;

  void* pa = kalloc();
  if(pa == 0)
    return -1;
  memset(pa, 0, PGSIZE);

  // 读取文件内容
  ilock(vf->ip);
  // 计算当前页面读取文件的偏移量，实验中p->vma[i].offset总是0
  // 要按顺序读读取，例如内存页面A,B和文件块a,b
  // 则A读取a，B读取b，而不能A读取b，B读取a
  int offset = p->vma[i].offset + PGROUNDDOWN(va - p->vma[i].addr);
  int readbytes = readi(vf->ip, 0, (uint64)pa, offset, PGSIZE);
  // 什么都没有读到
  if(readbytes == 0) {
    iunlock(vf->ip);
    kfree(pa);
    return -1;
  }
  iunlock(vf->ip);

  // 添加页面映射
  if(mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)pa, pte_flags) != 0) {
    kfree(pa);
    return -1;
  }

  return 0;
}
```

6). Implement `munmap` according to Prompt 6, and Prompt 7 shows that it can be written back without checking the dirty bit

```c
uint64
sys_munmap(void) {
  uint64 addr;
  int length;
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;

  int i;
  struct proc* p = myproc();
  for(i = 0; i < NVMA; ++i) {
    if(p->vma[i].used && p->vma[i].len >= length) {
      // 根据提示，munmap的地址范围只能是
      // 1. 起始位置
      if(p->vma[i].addr == addr) {
        p->vma[i].addr += length;
        p->vma[i].len -= length;
        break;
      }
      // 2. 结束位置
      if(addr + length == p->vma[i].addr + p->vma[i].len) {
        p->vma[i].len -= length;
        break;
      }
    }
  }
  if(i == NVMA)
    return -1;

  // 将MAP_SHARED页面写回文件系统
  if(p->vma[i].flags == MAP_SHARED && (p->vma[i].prot & PROT_WRITE) != 0) {
    filewrite(p->vma[i].vfile, addr, length);
  }

  // 判断此页面是否存在映射
  uvmunmap(p->pagetable, addr, length / PGSIZE, 1);


  // 当前VMA中全部映射都被取消
  if(p->vma[i].len == 0) {
    fileclose(p->vma[i].vfile);
    p->vma[i].used = 0;
  }

  return 0;
}
```

7). Recall that in the lazy experiment, if you call `uvmunmap` on lazily allocated pages, or if the child process calls `uvmcopy` in fork to copy pages lazily allocated by the parent process, it will cause panic, so you need to modify `uvmunmap` and `uvmcopy` checks No longer `panic` after `PTE_V`

```c
if((*pte & PTE_V) == 0)
  continue;
```

8). Modify `exit` according to prompt 8 to unmap the mapped area of the process

```c
void
exit(int status)
{
  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    ...
  }

  // 将进程的已映射区域取消映射
  for(int i = 0; i < NVMA; ++i) {
    if(p->vma[i].used) {
      if(p->vma[i].flags == MAP_SHARED && (p->vma[i].prot & PROT_WRITE) != 0) {
        filewrite(p->vma[i].vfile, p->vma[i].addr, p->vma[i].len);
      }
      fileclose(p->vma[i].vfile);
      uvmunmap(p->pagetable, p->vma[i].addr, p->vma[i].len / PGSIZE, 1);
      p->vma[i].used = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  ...
}
```

9). According to Tip 9, modify `fork`, copy the VMA of the parent process and increase the file reference count

```c
int
fork(void)
{
 // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    ...
  ...

  // 复制父进程的VMA
  for(i = 0; i < NVMA; ++i) {
    if(p->vma[i].used) {
      memmove(&np->vma[i], &p->vma[i], sizeof(p->vma[i]));
      filedup(p->vma[i].vfile);
    }
  }

  safestrcpy(np->name, p->name, sizeof(p->name));
  
  ...
}
```

#### Result of this lab

<img src="C:\Users\86199\Desktop\QQ图片20210831185533.png" style="zoom:67%;" />

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/image-20210831153609951.png" alt="image-20210831153609951" style="zoom:67%;" />

## Screenshot of this lab

<img src="C:\Users\86199\Desktop\QQ图片20210831192351.png" style="zoom:67%;" />
