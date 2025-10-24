#pragma once
#include"EventLoop.hpp"
#include "InetAddr.hpp"
#include"Socket.hpp"
#include"Channel.hpp"
#include"Connection.hpp"
#include <memory>


//监听连接管理
class Acceptor {
    private:
        Socket _socket;//用于创建监听套接字
        EventLoop *_loop; //用于对监听套接字进行事件监控
        //Channel _channel; //用于对监听套接字进行事件管理
        std::unique_ptr<Channel> _channel; //用于对监听套接字进行事件管理

        using AcceptCallback = std::function<void(int)>;
        AcceptCallback _accept_callback;
    private:
        /*监听套接字的读事件回调处理函数---获取新连接，调用_accept_callback函数进行新连接处理*/
        void HandleRead() {
            Socket newfd = _socket.Accept();
            if (newfd.fd() < 0) {
                return ;
            }
            if (_accept_callback) _accept_callback(newfd.Release());
        }
        int CreateServer(int port) {
            bool ret = _socket.BuildListenSocket(port);
            assert(ret == true);
            return _socket.fd();
        }
    public:
        /*不能将启动读事件监控，放到构造函数中，必须在设置回调函数后，再去启动*/
        /*否则有可能造成启动监控后，立即有事件，处理的时候，回调函数还没设置：新连接得不到处理，且资源泄漏*/
        Acceptor(EventLoop *loop, int port): _socket(CreateServer(port)), _loop(loop)
        {
            _channel = std::make_unique<Channel>(_socket.fd(),_loop);
            _channel->SetReadCallback(std::bind(&Acceptor::HandleRead, this));
        }
        void SetAcceptCallback(const AcceptCallback &cb) { _accept_callback = cb; }
        void Listen() { _channel->EnableRead(); }
};