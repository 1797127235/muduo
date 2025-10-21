#pragma once
#include <functional>
#include <sys/epoll.h>
class Poller;
class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(int fd,Poller* epoller)
        : _epoller(epoller),_fd(fd), _events(0), _revents(0) {}

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

    void SetRevents(uint32_t revents) { _revents = revents; }

    void HandleEvent()
    {
        if (_revents & (EPOLLERR | EPOLLHUP)) {
            if (_errorCallback) _errorCallback();
            if (_eventCallback) _eventCallback();
        }

        if (_revents & (EPOLLIN | EPOLLRDHUP | EPOLLPRI)) {
            if (_readCallback) _readCallback();
            if (_eventCallback) _eventCallback();
        }

        if (_revents & EPOLLOUT) {
            if (_writeCallback) _writeCallback();
            if (_eventCallback) _eventCallback();
        }

        if (_revents & (EPOLLRDHUP | EPOLLHUP)) {
            if (_eventCallback) _eventCallback();
            if (_closeCallback) _closeCallback();
        }

    }

    uint32_t Events()
    {
        return _events;
    }

    void Remove();

    void Update();

private:
    Poller* _epoller;//监控该Channel的Poller
    int _fd;
    uint32_t _events;
    uint32_t _revents;

    EventCallback _readCallback;
    EventCallback _writeCallback;
    EventCallback _closeCallback;
    EventCallback _errorCallback;
    EventCallback _eventCallback;
};
