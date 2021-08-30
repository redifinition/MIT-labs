# MIT labs 2020--lab8(Locks) experiment documentation

| Student id | student name |
| ---------- | ------------ |
| 1853572    | Qiao Liang   |

## Purpose of this Experiment

​	The purpose of this experiment is to let us understand the use of locks in the operating system, why we use locks, how to use locks, the pros and cons of locks, and have a certain understanding of the block cache of the file system.

## **Pre-knowledge of  this lab**

​	The experiment document reminds us that before the experiment, we need to read the part of chapter 6 of xv6 books, as well as the content of section 3.5 and 8.1-8.3. The following is a brief summary of knowledge I made when reading these materials, in preparation for the completion of the following experiments.

​	Before learning locks, we must understand that the kernel is full of **concurrently-accessed data**. Strategies aimed at the correctness of concurrency and the abstractions that support them are called **concurrency control techniques**. This is the reason why we adopt the lock technology, because the lock achieves parallelism through mutual exclusion technology, ensuring that only one CPU can hold the lock at a time.

#### Race conditions

​	A **race condition** refers to a situation where multiple processes read and write some shared data (at least one access is a write). Competition usually contains bugs, either missing updates (if the access is written), or reading data structures that have not completed the update. The result of competition depends on the exact timing of processes running on the processor and how the memory system sorts their memory operations, which may make it difficult to reproduce and debug errors caused by competition.

​	The basic code we implement the lock in the code is as follows:

```c
acquire(&listlock);
/////////////////
release(&listlock);
```

The sequence of instructions between `acquire` and `release` is usually called the critical section. The role of the lock is usually called protecting the `list`.

​	We can think of the lock as a critical area of **serialization concurrency**, so that only one process is running this part of the code at the same time. We can also treat critical regions protected by the same lock as atoms between each other, that is, each other can only see the complete set of changes in the previous critical region, and never see the partially completed update. Although the lock guarantees the correct concurrent execution of the process, the lock **limits the performance**.

#### Locks

Xv6 has two types of locks: **spinlocks** and **sleep-locks**. Logically speaking, xv6 uses the following code to acquire a lock:

```c
void
acquire(struct spinlock* lk) // does not work!
{
  for(;;) {
    if(lk->locked == 0) {
      lk->locked = 1;
      break;
    }
  }
}
```

​	Although the above program basically realizes mutual exclusion of locks, it cannot guarantee mutual exclusion on multiple processors. Because when two CPUs reach the fifth row at the same time, it will cause different CPUs to hold the lock at the same time. So we need to implement a way to make the fifth and sixth lines executed as **atomic steps**. RISC-V uses `amoswap r,a` to achieve this atomic operation.

| **lock**                   | **description**                                              |
| -------------------------- | ------------------------------------------------------------ |
| `bcache.lock`              | Protect the allocation of **block buffer cache entries**     |
| `cons.lock`                | Serialize access to console hardware to avoid mixed output   |
| `ftable.lock`              | The allocation of the file structure in the serialized file table |
| `icache.lock`              | Protect the allocation of cache items at index nodes         |
| `vdisk_lock`               | Serialize access to disk hardware and DMA descriptor queues  |
| `kmem.lock`                | Serialized memory allocation                                 |
| `log.lock`                 | Serialized transaction log operations                        |
| `pi->lock` of the pipeline | Serialize the operations of each pipeline                    |
| `pid_lock`                 | Serialize the increment of next_pid                          |
| Process `p->lock`          | Changes in the state of the serialization process            |
| `tickslock`                | Serialized clock counting operation                          |
| Index node `ip->lock`      | Operation of serialized index nodes and their contents       |
| `b->lock` of the buffer    | Serialize the operation of each block buffer                 |

The above table lists all the locks in xv6.

#### Lock and interrupt handling functions

