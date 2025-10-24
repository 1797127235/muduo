#pragma once
#include "Channel.hpp"
#include "Poller.hpp"
#include"TimeWheel.hpp"
#include<thread>
#include<memory>
#include<atomic>
#include<sys/eventfd.h>
#include<cassert>

//事件监控管理模块
//事件监控 就绪事件处理 执行任务
class EventLoop
{
private:
    using Functor = std::function<void()>;
    int _event_fd;

    std::unique_ptr<Channel> _eventChannel;//通知事件描述符
    std::thread::id _threadId;

    Poller _poller;//事件監控
    std::vector<Functor> _task;//任務隊列
    std::mutex _mutex;

    TimerWheel _timerWheel;//定時器
    std::atomic<bool> _quit;


    void RunAllTask()
    {
        std::vector<Functor> tmp;

        {
            std::lock_guard<std::mutex> lock(_mutex);
            tmp.swap(_task);
        }

        for (auto& cb : tmp)
        {
            cb();
        }
    }
    static int CreateEventFd()
    {
        int event_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (event_fd < 0)
        {
            LOG(ERROR,"CreateEventFd error");
            abort();
        }
        return event_fd;
    }
public:

    EventLoop():
    _event_fd(CreateEventFd()),
    _eventChannel(std::make_unique<Channel>(_event_fd,this)),
    _threadId(std::this_thread::get_id()),
    _timerWheel(this)
    {
        //设置事件回调
        _eventChannel->SetReadCallback(std::bind(&EventLoop::ReadEventfd,this));//每隔
        //设置事件类型
        _eventChannel->EnableRead();
        _quit.store(false, std::memory_order_relaxed);
    }


    ~EventLoop()
    {
        RemoveEvent(_eventChannel.get());
        ::close(_event_fd);
    }

    void ReadEventfd()
    {
        uint64_t res = 0;
        int ret = ::read(_event_fd,&res,sizeof(res));
        if(ret < 0)
        {
            if(errno == EINTR || errno == EAGAIN)
            {
                return ;
            }
            LOG(ERROR,"ReadEventfd error");
            abort();
        }
        return ;
    }

    void WakeUpEventfd()
    {
        uint64_t one = 1;
        int ret = ::write(_event_fd,&one,sizeof(one));
        if(ret < 0)
        {
            if(errno == EINTR || errno == EAGAIN)
            {
                return ;
            }
            LOG(ERROR,"WakeUpEventfd error");
            abort();
        }
    }

    void Start()
    {
        _quit.store(false, std::memory_order_relaxed);
        while(!_quit.load(std::memory_order_relaxed))
        {
            std::vector<Channel*> activeChannels;
            //监听活跃的监听事件
            _poller.Poll(&activeChannels);
            //遍歷活躍的channel
            for(auto& channel : activeChannels)
            {
                channel->HandleEvent();
            }
            RunAllTask();
        }
        RunAllTask();
    }

    void RunInLoop(const Functor& cb)
    {
        if(IsInLoop())
        {
            cb();
        }
        else
        {
            QueueInLoop(cb);
        }
    }

    void QueueInLoop(const Functor& cb)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _task.push_back(cb);
        }
        //唤醒当前线程
        WakeUpEventfd();
    }

    void UpdateEvent(Channel* channel)
    {
        _poller.UpdateEvent(channel);
    }

    void RemoveEvent(Channel* channel)
    {
        _poller.RemoveEvent(channel);
    }

    bool IsInLoop()
    {
        return std::this_thread::get_id() == _threadId;
    }

    void AssertInLoop()
    {
        assert(_threadId == std::this_thread::get_id());
    }

    //添加定時任務
    void TimerAdd(uint64_t id,uint32_t delay, const TaskFunc &cb)
    {
        _timerWheel.TimerAdd(id,delay,cb);
    }

    //取消定時任務
    void TimerCancel(uint64_t id)
    {
        _timerWheel.TimerCancel(id);
    }

    //刷新定時任務
    void TimerRefresh(uint64_t id)
    {
        _timerWheel.TimerRefresh(id);
    }

    void Quit()
    {
        _quit.store(true, std::memory_order_relaxed);
        WakeUpEventfd();
    }

    bool HasTimer(uint64_t id)
    {
        return _timerWheel.HasTimer(id);
    }
};


//內核epoll移除事件
inline void Channel::Remove()
{
    _loop->RemoveEvent(this);
}


//更新事件到內核epoll
inline void Channel::Update()
{
    _loop->UpdateEvent(this);
}


//添加定時任務
inline void TimerWheel::TimerAdd(uint64_t id,uint32_t delay, const TaskFunc &cb)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop,this,id,delay,cb));
}

//取消定時任務
inline void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop,this,id));
}

inline void TimerWheel::TimerRefresh(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop,this,id));
}

