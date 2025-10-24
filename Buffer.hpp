#pragma once
#include <cassert>
#include <string>
#include <sys/types.h>
#include<vector>
#include<cstring>
#include<cstdint>
/*
    服务器缓冲区模块
    存取数据
    取出数据
    面向字节流
*/
const uint64_t BUFFER_DEFAULT_SIZE = 1024;

class Buffer
{
public:
    char* Begin() {return &*_buffer.begin();}
    //获取写入位置
    char* WritePosition() {return Begin() + _writeIndex;}
    //获取读取位置
    char* ReadPosition() {return Begin() + _readIndex;}
    //获取缓冲区末尾空闲空间大小
    uint64_t TailIdleSize() {return _buffer.size() - _writeIndex;}
    //获取缓冲区起始空间大小
    uint64_t HeadIdleSize() {return _readIndex;}
    //获取可读数据大小
    uint64_t ReadAbleSize() {return _writeIndex - _readIndex;}
    //将读偏移向后移动
    void MoveReadOffset(uint64_t len)
    {
        if(len == 0) return ;
        assert(len <= ReadAbleSize());
        _readIndex += len;
    }
    //将写偏移向后移动
    void MoveWriteOffset(uint64_t len)
    {
        if(len == 0) return ;
        assert(len <= TailIdleSize());
        _writeIndex += len;
    }

    char* FindCRLF()
    {
        char* res = (char*)memchr(ReadPosition(),'\n',ReadAbleSize());
        return res;
    }


public:
    Buffer():_readIndex(0), _writeIndex(0), _buffer(BUFFER_DEFAULT_SIZE) {}
    //确保可写空间足够
    void EnsureWriteSpace(uint64_t len)
    {
        if(len <= TailIdleSize())
        {
            return ;
        }
        else if(len <= HeadIdleSize() + TailIdleSize())
        {
            //整体偏移到最前面
            uint64_t readable = ReadAbleSize();
            std::memmove(Begin(),ReadPosition(),readable);
            _readIndex = 0;
            _writeIndex = readable;
        }
        else  
        {
            //扩容
            //先将数据整体移动到最前面
            // uint64_t readable = ReadAbleSize();
            // std::memmove(Begin(),ReadPosition(),readable);
            // _readIndex = 0;
            // _writeIndex = readable;
            // //扩容
            // uint64_t newCapacity = _buffer.size() * 2;
            // while(newCapacity < _writeIndex + len)
            // {
            //     newCapacity *= 2;
            // }
            // _buffer.resize(newCapacity);

            //不移动数据直接扩容
            _buffer.resize(_writeIndex + len);
        }
    }

    //写入数据
    bool Write(const void* data, uint64_t len)
    {
        EnsureWriteSpace(len);
        void * p = std::memcpy(WritePosition(),data,len);
        if(p == nullptr)
        {
            return false;
        }
        MoveWriteOffset(len);
        return true;
    }
    //读取数据
    bool Read(void* data, uint64_t len)
    {
        if(len > ReadAbleSize())
        {
            return false;
        }
        std::memcpy(data,ReadPosition(),len);
        MoveReadOffset(len);
        return true;
    }

    bool WriteString(const std::string &str)
    {
        bool ret = Write(str.data(),str.size());
        if(ret)
        {
            MoveWriteOffset(str.size());
            return true;
        }
        return false;
    }

    bool ReadString(std::string * str,uint64_t len)
    {
        if(len > ReadAbleSize())
        {
            return false;
        }
        str->assign(ReadPosition(),len);
        MoveReadOffset(len);
        return true;
    }

    std::string GetLine()
    {
        char* pos = FindCRLF();
        if(pos == nullptr)
        {
            return "";
        }

        std:: string line;
        ReadString(&line,pos - ReadPosition() + 1);
        return line;
    }


    //清空缓冲区
    void Clear()
    {
        _readIndex = _writeIndex = 0;
    }

private:
    std::vector<char> _buffer;
    uint64_t _readIndex;
    uint64_t _writeIndex;
};