
#include "user/user.h"
#include "kernel/types.h"

int main(int argc, char *argv[])
{
    if (argc != 2) //表示用户没有传递参数
    {
        fprintf(2, "Error:User doesn't input any argument\n");
        exit(1);
    }
    else
    {
        sleep(atoi(argv[1])); //命令行第一个参数，也就是指定的滴答数
    }
    exit(0);
}
