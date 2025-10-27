#pragma once
#include <functional>
#include <sys/epoll.h>
class Poller;
class EventLoop;
class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(int fd,EventLoop* loop)
        : _loop(loop),_fd(fd), _events(0), _revents(0), _closing(false){}

    int Fd() const { return _fd; }

    bool ReadAble() { return _events & EPOLLIN; }
    bool WriteAble() { return _events & EPOLLOUT; }

    void EnableRead()  { _events |= EPOLLIN; Update(); }
    void EnableWrite() { _events |= EPOLLOUT; Update(); }
    void DisableRead() { _events &= ~EPOLLIN; Update(); }
    void DisableWrite(){ _events &= ~EPOLLOUT; Update(); }
    void DisableAll()  { _events = 0; Update(); }

    void SetReadCallback(EventCallback cb)  { _readCallback = std::move(cb); }
    void SetWriteCallback(EventCallback cb) { _writeCallback = std::move(cb); }
    void SetCloseCallback(EventCallback cb) { _closeCallback = std::move(cb); }
    void SetErrorCallback(EventCallback cb) { _errorCallback = std::move(cb); }
    void SetEventCallback(EventCallback cb) { _eventCallback = std::move(cb); }

    void SetRevents(uint32_t revents) { _revents = revents; }

    EventLoop* OwnerLoop() const { return _loop; }

    bool MarkClosing()
    {
        if (_closing) return false;
        _closing = true;
        return true;
    }

    void HandleEvent()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI)) {
            /*不管任何事件，都调用的回调函数*/
            if (_readCallback) _readCallback();
        }
        /*有可能会释放连接的操作事件，一次只处理一个*/
        if (_revents & EPOLLOUT) {
            if (_writeCallback) _writeCallback();
        }else if (_revents & EPOLLERR) {
            if (_errorCallback) _errorCallback();//一旦出错，就会释放连接，因此要放到前边调用任意回调
        }else if (_revents & EPOLLHUP) {
            if (_closeCallback) _closeCallback();
        }
        if (_eventCallback) _eventCallback();
    }

    uint32_t Events()
    {
        return _events;
    }

    void Remove();

    void Update();

private:
    EventLoop* _loop;
    int _fd;
    uint32_t _events;
    uint32_t _revents;

    EventCallback _readCallback;
    EventCallback _writeCallback;
    EventCallback _closeCallback;
    EventCallback _errorCallback;
    EventCallback _eventCallback;
    bool _closing;
};

