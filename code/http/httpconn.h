#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

class Http_Conn 
{
public:
    Http_Conn();
    ~Http_Conn();
    
    void init(int sockFd, const sockaddr_in &addr);
    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);
    void Close();
    int Get_Fd() const;
    int Get_Port() const;
    const char* Get_IP() const;
    sockaddr_in Get_Addr() const;
    bool process();

    int To_Write_Bytes()//返回待写入的字节数
    { 
        return iov[0].iov_len + iov[1].iov_len; 
    }

    bool Is_Keep_Alive() const 
    {
        return request.is_keep_alive();
    }

    static bool is_ET;
    static const char* conn_src_Dir;
    static std::atomic<int> conn_user_Count;
    
private:
    int conn_fd;
    struct  sockaddr_in conn_addr;
    bool is_Close;
    int iov_Cnt;
    struct iovec iov[2];//用于读写函数的向量
    Buffer read_Buff;//读缓冲区
    Buffer write_Buff;//写缓冲区
    Http_Request request;
    Http_Response response;
};
#endif
