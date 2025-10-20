#pragma once
#include<vector>
#include<cstdint>
/*
    服务器缓冲区模块
    存取数据
    取出数据
    面向字节流
*/
class Buffer
{
public:


private:
    std::vector<char> _buffer;
    uint64_t _readIndex;
    uint64_t _writeIndex;
};