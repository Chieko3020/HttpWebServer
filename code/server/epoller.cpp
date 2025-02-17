#include "epoller.h"
Epoller::Epoller(int max_Event):epoll_fd(epoll_create(512)),events(max_Event)
{
    assert(epoll_fd>=0&&events.size()>0);
}

Epoller::~Epoller()
{
    close(epoll_fd);
}

bool Epoller::Add_Fd(int fd, uint32_t events)
{
    if(fd<0)
        return false;
    epoll_event event={0};
    event.data.fd=fd;
    event.events=events;
    return (0==epoll_ctl(epoll_fd, EPOLL_CTL_ADD,fd,&event));//没有错误返回0
}

bool Epoller::Modify_Fd(int fd, uint32_t events)
{
    if(fd<0)
        return false;
    epoll_event event={0};
    event.data.fd=fd;
    event.events=events;
    return (0==epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&event));
}

bool Epoller::Del_fd(int fd)
{
    if(fd<0)
        return false;
    return (0==epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,0));
}

int Epoller::wait(int timeout)//向事件数组传入已就绪的事件
{
    return epoll_wait(epoll_fd,&events[0],static_cast<int>(events.size()),timeout);
} 

uint32_t Epoller::Get_Event(size_t i) const 
{
    assert(i<events.size()&&i>=0);
    return events[i].events;
}

int Epoller::Get_Event_Fd(size_t i) const
{
    assert(i<events.size()&&i>=0);
    return events[i].data.fd;
}