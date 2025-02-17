#include "webserver.h"
using namespace std;
Web_Server::Web_Server
(
    int port, int trig_Mode, int timeout_MS, bool Opt_Linger, 
    int sql_Port, const char* sql_User, const  char* sql_Pswd, 
    const char* db_Name, int conn_Pool_Num, int thread_Num,
    bool open_Log, int log_Level, int log_Que_Size
):port(port),open_Linger(Opt_Linger),time_out_MS(timeout_MS),is_Close(false),
timer(new Heap_Timer()),thread_pool(new Thread_Pool(thread_Num)),epoller(new Epoller)
{
    src_Dir=getcwd(nullptr,256);
    //将当前工作目录的绝对路径复制到参数buffer所指的内存空间中
    //如果传入nullptr 函数会自动分配一个足够大的缓冲区来存储路径
    assert(src_Dir);
    strcat(src_Dir,"/resources/");
    Http_Conn::conn_user_Count=0;
    Http_Conn::conn_src_Dir=src_Dir;

    Sql_Conn_Pool::Instance()->Init("localhost",sql_Port,sql_User,sql_Pswd,db_Name,conn_Pool_Num);
    //单例模式下 初始化MySQL连接池
    Init_Event_Mode(trig_Mode);
    //设置服务器和客户端的事件的触发模式 ET||LT
    if(!Init_Socket())
        is_Close=true;
    if(open_Log)
    {
        Log::Instance()->init(log_Level,"./log",".log",log_Que_Size);
        if(is_Close)
        {
            LOG_ERROR("---Server init error---");
        }
        else
        {
            LOG_INFO("---Server init---");
            LOG_INFO("PORT:%d, Open_Linger:%s",port,Opt_Linger ? "true":"false");
            LOG_INFO("Listen Mode: %s, Open_Conn Mode: %s",(listen_Event&EPOLLET?"ET":"LT"),(conn_Event&EPOLLET?"ET":"LT"));
            LOG_INFO("LOG_LEVEL: %d",log_Level);
            LOG_INFO("src_dir: %s",Http_Conn::conn_src_Dir);
            LOG_INFO("Sql_Conn_Pool_NUM: %d,Thread_Pool_NUM: %d",conn_Pool_Num,thread_Num);
        }
    }
}

Web_Server::~Web_Server()
{
    close(listen_Fd);
    is_Close=true;
    free(src_Dir);
    //释放用于存储资源路径的空间
    Sql_Conn_Pool::Instance()->Close_Pool();
}

void Web_Server::Init_Event_Mode(int trig_Mode)
{
    listen_Event=EPOLLRDHUP;
    //对端关闭连接(被动)
    //或者套接字处于半关闭状态(主动)
    //这个事件会被触发
    conn_Event=EPOLLONESHOT|EPOLLRDHUP;
    //确保一个文件描述符在同一时间仅被一个线程处理
    //确保及时检测到对端关闭连接 便于清理连接和释放资源
    switch (trig_Mode) 
    {
        case 0:
        //LT 事件发生后持续通知直到处理完毕
        //适用于逐步处理数据的情况
            break;
        case 1:
        //监听事件LT 连接事件为ET 事件发生时仅通知一次
        //适用于高并发环境下 连接事件的高效处理
            conn_Event |= EPOLLET;
            break;
        case 2:
        //监听事件为ET 连接事件LT 事件发生时仅通知一次
        //用于高并发环境下 监听事件的高效处理
            listen_Event |= EPOLLET;
            break;
        case 3:
        //监听事件连接事件ET 事件发生时仅通知一次
            listen_Event |= EPOLLET;
            conn_Event |= EPOLLET;
            break;
        default:
            listen_Event |= EPOLLET;
            conn_Event |= EPOLLET;
            break;
    }
    Http_Conn::is_ET=(conn_Event&EPOLLET);

}

void Web_Server::Start()
{
    int time_MS=-1;
    if(!is_Close)
        LOG_INFO("---Server Start---");
    while(!is_Close)
    {
        if(time_out_MS>0)
            time_MS=timer->Get_Next_Tick();
        //获取下一个即将到期的计时器的超时时间
        //作为epoll_wait动态的等待时间
        int event_count=epoller->wait(time_MS);
        for(int i=0;i<event_count;i++)
        {
            int fd=epoller->Get_Event_Fd(i);
            uint32_t events=epoller->Get_Event(i);
            //监听描述符
            if(fd==listen_Fd)
                Deal_Listen();
            //对端已经关闭
            else if(events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR))
            {
                //如果存在该连接
                assert(users.count(fd)>0);
                Close_Conn(&users[fd]);
            }
            //对端写本端读
            else if(events&EPOLLIN)
            {
                assert(users.count(fd)>0);
                Deal_Read(&users[fd]);
            }
            //对端读本地写
            else if(events&EPOLLOUT)
            {
                assert(users.count(fd)>0);
                Deal_Write(&users[fd]);
            }
            else
                LOG_ERROR("Unexpected event");
        }
    }
}

void Web_Server::Send_Error(int fd, const char* info)
{
    assert(fd>0);
    int ret=send(fd,info,strlen(info),0);
    if(ret<0)
        LOG_WARN("send error to client[%d] error",fd);
    close(fd);
}

void Web_Server::Close_Conn(Http_Conn* client)
{
    assert(client);
    LOG_INFO("Client[%d] quit",client->Get_Fd());
    epoller->Del_fd(client->Get_Fd());
    client->Close();
}

