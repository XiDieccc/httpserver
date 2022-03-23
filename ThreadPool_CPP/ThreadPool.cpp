#include "ThreadPool.h"
#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>

using namespace std;

/**
 * @p min 最小线程数
 * @p max 最大线程数
 */
template <typename T>
ThreadPool<T>::ThreadPool(int min, int max)
{

    do
    {
        //! new 加括号（），分配内存同时会初始化（调用构造）
        //! 例如new int[100]()，就会使数组初始化为 0
        //! 不加括号，则只会分配内存，不进行初始化
        // taskQ = new TaskQueue();
        taskQ = new TaskQueue<T>;
        if (taskQ == nullptr)
        {
            cout << "malloc taskQ fail" << endl;
            break;
        }

        // threadIDs = new pthread_t[max](); //!同上面不加括号
        threadIDs = new pthread_t[max];
        if (threadIDs == nullptr)
        {
            cout << "malloc threadIDs fail" << endl;
            break;
        }
        //初始化，使用string.h头文件
        memset(threadIDs, 0, sizeof(pthread_t) * max);
        minNum = min;
        maxNum = max;
        busyNum = 0;
        liveNum = min; //最小存活线程数
        exitNum = 0;
        shutdown = false; //默认不关闭

        if (pthread_mutex_init(&mutexPool, NULL) != 0 ||
            pthread_cond_init(&notEmpty, NULL) != 0)
        {
            cout << "mutex or condition init fail" << endl;
            break;
        }

        // 创建线程
        // ! 这里 manager必须是类外函数，或者是静态成员函数
        // ! 成员函数只有在实例化的时候，才会有地址
        // ! 静态成员函数或者是类外函数在编译后就有地址了
        // 管理者线程，管理操作为 manager，传递参数为 this
        pthread_create(&managerID, NULL, manager, this);
        for (int i = 0; i < min; ++i)
        {
            pthread_create(&threadIDs[i], NULL, worker, this);
        }

    } while (0);

    if (threadIDs)
        delete[] threadIDs;
    if (taskQ)
        delete taskQ;
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
    shutdown = true;

    //阻塞回收 管理者 manager线程
    pthread_join(managerID, NULL);

    // 唤醒阻塞的消费者线程
    for (int i = 0; i < liveNum; i++)
    {
        pthread_cond_signal(&notEmpty);
    }
    if (taskQ)
    {
        delete taskQ;
    }
    if (threadIDs)
    {
        delete[] threadIDs;
    }

    pthread_mutex_destroy(&mutexPool);
    pthread_cond_destroy(&notEmpty);
}

template <typename T>
void ThreadPool<T>::addTask(Task<T> task)
{
    // 不需要阻塞生产者，任务队列没有上限
    if (shutdown)
    {
        pthread_mutex_unlock(&mutexPool);
        return;
    }
    // 干正事，添加新任务
    taskQ->addTask(task);
    // 唤醒阻塞的消费者线程
    pthread_cond_signal(&notEmpty);
}

template <typename T>
int ThreadPool<T>::getBusyNum()
{
    pthread_mutex_lock(&mutexPool);
    int busyNum = this->busyNum;
    pthread_mutex_unlock(&mutexPool);
    return busyNum;
}

template <typename T>
int ThreadPool<T>::getAliveNum()
{
    pthread_mutex_lock(&mutexPool);
    int aliveNum = this->liveNum;
    pthread_mutex_unlock(&mutexPool);
    return aliveNum;
}

template <typename T>
void *ThreadPool<T>::worker(void *arg)
{
    //! 转换，方便静态函数worker对非静态数据操作
    ThreadPool *pool = static_cast<ThreadPool *>(arg);

    while (true)
    {
        pthread_mutex_lock(&pool->mutexPool);
        // 当前任务队列是否为空,若为空则消费者阻塞在这
        while (pool->taskQ->taskNuber() == 0 && !pool->shutdown)
        {
            //，若任务队列为空则阻塞工作进程
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            //! 若让管理者manager唤醒，则将进行线程销毁，见 239 行
            // 判断是否销毁线程
            if (pool->exitNum > 0)
            {
                pool->exitNum--; //! 应该在 下个 if 语句内
                if (pool->liveNum > pool->minNum)
                {
                    // pool->exitNum--;
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    pool->threadExit();
                }
            }
        }

        // 若线程池被关闭，线程退出
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            pool->threadExit();
        }

        // 若以上都未发生，消费者开始做正事：从任务队列中取出一个任务
        Task<T> task = pool->taskQ->takeTask();

        // 解锁
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexPool);

        cout << "thread " << to_string(pthread_self()) << " start working" << endl;

        // (* task.function)(task.arg);
        task.function(task.arg);
        delete task.arg;
        task.arg = nullptr;

        cout << "thread " << to_string(pthread_self()) << " end working" << endl;
        pthread_mutex_lock(&pool->mutexPool);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexPool);
    }
    return nullptr;
}

template <typename T>
void *ThreadPool<T>::manager(void *arg)
{
    ThreadPool *pool = static_cast<ThreadPool *>(arg);
    while (!pool->shutdown)
    {
        //每隔 3s 检测一次  <unistd.h>
        sleep(3);

        // 取出查看线程池中的线程数量相关
        pthread_mutex_lock(&pool->mutexPool);
        int taskNumber = pool->taskQ->taskNuber(); // 任务数量
        int liveNum = pool->liveNum;               // 线程存活数量
        int busyNum = pool->busyNum;               // 线程繁忙数量
        pthread_mutex_unlock(&pool->mutexPool);

        // 添加线程
        //! 任务个数 > 存活线程数 && 存活线程数 < 最大线程数
        if (taskNumber > liveNum && liveNum < pool->maxNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for (int i = 0; i < pool->maxNum &&
                            counter < NUMBER &&
                            pool->liveNum < pool->maxNum;
                 i++)
            {
                if (pool->threadIDs[i] = 0)
                {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        // 销毁线程
        //! 忙的线程*2 < 存活的线程数 && 存活的线程 > 最小线程数
        if (busyNum * 2 < liveNum && liveNum > pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);

            // 让工作的线程自杀
            for (int i = 0; i < NUMBER; i++)
            {
                pthread_cond_signal(&pool->notEmpty);
                //! 销毁见 146 行
            }
        }
    }
}

template <typename T>
void ThreadPool<T>::threadExit()
{
    pthread_t tid = pthread_self();
    for (int i = 0; i < maxNum; ++i)
    {
        if (threadIDs[i] == tid)
        {
            threadIDs[i] = 0;
            cout << "threadExit() called " << to_string(tid) << " exiting" << endl;
            break;
        }
    }

    // pthread_exit(nullptr);  //!错误，这是C函数
    pthread_exit(NULL);
}