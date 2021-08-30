# MIT labs 2020--lab9(fs) experiment documentation

| Student id | student name |
| ---------- | ------------ |
| 1853572    | Qiao Liang   |

## Purpose of this Experiment

​	This experiment requires us to add large files and symbolic links to the xv6 file system based on understanding the hierarchical structure of the file system.

## **Pre-knowledge of  this lab**

​	In the previous experiment, we have already touched part of the knowledge about the file system buffer cache layer. Before doing this experiment, we need to continue reading about the contents of Chapter 8 of xv6. The following is a brief summary.

<img src="https://github.com/duguosheng/6.S081-All-in-one/raw/main/tranlate_books/book-riscv-rev1/images/c8/p1.png" alt="img" style="zoom:67%;" />

​	First of all, still this picture, xv6 divides the disk into the above parts, which will be introduced one by one below.

#### **Log Level**

​		A common problem faced by file systems is crash recovery. **xv6 solves the crash problem during file system operation through a simple logging form**. The xv6 system call will not directly write the file system data structure on the disk, but will place the description of all the disk write operations that you want to perform in the `log` on the disk. Once the system call records all its write operations, it will write a special `commit` record to the disk, indicating that the log contains a complete operation. At this point, the system calls to copy the write operation to the file system data structure on the disk. After these writes are completed, the system call will erase the log on the disk.

#### Design of the log 

​	The log resides in a known fixed location specified in the super block. It consists of a header block and a series of logged blocks. The header block contains an array of sector numbers (**each logged block corresponds to a sector number**) and the count of the log block. The count in the header block on the disk is either zero, indicating that there are no transactions in the log; or non-zero, indicating that the log contains a complete committed transaction and has a specified number of logged blocks. Xv6 only writes data to the header block when the transaction is committed, and will not write data before that, and sets the count to zero after copying the logged blocks to the file system. Therefore, a crash in the middle of the transaction will cause the count in the log header block to be zero; a crash after the commit will result in a non-zero count.

​	The idea of submitting multiple transactions at the same time is called **group commit**. Group submission reduces the number of disk operations, because a single submission with a fixed cost amortizes multiple operations. Group submission also provides more concurrent write operations for the disk system, possibly allowing the disk to write all these operations within one disk rotation time.

​	A typical log usage in system calls is as follows:

```c
 begin_op();
 ...
 bp = bread(...);
 bp->data[...] = ...;
 log_write(bp);
 ...
 end_op(); 
```

#### Block Allocator

​	The block allocator provides two functions: `balloc` allocates a new disk block, and `bfree` releases a block. The contents of files and directories are stored in disk blocks, and the disk blocks must be allocated from the free pool. The xv6 block allocator maintains a free bitmap on the disk, and each bit represents a block. 0 means the corresponding block is free; 1 means it is in use. The program `mkfs` sets the bits corresponding to the boot sector, super block, log block, inode block and bitmap block.

#### Index Node Layer

​	The index nodes on the disk are all packed into a contiguous disk called an inode block. The size of each inode is the same, so it is easy to locate the node.

## Problem 1--Large Files

> Modify `bmap()` so that it implements a doubly-indirect block, in addition to direct blocks and a singly-indirect block. You'll have to have only 11 direct blocks, rather than 12, to make room for your new doubly-indirect block; you're not allowed to change the size of an on-disk inode. The first 11 elements of `ip->addrs[]` should be direct blocks; the 12th should be a singly-indirect block (just like the current one); the 13th should be your new doubly-indirect block. You are done with this exercise when `bigfile` writes 65803 blocks and `usertests` runs successfully:

#### 1.1 Goal Restatement

​	This experiment requires us to increase the maximum size of xv6 files by extending the layer relationship of blocks. By changing the xv6 file system code, the file system supports the "second-level indirect" block that can contain 256 first-level indirect block addresses in each inode.

#### 1.2 Problem Analysis

1). First, we add macro definitions in `fs.h`, as shown below:

```c
#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDINDIRECT ((BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)))
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)
#define NADDR_PER_BLOCK (BSIZE / sizeof(uint))  // 一个块中的地址数量
```

2).Because the value of `NDIRECT` has changed, one of the original twelve blocks has become a secondary indirect block, so we need to modify the number of `addrs` elements in the `inode` structure:

```c
// fs.h
struct dinode {
  ...
  uint addrs[NDIRECT + 2];   // Data block addresses
};
```

```c
// file.h
struct inode {
  ...
  uint addrs[NDIRECT + 2];
};
```

3).Modify `bmap` to support secondary index:

```c
static uint
bmap(struct inode *ip, uint bn)
{
////////////////////////////////////////
  if(bn < NINDIRECT){
    ...
  }
  bn -= NINDIRECT;
  // 二级间接块的情况
  if(bn < NDINDIRECT) {
    int level2_idx = bn / NADDR_PER_BLOCK;  // 要查找的块号位于二级间接块中的位置
    int level1_idx = bn % NADDR_PER_BLOCK;  // 要查找的块号位于一级间接块中的位置
    // 读出二级间接块
    if((addr = ip->addrs[NDIRECT + 1]) == 0)
      ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    if((addr = a[level2_idx]) == 0) {
      a[level2_idx] = addr = balloc(ip->dev);
      // 更改了当前块的内容，标记以供后续写回磁盘
      log_write(bp);
    }
    brelse(bp);

    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[level1_idx]) == 0) {
      a[level1_idx] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  panic("bmap: out of range");
}
```