​	Some xv6 spin locks protect data shared by threads and interrupt handlers. For example, the `clockintr` timer interrupt handler adds `ticks`(***kernel/trap.c***:163) while the kernel thread may be in `sys_sleep`(***kernel/sysproc.c** *:64) read `ticks`. The lock `tickslock` serializes these two accesses. In this case, deadlock may occur, because `sys_sleep` holds the lock and waits until `clockintr` returns, and `clockinr` waits for the lock to be released, which causes a deadlock.

​	In order to avoid deadlock, if a spin lock is used by the interrupt handler, the CPU must ensure that the lock can never be held when the interrupt is enabled. When the CPU acquires any lock, xv6 always disables the interrupt on that CPU. The interrupt may still occur on other CPUs. At this time, the interrupted `acquire` can wait for the thread to release the spin lock; because it is not on the same CPU, it will not cause a deadlock.

#### Ordering of instructions and memory access

​	Many compilers and central processors will execute codes out of order in order to achieve higher performance, but they need to follow certain rules before reordering to ensure that they will not change the results of correctly written serial codes. The sorting rules of this kind of CPU are called **memory model**. In order to tell the compiler and hardware that a specific reordering cannot be performed, xv6 uses `acquire`(kernel/spinlock.c22) and `release`(kernel/spinlock. c:47) uses `__sync_synchronize()`. `__sync_synchronize()` is a memory barrier: it tells the compiler and CPU not to reorder `load` or `store` instructions across the barrier. Because xv6 uses locks when accessing shared data, the obstacles in `acquire` and `release` of xv6 will be enforced in order in almost all important situations.

#### Sleep-locks

​	When xv6 needs to keep the lock for a long time and avoid wasting the CPU to keep the lock, we need a new lock that gives up the CPU while waiting for the lock to be acquired, and allows concessions while holding the lock. This kind of lock is called sleep- locks.

#### Extra content in the course video

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/image.png" style="zoom:50%;" />

​	The first is the above picture mentioned in the video, which explains why multiple CPU cores are used to improve performance. First, the clock frequency of the CPU indicated by the green line has never been increased, which has caused the single-thread performance of the CPU to reach a limit and has never been increased; the dark red line indicates that the number of transistors in the CPU is continuously increasing. The result of these lines is black lines, and the number of processor cores is constantly increasing.

​	In addition, the video also uses a very strict rule to explain when to lock: **If two processes access a shared data structure, and one of the processes will update the shared data structure, then you need Lock this shared data structure.** 

​	At the same time, the course also summarizes the three functions of locks:

- The lock can avoid losing updates.
- Locks can make operations atomic.
- Locks can maintain the immutability of shared data structures.

With the above pre-knowledge, it will be easier to complete the following experiments.

## Problem 1--Memory allocator

> Your job is to implement per-CPU freelists, and stealing when a CPU's free list is empty. You must give all of your locks names that start with "kmem". That is, you should call `initlock` for each of your locks, and pass a name that starts with "kmem". Run kalloctest to see if your implementation has reduced lock contention. To check that it can still allocate all of memory, run `usertests sbrkmuch`. Your output will look similar to that shown below, with much-reduced contention in total on kmem locks, although the specific numbers will differ. Make sure all tests in `usertests` pass. `make grade` should say that the kalloctests pass.

#### 1.1 Goal Restatement

​	The task completed in this experiment is to maintain a free list for each CPU. Initially, all the free memory is allocated to a certain CPU. After that, when each CPU needs memory, if the current CPU is not on the free list, steal other CPUs. . For example, all free memory is initially allocated to CPU0, and CPU0 will be stolen when CPU1 needs memory, and it will hang on the free list of CPU1 after it is used. After that, CPU1 can take it from its free list when it needs memory again.

#### 1.2 Problem Analysis

