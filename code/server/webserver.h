#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../timer/heaptimer.h"
#include "epoller.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../http/httpconn.h"


class Web_Server 
{
public:
    Web_Server
    //端口 触发模式[ET|LT] 超时时间 优雅关闭
    //mysql---端口 用户 密码 数据库名称
    //连接池 线程池 是否启用日志 日志等级 日志队列长度
    (
        int port, int trig_Mode, int timeout_MS, bool Opt_Linger, 
        int sql_Port, const char* sql_User, const  char* sql_Pswd, 
        const char* db_Name, int conn_Pool_Num, int thread_Num,
        bool open_Log, int log_Level, int log_Que_Size
    );

    ~Web_Server();
    void Start();

private:
    bool Init_Socket(); 
    void Init_Event_Mode(int trigMode);
    void Add_Client(int fd, sockaddr_in addr);
  
    void Deal_Listen();
    void Deal_Write(Http_Conn* client);
    void Deal_Read(Http_Conn* client);

    void Send_Error(int fd, const char* info);
    void Extent_Time(Http_Conn* client);//更新超时计时器
    void Close_Conn(Http_Conn* client);

    void On_Read(Http_Conn* client);
    void On_Write(Http_Conn* client);
    void On_Process(Http_Conn* client);

    static const int MAX_FD = 65536;

    static int Set_Fd_Non_block(int fd);//设置非阻塞
    int port;
    bool open_Linger;
    int time_out_MS;  //全局初始超时时间 毫秒
    bool is_Close;
    int listen_Fd;
    char* src_Dir;
    
    uint32_t listen_Event;  //监听事件
    uint32_t conn_Event;    //连接事件
   
    std::unique_ptr<Heap_Timer> timer;
    std::unique_ptr<Thread_Pool> thread_pool;
    std::unique_ptr<Epoller> epoller;
    std::unordered_map<int, Http_Conn> users;
};

#endif //WEBSERVER_H
