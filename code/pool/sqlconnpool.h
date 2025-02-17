#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class Sql_Conn_Pool 
{
public:
    static Sql_Conn_Pool *Instance();//单例

    MYSQL *Get_Conn();
    void Free_Conn(MYSQL * conn);
    int Get_Free_Conn_Count();

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int conn_Size);
    void Close_Pool();

private:
    Sql_Conn_Pool() = default;
    ~Sql_Conn_Pool() 
    { 
        Close_Pool(); 
    }

    int MAX_CONN;

    std::queue<MYSQL *> conn_Que;//连接队列
    std::mutex mtx;
    sem_t sem_Id;//信号量 控制可用连接数
};


class Sql_Conn_RAII 
{
public:
    Sql_Conn_RAII(MYSQL** sql_pointer, Sql_Conn_Pool *connpool) 
    {
        assert(connpool);
        *sql_pointer = connpool->Get_Conn();//将获取到的数据库连接指针赋值给(*sql_pointer)
        sql = *sql_pointer;//再赋值给sql
        conn_pool = connpool;//将连接池指针赋值
    }
    
    ~Sql_Conn_RAII() 
    {
        if(sql) 
            conn_pool->Free_Conn(sql);
    }
    
private:
    MYSQL* sql;
    Sql_Conn_Pool* conn_pool;
};

#endif // SQLCONNPOOL_H
