#pragma once
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string>
#include<sys/socket.h>


class InetAddr
{
public:
    InetAddr() = default;
    InetAddr(struct sockaddr_in addr):
        _addr(addr)
    {
        //_ip = inet_ntoa(_addr.sin_addr);
        char ip_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,&_addr.sin_addr,ip_buf,sizeof ip_buf);
        _ip = ip_buf;
        _port = ntohs(_addr.sin_port);
    }

    InetAddr(const std::string &ip, uint16_t port)
    {
        _ip = ip;
        _port = port;
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(_port);
        _addr.sin_addr.s_addr = inet_addr(ip.c_str());
    }

    std::string Ip() const
    {
        return _ip;
    }


    uint16_t Port() const
    {
        return _port;
    }

    struct sockaddr_in addr() const
    {
        return _addr;
    }

    bool operator==(const InetAddr& other) const
    {
        if(this == &other)
            return true;
        
        return _addr.sin_addr.s_addr == other._addr.sin_addr.s_addr
            && _addr.sin_port == other._addr.sin_port;
    }

    std::string AddrStr()
    {
        return _ip + ":" + std::to_string(_port);
    }

private:
    struct sockaddr_in _addr;
    std::string _ip;
    uint16_t _port;
};