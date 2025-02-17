#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <assert.h>

class Thread_Pool
{
public:
    Thread_Pool()=default;
    Thread_Pool(Thread_Pool&&)=default;//移动
    //创建线程池 分离线程使得每个线程独立运作
    //每个线程接收一个匿名函数并不断在执行该函数和等待两个状态切换 直到线程池销毁
    //使用 std::make_shared 初始化 pool 成员变量，避免内存碎片化。
    explicit Thread_Pool(int thread_count=8):pool(std::make_shared<Pool>())
    {
        assert(thread_count>0);
        for(int i=0;i<thread_count;i++)
        {
            std::thread([this]()
            {
                //每个线程在创建后会首先获取锁 确保访问任务队列时是线程安全的
                std::unique_lock<std::mutex> locker(pool->mtx);
                while(1)
                {
                    if(!pool->tasks.empty())
                    {
                        //右值引用 将队头任务的资源转移到std::funtion<void()> task
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        //线程从任务队列中取出任务并立即释放锁 避免阻塞其他线程访问任务队列
                        locker.unlock();
                        //执行任务
                        task();//operator()(Args...) 调用std::funtion<void()>存储的可调用对象 传递参数并返回结果
                        //任务执行完毕后线程重新获取锁 确保在检查和操作任务队列时是线程安全的
                        locker.lock();
                        //如果任务队列为空且线程池未关闭 
                        //线程进入等待状态 此时会自动释放锁
                        //当有新任务到来并调用 notify_one() 或 notify_all() 时
                        //等待的线程将被唤醒 继续执行匿名函数
                    }
                    else if(pool->is_closed)
                        break;
                    else
                        pool->condition.wait(locker);
                }

            }).detach();//分离线程 主线程不必等待该子线程结束
        }
    }

    ~Thread_Pool()
    {
        if(pool)
        {
            std::unique_lock<std::mutex> locker(pool->mtx);
            pool->is_closed=true;
        }
        pool->condition.notify_all();
    }

    template<typename T> void AddTask(T&& task)
    {
        std::unique_lock<std::mutex> locker(pool->mtx);
        pool->tasks.emplace(std::forward<T>(task));//向队列插入新任务task 用T&& task来构造
        pool->condition.notify_one();//唤醒一个线程
    }

private:
    struct Pool //线程池
    {
        std::mutex mtx;
        std::condition_variable condition;
        bool is_closed;
        //用queue封装的std::funtion<void()>任务队列
        //能够存储任何可调用对象 包括函数指针 lambda表达式 绑定器和其他函数对象 
        //std::function<void()> 表示一个没有参数且没有返回值的可调用对象
        //它将任务统一表示为 std::function<void()>使不同类型的任务能够存储在同一个队列中
        std::queue<std::function<void()>> tasks;
    };

    std::shared_ptr<Pool> pool;

};

#endif