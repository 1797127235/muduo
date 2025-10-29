#pragma once
#include"HttpContext.hpp"
#include"../TcpServer.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Util.hpp"
#include <any>
#include <map>
#include <vector>
#include <regex>
#include <sstream>

class HttpServer
{
public:
    using Handler = std::function<void(HttpRequest& req,HttpResponse* resp)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;
private:
    Handlers _get_route;
    Handlers _post_route;
    Handlers _delete_route;
    Handlers _put_route;

    std::string _basedir;

    TcpServer _server;
private:
    //处理错误的报文
    void ErrorHandler(HttpRequest& req,HttpResponse* resp)
    {
        //1. 组织一个错误展示页面
        std::string body;
        body += "<html>";
        body += "<head>";
        body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
        body += "</head>";
        body += "<body>";
        body += "<h1>";
        body += std::to_string(resp->GetCode());
        body += " ";
        body += Util::StatuDesc(resp->GetCode());
        body += "</h1>";
        body += "</body>";
        body += "</html>";
        //2. 将页面数据，当作响应正文，放入rsp中
        resp->SetContent(body, "text/html");
    
    }

    /*
        构造响应报文
        响应行
        响应头
        空行
        响应体
    */
    void WriteResponse(PtrConnection& conn,HttpRequest& req,HttpResponse* resp)
    {
        //1. 组织响应行
        if (req.Close()) {
            resp->SetHeader("Connection", "close");
        } else {
            resp->SetHeader("Connection", "keep-alive");
        }

        // 始终设置 Content-Length（即便为 0），避免在 keep-alive 下客户端等待数据而悬挂
        resp->SetHeader("Content-Length", std::to_string(resp->GetBody().size()));

        if (resp->GetBody().empty() == false && resp->HasHeader("Content-Type") == false) {
            resp->SetHeader("Content-Type", "application/octet-stream");
        }

        if (resp->IsRedirect()) {
            resp->SetHeader("Location", resp->GetRedirectUrl());
        }


        std::stringstream rsp_str;
        // 状态行：HTTP/1.1 <code> <desc>\r\n
        rsp_str << "HTTP/1.1 " << resp->GetCode() << " " << Util::StatuDesc(resp->GetCode()) << "\r\n";

        {
            auto headers = resp->GetHeaders();
            for (const auto& kv : headers) {
                rsp_str << kv.first << ": " << kv.second << "\r\n";
            }
        }

        rsp_str << "\r\n";
        rsp_str << resp->GetBody();

        // 先将响应串落地，避免对临时对象取指针两次
        std::string out = rsp_str.str();
        conn->Send(out.data(), out.size());
    }

    bool IsFileHandler(const HttpRequest& req)
    {
        if(_basedir.empty()) return false;

        if(req._method != "GET" && req._method != "HEAD")
        {
            return false;
        }
        
        //验证url合法性
        if(Util::BalidPath(req._path)== false)
        {
            return false;
        }

        std::string path = _basedir + req._path;

        if(path.back() == '/')
        {
            path += "index.html";
        }

        // 已校验了 URL 路径的合法性，这里改用文件系统判断是否存在对应资源
        return Util::IsRegular(path) || Util::IsDirectory(path);
    }

    void FileHandler(HttpRequest& req,HttpResponse* resp)
    {
        std::string req_path = _basedir + req._path;
        if(req_path.back() == '/')
        {
            req_path += "index.html";
        }

        bool ret = Util::ReadFile(req_path,&resp->GetBody());
        std::cout << "file" << ' ' << resp->GetBody().size() << std::endl;
        if(ret == false)
        {
            return ;
        }

        std::string mime = Util::ExtMime(req_path);
        resp->SetHeader("Content-Type",mime);

        return ;
    }

    void Dispatcher(HttpRequest& req,HttpResponse* resp,Handlers& handlers)
    {
        for(auto& [re,functor] : handlers)
        {
            bool ret = std::regex_match(req._path,req._matches,re);
            if(ret == false)
            {
                continue;
            }
            return functor(req,resp);
        }
        resp->SetCode(404);
    }

    void Route(HttpRequest& req,HttpResponse* resp)
    {
        //是否申请静态资源
        if(IsFileHandler(req) == true)
        {
            FileHandler(req,resp);
            return ;
        }

        if(req._method == "GET")
        {
            Dispatcher(req,resp,_get_route);
        }
        else if(req._method == "POST")
        {
            Dispatcher(req,resp,_post_route);
        }
        else if(req._method == "DELETE")
        {
            Dispatcher(req,resp,_delete_route);
        }
        else if(req._method == "PUT")
        {
            Dispatcher(req,resp,_put_route);
        }
    }



    void  OnConnected(PtrConnection& conn)
    {
        conn->SetContext(HttpContext());
    }

    //缓冲区解析处理
    void OnMessage(PtrConnection& conn,Buffer* buf)
    {
        while(buf->ReadAbleSize() > 0)
        {
            HttpContext* context = std::any_cast<HttpContext>(conn->GetContext());
            if(context == nullptr)
            {
                return;
            }
            context->RecvHttpRequest(buf);
            HttpRequest& req = context->Request();
            HttpResponse rsp;

            if(context->RespStatu() >= 400)
            {
                ErrorHandler(req,&rsp);
                WriteResponse(conn, req, &rsp);
                context->Reset();
                return;
            }

            //请求行没有接收完成
            if(context->RecvStatu() != RECV_HTTP_OVER)
            {
                return ;
            }

            Route(req,&rsp);
            // 对 4xx/5xx 且无正文的场景，生成一个简单错误页，避免空响应
            if (rsp.GetCode() >= 400 && rsp.GetBody().empty()) {
                ErrorHandler(req, &rsp);
            }
            WriteResponse(conn,req,&rsp);
            context->Reset();

            if(rsp.Close() == true) conn->Shutdown();
        }
    } 

public:
    HttpServer(int port):
    _server(port)
    {
        // 绑定回调
        _server.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    void Get(std::string& pattern,Handler handler)
    {
        _get_route.emplace_back(std::regex(pattern), handler);
    }

    void Post(std::string& pattern,Handler handler)
    {
        _post_route.emplace_back(std::regex(pattern), handler);
    }

    void Delete(std::string& pattern,Handler handler)
    {
        _delete_route.emplace_back(std::regex(pattern), handler);
    }

    void Put(std::string& pattern,Handler handler)
    {
        _put_route.emplace_back(std::regex(pattern), handler);
    }

    void SetBasedir(std::string& basedir)
    {
        _basedir = basedir;
    }

    void SetThreadCount(int cnt)
    {
        _server.SetThreadCount(cnt);
    }

    void Start()
    {
        _server.Start();
    }
};