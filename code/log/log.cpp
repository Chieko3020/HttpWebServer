#include "log.h"

Log::Log()
{
    fp=nullptr;
    deque=nullptr;
    write_Thread=nullptr;
    log_Line_Count=0;
    today=0;
    is_async=false;
}

Log::~Log()
{
    while(!deque->empty())//检查阻塞队列是否位空
        deque->flush();//唤醒线程处理剩余日志
    deque->Close();//关闭队列
    write_Thread->join();//等待线程处理日志
    if(fp)
    {
        lock_guard<mutex> locker(mtx);
        flush();//清空输出缓冲
        fclose(fp);
    }
}

void Log::flush()
{
    if(is_async)//如果是异步还需要唤醒线程处理队列内容写入日志
        deque->flush();
    fflush(fp);//清空输出缓冲 写入文件
}

Log* Log::Instance()
{
    static Log log;
    return &log;
}

void Log::Flush_Log_Thread()
{
    Log::Instance()->Async_Write();
}

void Log::Async_Write()
{
    string str="";
    while(deque->pop(str))
    {
        lock_guard<mutex> locker(mtx);
        fputs(str.c_str(),fp);
    }
}

void Log::init(int level, const char* path, const char* suffix,int max_Queue_Capacity)
{
    is_open=true;
    log_level=level;
    log_path=path;
    log_suffix=suffix;
    if(max_Queue_Capacity)
    {
        is_async=true;
        if(!deque)
        {
            //unique_ptr无法进行复制构造 复制赋值操作
            //使用move将申请的队列和线程给阻塞队列和负责写日志的线程的指针
            unique_ptr<Block_Queue<std::string>> new_deq(new Block_Queue<std::string>);
            deque=move(new_deq);
            unique_ptr<thread> new_thread(new thread(Flush_Log_Thread));
            write_Thread=move(new_thread);
        }
    }
    else
        is_async=false;
    log_Line_Count=0;
    time_t timer=time(nullptr);
    struct tm *sys_time=localtime(&timer);
    char file_name[LOG_NAME_LEN]={0};
    //输出对象 输出长度 路径年月日后缀
    snprintf(file_name,LOG_NAME_LEN-1,"%s/%04d_%02d_%02d%s",log_path,sys_time->tm_year+1900,sys_time->tm_mon+1,sys_time->tm_mday,log_suffix);
    today=sys_time->tm_mday;
    {
        lock_guard<mutex> locker(mtx);
        buff.Retrieve_All();
        if(fp)//关闭当前打开的文件
        {
            flush();//清空输出缓冲
            fclose(fp);
        }
        fp=fopen(file_name,"a");
        if(fp==nullptr)//没有目录 生成目录再打开
        {
            mkdir(file_name,0777);
            fp=fopen(file_name,"a");
        }
        assert(fp!=nullptr);
    }
}

void Log::write(int level,const char* format,...)
{
    struct timeval now={0,0};
    gettimeofday(&now,nullptr);
    time_t tSec=now.tv_sec;
    struct tm *sys_time=localtime(&tSec);
    struct tm t=*sys_time;
    va_list vaList;//不定参数列表
    //日志写入前会判断当前today是否为创建日志的时间 
    //若是则写入日志 否则按当前时间创建新的log文件并更新创建时间和行数。
    //日志写入前会判断行数是否超过最大行限制
    //若是则以 路径 时间 lineCount / MAX_LOG_LINES 为名创建新的log文件。

    if(today!=t.tm_mday||(log_Line_Count&&(log_Line_Count%MAX_LINES==0)))
    {
        unique_lock<mutex> locker(mtx);
        locker.unlock();//文件操作耗时 先解锁稍后再上锁
        char new_File_Name[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (today != t.tm_mday)    // 时间不匹配则替换为最新的日志文件名
        {
            snprintf(new_File_Name, LOG_NAME_LEN - 72, "%s/%s%s", log_path, tail, log_suffix);
            today = t.tm_mday;
            log_Line_Count = 0;
        }
        else
            snprintf(new_File_Name, LOG_NAME_LEN - 72, "%s/%s-%d%s", log_path, tail, (log_Line_Count  / MAX_LINES), log_suffix);
        
        locker.lock();
        flush();
        fclose(fp);
        fp = fopen(new_File_Name, "a");
        assert(fp != nullptr);
    }
    //在buffer内生成一条对应的日志信息
    {
        //日期 时间 不定参数(也就是日志内容)
        unique_lock<mutex> locker(mtx);
        log_Line_Count++;
        int n = snprintf(buff.Begin_Write(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff.Has_Written(n);
        Append_Log_Level_Title(level);    

        va_start(vaList, format);
        int m = vsnprintf(buff.Begin_Write(), buff.Writable_Bytes(), format, vaList);
        va_end(vaList);

        buff.Has_Written(m);
        buff.Append("\n\0", 2);

        if(is_async && deque && !deque->full()) 
            deque->push_back(buff.Retrieve_All_To_Str());
        else     
            fputs(buff.Peek(), fp);
        
        buff.Retrieve_All();//清空buff
    }

}

// 添加日志等级
void Log::Append_Log_Level_Title(int level) 
{
    switch(level) 
    {
    case 0:
        buff.Append("[debug]: ", 9);
        break;
    case 1:
        buff.Append("[info] : ", 9);
        break;
    case 2:
        buff.Append("[warn] : ", 9);
        break;
    case 3:
        buff.Append("[error]: ", 9);
        break;
    default:
        buff.Append("[info] : ", 9);
        break;
    }
}

int Log::Get_Level() 
{
    lock_guard<mutex> locker(mtx);
    return log_level;
}

void Log::Set_Level(int level) 
{
    lock_guard<mutex> locker(mtx);
    log_level = level;
}
