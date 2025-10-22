#pragma once

#include<sys/epoll.h>
#include <unistd.h>
#include <vector>
#include<unordered_map>
#include"Channel.hpp"
#include"Log.hpp"
using namespace log_ns;

const uint64_t MAX_EPOLLEVENTS = 1024;

class Poller {
public:
    Poller()
    {
        _epfd = epoll_create1(EPOLL_CLOEXEC);
        if(_epfd < 0)
        {
            LOG(ERROR,"epoll_create error:%d(%s)",errno,strerror(errno));
            abort();
        }
    }
    ~Poller()
    {
        if(_epfd > 0)
        close(_epfd);
    }

    //添加或修改描述符事件监控
    void UpdateEvent(Channel* channel)
    {

        if(_channels.count(channel->Fd()) == 0)
        {
            Update(channel,EPOLL_CTL_ADD);
        }
        else
        {
            Update(channel,EPOLL_CTL_MOD);
        }
    }
    //移除描述符事件监控
    bool RemoveEvent(Channel* channel)
    {
         if(_channels.count(channel->Fd()) == 0)
         {
            return false;
         }
         Update(channel,EPOLL_CTL_DEL);
         _channels.erase(channel->Fd());
         return true;
    }

    //轮询描述符事件
    //阻塞监控
    //返回一批活跃的描述符
    void Poll(std::vector<Channel*>* activeChannels)
    {
        int nfds = ::epoll_wait(_epfd,_events,MAX_EPOLLEVENTS,-1);
        if(nfds < 0)
        {
            if(errno == EINTR) return;
            LOG(ERROR,"epoll_wait error:%d(%s)",errno,strerror(errno));
            abort();
        }

        for(int i = 0;i < nfds; i++)
        {
            Channel* channel = _channels[_events[i].data.fd];
            //设置该Channel的活跃事件
            channel->SetRevents(_events[i].events);
            activeChannels->push_back(channel);

        }
    }
    

    //移除描述符事件监控
private:
    void Update(Channel* channel,int op)
    {
        int fd = channel->Fd();
        struct epoll_event event;
        event.data.fd = fd;
        event.events = channel->Events();
        if(epoll_ctl(_epfd,op,fd,&event) < 0)
        {
            LOG(ERROR,"epoll_ctl error:%d(%s)",errno,strerror(errno));
            return;
        }
        if(_channels.count(fd) == 0)
        {
            _channels[fd] = channel;
        }
        return;
    }

    int _epfd;
    struct epoll_event _events[MAX_EPOLLEVENTS];
    std::unordered_map<int, Channel*> _channels;
};

