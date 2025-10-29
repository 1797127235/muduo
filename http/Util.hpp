#pragma once
#include <unordered_map>
#include <cstddef>
#include<fstream>
#include <sstream>
#include <iomanip>
#include<vector>
#include<filesystem>

inline std::unordered_map<int, std::string> _statu_msg = {
    {100,  "Continue"},
    {101,  "Switching Protocol"},
    {102,  "Processing"},
    {103,  "Early Hints"},
    {200,  "OK"},
    {201,  "Created"},
    {202,  "Accepted"},
    {203,  "Non-Authoritative Information"},
    {204,  "No Content"},
    {205,  "Reset Content"},
    {206,  "Partial Content"},
    {207,  "Multi-Status"},
    {208,  "Already Reported"},
    {226,  "IM Used"},
    {300,  "Multiple Choice"},
    {301,  "Moved Permanently"},
    {302,  "Found"},
    {303,  "See Other"},
    {304,  "Not Modified"},
    {305,  "Use Proxy"},
    {306,  "unused"},
    {307,  "Temporary Redirect"},
    {308,  "Permanent Redirect"},
    {400,  "Bad Request"},
    {401,  "Unauthorized"},
    {402,  "Payment Required"},
    {403,  "Forbidden"},
    {404,  "Not Found"},
    {405,  "Method Not Allowed"},
    {406,  "Not Acceptable"},
    {407,  "Proxy Authentication Required"},
    {408,  "Request Timeout"},
    {409,  "Conflict"},
    {410,  "Gone"},
    {411,  "Length Required"},
    {412,  "Precondition Failed"},
    {413,  "Payload Too Large"},
    {414,  "URI Too Long"},
    {415,  "Unsupported Media Type"},
    {416,  "Range Not Satisfiable"},
    {417,  "Expectation Failed"},
    {418,  "I'm a teapot"},
    {421,  "Misdirected Request"},
    {422,  "Unprocessable Entity"},
    {423,  "Locked"},
    {424,  "Failed Dependency"},
    {425,  "Too Early"},
    {426,  "Upgrade Required"},
    {428,  "Precondition Required"},
    {429,  "Too Many Requests"},
    {431,  "Request Header Fields Too Large"},
    {451,  "Unavailable For Legal Reasons"},
    {501,  "Not Implemented"},
    {502,  "Bad Gateway"},
    {503,  "Service Unavailable"},
    {504,  "Gateway Timeout"},
    {505,  "HTTP Version Not Supported"},
    {506,  "Variant Also Negotiates"},
    {507,  "Insufficient Storage"},
    {508,  "Loop Detected"},
    {510,  "Not Extended"},
    {511,  "Network Authentication Required"}
};

inline std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac",        "audio/aac"},
    {".abw",        "application/x-abiword"},
    {".arc",        "application/x-freearc"},
    {".avi",        "video/x-msvideo"},
    {".azw",        "application/vnd.amazon.ebook"},
    {".bin",        "application/octet-stream"},
    {".bmp",        "image/bmp"},
    {".bz",         "application/x-bzip"},
    {".bz2",        "application/x-bzip2"},
    {".csh",        "application/x-csh"},
    {".css",        "text/css"},
    {".csv",        "text/csv"},
    {".doc",        "application/msword"},
    {".docx",       "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot",        "application/vnd.ms-fontobject"},
    {".epub",       "application/epub+zip"},
    {".gif",        "image/gif"},
    {".htm",        "text/html"},
    {".html",       "text/html"},
    {".ico",        "image/vnd.microsoft.icon"},
    {".ics",        "text/calendar"},
    {".jar",        "application/java-archive"},
    {".jpeg",       "image/jpeg"},
    {".jpg",        "image/jpeg"},
    {".js",         "text/javascript"},
    {".json",       "application/json"},
    {".jsonld",     "application/ld+json"},
    {".mid",        "audio/midi"},
    {".midi",       "audio/x-midi"},
    {".mjs",        "text/javascript"},
    {".mp3",        "audio/mpeg"},
    {".mpeg",       "video/mpeg"},
    {".mpkg",       "application/vnd.apple.installer+xml"},
    {".odp",        "application/vnd.oasis.opendocument.presentation"},
    {".ods",        "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt",        "application/vnd.oasis.opendocument.text"},
    {".oga",        "audio/ogg"},
    {".ogv",        "video/ogg"},
    {".ogx",        "application/ogg"},
    {".otf",        "font/otf"},
    {".png",        "image/png"},
    {".pdf",        "application/pdf"},
    {".ppt",        "application/vnd.ms-powerpoint"},
    {".pptx",       "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar",        "application/x-rar-compressed"},
    {".rtf",        "application/rtf"},
    {".sh",         "application/x-sh"},
    {".svg",        "image/svg+xml"},
    {".swf",        "application/x-shockwave-flash"},
    {".tar",        "application/x-tar"},
    {".tif",        "image/tiff"},
    {".tiff",       "image/tiff"},
    {".ttf",        "font/ttf"},
    {".txt",        "text/plain"},
    {".vsd",        "application/vnd.visio"},
    {".wav",        "audio/wav"},
    {".weba",       "audio/webm"},
    {".webm",       "video/webm"},
    {".webp",       "image/webp"},
    {".woff",       "font/woff"},
    {".woff2",      "font/woff2"},
    {".xhtml",      "application/xhtml+xml"},
    {".xls",        "application/vnd.ms-excel"},
    {".xlsx",       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml",        "application/xml"},
    {".xul",        "application/vnd.mozilla.xul+xml"},
    {".zip",        "application/zip"},
    {".3gp",        "video/3gpp"},
    {".3g2",        "video/3gpp2"},
    {".7z",         "application/x-7z-compressed"}
};

