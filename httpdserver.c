#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_PORT 80

static int debug = 1;

int get_line(int sock, char *buf, int size);
void *do_http_request(void *client_sock);
void do_http_response(int client_sock, const char *path);
int headers(int client_sock, FILE *resource);
void cat(int client_sock, FILE *resource);

void not_found(int client_sock);     // 404
void unimplemented(int client_sock); // 500
void bad_request(int client_sock);   // 400
void inner_error(int client_sock);

int main(void)
{

    int sock;
    struct sockaddr_in server_addr;

    //! 1.创建 服务端socket，设置为 IPV4,TCP流
    sock = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&server_addr, sizeof(server_addr)); // 先清内存

    server_addr.sin_family = AF_INET;                //选择协议族IPV4
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //监听本地所有IP地址
    server_addr.sin_port = htons(SERVER_PORT);       //绑定端口号

    //! 2.服务端socket绑定需要监听的端口 ip:port
    bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    //! 3.服务端设置监听，128为同时处理的最大连接数
    listen(sock, 128);

    printf("等待客户端的连接\n");

    int done = 1;
    while (done)
    {
        struct sockaddr_in client;
        int client_sock, len, i;
        char client_ip[64];
        char buf[256];
        pthread_t id;
        int *pclient_sock = NULL;

        socklen_t client_addr_len;
        client_addr_len = sizeof(client);

        //! 4.从监听缓冲队列中选取一个建立 TCP连接
        client_sock = accept(sock, (struct sockaddr *)&client, &client_addr_len);

        // 打印客服端IP地址和端口号
        printf("client ip: %s\t port : %d\n",
               inet_ntop(AF_INET, &client.sin_addr.s_addr, client_ip, sizeof(client_ip)),
               ntohs(client.sin_port));

        /*处理http 请求,读取客户端发送的数据*/
        // do_http_request(client_sock);

        //启动线程处理http 请求
        pclient_sock = (int *)malloc(sizeof(int));
        *pclient_sock = client_sock;

        pthread_create(&id, NULL, do_http_request, (void *)pclient_sock);

        // close(client_sock);
    }
    close(sock);
    return 0;
}

void *do_http_request(void *pclient_sock)
{
    int len = 0;
    char buf[256];
    char method[64];
    char url[256];
    char path[256];
    int client_sock = *(int *)pclient_sock;

    struct stat st;

    /*读取客户端发送的http 请求*/

    // 1.读取请求行
    len = get_line(client_sock, buf, sizeof(buf));

    if (len > 0)
    { //读到了请求行
        int i = 0, j = 0;
        while (!isspace(buf[j]) && (i < sizeof(method) - 1))
        {
            method[i] = buf[j];
            i++;
            j++;
        }

        method[i] = '\0';
        if (debug)
            printf("request method: %s\n", method);
        if (strncasecmp(method, "GET", i) == 0)
        { //只处理get请求
            if (debug)
                printf("method = GET\n");

            //获取url
            while (isspace(buf[j++]))
                ; //跳过白空格
            i = 0;

            while (!isspace(buf[j]) && (i < sizeof(url) - 1))
            {
                url[i] = buf[j];
                i++;
                j++;
            }

            url[i] = '\0';

            if (debug)
                printf("url: %s\n", url);

            //继续读取http 头部
            do
            {
                len = get_line(client_sock, buf, sizeof(buf));
                if (debug)
                    printf("read: %s\n", buf);

            } while (len > 0);

            //***定位服务器本地的html文件***

            //处理url 中的?   ?后跟的是json参数
            {
                char *pos = strchr(url, '?');
                if (pos)
                {
                    *pos = '\0';
                    printf("real url: %s\n", url);
                }
            }

            //! 将url拼接到html_docs文件夹下的具体文件，得到完整路径 path
            sprintf(path, "./html_docs/%s", url);
            if (debug)
                printf("path: %s\n", path);

            //执行http 响应
            //判断文件是否存在，如果存在就响应200 OK，同时发送相应的html 文件,如果不存在，就响应 404 NOT FOUND.
            //! stat()用来将参数 file_name 所指的文件状态, 复制到参数 buf 所指的结构中
            //! 返回值：执行成功则返回0，失败返回-1，错误代码存于errno
            if (stat(path, &st) == -1)
            { //文件不存在或是出错
                fprintf(stderr, "stat %s failed. reason: %s\n", path, strerror(errno));
                not_found(client_sock);
            }
            else
            { //文件存在
                // 若是目录，则默认添加/index.html，默认返回该文件夹下的index.html文件
                //! S_ISDIR()函数的作用是判断一个路径是不是目录
                if (S_ISDIR(st.st_mode))
                {
                    strcat(path, "/index.html");
                }

                do_http_response(client_sock, path);
            }
        }
        else
        { //非get请求, 读取http 头部，并响应客户端 501 	Method Not Implemented
            fprintf(stderr, "warning! other request [%s]\n", method);
            do
            {
                len = get_line(client_sock, buf, sizeof(buf));
                if (debug)
                    printf("read: %s\n", buf);

            } while (len > 0);

            unimplemented(client_sock); //请求未实现
        }
    }
    else
    {                             //请求格式有问题，出错处理
        bad_request(client_sock); //在响应时再实现
    }

    close(client_sock);
    if (pclient_sock)
        free(pclient_sock); //释放动态分配的内存

    return NULL;
}

