#include "TcpServer.hpp"
#include "Log.hpp"

using namespace log_ns;

int main()
{
    EnableScreen();

    auto server = std::make_unique<TcpServer>(8080);
    server->SetThreadCount(2);

    server->SetConnectedCallback([](TcpServer::PtrConnection& conn){
        LOG(INFO, "client connected, id=%d", conn->Id());
    });

    server->SetMessageCallback([](TcpServer::PtrConnection& conn, Buffer* buffer){
        std::string msg(buffer->ReadPosition(), buffer->ReadAbleSize());
        LOG(INFO, "recv from %d: %s", conn->Id(), msg.c_str());
        buffer->Clear();
        conn->Send(msg.c_str(), msg.size());
    });

    server->SetClosedCallback([](TcpServer::PtrConnection& conn){
        LOG(INFO, "client closed, id=%d", conn->Id());
    });

    // 每 10 秒打印一次连接数
    server->RunAfter([&server]{
        LOG(INFO, "scheduled task running, connections=%zu", server->ConnectionCount());
    }, 10);

    server->Start();
    return 0;
}