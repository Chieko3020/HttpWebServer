#include "sqlconnpool.h"

Sql_Conn_Pool* Sql_Conn_Pool::Instance() 
{
    static Sql_Conn_Pool pool;
    return &pool;
}

//初始化
void Sql_Conn_Pool::Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int conn_Size = 10) 
              {
    assert(conn_Size > 0);
    int conn_create_count=0;//实际创建成功的连接数
    for(int i = 0; i < conn_Size; i++) 
    {
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);//初始化一个MYSQL对象
        if(!conn) 
        {
            LOG_ERROR("MySql init error!");
            continue;
        }
        //连接MYSQL
        conn = mysql_real_connect(conn, host, user, pwd, dbName, port, nullptr, 0);
        if (!conn)
        {
            LOG_ERROR("MySql Connect error!");
            continue;
        }
        conn_Que.emplace(conn);//添加至连接队列
        conn_create_count++;
    }
    MAX_CONN = conn_create_count;
    sem_init(&sem_Id, 0, MAX_CONN);//初始化信号量为最大可用连接数
}

MYSQL* Sql_Conn_Pool::Get_Conn() 
{
    MYSQL* conn = nullptr;
    if(conn_Que.empty()) 
    {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&sem_Id);  //信号量-1
    lock_guard<mutex> locker(mtx);
    conn = conn_Que.front();
    conn_Que.pop();
    return conn;
}

//存入连接池但不关闭 表示增加了一个可用连接
void Sql_Conn_Pool::Free_Conn(MYSQL* conn) 
{
    assert(conn);
    lock_guard<mutex> locker(mtx);
    conn_Que.push(conn);
    sem_post(&sem_Id);  //信号量+1
}

void Sql_Conn_Pool::Close_Pool() 
{
    lock_guard<mutex> locker(mtx);
    while(!conn_Que.empty())
    {
        auto conn = conn_Que.front();
        conn_Que.pop();
        mysql_close(conn);//关闭MYSQL连接
    }
    mysql_library_end();//清理MySQL库环境
}

int Sql_Conn_Pool::Get_Free_Conn_Count() 
{
    lock_guard<mutex> locker(mtx);
    return conn_Que.size();
}