void Web_Server::Add_Client(int fd,sockaddr_in addr)
{
    assert(fd>0);
    users[fd].init(fd,addr);
    if(time_out_MS>0)//如果设置了初始超时时间
        //std::function<void()>
        //Close_Conn是超时计时器的回调函数
        timer->add(fd,time_out_MS,std::bind(&Web_Server::Close_Conn,this,&users[fd]));
    epoller->Add_Fd(fd,EPOLLIN|conn_Event);
    Set_Fd_Non_block(fd);
    LOG_INFO("Client[%d] in",users[fd].Get_Fd());
}

void Web_Server::Deal_Listen()
{
    struct sockaddr_in addr;
    socklen_t len=sizeof(addr);
    do
    {
        int fd=accept(listen_Fd,(struct sockaddr*)&addr,&len);
        if(fd<=0)
            return;
        else if(Http_Conn::conn_user_Count>=MAX_FD)
        {
            Send_Error(fd,"Server busy");
            LOG_WARN("CLient is full");
            return;
        }
        else
            Add_Client(fd,addr);
    }
    while(listen_Event&EPOLLET);
}

void Web_Server::Deal_Read(Http_Conn *client)
{
    assert(client);
    Extent_Time(client);//仍然处于活动 重置超时倒计时
    //<std::function<void()>> tasks函数对象
    //这里的临时对象是右值 void AddTask(T&& task)
    thread_pool->AddTask(std::bind(&Web_Server::On_Read,this,client));
}

void Web_Server::Deal_Write(Http_Conn *client)
{
    assert(client);
    Extent_Time(client);
    thread_pool->AddTask(std::bind(&Web_Server::On_Write, this, client));
}

void Web_Server::Extent_Time(Http_Conn *client)
{
    assert(client);
    if(time_out_MS>0)
        timer->adjust(client->Get_Fd(),time_out_MS);
}

void Web_Server::On_Read(Http_Conn *client)
{
    //当有数据可读时被调用 从fd中读取数据
    assert(client);
    int ret=-1;
    int read_err=0;
    ret=client->read(&read_err);//从fd读入缓冲区
    if(ret<=0&&read_err!=EAGAIN)
    {
        Close_Conn(client);
        return;
    }
    else//从缓冲区读出 解析并响应
        On_Process(client);
}

void Web_Server::On_Process(Http_Conn *client)
{
    //根据解析结果决定后续读写
    //如果缓冲区内有可读数据 应该写入fd
    if(client->process())
        epoller->Modify_Fd(client->Get_Fd(),conn_Event|EPOLLOUT);
    else
    //否则 应该从fd读出到缓冲区
        epoller->Modify_Fd(client->Get_Fd(),conn_Event|EPOLLIN);
}

void Web_Server::On_Write(Http_Conn *client)
{
    //当fd可写时被调用 向文件描述符写入数据
    //如果数据都已写出且保持连接选项为真 重新监听读事件
    //否则 如果写入失败且错误是资源暂时不可用
    //则继续监听写事件
    //如果写入失败且错误不是EAGAIN 关闭连接
    assert(client);
    int ret=-1;
    int write_err=0;
    ret=client->write(&write_err);
    if(client->To_Write_Bytes()==0)
    {
        if(client->Is_Keep_Alive())
        {
            epoller->Modify_Fd(client->Get_Fd(),conn_Event|EPOLLIN);
            return;
        }
    }
    else if(ret<0)
    {
        if(write_err==EAGAIN)
        {
            epoller->Modify_Fd(client->Get_Fd(),conn_Event|EPOLLOUT);
            return;
        }
    }
    Close_Conn(client);    
}

bool Web_Server::Init_Socket()
{
    int ret;
    struct sockaddr_in addr;
    if(port>65535||port<1024)
    {
        LOG_ERROR("PORT:%d error",port);
        return false;
    }
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(port);
{
    struct linger Opt_Linger={0};
    if(open_Linger)
    {
        Opt_Linger.l_onoff=1;
        Opt_Linger.l_linger=1;
    }
    listen_Fd=socket(AF_INET,SOCK_STREAM,0);
    if(listen_Fd<0)
    {
        LOG_ERROR("Create socket error",port);
        return false;
    }
    ret = setsockopt(listen_Fd, SOL_SOCKET, SO_LINGER, &Opt_Linger, sizeof(Opt_Linger));
    if(ret < 0) 
    {
        close(listen_Fd);
        LOG_ERROR("Init linger error", port);
        return false;
    }
}
    int optval = 1;
    //端口复用
    ret = setsockopt(listen_Fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) 
    {
        LOG_ERROR("set socket setsockopt error");
        close(listen_Fd);
        return false;
    }
    ret = bind(listen_Fd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) 
    {
        LOG_ERROR("Bind Port:%d error", port);
        close(listen_Fd);
        return false;
    }
    ret = listen(listen_Fd, 6);
    if(ret < 0) 
    {
        LOG_ERROR("Listen port:%d error", port);
        close(listen_Fd);
        return false;
    }
    ret = epoller->Add_Fd(listen_Fd,  listen_Event | EPOLLIN);
    //将lfd加入epoller
    if(ret == 0) 
    {
        LOG_ERROR("Add listen error");
        close(listen_Fd);
        return false;
    }
    Set_Fd_Non_block(listen_Fd);   
    LOG_INFO("Server port:%d", port);
    return true;
}

int Web_Server::Set_Fd_Non_block(int fd) 
{
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}