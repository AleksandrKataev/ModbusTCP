#pragma once
#include <vector>
#include <string>
#include <array>

#ifdef _WIN32
    #pragma comment( lib, "wsock32.lib" )
    #include <winsock2.h>

    #define OS_socket_hndl SOCKET
    #define OS_SOCKET_INVALID INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <fcntl.h>
    #include <unistd.h>

    #define OS_socket_hndl int
    #define OS_SOCKET_INVALID -1
    #define SD_BOTH SHUT_RDWR
    #define SOCKET_ERROR -1
#endif

class Address {
public:
    Address() : address(0), port(0) { }
    Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) : port(port) {
        address = (a << 24) | (b << 16) | (c << 8) | d;
    }
    Address(uint32_t addr, uint16_t port) : address(addr), port(port) { }
    Address(const Address& A) : address(A.address), port(A.port) { }

    uint32_t GetAddress() const { return address; }
    std::string GetAddressString() const;
    std::wstring GetAddressWString() const;
    uint16_t GetPort() const { return port; }

    bool operator== (Address addr) { return (address == addr.address) && (port == addr.port); }
    bool operator< (const Address& addr) const {
        if (address == addr.address)
            return port < addr.port;
        else
            return address < addr.address;
    }
private:
    uint32_t address;
    uint16_t port;
};

const uint16_t SOCKET_SEND_BUFFER_MAX_SIZE = 65507; // SO_MAX_MSG_SIZE

class Socket {
public:
    Socket();

    ~Socket();

    virtual bool Open(uint16_t port) = 0;

    virtual bool Close();

    virtual bool isOpen() { return socket != OS_SOCKET_INVALID; }

    // Отправляет данные по адресу addr
    virtual bool Send(const void* data, size_t size) = 0;

    // Принимает данные в buffer, увеличивает totalSize
    // Возвращает totalSize если изменился, 0 если ничего нового, код ошибки отрицательный
    virtual int Receive() = 0;
    
    // Откуда пришли данные, либо куда отправить
    Address addr = Address();

    // буфер для накопления данных
    std::array<uint8_t, SOCKET_SEND_BUFFER_MAX_SIZE> buffer;
    // Не нашел причины делать буфер диамическим
    // Если делать через буфер, то нужно как-то не давать пользователю его обнулять
    //std::vector<uint8_t> buffer;

    // начало куда писать новые данные
    //uint16_t bufOffset = 0; // по идее не нужен, должно хватить totalSize
    
    // общая длина данных
    // растет после каждого успешного приема
    // Очищается пользователем при полном приеме пакета
    uint16_t totalSize = 0;

protected:
    // Для автоматического определения когда нужно запускать InitializeSockets
    // socketNum еще используется для определения что InitializeSockets прошло корректно
    static int socketNum;
    // Сам сокет, он не зависит от TCP/UDP
    OS_socket_hndl socket = OS_SOCKET_INVALID;
};