#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <memory>
#include <unistd.h>
#include <sys/timerfd.h>
#include"Channel.hpp"
#include"Log.hpp"
using namespace log_ns;
//定時器對象

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask{
    private:
        uint64_t _id;       // 定时器任务对象ID
        uint32_t _timeout;  //定时任务的超时时间
        bool _canceled;     // false-表示没有被取消， true-表示被取消
        TaskFunc _task_cb;  //定时器对象要执行的定时任务
        ReleaseFunc _release; //用于删除TimerWheel中保存的定时器对象信息
        
    public:
        TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb): 
            _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}
        ~TimerTask() { 
            if (_canceled == false) _task_cb(); 
            _release(); 
        }
        void Cancel() { _canceled = true; }
        void SetRelease(const ReleaseFunc &cb) { _release = cb; }
        uint32_t DelayTime() { return _timeout; }
};

class TimerWheel {
    private:
        using WeakTask = std::weak_ptr<TimerTask>;
        using PtrTask = std::shared_ptr<TimerTask>;
        int _tick;      //当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
        int _capacity;  //表盘最大数量---其实就是最大延迟时间
        std::vector<std::vector<PtrTask>> _wheel;


        int timer_fd;
        EventLoop* _loop;
        std::unique_ptr<Channel> _timer_channel;

        std::unordered_map<uint64_t, WeakTask> _timers;
    private:
        void RemoveTimer(uint64_t id) {
            auto it = _timers.find(id);
            if (it != _timers.end()) {
                _timers.erase(it);
            }
        }

        static int CreateTimerFd()
        {
            int timerfd = timerfd_create(CLOCK_MONOTONIC,0);
            if(timerfd < 0)
            {
                LOG(ERROR,"CreateTimerFd error");
                abort();
            }


            struct itimerspec itime;
            memset(&itime,0,sizeof(itime));
            itime.it_value.tv_sec = 1;
            itime.it_value.tv_nsec = 0;
            itime.it_interval.tv_sec = 1;
            itime.it_interval.tv_nsec = 0;

            if(timerfd_settime(timerfd,0,&itime,NULL) < 0)
            {
                LOG(ERROR,"timerfd_settime error");
                abort();
            }

            return timerfd;
        }

        int ReadTimerfd()
        {
            uint64_t exp;
            int ret = ::read(timer_fd,&exp,sizeof(exp));
            if(ret < 0)
            {
                if(errno == EINTR || errno == EAGAIN)
                {
                    return 0;
                }
                LOG(ERROR,"ReadTimerfd error");
                abort();
            }
            return ret;
        }

    public:
        TimerWheel(EventLoop* loop):
         _tick(0),
         _capacity(60),
         _wheel(_capacity),
         timer_fd(CreateTimerFd()),
         _loop(loop),
         _timer_channel(std::make_unique<Channel>(timer_fd, loop))
        {
            //註冊可讀事件保證每秒鐘執行一次
            _timer_channel->SetReadCallback(std::bind(&TimerWheel::RunOnTime, this));
            _timer_channel->EnableRead();
        }
        
        //定時器事件處理
        void RunOnTime()
        {
            ReadTimerfd();
            RunTimerTask();
        }
   
        //这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
        void RunTimerTask()
        {
            _tick = (_tick + 1) % _capacity;
            _wheel[_tick].clear();//清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
        }


        bool HasTimer(uint64_t id)
        {
            auto it = _timers.find(id);
            if(it == _timers.end())
            {
                return false;
            }
            return true;
        }

        void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb)
        {
            if(delay == 0) delay = 1;

            PtrTask pt(new TimerTask(id, delay, cb));
            pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
            int pos = (_tick + delay) % _capacity;
            _wheel[pos].push_back(pt);
            _timers[id] = WeakTask(pt);
        
        }
        //刷新/延迟定时任务
        void TimerRefreshInLoop(uint64_t id) {
            //通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
            auto it = _timers.find(id);
            if (it == _timers.end()) {
                return;//没找着定时任务，没法刷新，没法延迟
            }
            PtrTask pt = it->second.lock();//lock获取weak_ptr管理的对象对应的shared_ptr
            int delay = pt->DelayTime();
            int pos = (_tick + delay) % _capacity;
            _wheel[pos].push_back(pt);
        }

        void TimerCancelInLoop(uint64_t id) {
            auto it = _timers.find(id);
            if (it == _timers.end()) {
                return;//没找着定时任务，没法刷新，没法延迟
            }
            PtrTask pt = it->second.lock();
            if (pt) pt->Cancel();
        } 


        void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
        void TimerRefresh(uint64_t id);
        void TimerCancel(uint64_t id);
};
