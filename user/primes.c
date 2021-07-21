#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void newPipe(int fd[2])
{
    int prime;
    int flag;
    int num;                                             //存放后续读出的数字
    close(fd[1]);                                        //在读时关闭写文件描述符
    if (read(fd[0], &prime, sizeof(int)) != sizeof(int)) //将文件中的数逐个读入prime中，显然，第一个读的数字肯定是prime
    {
        fprintf(2, "child process failed when reading the number.\n");
        exit(1);
    }
    printf("prime %d\n", prime); //输出筛选得出的素数

    flag = read(fd[0], &num, sizeof(int));
    if (flag)
    {
        int newpipefd[2]; //存放新的管道的文件描述符
        pipe(newpipefd);
        if (fork() == 0)
        {
            newPipe(newpipefd);
        }
        else
        {
            close(newpipefd[0]); //在写时关闭读文件描述符
            if (num % prime != 0)
                write(newpipefd[1], &num, sizeof(num));

            while (read(fd[0], &num, sizeof(int)))
            {
                if (num % prime)
                    write(newpipefd[1], &num, sizeof(int));
            }
            close(fd[0]);
            close(newpipefd[1]);
            wait(0); //递归等待子进程结束
        }
    }
    exit(0);
}

int main(int argc, char *argv)
{
    int pipefd[2];   //用于存放新建的读写文件描述符
    pipe(pipefd);    //创建筛选前的第一个管道
    if (fork() == 0) //子进程
    {
        newPipe(pipefd);
    }
    else
    {
        close(pipefd[0]); //写时释放读描述符
        //依次写入2-35数字
        for (int i = 2; i <= 35; i++)
        {
            if (write(pipefd[1], &i, sizeof(int)) != sizeof(int)) //我们认为只要写的字符数不等于可以写的最大字符数均为写失败
            {
                fprintf(2, "writing the number %d to the pipe failed!\n", i);
                exit(1);
            }
        }
        close(pipefd[1]); //关闭写通道
        wait(0);          //等待子进程的结束
        exit(0);
    }
    return 0;
}