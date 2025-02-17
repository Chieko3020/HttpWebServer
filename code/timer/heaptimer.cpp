#include "heaptimer.h"

void Heap_Timer::Swap_Node(size_t i, size_t j) 
{
    assert(i >= 0 && i <heap.size());
    assert(j >= 0 && j <heap.size());
    swap(heap[i], heap[j]);
    //更新索引键值对
    ref[heap[i].id] = i;
    ref[heap[j].id] = j;    
}

void Heap_Timer::sift_up(size_t i) 
{
    //如果父节点比自身大 交换上浮
    //再次更新父节点并重复比较
    assert(i >= 0 && i < heap.size());
    size_t parent = (i-1) / 2;
    while(parent >= 0) 
    {
        if(heap[parent] > heap[i]) 
        {
            Swap_Node(i, parent);
            i = parent;
            parent = (i-1)/2;
        } 
        else 
            break;
    }
}


bool Heap_Timer::sift_down(size_t i, size_t n) 
{
    //如果右孩子更小 下沉到右孩子那个序列
    //否则用左孩子那个序列
    //如果孩子节点比自身小 交换下沉
    //再次更新左孩子节点并重复比较
    assert(i >= 0 && i < heap.size());
    assert(n >= 0 && n <= heap.size());    // n:共几个结点
    auto index = i;
    auto child = 2*index+1;
    while(child < n) 
    {
        if(child+1 < n && heap[child+1] < heap[child])
            child++;
        if(heap[child] < heap[index]) 
        {
            Swap_Node(index, child);
            index = child;
            child = 2*child+1;
        }
        else
            break;
    }
    return index > i;
}

// 删除指定位置的结点
void Heap_Timer::del(size_t index) 
{
    assert(index >= 0 && index < heap.size());
    // 将要删除的结点换到队尾 然后调整堆
    size_t tmp = index;
    size_t n = heap.size() - 1;
    assert(tmp <= n);//如果就在队尾则不必移动
    if(index < heap.size()-1) 
    {
        Swap_Node(tmp, heap.size()-1);
        if(!sift_down(tmp, n))
        //先进行下移操作再进行上移操作
            sift_up(tmp);
    }
    ref.erase(heap.back().id);
    heap.pop_back();
}

// 调整指定id的结点
void Heap_Timer::adjust(int id, int new_Expires) 
{
    assert(!heap.empty() && ref.count(id));
    //更新时间
    heap[ref[id]].expires = Clock::now() + MS(new_Expires);
    sift_down(ref[id], heap.size());
}

void Heap_Timer::add(int id, int time_Out, const Timeout_Call_Back& cb) 
{
    assert(id >= 0);
    //如果有 直接更新
    if(ref.count(id)) 
    {
        int tmp = ref[id];
        heap[tmp].expires = Clock::now() + MS(time_Out);
        heap[tmp].cb = cb;
        if(!sift_down(tmp, heap.size())) 
            sift_up(tmp);
    } 
    else 
    //如果ref哈希表中不存在id对应的定时器 添加一个新的
    {
        size_t n = heap.size();
        ref[id] = n;
        //堆的大小在插入前正好等于新元素的索引位置
        heap.push_back({id, Clock::now() + MS(time_Out), cb});
        sift_up(n);
        //从新节点的位置向上调整堆
    }
}

// 删除指定id，并触发回调函数
void Heap_Timer::doWork(int id) 
{
    if(heap.empty() || ref.count(id) == 0)
        return;
    size_t i = ref[id];
    auto node = heap[i];
    node.cb();  // 触发回调函数
    del(i);
}

void Heap_Timer::tick() 
{
    // 清除超时结点 
    if(heap.empty())
        return;
    while(!heap.empty()) 
    {
        Timer_Node node = heap.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0)  
            break; 
        node.cb();
        pop();
    }
}

void Heap_Timer::pop() 
{
    assert(!heap.empty());
    del(0);
}

void Heap_Timer::clear() 
{
    ref.clear();
    heap.clear();
}
//获得时间间隔 获取下一个计时器的剩余时间 同时也是最小的那个计时器
int Heap_Timer::Get_Next_Tick() 
{
    tick();
    size_t res = -1;
    if(!heap.empty()) 
    {
        res = std::chrono::duration_cast<MS>(heap.front().expires - Clock::now()).count();
        if(res < 0) 
            res = 0;
    }
    return res;
}
