#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <cstring>
#include <cassert>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "InetAddr.hpp"
#include "Log.hpp"
using namespace log_ns; // 避免头文件污染命名空间

constexpr int DEFAULT_BACKLOG = 128;

class Socket {
public:
    Socket() noexcept : _fd(-1) {}
    explicit Socket(int fd) noexcept : _fd(fd) {}

    // 不允许拷贝，避免重复关闭
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // 允许移动，转移所有权
    Socket(Socket&& other) noexcept : _fd(other._fd) { other._fd = -1; }
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close_if_valid();
            _fd = other._fd;
            other._fd = -1;
        }
        return *this;
    }

    ~Socket() { close_if_valid(); }

    int fd() const noexcept { return _fd; }
    bool valid() const noexcept { return _fd >= 0; }

    // 创建 TCP 套接字
    bool Create(bool nonblock = true, bool cloexec = true) {
        close_if_valid();

        int type = SOCK_STREAM;
#ifdef SOCK_NONBLOCK
        if (nonblock) type |= SOCK_NONBLOCK;
#endif
#ifdef SOCK_CLOEXEC
        if (cloexec) type |= SOCK_CLOEXEC;
#endif
        _fd = ::socket(AF_INET, type, 0);
        if (_fd < 0) {
            LOG(ERROR, "socket() failed: %d(%s)", errno, strerror(errno));
            return false;
        }

#ifndef SOCK_NONBLOCK
        if (nonblock && !SetNonBlock(true)) return false;
#endif
#ifndef SOCK_CLOEXEC
        if (cloexec && !SetCloExec(true)) return false;
#endif
        return true;
    }

    // 服务器：Bind + Listen
    bool BuildListenSocket(uint16_t port, int backlog = DEFAULT_BACKLOG, bool reuse_port = true) {
        if (!Create(/*nonblock*/true, /*cloexec*/true)) return false;
        if (!SetReuseAddress(true)) return false;
        if (reuse_port) SetReusePort(true); // 非致命

        if (!Bind(port)) return false;
        if (!Listen(backlog)) return false;
        return true;
    }

    // 客户端：Create + Connect(ip:port)
    bool BuildClientSocket(const std::string& ip, uint16_t port) {
        if (!Create(/*nonblock*/false, /*cloexec*/true)) return false;
        return Connect(ip, port);
    }

    // ==== 服务端操作 ====
    bool Bind(uint16_t port) {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (::bind(_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            LOG(ERROR, "bind() failed: %d(%s)", errno, strerror(errno));
            return false;
        }
        return true;
    }

    bool Listen(int backlog = DEFAULT_BACKLOG) {
        if (::listen(_fd, backlog) < 0) {
            LOG(ERROR, "listen() failed: %d(%s)", errno, strerror(errno));
            return false;
        }
        return true;
    }

    // 返回“已连接 Socket”；可选拿到对端地址
    // 返回的 Socket 默认是 non-block & cloexec（Linux用 accept4）
    Socket Accept(InetAddr* peer = nullptr) {
        for (;;) {
#ifdef __linux__
            sockaddr_in addr{};
            socklen_t len = sizeof(addr);
            int nfd = ::accept4(_fd, reinterpret_cast<sockaddr*>(&addr), &len,
                                SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
            sockaddr_in addr{};
            socklen_t len = sizeof(addr);
            int nfd = ::accept(_fd, reinterpret_cast<sockaddr*>(&addr), &len);
#endif
            if (nfd < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞且无新连接
                    return Socket{-2}; // 约定：-2 表示“可重试/暂无”
                }
                LOG(ERROR, "accept() failed: %d(%s)", errno, strerror(errno));
                return Socket{-1};
            }

#ifndef __linux__
            // 非 Linux：补齐 nonblock / cloexec
            Socket s(nfd);
            s.SetNonBlock(true);
            s.SetCloExec(true);
#else
            Socket s(nfd);
#endif
            if (peer) *peer = InetAddr(addr);
            return s;
        }
    }

    // ==== 客户端操作 ====
    bool Connect(const std::string& ip, uint16_t port) {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);

        if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
            LOG(ERROR, "inet_pton() failed for %s", ip.c_str());
            return false;
        }
        if (::connect(_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            LOG(ERROR, "connect() failed: %d(%s)", errno, strerror(errno));
            return false;
        }
        return true;
    }

    // ==== I/O：明确返回语义 ====
    // Recv：
    //   >0: 读到的字节数
    //   =0: 对端有序关闭
    //   =-1: 真错误（不可恢复）
    //   =-2: 可恢复/可重试（EINTR/EAGAIN/EWOULDBLOCK）
    ssize_t Recv(void* buf, size_t len, int flags = 0) {
        assert(buf);
        for (;;) {
            ssize_t n = ::recv(_fd, buf, len, flags);
            if (n > 0) return n;
            if (n == 0) return 0; // peer closed
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -2;
            LOG(ERROR, "recv() failed: %d(%s)", errno, strerror(errno));
            return -1;
        }
    }

    // Send：
    //   >0: 实际发送字节数
    //   =-1: 真错误
    //   =-2: 可重试
    ssize_t Send(const void* buf, size_t len, int flags = 0) {
        assert(buf);
        for (;;) {
            ssize_t n = ::send(_fd, buf, len, flags);
            if (n >= 0) return n;
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -2;
            LOG(ERROR, "send() failed: %d(%s)", errno, strerror(errno));
            return -1;
        }
    }

    // 非阻塞便捷函数
    ssize_t NonBlockRecv(void* buf, size_t len) { return Recv(buf, len, MSG_DONTWAIT); }
    ssize_t NonBlockSend(const void* buf, size_t len) { return Send(buf, len, MSG_DONTWAIT); }

    // ==== 设置选项 ====
    bool SetNonBlock(bool on = true) {
        int fl = ::fcntl(_fd, F_GETFL, 0);
        if (fl < 0) return false;
        if (on) fl |= O_NONBLOCK; else fl &= ~O_NONBLOCK;
        return ::fcntl(_fd, F_SETFL, fl) == 0;
    }

    // 设置文件描述符的 close-on-exec，
    // 当进程调用 exec 系列函数时，文件描述符会被自动关闭
    bool SetCloExec(bool on = true) {
        int fl = ::fcntl(_fd, F_GETFD, 0);
        if (fl < 0) return false;
        if (on) fl |= FD_CLOEXEC; else fl &= ~FD_CLOEXEC;
        return ::fcntl(_fd, F_SETFD, fl) == 0;
    }

    bool SetReuseAddress(bool on = true) {
        int opt = on ? 1 : 0;
        if (::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            LOG(ERROR, "setsockopt(SO_REUSEADDR) failed: %d(%s)", errno, strerror(errno));
            return false;
        }
        return true;
    }

    bool SetReusePort(bool on = true) {
#ifdef SO_REUSEPORT
        int opt = on ? 1 : 0;
        if (::setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            LOG(WARNING, "setsockopt(SO_REUSEPORT) failed: %d(%s)", errno, strerror(errno));
            return false; // 非致命，可按需返回 true
        }
        return true;
#else
        (void)on; return true;
#endif
    }

    // 设置 TCP_NODELAY，禁用 Nagle 算法
    bool SetNoDelay(bool on) {
        int opt = on ? 1 : 0;
        if (::setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
            LOG(ERROR, "setsockopt(TCP_NODELAY) failed: %d(%s)", errno, strerror(errno));
            return false;
        }
        return true;
    }

    // 设置 SO_KEEPALIVE，启用 TCP keepalive
    bool SetKeepAlive(bool on) {
        int opt = on ? 1 : 0;
        if (::setsockopt(_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
            LOG(ERROR, "setsockopt(SO_KEEPALIVE) failed: %d(%s)", errno, strerror(errno));
            return false;
        }
        return true;
    }

    // 设置 SO_LINGER，启用 TCP linger
    // 当文件描述符关闭时，如果 linger 设置为 1，
    // 则等待 linger 秒后才真正关闭
    // 如果 linger 设置为 0，则立即关闭
    bool SetLinger(bool on, int seconds) {
        linger opt{ on ? 1 : 0, seconds };
        if (::setsockopt(_fd, SOL_SOCKET, SO_LINGER, &opt, sizeof(opt)) < 0)
        {
            LOG(ERROR, "setsockopt(SO_LINGER) failed: %d(%s)", errno, strerror(errno));
            return false;
        }
        return true;
    }

    // 主动关闭
    void Close() { close_if_valid(); }

private:
    int _fd;

    void close_if_valid() {
        if (_fd >= 0) {
            ::close(_fd);
            _fd = -1;
        }
    }
};
