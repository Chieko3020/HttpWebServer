#include "httpconn.h"
using namespace std;
bool Http_Conn::is_ET;
const char* Http_Conn::conn_src_Dir;
std::atomic<int> Http_Conn::conn_user_Count;

Http_Conn::Http_Conn()
{
    conn_fd=-1;
    bzero(&conn_addr, sizeof(conn_addr));
    is_Close=true;
}

Http_Conn::~Http_Conn()
{
    Close();
}

void Http_Conn::init(int fd, const sockaddr_in &addr)
{
    assert(fd>0);
    conn_addr=addr;
    conn_user_Count++;
    conn_fd=fd;
    write_Buff.Retrieve_All();
    read_Buff.Retrieve_All();
    is_Close=false;
    LOG_INFO("Client[%d](%s:%d) in, user_Count:%d",conn_fd,Get_IP(),Get_Port(),(int)conn_user_Count);
}

void Http_Conn::Close()
{
    response.Un_map_File();
    if(is_Close==false)
    {
        is_Close=true;
        conn_user_Count--;
        close(conn_fd);
        LOG_INFO("Client[%d](%s:%d) quit, User_Count:%d", conn_fd, Get_IP(), Get_Port(), (int)conn_user_Count);
    }
}

int Http_Conn::Get_Fd() const 
{
    return conn_fd;
};

struct sockaddr_in Http_Conn::Get_Addr() const 
{
    return conn_addr;
}

const char* Http_Conn::Get_IP() const 
{
    return inet_ntoa(conn_addr.sin_addr);
}

int Http_Conn::Get_Port() const 
{
    return conn_addr.sin_port;
}

ssize_t Http_Conn::read(int *saveErrno)
{
    ssize_t len=-1;
    do
    {
        len=read_Buff.Read_Fd(conn_fd,saveErrno);
        if(len<=0)
            break;
    }
    while(is_ET);//未启用ET只取出一次
    return len;
}

ssize_t Http_Conn::write(int *saveErrno)
{
    ssize_t len=-1;
    do
    {
        len=writev(conn_fd,iov,iov_Cnt);
        if(len<=0)
        {
            *saveErrno=errno;
            break;
        }
        //process事先将响应报文基址保存在iov[0] 文件则在iov[1]
        //所有数据已经写入 跳出循环
        if(iov[0].iov_len+iov[1].iov_len==0)
            break;
        //如果还有数据且已经写入的字节数超出第一块缓冲区可写数据长度
        //说明第一缓冲区全部写入 write_buff读取完毕 需要置零
        //说明第二块缓冲区部分写入 需要更新防止重复写入
        //现基址为原基址加上 已写入长度减去第一缓冲区可写长度
        //现可写长度为原长度减去 已写入长度减去第一缓冲区可写长度
        else if(static_cast<size_t>(len) > iov[0].iov_len)
        {
            iov[1].iov_base=(uint8_t*)iov[1].iov_base+(len-iov[0].iov_len);
            iov[1].iov_len-=(len-iov[0].iov_len);
            if(iov[0].iov_len)//第一缓冲区可写长度置零 防止下一次重复写入报文
            {
                write_Buff.Retrieve_All();
                iov[0].iov_len=0;
            }
        }
        //如果还有数据且已经写入的字节数 没有超出第一块缓冲区可写数据长度
        //说明第一缓冲区部分写入
        //更新基址和可写长度
        else
        {
            iov[0].iov_base=(uint8_t*)iov[0].iov_base+len;
            iov[0].iov_len-=len;
            write_Buff.Retrieve(len);
        }
    }
    while(is_ET||To_Write_Bytes()>10240);//ET启用或者待写字节数超过10240则继续写入
    return len;
}

bool Http_Conn::process()
{
    //如果不可读 返回false 后续应该从fd写入请求 EPOLLIN
    //如果写入响应完毕 返回true 后续应该读出到fd EPOLLOUT
    request.Init();
    if(read_Buff.Readable_Bytes()<=0)
        return false;
    else if(request.parse(read_Buff))
    {
        LOG_DEBUG("%s",request.Get_path().c_str());
        response.Init(conn_src_Dir,request.Get_path(),request.is_keep_alive(),200);
    }
    else
        response.Init(conn_src_Dir,request.Get_path(),false,400);
    response.Make_Response(write_Buff);
    //将返回的const char*转换为char*类型以便将其存储在iov_base中
    iov[0].iov_base=const_cast<char*>(write_Buff.Peek());
    iov[0].iov_len=write_Buff.Readable_Bytes();
    iov_Cnt=1;
    //如果文件存在 将文件指针存储在iov[1].iov_base中
    //将文件长度存储在 iov[1].iov_len 中 
    //并将 iovec 结构体的计数 iovCnt 设置为 2
    if(response.File_Len()>0&&response.File())
    {
        iov[1].iov_base=response.File();
        iov[1].iov_len=response.File_Len();
        iov_Cnt=2;
    }
    //如果有文件需要写入 iov_Cnt为2 待写字节为 write_Buff + File
    //它们分别先后存储在iov[0]和iov[1]
    //记录文件大小 iovec 结构体的计数 待写入字节数
    LOG_DEBUG("filesize:%d, %d to %d",response.File_Len(),iov_Cnt,To_Write_Bytes())
    return true;
}   