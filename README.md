# 项目概述

此项目为个人学习Linux服务器编程的一个总结性的项目，是Linux系统下使用C实现的轻量级HTTP服务器，涵盖内容如下：

- 基于 **`epoll`** 实现了 **单Reactor单线程** 模型
- 基于 **线程池** 实现了多线程的并发模式
- 复现了使用 **`libevent` 库** 封装的版本
- 目前仅支持 HTTP **GET** 请求，支持返回多种文件以及文件目录
- 待补充和完善......



## 项目框架

- 该框架图为使用线程池的 **单Reactor多线程** 模式

<img src="https://img-blog.csdnimg.cn/a94e817159754b7999b08339edcf0640.png?x-oss-process=image/watermark,type_d3F5LXplbmhlaQ,shadow_50,text_Q1NETiBAWGlEaWVjY2M=,size_20,color_FFFFFF,t_70,g_se,x_16#pic_center" style="zoom:73%;" />

**Reactor模型**

- **Reactor** :  主要功能是监听事件，可监听客户端建立连接的请求，以及已连接客户端的通信事件
- **Acceptor** ：用于与客户端建立连接，并将客户端的socket通信文件标识符返回给Reactor
- **Handler** :  负责内核和用户态的数据传送，即读取客户端内容，并将结果响应客户端，其数据处理业务交由线程池

<br>

**线程池**

- 任务队列： 是线性队列，存储由Handler派发的任务，可以认为 Handler 是一个任务生产者
- 管理者线程：它的任务是对任务队列中的任务数量以及处于忙状态的工作线程个数进行检测，负责平衡工作线程数量和任务数量关系
- 工作线程： 专门处理业务的工作线程，不断从任务队列中取出任务，可将其作为消费者，注意工作线程有多个

<br>

:coffee: 关于 `libevent` 和 `epoll` 的两个版本，这两个版本架构都类似上图，`libevent` 本身就是一个Reactor模型，内部也是对 `select`、`poll`、`epoll`等做了封装





# 相关知识储备

## **HTTP协议**

- HTTP请求报文格式

  包括 请求行和请求行中的请求方法，请求头部键值对，请求体数据

- HTTP响应报文格式

  包括 响应行和响应行的状态码，响应头部键值对，响应体数据

- URL格式

  包括 域名或IP地址，端口号，参数，中文字符解码编码等

- Linux系统防火墙有关 **端口和 `httpd` 服务**的设置

- web 前端知识相关

  三剑客 `HTML`、`CSS`、`JavaScript` ，这条我硬加进来的，暂时是无关紧要的，但是聊前端开发我能聊几天  :sob:

<br>

## 网络通信流程

主要是 Linux `Socket` 套接字通信 服务器端 TCP 通信流程

- `socket()`、`bind()`、`listen()`、`accept()`、`read()/recv()`、`write()/send()`

  <img src="./assets/socket.png" style="zoom:67%;" />

  

- 网络字节序和主机字节序

  大端小端，单字节的字符组成的字符串不受影响；

  两套转换函数 `htonl()` 、`inet_pton()`等

<br>

## TCP协议

- TCP头部报文格式

- 三次握手

  包括具体流程，双方状态转换

- 四次挥手

  同上，具体理解 **半关闭** 和 **`2MSL `**，知道端口复用

- 流量控制

- TCP粘包

<br>

## I/O多路复用

先检测事件是否准备，后进行操作

- 非阻塞模式

  理解 同步、异步、阻塞、非阻塞 概念

  知道如何设置I/O为非阻塞

- `select`

  `fd_set` 位图，`select()`函数

- `poll`

  没有最大文件描述符的限制

- `epoll`

  解决了 `select` 和 `poll` 的 线性检测和轮询问题，内部使用红黑树，**ET** 和 **LT** 工作模式

  `epoll_create()`、`epoll_ctl()`、`epoll_wait()` 操作函数

  结构体`epoll_event`内部 **联合体 `epoll_data`** 的指针成员，以此传递回调函数

<br>



## `libevent` 库

- Reactor 反应堆模式
- `event_base` 结构体
- 事件循环`event_loop`
- 事件 `event`
- 数据缓冲 `bufferevent`
- 监听器 `evconnlistener`



<br>

## 服务器并发

- 多进程
- 多线程
- 线程同步
- 线程池
- 

 <br>

## 其他补充知识

- 编码方面

  Linux文件以及目录相关操作

  Linux文件I/O部分

- Linux 命令

- makefile 规则

- `gcc`、`gdb` 工具

- ......

<br>

# 个人日志

以下为个人开发及调试过程中的日志，记录了自己遇到的问题及解决办法，以及一些小点子 :tv:

<br>

- 线程池内的线程执行完业务，得到结果，是将结果直接写入`write()`内核，还是将结果传出给 Handler ，然后 Handler 负责写入内核呢？虽然项目框架图是后者，但是我真正实现的是前者 :no_good:

  伪代码如下：

```c
// ********** Reactor **********
event_base;

// ********** Acceptor **********
evconnlistenet(listener_cb);

// Acceptor的任务，处理新连接，并返回给 Reactor
listener_cb(){
	cfd = accept();
    
    // ********** Handler **********
    bufferevent bev = new bufferevent(event_base,cfd,...) ;
    
    // 新连接接下来的事件,读写回调
    befferevent_setcb(bev, read_cb, write_cb, ...);
}

// 读数据
read_cb(){
    read();
    
    // 将读出的数据相关的业务逻辑 交由线程池
    threadpooladd(service_func,ret);
    
}

// 响应数据
write_cb(){
    P(ret); //！！！ 阻塞在这，等待线程处理完成,将结果ret返回，PV可以理解为加锁解锁过程
    send();
}
// 但是这个函数非常鸡肋，它是数据写回内核之后再回调执行的，所以此处的send()并无用处
// 所以，这个 send() 操作应当在 工作线程内完成，而非：
// （工作线程只负责处理得到结果，将结果返回给bufferevent，然后将结果写入内核）。
// 正确情况是，将通信socket文件描述符 cfd 当成任务参数，传递给线程，线程完成后直接write()写入内核
// 然后调用 write_cb() 回调，检测线程是否完成任务并写入内核

// 业务逻辑，比如读取数据库啥的，比如计算数据转换格式啥的...尽管我还没有实现数据库
service_func(void *ret){
    // 业务逻辑
    // ret 为传出参数，处理结果
    
    //！！！ 业务本身具有多样性，有的任务不一定有返回结果，所以可以在此处 解锁，唤醒将结果写入内核的线程
    V(ret);
}

// 将任务添加到线程池中的任务队列
threadpooladd(service_func,ret);

// 某个工作线程从任务队列中拿到任务
thread_work(service_func,ret){
    // 线程开始处理业务函数
	// 处理完成发出信号
    
    //！！！ 可以判断业务是否要返回的结果数据，若是则解锁,表示结果已经处理好了，可以返回给客户端了
	V(ret);
}

```

  

- 测试了半天，发现无法收到数据，检查是 `bufferevent` 默认 读和写缓冲区都是 disable，设置如下：

  ```c
  bufferevent_enable(bev, EV_READ);
  bufferevent_enable(bev, EV_WRITE);
  ```

- ...