void do_http_response(int client_sock, const char *path)
{
    int ret = 0;
    FILE *resource = NULL;

    resource = fopen(path, "r");
    if (resource == NULL)
    {
        not_found(client_sock);
        return;
    }

    // 1.发送 http 头部, 主要是确定 resource 的文件长度，以及是否读成功
    ret = headers(client_sock, resource);

    // 2.发送http body .
    if (!ret)
    {
        cat(client_sock, resource);
    }

    fclose(resource);
}

/****************************
 *返回关于响应文件信息的http 头部
 *输入：
 *     client_sock - 客服端socket 句柄
 *     resource    - 文件的句柄
 *返回值： 成功返回0 ，失败返回-1
 *头部格式：
 *       版本 状态码 短语 CRLF
 ******************************/
int headers(int client_sock, FILE *resource)
{
    struct stat st;
    int fileid = 0;
    char tmp[64];
    char buf[1024] = {0};
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    strcat(buf, "Server: Martin Server\r\n");
    strcat(buf, "Content-Type: text/html\r\n");
    strcat(buf, "Connection: Close\r\n");

    //! fileno()用来取得参数 stream 指定的文件流所使用的文件描述词 文件描述符？
    fileid = fileno(resource);

    //! fstat()用来将参数 fildes 所指的文件状态, 复制到参数buf 所指的结构中(struct stat).
    //! fstat()与stat()作用完全相同, 不同处在于传入的参数为已打开的文件描述词. 详细内容请参考stat().
    //! 返回值：执行成功则返回0, 失败返回-1, 错误代码存于errno.
    if (fstat(fileid, &st) == -1)
    {
        inner_error(client_sock);
        return -1;
    }

    snprintf(tmp, 64, "Content-Length: %ld\r\n\r\n", st.st_size);
    strcat(buf, tmp);

    if (debug)
        fprintf(stdout, "header: %s\n", buf);

    //! 向客户端发送 响应头部数据
    if (send(client_sock, buf, strlen(buf), 0) < 0)
    {
        fprintf(stderr, "send failed. data: %s, reason: %s\n", buf, strerror(errno));
        return -1;
    }

    return 0;
}

/****************************
 *说明：实现将html文件的内容按行
        读取并送给客户端
 ****************************/
void cat(int client_sock, FILE *resource)
{
    char buf[1024];

    //! 函数说明：fgets()用来从参数stream 所指的文件内读入字符并存到参数s 所指的内存空间,
    //!直到出现换行字符、读到文件尾或是已读了size-1 个字符为止, 最后会加上NULL 作为字符串结束.
    //! 返回值：gets()若成功则返回s 指针, 返回NULL 则表示有错误发生.
    fgets(buf, sizeof(buf), resource);

    while (!feof(resource))
    {
        int len = write(client_sock, buf, strlen(buf));

        if (len < 0)
        { //发送body 的过程中出现问题,怎么办？1.重试？ 2.
            fprintf(stderr, "send body error. reason: %s\n", strerror(errno));
            break;
        }

        if (debug)
            fprintf(stdout, "%s", buf);
        fgets(buf, sizeof(buf), resource);
    }
}

