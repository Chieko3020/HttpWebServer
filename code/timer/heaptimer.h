#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> Timeout_Call_Back;//回调函数
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point Time_Stamp;//时间戳

struct Timer_Node 
{
    int id;
    Time_Stamp expires;  // 超时时间点
    Timeout_Call_Back cb; // 回调function<void()>
    bool operator<(const Timer_Node& t) 
    {    // 重载比较运算符
        return expires < t.expires;
    }
    bool operator>(const Timer_Node& t) 
    {    // 重载比较运算符
        return expires > t.expires;
    }
};
class Heap_Timer 
{
public:
    Heap_Timer() 
    { 
        heap.reserve(64); 
    }  // 保留（扩充）容量

    ~Heap_Timer() 
    {
         clear(); 
    }
    
    void adjust(int id, int new_Expires);//调整时间
    void add(int id, int time_Out, const Timeout_Call_Back& cb);//添加定时器
    void doWork(int id);//计算超时时间 触发回调函数 删除计时器
    void clear();//清空计时器
    void tick();//清除超时过期的计时器
    void pop();//移除第一个到期的计时器
    int Get_Next_Tick();//获取下一个计时器的剩余时间

private:
    void del(size_t i);//删除计时器
    void sift_up(size_t i);//堆上浮
    bool sift_down(size_t i, size_t n);//堆下沉
    void Swap_Node(size_t i, size_t j);//交换元素

    std::vector<Timer_Node> heap;
    // key:id value:vector的下标
    std::unordered_map<int, size_t> ref;   
    // id对应的在heap中的下标，方便用heap的时候查找
};
#endif //HEAP_TIMER_H
