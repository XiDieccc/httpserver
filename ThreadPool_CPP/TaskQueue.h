#pragma once
#include <queue>
#include <pthread.h>

using callback = void (*)(void *arg);

// 任务类型，有回调函数地址，以及函数内参数
template <typename T>
struct Task
{
    Task<T>()
    {
        function = nullptr;
        arg = nullptr;
    }
    Task<T>(callback f, void *arg)
    {
        this->function = f;
        this->arg = (T *)arg;
    }
    callback function;
    T *arg;
};

template <typename T>
class TaskQueue
{
    // ! 参数
    // 容量，对头，队尾，当前任务数量

    // ! 操作：
    // 1.生产者生产任务，任务进入队列
    // 2.消费者使用任务，任务出队列
public:
    TaskQueue();
    ~TaskQueue();

    // 1. 添加任务
    void addTask(Task<T> task);
    void addTask(callback func, void *arg); //重载

    // 2. 取出一个任务
    Task<T> takeTask();

    // 3. 获取当前任务个数
    //! 内联函数直接复制替换函数
    //!（编译内联，或者编译器知道确切的内联函数，否则在运行时比如多态，就不可内联）
    //! 成员函数默认是内联函数
    inline int taskNuber()
    {
        return this->m_taskQ.size();
    }

private:
    pthread_mutex_t m_mutex;
    std ::queue<Task<T>> m_taskQ;
};