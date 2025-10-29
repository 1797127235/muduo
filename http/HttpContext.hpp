#pragma once
#include"../TcpServer.hpp"
#include"HttpRequest.hpp"
#include"Util.hpp"

//http接收状态
typedef enum {
    RECV_HTTP_ERROR,
    RECV_HTTP_LINE,
    RECV_HTTP_HEAD,
    RECV_HTTP_BODY,
    RECV_HTTP_OVER
}HttpRecvStatu;

const int MAX_LINE = 8192;

//Http上下文模块
class HttpContext
{
private:
    int _resp_statu;
    HttpRecvStatu _recv_statu;
    HttpRequest _request;
private:
    //解析请求行
    bool ParseHttpLine(std::string& line)
    {
        std::smatch matches;
        std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        bool ret = std::regex_match(line,matches,e);
        if(ret == false)
        {
            return false;
        }

        _request._method = matches[1];
        _request._path = Util::UrlDecode(matches[2],false);
        if(_request._path.size() == 0)
        {
            return false;
        }
        _request._version = matches[4];

        std::vector<std::string> query_string_arry;
        Util::Split(matches[3],"&",&query_string_arry);

        for(auto& str:query_string_arry)
        {
            size_t pos = str.find("=");
            if(pos == std::string::npos)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 400;
                return false;
            }
            std::string key = Util::UrlDecode(str.substr(0,pos),true);
            std::string value = Util::UrlDecode(str.substr(pos+1),true);
            _request.SetParms(key,value);
        }

        return true;
    } 
    //接收请求行
    bool RecvHttpLine(Buffer* buf)
    {
        if(_recv_statu != RECV_HTTP_LINE) return false;
        std::string line = buf->GetLine();

        // 没有读到一整行，请求行尚未完整
        if (line.size() == 0)
        {
            if (buf->ReadAbleSize() > MAX_LINE)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414; // URI Too Long / Line too long
                return false;
            }
            return true; // 继续等待更多数据
        }

        if (line.size() > MAX_LINE)
        {
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 414;
            return false;
        }

        // 解析请求行
        bool ok = ParseHttpLine(line);
        if (!ok)
        {
            _recv_statu = RECV_HTTP_ERROR;
            if (_resp_statu == 200) _resp_statu = 400; // Bad Request
            return false;
        }

        // 进入请求头解析阶段
        _recv_statu = RECV_HTTP_HEAD;
        return true;
    }
    //接收请求头
    bool RecvHttpHead(Buffer* buf)
    {
        if(_recv_statu != RECV_HTTP_HEAD) return false;
        while(1)
        {
            std::string line = buf->GetLine();
            if(line.size() == 0)
            {
                if(buf->ReadAbleSize() > MAX_LINE)
                {
                    _recv_statu = RECV_HTTP_ERROR;
                    _resp_statu = 414;
                    return false;
                }
                return true;
            }
            if(line.size() > MAX_LINE)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414;
                return false;
            }

            if(line == "\r\n" || line == "\n") break;

            bool ret = ParseHttpHead(line);
            if(ret == false)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 400;
                return false;
            }

        }
        _recv_statu = RECV_HTTP_BODY;
        return true;
    }

    //解析请求头
    bool ParseHttpHead(std::string& line)
    {
        if(line.back() == '\n') line.pop_back();
        if(line.back() == '\r') line.pop_back();
        size_t pos = line.find(": ");
        if(pos == std::string::npos)
        {
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400;
            return false;
        }
        std::string key = line.substr(0,pos);
        std::string value = line.substr(pos+2);
        _request.SetHeader(key,value);
        return true;
    }

    //接收请求体
    bool RecvHttpBody(Buffer* buf)
    {
        if(_recv_statu != RECV_HTTP_BODY) return false;
        
        unsigned long length = 0;
        if(_request.HasHeader("Content-Length"))
        {
            length = std::stoul(_request.GetHeader("Content-Length"));
        }
        if(length == 0)
        {
            _recv_statu = RECV_HTTP_OVER;
            return true;
        }


        //2. 当前已经接收了多少正文,其实就是往  _request._body 中放了多少数据了
        size_t real_len = length - _request._body.size();//实际还需要接收的正文长度
        //3. 接收正文放到body中，但是也要考虑当前缓冲区中的数据，是否是全部的正文
        //  3.1 缓冲区中数据，包含了当前请求的所有正文，则取出所需的数据
        if (buf->ReadAbleSize() >= real_len) {
            _request._body.append(buf->ReadPosition(), real_len);
            buf->MoveReadOffset(real_len);
            _recv_statu = RECV_HTTP_OVER;//接收完成
            return true;
        }
        //  3.2 缓冲区中数据，无法满足当前正文的需要，数据不足，取出数据，然后等待新数据到来
        _request._body.append(buf->ReadPosition(), buf->ReadAbleSize());
        buf->MoveReadOffset(buf->ReadAbleSize());

        return true;
    }
    
public:
    HttpContext():
    _resp_statu(200),
    _recv_statu(RECV_HTTP_LINE)
    {

    }

    void Reset()
    {
        _resp_statu = 200;
        _recv_statu = RECV_HTTP_LINE;
        _request.Reset();
    }

    int RespStatu() const
    {
        return _resp_statu;
    }

    HttpRecvStatu RecvStatu() const
    {
        return _recv_statu;
    }
    
    HttpRequest& Request()
    {
        return _request;
    }

    //接收并解析HTTP请求
    void RecvHttpRequest(Buffer* buf)
    {
        switch(_recv_statu)
        {
            case RECV_HTTP_LINE:RecvHttpLine(buf);
            case RECV_HTTP_HEAD:RecvHttpHead(buf);
            case RECV_HTTP_BODY:RecvHttpBody(buf);      
       }
       return;
    }
};