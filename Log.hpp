#pragma once
#include <iostream>
#include <mutex>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include <cstdarg>
#include <fstream>
#include <cstring>
#include <pthread.h>
#include<mutex>


namespace log_ns
{

    enum
    {
        DEBUG = 1,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    std::string LevelToString(int level)
    {
        switch (level)
        {
        case DEBUG:
            return "DEBUG";
        case INFO:
            return "INFO";
        case WARNING:
            return "WARNING";
        case ERROR:
            return "ERROR";
        case FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
        }
    }

    std::string GetCurrTime()
    {
        time_t now = time(nullptr);
        struct tm *curr_time = localtime(&now);
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%d-%02d-%02d %02d:%02d:%02d",
                 curr_time->tm_year + 1900,
                 curr_time->tm_mon + 1,
                 curr_time->tm_mday,
                 curr_time->tm_hour,
                 curr_time->tm_min,
                 curr_time->tm_sec);
        return buffer;
    }

    class logmessage
    {
    public:
        std::string _level;
        pid_t _id;
        std::string _filename;
        int _filenumber;
        std::string _curr_time;
        std::string _message_info;
    };

#define SCREEN_TYPE 1
#define FILE_TYPE 2

    const std::string glogfile = "./log.txt";
    pthread_mutex_t glock = PTHREAD_MUTEX_INITIALIZER;

    // log.logMessage("", 12, INFO, "this is a %d message ,%f, %s hellwrodl", x, , , );
    class Log
    {
    public:
        Log(const std::string &logfile = glogfile) : _logfile(logfile), _type(SCREEN_TYPE)
        {
        }
        void Enable(int type)
        {
            _type = type;
        }
        void FlushLogToScreen(const logmessage &lg)
        {
            printf("[%s][%d][%s][%d][%s] %s",
                   lg._level.c_str(),
                   lg._id,
                   lg._filename.c_str(),
                   lg._filenumber,
                   lg._curr_time.c_str(),
                   lg._message_info.c_str());
        }
        void FlushLogToFile(const logmessage &lg)
        {
            std::ofstream out(_logfile, std::ios::app);
            if (!out.is_open())
                return;
            char logtxt[2048];
            snprintf(logtxt, sizeof(logtxt), "[%s][%d][%s][%d][%s] %s",
                     lg._level.c_str(),
                     lg._id,
                     lg._filename.c_str(),
                     lg._filenumber,
                     lg._curr_time.c_str(),
                     lg._message_info.c_str());
            out.write(logtxt, strlen(logtxt));
            out.close();
        }
        void FlushLog(const logmessage &lg)
        {
            // 加过滤逻辑 --- TODO

            std::lock_guard<std::mutex> lockguard(_mutex);
            switch (_type)
            {
            case SCREEN_TYPE:
                FlushLogToScreen(lg);
                break;
            case FILE_TYPE:
                FlushLogToFile(lg);
                break;
            }
        }
        void logMessage(std::string filename, int filenumber, int level, const char *format, ...)
        {
            logmessage lg;

            lg._level = LevelToString(level);
            lg._id = getpid();
            lg._filename = filename;
            lg._filenumber = filenumber;
            lg._curr_time = GetCurrTime();

            va_list ap;
            va_start(ap, format);
            char log_info[1024];
            vsnprintf(log_info, sizeof(log_info), format, ap);
            va_end(ap);
            lg._message_info = log_info;

            // 打印出来日志
            FlushLog(lg);
        }
        ~Log()
        {
        }

    private:
        int _type;
        std::string _logfile;
        std::mutex _mutex;
    };

    Log lg;

#define LOG(Level, Format, ...)                                        \
    do                                                                 \
    {                                                                  \
        lg.logMessage(__FILE__, __LINE__, Level, Format, ##__VA_ARGS__); \
    } while (0)
#define EnableScreen()          \
    do                          \
    {                           \
        lg.Enable(SCREEN_TYPE); \
    } while (0)
#define EnableFILE()          \
    do                        \
    {                         \
        lg.Enable(FILE_TYPE); \
    } while (0)
};