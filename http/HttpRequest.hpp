#pragma once
#include<string>
#include <unordered_map>
#include<regex>
#include<unordered_map>
/*
    HTTP请求报文
    请求行  方法 URL 协议版本
    请求头
    空行
    请求体
*/
class HttpRequest
{
public:
    //请求方法
    std::string _method;
    //资源路径
    std::string _path;
    //协议版本
    std::string _version;

    //请求体
    std::string _body;

    //存储解析结果
    std::smatch _matches;

    //头部字段
    std::unordered_map<std::string, std::string> _headers;

    //查询字符串
    std::unordered_map<std::string, std::string> _params;
public:
    HttpRequest():
    _version("HTTP/1.1")
    {}

    void Reset()
    {
        _method.clear();
        _path.clear();
        _version.clear();
        _body.clear();
        _matches = {};
        _headers.clear();
        _params.clear();
    }

    void SetMethon(std::string& method)
    {
        _method = method;
    }

    void SetPath(std::string& path)
    {
        _path = path;
    }

    void SetVersion(std::string& version)
    {
        _version = version;
    }

    void SetBody(std::string& body)
    {
        _body = body;
    }

    bool HasHeader(const std::string& head) const
    {
        auto it = _headers.find(head);
        if(it == _headers.end())
        {
            return false;
        }
        return true;
    }

    void SetParms(std::string& key,std::string& value)
    {
        _params[key] = value;
    }

    void SetHeader(const std::string& key,const std::string& value)
    {
        _headers[key] = value;
    }

    //获取头部字段
    std::string GetHeader(const std::string& key) const
    {
        auto it = _headers.find(key);
        if(it == _headers.end())
        {
            return "";
        }

        return it->second;
    }

        //获取指定的查询字符串
    std::string GetParam(const std::string &key) const {
        auto it = _params.find(key);
        if (it == _params.end()) {
            return "";
        }
        return it->second;
    }


    //获取正文长度
    size_t ContentLength() const
    {
        bool ret = HasHeader("Content-Length");
        if (ret == false) {
            return 0;
        }
        std::string clen = GetHeader("Content-Length");
        return std::stol(clen);
    }


    //判断是否为短连接（是否需要在响应后关闭）
    bool Close() const
    {
        // HTTP/1.1 默认 keep-alive；HTTP/1.0 默认 close
        if (HasHeader("Connection")) {
            std::string v = GetHeader("Connection");
            // 大小写不敏感处理（简单起见只判断小写/原样）
            return !(v == "keep-alive" || v == "Keep-Alive");
        }
        // 没有头：依据版本判断
        if (_version == "HTTP/1.1") return false; // keep-alive
        return true; // 其他版本按关闭处理
    }
};