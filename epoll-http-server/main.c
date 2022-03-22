#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "epoll_server.h"

int main(int argc, const char *argv[])
{
    // 指定运行格式，两个参数为 服务器端口 服务器运行路径
    if (argc < 3)
    {
        printf("try like: ./server port path\n");
        exit(1);
    }

    // 采用指定的端口
    int port = atoi(argv[1]);

    // 修改进程工作目录, 方便后续操作
    int ret = chdir(argv[2]);
    if (ret == -1)
    {
        perror("chdir error");
        exit(1);
    }

    epoll_run(port);
    return 0;
}