1）.First, we define `kmem` as an array, the number of elements is `NCPU`, that is, the number of CPUs:

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];
```

​	The reason for the modification to an array is that in a multi-threaded environment, there are a lot of competitions when acquiring `kmem` and `bcache` locks, which affects CPU performance. The reason for the high number of competitions is that xv6 has only one memory free linked list` The freelist` is used by multiple CPUs, and because a lock is added when applying for memory, other CPUs must wait for the current thread to release the lock when applying for memory allocation. Therefore, we let each CPU have its own memory linked list for allocation, so that there is no need to obtain the memory linked list of the current thread. And when the memory linked list of a CPU is insufficient, we can also get the part from the memory linked list of other CPUs for allocation.

2）. After modifying `kmem`, an error will be reported where the data structure is used, and we will make corresponding modifications according to the location of the error. The first is the `kinit` function. We initialize a lock for each CPU, and then the `freerange` function will call `kfree` to hang all free memory on the free list of the CPU.

```c
void
kinit()
{
  char lockname[8];
  for(int i = 0;i < NCPU; i++) {
    snprintf(lockname, sizeof(lockname), "kmem_%d", i);
    initlock(&kmem[i].lock, lockname);
  }
  freerange(end, (void*)PHYSTOP);
}
```

3). Then modify the `kfree` function. When using `cpuid()` and the result it returns, you must use `push_off()` to turn off the interrupt, then obtain the corresponding CPU number, and acquire and release the corresponding lock:

```c
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off(); // 关中断
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();//开启中断
}
```

4).Finally, modify the `kalloc` function to steal the memory of other CPUs when the free list of the current CPU has no available memory.

```c
void *
kalloc(void)
{
  struct run *r;

  push_off();// 关中断
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  else {
    int antid;  // another id
    // 遍历所有CPU的空闲列表
    for(antid = 0; antid < NCPU; ++antid) {
      if(antid == id)
        continue;
      acquire(&kmem[antid].lock);
      r = kmem[antid].freelist;
      if(r) {
        kmem[antid].freelist = r->next;
        release(&kmem[antid].lock);
        break;
      }
      release(&kmem[antid].lock);
    }
  }
  release(&kmem[id].lock);
  pop_off();  //开中断

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

#### 1.3 Result of this problem

`kalloctest`

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab8-1.png" style="zoom: 67%;" />

`usertest`

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab8-2.png" style="zoom:67%;" />

## Problem 2-- Buffer cache

> Modify the block cache so that the number of `acquire` loop iterations for all locks in the `bcache` is close to zero when running `bcachetest`. Ideally the sum of the counts for all locks involved in the block cache should be zero, but it's OK if the sum is less than 500. Modify `bget` and `brelse` so that concurrent lookups and releases for different blocks that are in the `bcache` are unlikely to conflict on locks (e.g., don't all have to wait for `bcache.lock`). You must maintain the invariant that at most one copy of each block is cached. When you are done, your output should be similar to that shown below (though not identical). Make sure `usertests` still passes. `make grade` should pass all tests when you are done.

#### 2.1 Goal Restatement

​	The purpose of this experiment is to parallelize the allocation and recovery of buffers to improve performance. For this experiment, we need to read 8.1-8.3 in advance to understand the content of block caching.

​	In a multithreaded environment, when the structure of the buffer cache is not modified, because the structure has only one global lock, it will cause serious competition when acquiring the buffer and significantly reduce performance.

#### 2.2 Pre-knowledge of this problem

This question is very difficult. Since I am not familiar with the content of the block cache part of the file system, I first read the content of the 8.1-8.3 textbook, which is summarized as follows:

The first is the layering of the XV6 file system, as shown in the following table:

| File descriptor |
| --------------- |
| Pathname        |
| Directory       |
| Inode           |
| Logging         |
| Buffer cache    |
| Disk            |

​	The disk layer reads and writes blocks on the hard disk; the buffer cache layer caches disk blocks and synchronizes access to them, ensuring that only one kernel process can modify the data stored in any particular block at a time; the logging layer allows more The high-level packs the update into multiple blocks in one transaction, and ensures that these blocks are automatically updated in the event of a crash; the index node layer provides separate files, and each file is represented as an index node, which contains a unique ** Index number** and some blocks for saving file data; the directory layer implements each directory as a special index node, and its content is a series of directory entries, each directory entry contains a file name and index number; path name layer Provide hierarchical path names, such as `/usr/rtm/xv6/fs.c`, and resolve them through recursive search. The file descriptor layer uses the file system interface to abstract many Unix resources (for example, pipes, devices, files, etc.), simplifying the work of application programmers.

