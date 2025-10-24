#pragma once
#include "EventLoop.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>

class LoopThread {
public:
    LoopThread()
        : _loop(nullptr),
          _started(false),
          _stopped(false),
          _loop_thread(&LoopThread::ThreadEntry, this) {}

    // 禁止拷贝/移动
    LoopThread(const LoopThread&) = delete;
    LoopThread& operator=(const LoopThread&) = delete;
    LoopThread(LoopThread&&) = delete;
    LoopThread& operator=(LoopThread&&) = delete;

    ~LoopThread() { Stop(); }

    // 阻塞直到 EventLoop 创建完成；若线程已停止则返回 nullptr
    EventLoop* GetLoop()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cond.wait(lock, [this] { return _loop != nullptr || _stopped; });
        return _loop; // 可能为 nullptr（线程已退出）
    }

    // 幂等停止：请求退出并 join
    void Stop() {
        EventLoop* loop = nullptr;

        {
            std::unique_lock<std::mutex> lock(_mtx);
            // 若还没启动完成，也等一下，避免错过 Quit
            _cond.wait(lock, [this] { return _started || _stopped; });
            loop = _loop;
        }

        if (loop) loop->Quit();        // 唤醒 epoll，令 Start() 退出

        if (_loop_thread.joinable()) {
            _loop_thread.join();
        }
    }

private:
    void ThreadEntry() {
        try {
            EventLoop loop; // 栈对象：生命周期覆盖整个线程
            {
                std::lock_guard<std::mutex> lock(_mtx);
                _loop = &loop;
                _started = true;
                _stopped = false;
            }
            _cond.notify_all(); // 通知 GetLoop/Stop

            _loop->Start();     // 阻塞直到 Quit()

            // 线程退出前清理观察指针并标记停止
            {
                std::lock_guard<std::mutex> lock(_mtx);
                _loop = nullptr;
                _stopped = true;
            }
            _cond.notify_all();
        } catch (...) {
            // 出异常也要释放等待者
            {
                std::lock_guard<std::mutex> lock(_mtx);
                _loop = nullptr;
                _started = true; // 表示已经“完成启动流程”（尽管失败） 
                _stopped = true;
            }
            _cond.notify_all();
            throw; // 或记录日志后吞掉
        }
    }

private:
    EventLoop* _loop;   
    bool _started;
    bool _stopped;

    std::thread _loop_thread;//线程对象
    std::mutex _mtx;
    std::condition_variable _cond;
};
