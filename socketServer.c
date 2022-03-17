#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define SERVER_PORT 80
static int debug = 1;

void do_http_request(int client_sock);
int get_line(int client_sock, char *buf, int size);

int main(void *arg)
{
    int sock;
    struct sockaddr_in server_addr;

    //! 创建服务端 用于监听的套接字
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("listening socket create failed...\n");
        exit(0);
    }

    bzero(&server_addr, sizeof(server_addr)); // 清空地址短裤结构体信息，便于赋值

    server_addr.sin_family = AF_INET;          // 指定IP协议版本
    server_addr.sin_port = htons(SERVER_PORT); // 网络字节序端口号
    // server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 本地任意IP地址，每个网卡对应一个

    inet_pton(AF_INET, "192.168.233.101", &server_addr.sin_addr.s_addr); //指定IP地址

    //! 指定端口复用
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //! 绑定监听的套接字    socket = IP:port
    int ret = bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == -1)
    {
        perror("listening socket bind failed...\n");
        exit(0);
    }

    //! 设置监听，启动监听端口
    ret = listen(sock, 128); // 同时处理的最大连接数，最大值为128
    if (ret == -1)
    {
        perror("listening socket listen failed...\n");
        exit(0);
    }

    printf("listening socket is ready to accept!\n");

    int done = 1;

    while (done)
    {
        struct sockaddr_in client;
        socklen_t client_addr_len = sizeof(client);
        char client_ip[64] = {0};

        //! 从sock的读缓冲队列中 接收连接请求，若空则阻塞
        int client_sock = accept(sock, (struct sockaddr *)&client, &client_addr_len);

        printf("accept succeed, Client ip : %s\t port: %d\n",
               inet_ntop(AF_INET, &client.sin_addr.s_addr, client_ip, sizeof(client_ip)),
               ntohs(client.sin_port)); // 打印客户端IP和端口

        // todo 处理HTTP请求
        do_http_request(client_sock);
        close(client_sock);
    }
    close(sock);
    return 0;
}

/**
 * 处理HTTP请求
 */
void do_http_request(int client_sock)
{
    char buf[256] = {0};
    int len = 0;
    char method[64]; // 存储HTTP请求方法
    char url[256];   // 存储HTTP请求URL

    //! 1. 读取请求行
    len = get_line(client_sock, buf, sizeof(buf));

    //! 2. 读取到请求行头部，判断请求方法和URL
    //格式为:   method url type \r\n
    if (len > 0)
    {
        int i = 0, j = 0;
        while (!isspace(buf[j]) && i < sizeof(method) - 1 && j < sizeof(buf) - 1)
        {
            method[i++] = buf[j++];
        }
        method[i] = '\0';
        printf("method%s\n", method);
    }

    //! 只处理get请求
    if (strncasecmp(method, "GET", strlen(method)))
    {
        if(debug){
            printf("method = GET\n");
        }
    }

    printf("请求执行了\n");
}

/**
 * 读取HTTP请求头部一行一行读
 * @return -1 表示读取出错，    0 表示读取空行；    >0 表示读取成功的字符数
 */
int get_line(int client_sock, char *buf, int size)
{
    // 实现方法是 从 sock的读缓冲区一个一个字符读取，判断HTTP每一行都已 \n 结束
    int count = 0;
    char ch = '\0';
    int len = 0;
    // 循环条件，最后一个字符需要手动设置为 \0
    while ((count < size - 1) && ch != '\n')
    {
        len = read(client_sock, &ch, 1);
        if (len == 1)
        {
            if (ch == '\r') // 若是回车符 CR
            {
                continue;
            }
            else if (ch == '\n') // 若是换行符 LF
            {
                // buf[count] = '\0';
                break;
            }
            buf[count++] = ch; // 处理一般字符
        }
        else if (len == 0)
        {
            fprintf(stderr, "客户端已断开 ..."); //!向标准错误流输出
            count = -1;
            break;
        }
        else
        {
            perror("read failed ...");
            count = -1;
            break;
        }
    }
    if (count >= 0)
        buf[count] = '\0';
    return count;
}
