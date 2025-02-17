#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H
#include <deque>
#include <condition_variable>
#include <mutex>
#include <sys/time.h>
#include <assert.h>
using namespace std;

template <typename T>
class Block_Queue
{
public:
    explicit Block_Queue(size_t maxsize=1000);
    ~Block_Queue();
    bool empty();//检查空队列
    bool full();//检查满队列
    void push_back(const T &item);//尾插
    void push_front(const T &item);//头插
    bool pop(T &item);//出队(队头)
    bool pop(T &item, int timeout);
    void clear();//清空
    T front();//返回队头
    T back();//返回队尾
    size_t capacity();//返回队列容量
    size_t size();//返回队列大小
    void flush();//唤醒线程处理队列写日志
    void Close();//禁用队列
private:
    deque <T> deq;
    mutex mtx;      //锁
    bool is_Close;//禁用状态
    size_t Capacity;    //队列容量
    condition_variable Consumer;    //消费者条件变量
    condition_variable Producer;    //生产者条件变量

};

template<typename T> Block_Queue<T>::Block_Queue(size_t maxsize):Capacity(maxsize)
{
    assert(maxsize>0);
    is_Close=false;
}

template<typename T> Block_Queue<T>::~Block_Queue()
{
    Close();
}

template<typename T> void Block_Queue<T>::Close()
{
    clear();
    is_Close=true;
    //确保所有线程都能被及时通知并终止操作 避免死锁或长时间等待
    Consumer.notify_all();
    Producer.notify_all();
}

template<typename T> void Block_Queue<T>::clear()
{
    lock_guard<mutex> locker(mtx); //RAII
    deq.clear();
}

template<typename T> bool Block_Queue<T>::empty()
{
    lock_guard<mutex> locker(mtx);
    return deq.empty();
}

template<typename T> bool Block_Queue<T>::full()
{
    lock_guard<mutex> locker(mtx);
    return deq.size() >= Capacity;
}

template<typename T> void Block_Queue<T>::push_back(const T &item)
{
    unique_lock<mutex> locker(mtx);//条件变量使用的锁
    while(deq.size() >= Capacity)//队列满时阻塞
        Producer.wait(locker);
    deq.push_back(item);
    Consumer.notify_one();//唤醒一个线程来处理队列写日志
}

template<typename T> void Block_Queue<T>::push_front(const T &item)
{
    unique_lock<mutex> locker(mtx);
    while(deq.size()>=Capacity)
        Producer.wait(locker);
    deq.push_front(item);
    Consumer.notify_one();
}

template<typename T> bool Block_Queue<T>::pop(T &item)
{
    unique_lock<mutex> locker(mtx);
    while(deq.empty())//队空阻塞
        Consumer.wait(locker);
    item=deq.front();//取出队头
    deq.pop_front();
    Producer.notify_one();//唤醒一个线程来向队列增加日志
    return true;
}

template<typename T> bool Block_Queue<T>::pop(T &item, int timeout)
{
    unique_lock<mutex> locker(mtx);
    while(deq.empty())
    {
        //在当前线程收到通知或者超时之前该线程都会处于阻塞状态
        if(Consumer.wait_for(locker,std::chrono::seconds(timeout))==std::cv_status::timeout)
            return false;
        if(is_Close)
            return false;
    }
    item=deq.front();
    deq.pop_front();
    Producer.notify_one();
    return true;
}

template<typename T> T Block_Queue<T>::front()
{
    lock_guard<mutex> locker(mtx);
    return deq.front();
}

template<typename T> T Block_Queue<T>::back() 
{
    lock_guard<mutex> locker(mtx);
    return deq.back();
}

template<typename T> size_t Block_Queue<T>::capacity() 
{
    lock_guard<mutex> locker(mtx);
    return Capacity;
}

template<typename T> size_t Block_Queue<T>::size() 
{
    lock_guard<mutex> locker(mtx);
    return deq.size();
}


template<typename T> void Block_Queue<T>::flush() 
{
    Consumer.notify_one();
}
# endif