#ifndef  COMMON_H
#define  COMMON_H
#include <string>
#include<mutex>
#include<memory>

struct ClientInfo
{
    int fd;
    std::string username;
    std::string nickname;
    bool logged_in;
    std::string  inbuf;
    std::mutex send_mutex;

    ClientInfo(int _fd) :fd(_fd), logged_in(false){}
};

#endif