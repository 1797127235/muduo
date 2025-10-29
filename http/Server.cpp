#include"HttpServer.hpp"

// 将静态文件根目录设置为仓库内的 http/wwwroot
static std::string base_url = "./http/wwwroot";

void LoginHandler(HttpRequest& req, HttpResponse* resp)
{
    // 简单示例：返回JSON
    std::string body = "{\"msg\":\"login ok\"}";
    resp->SetContent(body, "application/json");
}

int main()
{
    HttpServer server(8888);
    server.SetBasedir(base_url);
    server.SetThreadCount(4);
    std::string login_pattern = "/login";
    server.Get(login_pattern, LoginHandler);

    server.Start();
}