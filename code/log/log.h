#ifndef LOG_H
#define LOG_H
#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>//不定参数
#include <assert.h>
#include <sys/stat.h>
#include "blockqueue.h"
#include "../buffer/buffer.h"
class Log
{
public:
    //日志等级(debug warn info erro) 路径 后缀 日志队列容量
    void init(int level, const char* path="./log", const char* suffix=".log",int max_Queue_Capacity=1024);
    static Log* Instance();//返回单例实例指针
    static void Flush_Log_Thread();//异步IO调用Async_Write()写日志
    void write(int level, const char* format,...);//不定参数
    void flush();//清空输出缓冲区
    int Get_Level();//获取日志等级
    void Set_Level(int level);//设置日志等级
    bool Is_Open()//检查日志是否打开
    {
        return is_open;
    }
private:
    static const int LOG_PATH_LEN=256;
    static const int LOG_NAME_LEN=256;
    static const int MAX_LINES=50000;

    const char* log_path;//路径
    const char* log_suffix;//后缀名
    int log_MAX_LINES;//最大行数
    int log_Line_Count;//当前行数
    int today;//日期 按日期区分日志
    bool is_open;
    Buffer buff;//输入缓冲区
    int log_level;//日志等级
    bool is_async;//异步IO启用 需要队列

    FILE *fp;//打开log的文件指针
    std::unique_ptr<Block_Queue<std::string>> deque;//阻塞队列
    std::unique_ptr<std::thread> write_Thread; //写线程的指针
    std::mutex mtx;

    Log();
    virtual ~Log();
    void Append_Log_Level_Title(int level);
    void Async_Write();
};

//优先将级别更高的日志写入
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->Is_Open() && log->Get_Level() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

//四个宏定义 主要用于不同类型的日志输出 也是外部使用日志的接口
//...表示可变参数，__VA_ARGS__就是将...的值复制到这里
//前面加上##的作用是当可变参数的个数为0时 这里的##可以把把前面多余的","去掉,否则会编译出错。
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);    
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H
