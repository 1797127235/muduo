#pragma once
#include "EventLoop.hpp"
#include<thread>
#include<mutex>
#include<condition_variable>
/*
    整合EventLoop和线程
    在线程内部创建EventLoop
*/
class LoopThread
{
public:
    LoopThread():
    _loop(nullptr),
    _loop_thread(std::thread(&LoopThread::ThreadEntry,this)),
    _started(false),
    _stopped(false)
    {}
    ~LoopThread()
    {
        EventLoop* loop = nullptr;
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _cond.wait(lock, [this] { return _started; });
            loop = _loop;
        }

        if (loop)
        {
            loop->Quit();
        }

        if (_loop_thread.joinable())
        {
            _loop_thread.join();
        }
    }

    EventLoop* GetLoop()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cond.wait(lock, [this] { return _loop != nullptr; });
        return _loop;
    }

private:
    //初始化当前线程的EventLoop
    void ThreadEntry()
    {
        EventLoop loop;
        /*
            C++ 的内存模型规定
            如果多个线程同时访问同一个变量，且至少一个线程在写，
            而访问没有被互斥保护，那么结果是未定义行为。

            保证跨线程内存可见性
        */
        {
            std::unique_lock<std::mutex> lock(_mtx);
            _loop = &loop;
            _started = true;
            _stopped = false;
        }
        _cond.notify_all();//通知所有等待的
        _loop->Start();

        {
            std::unique_lock<std::mutex> lock(_mtx);
            _loop = nullptr;
            _stopped = true;
        }
        _cond.notify_all();
    }

    EventLoop* _loop;
    std::thread _loop_thread;

    std::mutex _mtx;
    std::condition_variable _cond;
    bool _started;
    bool _stopped;
};