class Util
{
public:
    static size_t Split(const std::string& src,const std::string& sep,std::vector<std::string>* array)
    {
        if(array == nullptr || sep.size() == 0 )
        {
            return 0;
        }

        array->clear();

        size_t pos = 0;
        size_t start = 0;

        while((pos = src.find(sep,start)) != std::string::npos)
        {
            array->push_back(src.substr(start,pos-start));
            start = pos + sep.size();
        }

        if(start < src.size())
        {
            array->push_back(src.substr(start));
        }

        return array->size();
    }
    //读文件
    static bool ReadFile(const std::string& filename,std::string* buf)
    {
        if(buf == nullptr) return false;
        std::ifstream file(filename,std::ios::in | std::ios::binary);
        if(!file.is_open())
        {
            return false;
        }

        file.seekg(0,std::ios::end);
        std::streamsize size = file.tellg();
        if(size < 0)
        {
            file.close();
            return false;
        }

        buf->resize(static_cast<size_t>(size)); // 分配足够的空间

        file.seekg(0,std::ios::beg);
        file.read(&(*buf)[0],size);
        file.close();
        return true;
    }
    //写文件
    static bool WriteFile(const std::string& filename,const std::string& data)
    {
        std::ofstream ofs(filename,std::ios::out | std::ios::binary | std::ios::trunc);
        if(!ofs.is_open())
        {
            return false;
        }

        ofs.write(data.c_str(),data.size());
        if(!ofs.good())
        {
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }

    //URL编码
    static bool UrlEncode(const std::string& input,std::string* output,bool flag)
    {
        std::ostringstream encoded;
        encoded.fill('0');
        //转换为十六进制 并且大写
        encoded << std::hex << std::uppercase;

        for(auto c:input)
        {
            if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            {
                encoded << c;
            }
            else if(c == ' ')
            {
                if(flag)
                encoded << '+';
                else
                encoded << ' ';
            }
            else
            {
                encoded << '%' << std::setw(2) << static_cast<int>(c);
            }
        }
        *output = encoded.str();
        return true;
    }

    //URL解码
    static std::string UrlDecode(const std::string& input,bool flag = true)
    {
        std::string output;
        output.reserve(input.size());

        auto hexVal = [](char ch) -> int{
            if(ch >= '0' && ch <='9') return ch - '0';
            if(ch >= 'a' && ch <='f') return ch - 'a' + 10;
            if(ch >= 'A' && ch <='F') return ch - 'A' + 10;
            return -1;
        };

        for(int i = 0; i < input.size(); ++i)
        {
            char c = input[i];

            if(c == '+' && flag)
            {
                output.push_back(' ');
            }
            else if(c == '%')
            {
                // 需要两个十六进制字符
                if (i + 2 >= input.size()) return "";
                int hi = hexVal(input[i + 1]);
                int lo = hexVal(input[i + 2]);
                if (hi < 0 || lo < 0) return "";

                unsigned char byte = static_cast<unsigned char>((hi << 4) | lo);
                output.push_back(static_cast<char>(byte));
                i += 2; // 跳过两个十六进制字符
            }
            else {
                output.push_back(c);
            }
        }
        return output;
    }

    //响应状态吗的描述信息获取
    static std::string StatuDesc(int status)
    {
        auto it = _statu_msg.find(status);
        if(it == _statu_msg.end())
        {
            return "UnKnown";
        }
        return it->second;
    }

    //根据文件后缀名获取文件mime
    static std::string ExtMime(const std::string &filename) {
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos) return "application/octet-stream";

        std::string ext = filename.substr(pos);
        //将后缀名转换为小写
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end()) return "application/octet-stream";
        return it->second;
    }


    //判断文件是否是一个目录
    static bool IsDirectory(const std::string& path)
    {
        try {
            return std::filesystem::is_directory(path);
        } catch (const std::filesystem::filesystem_error&) {
            return false;
        }
    }
    //判断文件是否是一个普通文件
    static bool IsRegular(const std::string& path)
    {
        try {
            return std::filesystem::is_regular_file(path);
        } catch (const std::filesystem::filesystem_error&) {
            return false;
        }
    }

    //http请求的资源路径有效性判断
    // static bool ValidPath(const std::string &path) {
    //     //思想：按照/进行路径分割，根据有多少子目录，计算目录深度，有多少层，深度不能小于0
    //     std::vector<std::string> subdir;
    //     Split(path, "/", &subdir);
    //     int level = 0;
    //     for (auto &dir : subdir) {
    //         if (dir == "..") {
    //             level--; //任意一层走出相对根目录，就认为有问题
    //             if (level < 0) return false;
    //             continue;
    //         }
    //         level++;
    //     }
    //     return true;
    // }

    // http请求的资源路径有效性判断
    static bool BalidPath(const std::string& path)
    {
        if (path.empty()) return false;
        if (path[0] != '/') return false;

        // 合理长度限制（可按需调整）
        if (path.size() > 2048) return false;

        // 禁止目录穿越 & 重复斜杠
        if (path.find("..") != std::string::npos) return false;
        if (path.find("//") != std::string::npos) return false;

        // 仅允许常见URL安全字符（ASCII）
        for (unsigned char c : path) {
            if (c < 0x20 || c == 0x7F) return false; // 控制字符
            if (!(std::isalnum(c) || c=='/' || c=='-' || c=='_' || c=='.' ||
                c=='~' || c=='%' || c==':' || c=='=' || c=='&' || c=='?' || c=='+')) {
                return false;
            }
        }
        return true;
    }

    
};