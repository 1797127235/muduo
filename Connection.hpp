#pragma once
#include"Buffer.hpp"
#include "Channel.hpp"
#include"Socket.hpp"
#include"EventLoop.hpp"
#include <cassert>
#include <cstdint>
#include<any>
#include<memory>

//對通信連接的所有操作管理
/*
1.套接字的管理
2.連接事件的管理，可讀，可寫，錯誤
3.緩衝區的管理
4.連接的生命周期管理
5.協議上下文管理
*/

typedef enum{
    DISCONECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
}ConnState;

class Connection;




class Connection:public std::enable_shared_from_this<Connection>
{
public:
    using PtrConnection = std::shared_ptr<Connection>;
    using ConnectedCallback = std::function<void(PtrConnection&)>;
    using MessageCallback = std::function<void(PtrConnection&, Buffer*)>;
    using ClosedCallback = std::function<void(PtrConnection&)>;
    using AnyEventCallback = std::function<void(PtrConnection&)>;
public:
    Connection(EventLoop* loop,uint64_t conn_id,int sockfd)
        : _conn_id(conn_id),
          _sock(sockfd),
          _enable_inactive_release(false),
          _loop(loop),
          _timer_id(0),
          _channel(sockfd, loop),
          _state(CONNECTING)
    {
        //设置事件回调
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
    }
    ~Connection()
    {
        
    }

    int Fd() const
    {
        return _sock.fd();
    }

    int Id() const
    {
        return _conn_id;
    }

    bool  Connected()
    {
        return _state == CONNECTED;
    }


    void SetContext(const std::any& context)
    {
        _context = context;
    }

    std::any* GetContext()
    {
        return &_context;
    }

    void SetConnectedCallback(const ConnectedCallback& cb)
    {
        _connected_cb = cb;
    }

    void SetMessageCallback(const MessageCallback&cb)
    {
        _message_cb = cb;
        
    }

    void SetClosedCallback(const ClosedCallback&cd)
    {
        _closed_cb = cd;
    }

    void SetAnyEventCallback(const AnyEventCallback& cb)
    {
        _any_event_cb = cb;

    }

    void SetServerClosedCallback(const ClosedCallback& cb)
    {
        _server_closed_callback = cb;
    }




    void Send(const char* data,size_t len)
    {
        return  _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, data, len));
    }

    void Shutdown()
    {
        return _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop, this));
    }

    void Release()
    {
        return _loop->RunInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }

    void EnableInactiveRelease(int sec)
    {
        return _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop,this,sec));
    }

    void CancelInactiveRelease()
    {
        return _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop,this));
    }

    //设置回调
    //就绪
    void Established()
    {
        return  _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop,this));
    }

    //协议切换
    //这个接口必须在EventLoop线程中立即执行，防止新的事件触发的时候，切换任务没有执行
    void Upgrade(const std::any& context,const ConnectedCallback& cb,
    const MessageCallback& msg_cb,
    const ClosedCallback& closed_cb,
    const AnyEventCallback& any_event_cb)
    {
        //这个函数必须在线程立即调用，防止新的事件触发的时候，切换任务没有执行
        _loop->AssertInLoop();

        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop,this,context,cb,msg_cb,closed_cb,any_event_cb));
    }

