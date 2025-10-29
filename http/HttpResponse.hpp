#pragma once
#include<string>
#include <unordered_map>

class HttpResponse
{
private:
    int _code;
    std::string _version;
    bool _redirect_flag;
    std::unordered_map<std::string, std::string> _headers;
    std::string _body;
    std::string _redirect_url;
public:
    HttpResponse(int code = 200):
    _code(code),
    _version("HTTP/1.1"),
    _redirect_flag(false)
    {}

    void Reset()
    {
        _code = 200;
        _version.clear();
        _redirect_flag = false;
        _headers.clear();
        _body.clear();
        _redirect_url.clear();
    }

    void SetVersion(std::string version)
    {
        _version = version;
    }

    void SetRedirect(bool flag)
    {
        _redirect_flag = flag;
    }

    void SetHeader(const std::string& key,const std::string& value)
    {
        _headers[key] = value;
    }

    void SetBody(std::string body)
    {
        _body = body;
    }

    void SetRedirectUrl(std::string url)
    {
        _redirect_url = url;
    }

    bool HasHeader(const std::string& key)
    {
        auto it = _headers.find(key);
        if(it == _headers.end())
        {
            return false;
        }

        return true;
    }

    std::string GetHeader(const std::string& key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end()) {
            return "";
        }
        return it->second;
    }

    void SetContent(const std::string &body,  const std::string &type = "text/html")
    {
            _body = body;
            SetHeader("Content-Type", type);
    }


    void SetRedirect(const std::string &url, int code = 302)
    {
            _code = code;
            _redirect_flag = true;
            _redirect_url = url;
    }

    int GetCode()
    {
        return _code;
    }

    std::string& GetBody()
    {
        return _body;
    }

    bool IsRedirect() const
    {
        return _redirect_flag;
    }

    std::string GetRedirectUrl() const
    {
        return _redirect_url;
    }

    std::unordered_map<std::string, std::string> GetHeaders() const
    {
        return _headers;
    }

    void SetCode(int code)
    {
        _code = code;
    }


    //判断是否是短链接
    bool Close() 
    {
        // 没有Connection字段，或者有Connection但是值是close，则都是短链接，否则就是长连接
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive") {
            return false;
        }
        return true;
    }


};