#include "Socket.hxx"

std::string Address::GetAddressString() const {
    std::string ret;
    ret += std::to_string(address >> 24);
    ret += ".";
    ret += std::to_string((address >> 16) & 0xFF);
    ret += ".";
    ret += std::to_string((address >> 8) & 0xFF);
    ret += ".";
    ret += std::to_string(address & 0xFF);
    return ret;
}
std::wstring Address::GetAddressWString() const {
    std::wstring ret;
    ret += std::to_wstring(address >> 24);
    ret += L".";
    ret += std::to_wstring((address >> 16) & 0xFF);
    ret += L".";
    ret += std::to_wstring((address >> 8) & 0xFF);
    ret += L".";
    ret += std::to_wstring(address & 0xFF);
    return ret;
}


int Socket::socketNum = 0;

inline bool InitializeSockets()
{
#ifdef _WIN32
    WSADATA WsaData = { 0 };
    return WSAStartup(MAKEWORD(2, 2), &WsaData) == NO_ERROR;
#else
    return true;
#endif
}

Socket::Socket(uint32_t max_rec_size) : buffer(max_rec_size) {
    if (socketNum == 0)
        if (!InitializeSockets())
            return;

    socketNum++;
}

Socket::~Socket() {
    socketNum--;
#ifdef _WIN32
    if (socketNum == 0)
        WSACleanup();
#endif
}

bool Socket::Close() {
    if (!isOpen()) {
        return true;
    }
    shutdown(socket, SD_BOTH);
#ifdef _WIN32
    if (closesocket(socket) == SOCKET_ERROR)
#else
    if (close(socket) == -1)
#endif
    {
        socket = OS_SOCKET_INVALID;
        return false;
    }
    socket = OS_SOCKET_INVALID;
    return true;
}