private:
    uint64_t _conn_id;
    Socket _sock;
    //是否启用空闲释放
    bool _enable_inactive_release;

    //连接所关联的事件循环
    EventLoop* _loop;

    //定时器id
    uint64_t _timer_id;
    
    //回调函数
    Channel _channel;

    //连接状态
    ConnState _state;

    Buffer _in_buffer;
    Buffer _out_buffer;

    //请求接收处理上下文
    std::any _context;




    ConnectedCallback _connected_cb;
    MessageCallback _message_cb;
    ClosedCallback _closed_cb;
    AnyEventCallback _any_event_cb;

    //组件内连接关闭回调
    ClosedCallback _server_closed_callback;

    //处理读事件
    void HandleRead()
    {
        char buff[65536];
        ssize_t ret = _sock.NonBlockRecv(buff, sizeof(buff) -1);
        if(ret == 0)
        {
            return ShutdownInLoop();
        }

        if(ret < 0)
        {
            if(ret == -2)
            {
                return; // 可重试，等待下次读事件
            }
            return ShutdownInLoop();
        }

        _in_buffer.Write(buff,ret);

        //调用回调
        if(_in_buffer.ReadAbleSize() > 0)
        {
            auto self = shared_from_this();
            return _message_cb(self, &_in_buffer);
        }
    }

    //处理写事件
    void HandleWrite()
    {
        //_out_buffer中保存的数据就是要发送的数据
        ssize_t ret = _sock.NonBlockSend(_out_buffer.ReadPosition(), _out_buffer.ReadAbleSize());
        if(ret < 0)
        {
            if(ret == -2)
            {
                return; // 写缓冲区满，等待下一次写事件
            }
            if(_in_buffer.ReadAbleSize() > 0)
            {
                auto self = shared_from_this();
                _message_cb(self, &_in_buffer);
            }
            return Release();//这时候就是实际的关闭释放操作了。
        }

        _out_buffer.MoveReadOffset(ret);//千万不要忘了，将读偏移向后移动
        if (_out_buffer.ReadAbleSize() == 0) {
            _channel.DisableWrite();// 没有数据待发送了，关闭写事件监控
            //如果当前是连接待关闭状态，则有数据，发送完数据释放连接，没有数据则直接释放
            if (_state == DISCONNECTING) {
                return Release();
            }
        }
        return;
    }

    //处理关闭事件
    void HandleClose()
    {
        if(_in_buffer.ReadAbleSize() > 0)
        {
            auto self = shared_from_this();
            _message_cb(self, &_in_buffer);
        }

        return Release();

    }

    //处理错误事件
    void HandleError()
    {
        return HandleClose();
    }                                                                                                  


    //处理任意事件
    void HandleEvent()
    {
        if(_state != CONNECTED)
        {
            return;
        }

        if(_enable_inactive_release == true)
        {
            _loop->TimerRefresh(_conn_id);
        }

        if(_any_event_cb)
        {
            auto self = shared_from_this();
            _any_event_cb(self);
        }
    }



    //发送
    void SendInLoop(const char* data,size_t len)
    {
        if(_state != CONNECTED || len == 0)
        {
            return ;
        }

        _out_buffer.Write(data,len);
        if(_out_buffer.ReadAbleSize() > 0)
        {
            _channel.EnableWrite();
        }
    }

    //并不是实际连接释放操作，需要判断还有没有数据待处理发送,然后释放
    void ShutdownInLoop()
    {
        _state = DISCONNECTING;
        if(_in_buffer.ReadAbleSize() > 0)
        {
            auto self = shared_from_this();
            _message_cb(self, &_in_buffer);
        }

        if(_out_buffer.ReadAbleSize() > 0)
        {
            _channel.EnableWrite();
        }

        if(_out_buffer.ReadAbleSize() == 0)
        {
            return Release();
        }
    }

    //开启空闲释放
    void EnableInactiveReleaseInLoop(int sec)
    {
        _enable_inactive_release = true;

        if(_loop -> HasTimer(_conn_id))
        {
            _loop->TimerRefresh(_conn_id);
        }


        _loop->TimerAdd(_conn_id,sec,std::bind(&Connection::Release,this));
    }

    //取消空闲释放
    void CancelInactiveReleaseInLoop()
    {
        _enable_inactive_release = false;

        if(_loop->HasTimer(_conn_id))
        {
            _loop->TimerCancel(_conn_id);
        }
    }

    //实际释放
    /*
        修改连接状态
        移除连接的事件监控
        关闭描述符
        如果当前定时队列中还有定时任务，取消定时任务
        调用关闭回调
        调用服务器关闭回调
    */
    void ReleaseInLoop()
    {
        _state = DISCONECTED;
        _channel.Remove();
        _sock.Close();
        if(_loop->HasTimer(_conn_id))
        {
            _loop->TimerCancel(_conn_id);
        }

        if(_closed_cb)
        {
            auto self = shared_from_this();
            _closed_cb(self);
        }

        if(_server_closed_callback)
        {
            auto self = shared_from_this();
            _server_closed_callback(self);
        }
    }

    //连接获取后，所处状态下要进行各种设置(启动读监控，调用回调函数)
    void EstablishedInLoop()
    {
        assert(_state == CONNECTING);

        _state = CONNECTED;
        _channel.EnableRead();
        if(_connected_cb)
        {
            auto self = shared_from_this();
            _connected_cb(self);
        }
    }


    void UpgradeInLoop(const std::any& context,const ConnectedCallback& cb,
    const MessageCallback& msg_cb,
    const ClosedCallback& closed_cb,
    const AnyEventCallback& any_event_cb)
    {
        _context = context;
        _connected_cb = cb;
        _message_cb = msg_cb;
        _closed_cb = closed_cb;
        _any_event_cb = any_event_cb;
    }
};