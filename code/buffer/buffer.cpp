#include "buffer.h"

Buffer::Buffer(int init_Buff_Size): buffer(init_Buff_Size),read_Pos(0),write_Pos(0) {}
Buffer::~Buffer(){}
size_t Buffer::Writable_Bytes() const   //buffer空间减去已写空间
{
    return buffer.size()-write_Pos;
}

size_t Buffer::Readable_Bytes() const   //已写数据减去已读数据
{
    return write_Pos-read_Pos;
}

size_t Buffer::Prependable_Bytes() const    //已读数据可被覆盖
{
    return read_Pos;
}

const char* Buffer::Peek() const    //返回读指针 在buffer中的下标
{   
    return  &buffer[read_Pos];
}

char* Buffer::Begin_Write()     //返回写指针 在buffer中的下标
{
    return &buffer[write_Pos];
}

const char* Buffer::Begin_Write_Const() const   //返回写指针 在buffer中的下标
{
    return &buffer[write_Pos];
}

void Buffer::Ensure_Writable(size_t len)
{
    if(len > Writable_Bytes())//写之前先确认，如果要写超过可写字节数
        Make_Space(len);//那么扩展空间
    assert(len<=Writable_Bytes());//再次检查写操作是否合法
}

void Buffer::Has_Written(size_t len)    //更新写操作指针
{
    write_Pos+=len;
}

void Buffer::Retrieve(size_t len)   //更新读指针
{
    read_Pos+=len;
}

void Buffer::Retrieve_Until(const char* end)    //读到end位置
{
    assert(Peek()<=end);//检查读指针在end位置之前才能向end方向读
    Retrieve(end-Peek());//更新读指针
}

std::string Buffer::Retrieve_All_To_Str()
{
    std::string str(Peek(),Readable_Bytes());//读出所有可读数据
    Retrieve_All();//归零指针 清空buffer
    return str;
}

void Buffer::Retrieve_All()
{
    bzero(&buffer[0],buffer.size());//将所有位置零
    read_Pos=0;
    write_Pos=0;
}

void Buffer::Append(const char* str, size_t len)
{
    assert(str);//检查str是否为空指针
    Ensure_Writable(len);//检查是否有len长度可写，若无则扩展
    std::copy(str,str+len,Begin_Write());//从str开始写入len个到可写位置
    Has_Written(len);//更新写指针
}

void Buffer::Append(const std::string &str)
{
    Append(str.c_str(),str.size());//string转const char*
}

void Buffer::Append(const void* data, size_t len)
{
    Append(static_cast<const char*>(data),len);//强转const char*
}

void Buffer::Append(const Buffer &buff)    //将buff中可读数据覆盖到写指针位置
{
    Append(buff.Peek(),buff.Readable_Bytes());
}

ssize_t Buffer::Read_Fd(int fd, int* Errno) {
    char buff[65535];   // 栈区temp 用于缓存超出buffer空间的数据
    struct iovec iov[2];
    size_t writable = Writable_Bytes(); //buffer可写空间大小
    //分散读
    iov[0].iov_base = Begin_Write();
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    //readv将fd中数据读到 buffer可用空间 + buff缓存空间 两个内存块
    //返回成功的字节数
    ssize_t len = readv(fd, iov, 2);
    //如果读入数量小于可用空间则无需扩展
    //如果超出可用空间，超出部分已经缓存至buff
    //将buff内容扩展写入buff
    if(len < 0)
        *Errno = errno;
    else if(static_cast<size_t>(len) <= writable)
        write_Pos += len;   //更新写指针
    else 
    {    
        write_Pos = buffer.size(); //更新写指针到末尾
        Append(buff, static_cast<size_t>(len - writable));//写入剩余部分
    }
    return len;
}

ssize_t Buffer::Write_Fd(int fd, int *Errno)
{
    ssize_t len =write(fd,Peek(),Readable_Bytes());
    if(len < 0)
    {
        *Errno=errno;
        return len;
    }
    Retrieve(len);
    return len;
}

char* Buffer:: Begin_Ptr()
{
    return &buffer[0];
}
const char* Buffer::Begin_Ptr() const 
{
    return &buffer[0];
}

void Buffer::Make_Space(size_t len)
{
    //扩展空间分两种情况
    //如果可覆盖空间+可写空间不满足len申请需要 直接resize所需空间
    //否则将buffer中已有数据重整至首部 最后检查这部分数据是否缺失
    if(Writable_Bytes()+Prependable_Bytes() < len)
        buffer.resize(write_Pos+len+1);
    else
    {
        size_t readable=Readable_Bytes();
        std::copy(Begin_Ptr()+read_Pos,Begin_Ptr()+write_Pos,Begin_Ptr());
        read_Pos=0;
        write_Pos=readable;
        assert(readable==Readable_Bytes());
    }
}