4).Then we modify `itrunc` to release all blocks:

```c
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    ...
  }

  if(ip->addrs[NDIRECT]){
    ...
  }

  struct buf* bp1;
  uint* a1;
  if(ip->addrs[NDIRECT + 1]) {
    bp = bread(ip->dev, ip->addrs[NDIRECT + 1]);
    a = (uint*)bp->data;
    for(i = 0; i < NADDR_PER_BLOCK; i++) {
      // 每个一级间接块的操作都类似于上面的
      // if(ip->addrs[NDIRECT])中的内容
      if(a[i]) {
        bp1 = bread(ip->dev, a[i]);
        a1 = (uint*)bp1->data;
        for(j = 0; j < NADDR_PER_BLOCK; j++) {
          if(a1[j])
            bfree(ip->dev, a1[j]);
        }
        brelse(bp1);
        bfree(ip->dev, a[i]);
      }
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT + 1]);
    ip->addrs[NDIRECT + 1] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}
```

#### 1.3 Result of this lab

According to the screenshot below, we can find that the operating system has written a block containing 65803.

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab9-1.png" style="zoom: 67%;" />

## Problem 2--Symbolic links

> You will implement the `symlink(char *target, char *path)` system call, which creates a new symbolic link at path that refers to file named by target. For further information, see the man page `symlink`. To test, add `symlinktest` to the `Makefile` and run it. Your solution is complete when the tests produce the following output (including `usertests` succeeding).

#### 2.1 Goal Restatement 

​	This exercise requires us to add a soft link to xv6. A symbolic link (or soft link) refers to a file linked by a path name; when a symbolic link is opened, the kernel follows the link to point to the referenced file. Symbolic links are similar to hard links, but hard links are limited to pointing to files on the same disk, and symbolic links can span disk devices.

#### 2.2 Problem Analysis

1).This exercise requires us to add a system call. The method of adding is similar to the system call in lab2, so I won't elaborate on it here. We add an entry in ***user/usys.pl***, ***user/user.h***, in ***kernel/syscall.c***, ***kernel/syscall. Add relevant content in h***.

2). Then we add the relevant definitions in the prompt, which are `T_SYMLINK` and `O_NOFOLLOW`

```c
// fcntl.h
#define O_NOFOLLOW 0x004
// stat.h
#define T_SYMLINK 4
```

3). Implement `sys_symlink` in ***kernel/sysfile.c***, here we need to note that `create` returns the locked `inode`, in addition, `iunlockput` both unlocks the `inode` and also The reference count is decremented by 1, and the `inode` is reclaimed when the count reaches 0.

```c
uint64
sys_symlink(void) {
  char target[MAXPATH], path[MAXPATH];
  struct inode* ip_path;

  if(argstr(0, target, MAXPATH) < 0 || argstr(1, path, MAXPATH) < 0) {
    return -1;
  }

  begin_op();
  // 分配一个inode结点，create返回锁定的inode
  ip_path = create(path, T_SYMLINK, 0, 0);
  if(ip_path == 0) {
    end_op();
    return -1;
  }
  // 向inode数据块中写入target路径
  if(writei(ip_path, 0, (uint64)target, 0, MAXPATH) < MAXPATH) {
    iunlockput(ip_path);
    end_op();
    return -1;
  }

  iunlockput(ip_path);
  end_op();
  return 0;
}
```

4).Then modify sys_open to support opening symbolic links

```c
uint64
sys_open(void)
{
  ...
  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    ...
  }
  // 处理符号链接
  if(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)) {
    // 若符号链接指向的仍然是符号链接，则递归的跟随它
    // 直到找到真正指向的文件
    // 但深度不能超过MAX_SYMLINK_DEPTH
    for(int i = 0; i < MAX_SYMLINK_DEPTH; ++i) {
      // 读出符号链接指向的路径
      if(readi(ip, 0, (uint64)path, 0, MAXPATH) != MAXPATH) {
        iunlockput(ip);
        end_op();
        return -1;
      }
      iunlockput(ip);
      ip = namei(path);
      if(ip == 0) {
        end_op();
        return -1;
      }
      ilock(ip);
      if(ip->type != T_SYMLINK)
        break;
    }
    // 超过最大允许深度后仍然为符号链接，则返回错误
    if(ip->type == T_SYMLINK) {
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    ...
  }
  ...
  return fd;
}
```

This completes the experiment.

#### 2.3 Result of this lab

<img src="https://joes-bucket.oss-cn-shanghai.aliyuncs.com/img/lab9-2.png" style="zoom:67%;" />

## Experience of this lab 

​	Through this experiment, I mainly learned more about the file system, and learned several outstanding features of the file system; the mechanism behind the file system; the basic implementation of the file system and so on.

## Screenshot of the lab

<img src="C:\Users\86199\Desktop\lab9\lab9-3.png" style="zoom:67%;" />
