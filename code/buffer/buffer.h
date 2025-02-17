#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <atomic>
#include <assert.h>

class Buffer 
{
public:
    Buffer(int init_Buff_Size = 1024);
    ~Buffer();

    size_t Writable_Bytes() const;  //获取可写的字节数
    size_t Readable_Bytes() const;  //获取可读的字节数
    size_t Prependable_Bytes() const;   //获取可预留的字节数

    void Has_Written(size_t len);    //更新写操作指针

    const char* Peek() const;   //返回读指针 在buffer中的下标
    void Ensure_Writable(size_t len);    //   确保有可写的字节

    void Retrieve(size_t len);  //更新读操作指针
    void Retrieve_Until(const char* end);   //读取直到end所在位置
    void Retrieve_All();    //清零buffer和读写下标 用于下面这个函数
    std::string Retrieve_All_To_Str();  //读取剩余可读的字节 返回字符串

    const char* Begin_Write_Const() const;  //返回写指针
    char* Begin_Write();    //返回写指针 在buffer中的下标

    //写入buffer
    void Append(const std::string& str);    //写入str字符串
    void Append(const char* str, size_t len);  //写入从str指向位置开始len个 
    void Append(const void* data, size_t len);
    void Append(const Buffer &buff);    //将buff中可读数据覆盖到写指针位置

    //与客户端的读写接口
    ssize_t Read_Fd(int fd, int *Errno);    //读取fd数据写到buffer
    ssize_t Write_Fd(int fd, int *Errno);   //向fd写数据

private:
    char* Begin_Ptr();     //buffer首地址
    const char* Begin_Ptr() const;
    void Make_Space(size_t len);    //扩展buffer存储空间

    //atomic 原子类型
    std::vector<char> buffer;   //buffer存储空间
    std::atomic<std::size_t> read_Pos;      //读指针
    std::atomic<std::size_t> write_Pos;     //写指针
};

#endif