#include "user/user.h"
#include "kernel/types.h"

int main(void)
{
    //首先使用一个大小为2的二维数组创建一个管道
    int parent_pipefd[2];//用于父进程发送消息
    int child_pipefd[2];//用于子进程发送消息
    pipe(parent_pipefd);//创建管道
    pipe(child_pipefd);//创建管道

    char msg='0';//子进程返回的消息
    char* buf=(char*)malloc(sizeof(char));//写缓冲区

    int pid=fork();//创建子进程
    
    if(!pid)//子进程
    {
        close(parent_pipefd[1]); //关闭父进程的写文件描述符
        close(child_pipefd[0]);  //关闭子进程的读文件描述符
        read(parent_pipefd[0], &buf, 1);
        printf("%d: received ping\n", getpid());
        write(child_pipefd[1], &msg, 1);
    }
    else{
        close(parent_pipefd[0]); //关闭父进程管道的读文件描述符
        close(child_pipefd[1]);  //关闭子进程管道的写文件描述符
        write(parent_pipefd[1], &msg, 1);
        read(child_pipefd[0], &buf, 1);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}