<img src="https://github.com/duguosheng/6.S081-All-in-one/raw/main/tranlate_books/book-riscv-rev1/images/c8/p1.png" alt="img" style="zoom: 67%;" />

​	The above figure shows the division of the disk. The file system does not use block 0 (it holds the boot sector). Block 1 is called a super block: it contains metadata about the file system (file system size (in blocks), number of data blocks, number of inodes, and number of blocks in the log). Blocks starting from 2 save the log. After the log is the index node, each block has multiple index nodes. Then there is the bitmap block, which keeps track of the data block being used. The remaining blocks are data blocks: each is either marked as free in the bitmap block or holds the contents of a file or directory.

**Level of Buffer cache**

​	Then there is the key part of this experiment, the buffer cache layer. The main interfaces exported by the Buffer cache layer are mainly `bread` and `bwrite`; the former obtains a *buf*, which contains a copy of a block that can be read or modified in memory, and the latter writes the modified buffer The corresponding block on the disk. The kernel thread must release the buffer by calling `brelse`. Buffer cache uses a sleep lock for each buffer to ensure that each buffer (and therefore each disk block) is only used by one thread at a time; `bread` returns a locked buffer, and `brelse` releases the lock .

#### 2.3 Problem Analysis

1). First, we need to define the hash bucket structure in `bio.c`, and delete the global buffer list in `bcahe`, and use prime number hash buckets instead. Combining the basic knowledge of hash buckets we learned in the database and the hints given by the topic, the use of prime number of hash buckets will make the distribution of the buckets more uniform, and at the same time, use the modular operation to determine the number of the bucket:

```c
#define NBUCKET 13
#define HASH(id) (id % NBUCKET)

struct hashbuf {
  struct buf head;       // 头节点
  struct spinlock lock;  // 锁
};

struct {
  struct buf buf[NBUF];
  struct hashbuf buckets[NBUCKET];  // 散列桶
} bcache;
```

We define the head node and spinlock attributes for the hash bucket structure, and then define bcache as a buffer and hash bucket structure.

2). Initialize the lock of the hash bucket in the `binit` function, and then point the `head->prev` and `head->next` of all the hash buckets to themselves as empty, and mount all the buffers to ` bucket[0]`On the bucket, the code is as follows

```c
void
binit(void) {
  struct buf* b;
  char lockname[16];

  for(int i = 0; i < NBUCKET; ++i) {
    // 初始化散列桶的自旋锁
    snprintf(lockname, sizeof(lockname), "bcache_%d", i);
    initlock(&bcache.buckets[i].lock, lockname);

    // 初始化散列桶的头节点
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
    // 利用头插法初始化缓冲区列表,全部放到散列桶0上
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}
```

(3). Add a new field `timestamp` in ***buf.h***, here to understand the purpose of this field: In the original solution, every time `brelse` will mount the released buffer to the head of the linked list, This buffer has just been used recently. When it is allocated in `bget`, it is searched forward from the end of the linked list, so that the first one that meets the conditions is the one that has not been used for a long time. In the prompt, it is recommended to use the timestamp as the rule of LRU determination, so that we don't need to change the node position by header interpolation in `brelse`:

```c
struct buf {  ...  uint timestamp;  // 时间戳};
```

(4). Change `brelse`, no longer acquire global lock

```c
voidbrelse(struct buf* b) {  if(!holdingsleep(&b->lock))    panic("brelse");  int bid = HASH(b->blockno);  releasesleep(&b->lock);  acquire(&bcache.buckets[bid].lock);  b->refcnt--;  // 更新时间戳  // 由于LRU改为使用时间戳判定，不再需要头插法  acquire(&tickslock);  b->timestamp = ticks;  release(&tickslock);  release(&bcache.buckets[bid].lock);}
```

