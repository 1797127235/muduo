#include"Socket.hpp"
#include<string>
#include<iostream>
int main()
{
    Socket socket;
    socket.BuildClientSocket("127.0.0.1",8080);
    while(1)
    {
        std::string line;
        std::getline(std::cin, line);
        socket.Send(line.c_str(),line.size());
        char buff[1024] = {0};
        int ret = socket.Recv(buff,sizeof(buff));
        if(ret > 0)
        {
            std::cout << "recv: " << buff << std::endl;
        }
        else if(ret == 0)
        {
            std::cout << "peer closed" << std::endl;
            break;
        }
        else
        {
            std::cout << "recv error" << std::endl;
            break;
        }
    }
    return 0;
}