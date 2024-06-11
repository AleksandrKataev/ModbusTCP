#pragma once
#include "Socket.hxx"

#ifdef _WIN32
#pragma comment( lib, "wsock32.lib" )
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#endif

class UDP_Socket : public Socket
{
public:
    UDP_Socket();
    ~UDP_Socket();
    // port может быть равен 0, система сама выделит свободный
    virtual bool Open(uint16_t port) final;
    bool Send(const Address& destination, const void* data, size_t size) {
        addr = destination;
        return Send(data, size);
    }
    virtual bool Send(const void* data, size_t size) final;
    virtual int Receive() final;
};