void do_http_response1(int client_sock)
{
    const char *main_header = "HTTP/1.0 200 OK\r\nServer: Martin Server\r\nContent-Type: text/html\r\nConnection: Close\r\n";

    const char *welcome_content = "\
<html lang=\"zh-CN\">\n\
<head>\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\n\
<title>This is a test</title>\n\
</head>\n\
<body>\n\
<div align=center height=\"500px\" >\n\
<br/><br/><br/>\n\
<h2>大家好，欢迎来到奇牛学院VIP 试听课！</h2><br/><br/>\n\
<form action=\"commit\" method=\"post\">\n\
尊姓大名: <input type=\"text\" name=\"name\" />\n\
<br/>芳龄几何: <input type=\"password\" name=\"age\" />\n\
<br/><br/><br/><input type=\"submit\" value=\"提交\" />\n\
<input type=\"reset\" value=\"重置\" />\n\
</form>\n\
</div>\n\
</body>\n\
</html>";

    // 1. 发送main_header
    int len = write(client_sock, main_header, strlen(main_header));

    if (debug)
        fprintf(stdout, "... do_http_response...\n");
    if (debug)
        fprintf(stdout, "write[%d]: %s", len, main_header);

    // 2. 生成Content-Length
    char send_buf[64];
    int wc_len = strlen(welcome_content);
    len = snprintf(send_buf, 64, "Content-Length: %d\r\n\r\n", wc_len);
    len = write(client_sock, send_buf, len);

    if (debug)
        fprintf(stdout, "write[%d]: %s", len, send_buf);

    len = write(client_sock, welcome_content, wc_len);
    if (debug)
        fprintf(stdout, "write[%d]: %s", len, welcome_content);
}

//返回值： -1 表示读取出错， 等于0表示读到一个空行， 大于0 表示成功读取一行
int get_line(int sock, char *buf, int size)
{
    int count = 0;
    char ch = '\0';
    int len = 0;

    while ((count < size - 1) && ch != '\n')
    {
        len = read(sock, &ch, 1);

        if (len == 1)
        {
            if (ch == '\r')
            {
                continue;
            }
            else if (ch == '\n')
            {
                // buf[count] = '\0';
                break;
            }

            //这里处理一般的字符
            buf[count] = ch;
            count++;
        }
        else if (len == -1)
        { //读取出错
            perror("read failed");
            count = -1;
            break;
        }
        else
        { // read 返回0,客户端关闭sock 连接.
            fprintf(stderr, "client close.\n");
            count = -1;
            break;
        }
    }

    if (count >= 0)
        buf[count] = '\0';

    return count;
}

void not_found(int client_sock)
{
    const char *reply = "HTTP/1.0 404 NOT FOUND\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>NOT FOUND</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
	<P>文件不存在！\r\n\
    <P>The server could not fulfill your request because the resource specified is unavailable or nonexistent.\r\n\
</BODY>\r\n\
</HTML>";

    //! 将上述404页面文件写入写缓冲区，通信socket会发送到对方的读缓冲区中
    int len = write(client_sock, reply, strlen(reply));
    if (debug)
        fprintf(stdout, reply);

    if (len <= 0)
    {
        // fprintf(stderr,"发送404数据失败: %s\n",stderror(errno));
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}

void unimplemented(int client_sock)
{
    const char *reply = "HTTP/1.0 501 Method Not Implemented\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>Method Not Implemented</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>HTTP request method not supported.\r\n\
</BODY>\r\n\
</HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if (debug)
        fprintf(stdout, reply);

    if (len <= 0)
    {
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}

void bad_request(client_sock)
{
    const char *reply = "HTTP/1.0 400 BAD REQUEST\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML>\r\n\
<HEAD>\r\n\
<TITLE>BAD REQUEST</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>Your browser sent a bad request！\r\n\
</BODY>\r\n\
</HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if (len <= 0)
    {
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}

void inner_error(int client_sock)
{
    const char *reply = "HTTP/1.0 500 Internal Sever Error\r\n\
Content-Type: text/html\r\n\
\r\n\
<HTML lang=\"zh-CN\">\r\n\
<meta content=\"text/html; charset=utf-8\" http-equiv=\"Content-Type\">\r\n\
<HEAD>\r\n\
<TITLE>Inner Error</TITLE>\r\n\
</HEAD>\r\n\
<BODY>\r\n\
    <P>服务器内部出错.\r\n\
</BODY>\r\n\
</HTML>";

    int len = write(client_sock, reply, strlen(reply));
    if (debug)
        fprintf(stdout, reply);

    if (len <= 0)
    {
        fprintf(stderr, "send reply failed. reason: %s\n", strerror(errno));
    }
}