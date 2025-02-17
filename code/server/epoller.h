#ifndef EPOLLER_H
#define EPOLLER_H
#include <sys/epoll.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller
{
public:
    explicit Epoller(int max_Event=1024);
    ~Epoller();
    bool Add_Fd(int fd,uint32_t events);//添加fd
    bool Modify_Fd(int fd,uint32_t events);//修改指定fd
    bool Del_fd(int fd);
    int wait(int timeout=-1);//毫秒
    uint32_t Get_Event(size_t i) const;
    int Get_Event_Fd(size_t i) const;
private:
    int epoll_fd;//epoll事件表
    std::vector<struct epoll_event> events;
};
#endif