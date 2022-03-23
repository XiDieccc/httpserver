#pragma once
#include "TaskQueue.h"
#include "TaskQueue.cpp"
template <typename T>
class ThreadPool
{
    //! 内容
    // 任务队列
    // 管理者线程、工作的线程、
    // 线程数量
public:
    // 创建线程池初始化，参数指定最小和最大线程数量
    ThreadPool(int min, int max);
    // 销毁线程池
    ~ThreadPool();

    // 给线程池里面的任务队列添加任务
    void addTask(Task<T> task);

    // 获取线程池中工作线程的数量
    int getBusyNum();

    // 获取线程池中 活着的（工作或闲）线程数量
    int getAliveNum();

private:
    //! 以下函数不需暴露给调用者，即线程池内部完成

    //! 往任务队列中添加任务 是 生产者生产过程，不需要线程来完成

    //! 必须是静态 static，有静态地址，详见实现函数的54行
    // 工作的线程（消费者线程）任务函数
    static void *worker(void *arg);

    // 管理者线程任务函数
    static void *manager(void *arg);

    // 单个线程退出
    void threadExit();

private:
    TaskQueue<T> *taskQ;

    pthread_t managerID;       //管理者线程ID
    pthread_t *threadIDs;      //工作的线程ID
    int minNum;                //最小线程数量
    int maxNum;                //最大线程数量
    int busyNum;               //忙的线程数
    int liveNum;               //存活线程个数
    int exitNum;               //销毁的线程个数
    pthread_mutex_t mutexPool; //锁整个线程池
    pthread_cond_t notEmpty;   //线程池是否为空
    //! 不需要阻塞生产者，因为队列 queue 可以“无限”增大

    static const int NUMBER = 2; //用于管理者 创建或销毁线程，最多一次性创建或销毁2个线程
    bool shutdown;               //线程池是否关闭
};