(5). Change `bget` to allocate when the specified buffer is not found. The allocation method is to traverse the current list first, find a buffer with no reference and the smallest `timestamp`, if not, apply for the lock of the next bucket and traverse After finding the bucket, move the buffer from the original bucket to the current bucket, and at most all buckets will be traversed. Pay attention to the release of the lock in the code

```c
static struct buf*bget(uint dev, uint blockno) {  struct buf* b;  int bid = HASH(blockno);  acquire(&bcache.buckets[bid].lock);  // Is the block already cached?  for(b = bcache.buckets[bid].head.next; b != &bcache.buckets[bid].head; b = b->next) {    if(b->dev == dev && b->blockno == blockno) {      b->refcnt++;      // 记录使用时间戳      acquire(&tickslock);      b->timestamp = ticks;      release(&tickslock);      release(&bcache.buckets[bid].lock);      acquiresleep(&b->lock);      return b;    }  }  // Not cached.  b = 0;  struct buf* tmp;  // Recycle the least recently used (LRU) unused buffer.  // 从当前散列桶开始查找  for(int i = bid, cycle = 0; cycle != NBUCKET; i = (i + 1) % NBUCKET) {    ++cycle;    // 如果遍历到当前散列桶，则不重新获取锁    if(i != bid) {      if(!holding(&bcache.buckets[i].lock))        acquire(&bcache.buckets[i].lock);      else        continue;    }    for(tmp = bcache.buckets[i].head.next; tmp != &bcache.buckets[i].head; tmp = tmp->next)      // 使用时间戳进行LRU算法，而不是根据结点在链表中的位置      if(tmp->refcnt == 0 && (b == 0 || tmp->timestamp < b->timestamp))        b = tmp;    if(b) {      // 如果是从其他散列桶窃取的，则将其以头插法插入到当前桶      if(i != bid) {        b->next->prev = b->prev;        b->prev->next = b->next;        release(&bcache.buckets[i].lock);        b->next = bcache.buckets[bid].head.next;        b->prev = &bcache.buckets[bid].head;        bcache.buckets[bid].head.next->prev = b;        bcache.buckets[bid].head.next = b;      }      b->dev = dev;      b->blockno = blockno;      b->valid = 0;      b->refcnt = 1;      acquire(&tickslock);      b->timestamp = ticks;      release(&tickslock);      release(&bcache.buckets[bid].lock);      acquiresleep(&b->lock);      return b;    } else {      // 在当前散列桶中未找到，则直接释放锁      if(i != bid)        release(&bcache.buckets[i].lock);    }  }  panic("bget: no buffers");}
```

(6).Finally, the two functions at the end are also modified

```c
voidbpin(struct buf* b) {  int bid = HASH(b->blockno);  acquire(&bcache.buckets[bid].lock);  b->refcnt++;  release(&bcache.buckets[bid].lock);}voidbunpin(struct buf* b) {  int bid = HASH(b->blockno);  acquire(&bcache.buckets[bid].lock);  b->refcnt--;  release(&bcache.buckets[bid].lock);}
```

#### Result of this problem

`bcachetest`

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab8-3.png" style="zoom: 67%;" />

`usertests`

<img src="C:\Users\86199\Desktop\lab8\lab8-4.png" style="zoom: 67%;" />



## Experience of this lab 

​	This experiment is closely related to the deadlock and semaphore that we learned in class. I felt familiar at the beginning, but it is very difficult to complete. The reason is that the semaphore we wrote in the class is only a pseudo-code representation, and here we need to use code to implement locks and improve locks, which involves modifying the data structure and modifying the allocation and release logic of the lock, which is more difficult. Therefore, for this experiment, I looked for some analysis on the Internet and referred to it, and finally completed the experiment.

## Screenshot of this lab

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab8-5.png" style="zoom:67%;" />
