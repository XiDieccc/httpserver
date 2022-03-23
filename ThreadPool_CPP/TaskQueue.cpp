#include "TaskQueue.h"
template <typename T>
TaskQueue<T>::TaskQueue()
{
    pthread_mutex_init(&m_mutex, NULL);
}

template <typename T>
TaskQueue<T>::~TaskQueue()
{
    pthread_mutex_destroy(&m_mutex);
}

template <typename T>
void TaskQueue<T>::addTask(Task<T> task)
{
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(task);
    pthread_mutex_unlock(&m_mutex);
}

//重载
template <typename T>
void TaskQueue<T>::addTask(callback func, void *arg)
{
    pthread_mutex_lock(&m_mutex);
    m_taskQ.push(Task<T>(func, arg));
    pthread_mutex_unlock(&m_mutex);
}

template <typename T>
Task<T> TaskQueue<T>::takeTask()
{
    Task<T> ret;
    pthread_mutex_lock(&m_mutex);
    if (!m_taskQ.empty())
    {
        ret = m_taskQ.front();
        m_taskQ.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return ret;
}