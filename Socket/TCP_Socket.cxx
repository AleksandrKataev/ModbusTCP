#include "TCP_Socket.hxx"
#include <climits>

#ifdef _WIN32
    // link with Ws2_32.lib
    #pragma comment(lib,"Ws2_32.lib")
    #include <mstcpip.h>
    #include <tchar.h>
#else
    #include <netinet/tcp.h>
    #define _tprintf printf
    #define _T(x) x
#endif

#ifdef _WIN32 // Windows NT
typedef int SockLen_t;
typedef SOCKADDR_IN SocketAddr_in;
typedef u_long ka_prop_t;
// Макросы для выражений зависимых от OS
#define WIN(exp) exp
#define NIX(exp)

// Конвертировать WinSocket код ошибки в Posix код ошибки
inline int convertError() {
    switch (WSAGetLastError()) {
    case 0:
        return 0;
    case WSAEINTR:
        return EINTR;
    case WSAEINVAL:
        return EINVAL;
    case WSA_INVALID_HANDLE:
        return EBADF;
    case WSA_NOT_ENOUGH_MEMORY:
        return ENOMEM;
    case WSA_INVALID_PARAMETER:
        return EINVAL;
    case WSAENAMETOOLONG:
        return ENAMETOOLONG;
    case WSAENOTEMPTY:
        return ENOTEMPTY;
    case WSAEWOULDBLOCK:
        return EAGAIN;
    case WSAEINPROGRESS:
        return EINPROGRESS;
    case WSAEALREADY:
        return EALREADY;
    case WSAENOTSOCK:
        return ENOTSOCK;
    case WSAEDESTADDRREQ:
        return EDESTADDRREQ;
    case WSAEMSGSIZE:
        return EMSGSIZE;
    case WSAEPROTOTYPE:
        return EPROTOTYPE;
    case WSAENOPROTOOPT:
        return ENOPROTOOPT;
    case WSAEPROTONOSUPPORT:
        return EPROTONOSUPPORT;
    case WSAEOPNOTSUPP:
        return EOPNOTSUPP;
    case WSAEAFNOSUPPORT:
        return EAFNOSUPPORT;
    case WSAEADDRINUSE:
        return EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;
    case WSAENETDOWN:
        return ENETDOWN;
    case WSAENETUNREACH:
        return ENETUNREACH;
    case WSAENETRESET:
        return ENETRESET;
    case WSAECONNABORTED:
        return ECONNABORTED;
    case WSAECONNRESET:
        return ECONNRESET;
    case WSAENOBUFS:
        return ENOBUFS;
    case WSAEISCONN:
        return EISCONN;
    case WSAENOTCONN:
        return ENOTCONN;
    case WSAETIMEDOUT:
        return ETIMEDOUT;
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case WSAELOOP:
        return ELOOP;
    case WSAEHOSTUNREACH:
        return EHOSTUNREACH;
    default:
        return EIO;
    }
}
#else // POSIX
typedef socklen_t SockLen_t;
typedef struct sockaddr_in SocketAddr_in;
typedef int ka_prop_t;
// Макросы для выражений зависимых от OS
#define WIN(exp)
#define NIX(exp) exp
#endif

std::recursive_mutex TCP_Socket::tcpConns_mutex;
std::list<TCP_Socket> TCP_Socket::tcpConnections;

// Конфигурация Keep-Alive соединения
struct KeepAliveConfig {
    ka_prop_t ka_idle = 30; // Время после последнего пакета данных
    ka_prop_t ka_intvl = 3; // Период отправки тестовых пакетов после истечения ka_idle
    ka_prop_t ka_cnt = 5; // Количество попыток отправки тестовых пакетов
};

KeepAliveConfig ka_conf;    // Пока глобальная конфигурация, возможно в будущем переделаю на настраиваемую

// Функция запуска и конфигурации Keep-Alive для сокета
bool enableKeepAlive(OS_socket_hndl socket) {
    int flag = 1;
#ifdef _WIN32
    tcp_keepalive ka{ 1, ka_conf.ka_idle * 1000, ka_conf.ka_intvl * 1000 };
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&flag, sizeof(flag)) != 0) return false;
    unsigned long numBytesReturned = 0;
    if (WSAIoctl(socket, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &numBytesReturned, 0, nullptr) != 0) return false;
    return true;
#else //POSIX
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) == -1) return false;
    if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &ka_conf.ka_idle, sizeof(ka_conf.ka_idle)) == -1) return false;
    if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &ka_conf.ka_intvl, sizeof(ka_conf.ka_intvl)) == -1) return false;
    if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &ka_conf.ka_cnt, sizeof(ka_conf.ka_cnt)) == -1) return false;
#endif
    return true;
}

void handlingAcceptLoop(OS_socket_hndl& sock)
{
    SockLen_t addrlen = sizeof(SocketAddr_in);
    while (sock != OS_SOCKET_INVALID) {
        SocketAddr_in client_addr;
        // Принятие нового подключения (блокирующий вызов)
        OS_socket_hndl client_socket = accept(sock, (struct sockaddr*)&client_addr, &addrlen);
        if (client_socket WIN(!= 0)NIX(>= 0) && sock != OS_SOCKET_INVALID) {
            // Если получен сокет с ошибкой продолжить ожидание
            if (client_socket == OS_SOCKET_INVALID) continue;

            // Активировать Keep-Alive для клиента
            if (!enableKeepAlive(client_socket)) {
                shutdown(client_socket, 0);
                WIN(closesocket)NIX(close)(client_socket);
            }

            unsigned int from_address = ntohl(client_addr.sin_addr.s_addr);
            uint16_t from_port = ntohs(client_addr.sin_port);
            Address addr = Address(from_address, from_port);

            //auto port = addr.GetPort();
            //_tprintf(_T("accept addr  = %s:%hu\n"), addr.GetAddressWString().c_str(), port);

            //в неблокирующий режим 
            unsigned long block = 1;
            WIN(ioctlsocket(client_socket, FIONBIO, &block);)   // На линуксе не завелось
            NIX(const int flags = fcntl(client_socket, F_GETFL, 0);
            fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);)

            // Добавить клиента в список клиентов
            TCP_Socket::tcpConns_mutex.lock();
            TCP_Socket::tcpConnections.emplace_back(client_socket,addr);
            TCP_Socket::tcpConns_mutex.unlock();
        }
    }
    //_tprintf(_T("handlingAcceptLoop closed\n"));
}

TCP_Socket::TCP_Socket()
{
    socket = OS_SOCKET_INVALID;
}

TCP_Socket::TCP_Socket(const Address addr)
{
    this->addr = addr;
}

TCP_Socket::TCP_Socket(const OS_socket_hndl socket, const Address addr)
{
    this->socket = socket;
    this->addr = addr;
}

TCP_Socket::~TCP_Socket() {
    Close(); 
    if (AcceptLoopThread.joinable())
        AcceptLoopThread.join(); 
}

bool TCP_Socket::Open(uint16_t port)
{
    if (isOpen())
        Close();

    if (socketNum == 0)
        return false;

    socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!isOpen()) {
        _tprintf(_T("socket function failed with error = %d\n"), WIN(WSAGetLastError())NIX(socket)) ;
        return false;
    }

    // Задаём адрес сервера
    SocketAddr_in address;
    // INADDR_ANY - любой IP адрес
    address.sin_addr
        WIN(.S_un.S_addr)NIX(.s_addr) = INADDR_ANY;
    // Задаём порт сервера
    address.sin_port = htons(port);
    // Семейство сети AF_INET - IPv4 (AF_INET6 - IPv6)
    address.sin_family = AF_INET;

    int flag = true;
    // Устанавливаем параметр сокета SO_REUSEADDR в true (подробнее https://it.wikireading.ru/7093)
    if ((setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, WIN((char*)) & flag, sizeof(flag)) == -1) ||
        // Привязываем к сокету адрес и порт
        (bind(socket, (struct sockaddr*)&address, sizeof(address)) WIN(== SOCKET_ERROR)NIX(< 0)))
    {
        Close();
        return false;
    }
    
    // Активируем ожидание фходящих соединений
    if (listen(socket, SOMAXCONN) WIN(== SOCKET_ERROR)NIX(< 0)) {
        Close();
        return false;
    }
    AcceptLoopThread = std::thread(&handlingAcceptLoop,std::ref(socket));

	return true;
}

TCP_Socket* TCP_Socket::connectTo(Address addr)
{
    SocketAddr_in address;
    TCP_Socket* tcpCon = nullptr;

    const std::lock_guard<std::recursive_mutex> lock(tcpConns_mutex);

    // Ищем существующее соединение
    for (auto& con : tcpConnections) {
        if (con.addr == addr)
            tcpCon = &con;
    }
    // Если оно есть и соединение устновлено, возвращаем его
    if (tcpCon && tcpCon->isOpen())
        return tcpCon;

    // Если нет, создаем соединение сразу, чтобы прошла инициализцаия сокетов
    if (!tcpCon){
        tcpConnections.emplace_back(addr);
        tcpCon = &tcpConnections.back();
    }

    // Создание TCP сокета
    tcpCon->socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (tcpCon->socket WIN(== INVALID_SOCKET) NIX(< 0)) return nullptr;

    new(&address) SocketAddr_in;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(addr.GetAddress());
    address.sin_port = htons(addr.GetPort());

    // Установка соединения
    if (connect(tcpCon->socket, (sockaddr*)&address, sizeof(address))
        WIN(== SOCKET_ERROR)NIX(!= 0)) 
    {
        WIN(closesocket)NIX(close)(tcpCon->socket);
        return nullptr;
    }

    // Активация Keep-Alive
    if (!enableKeepAlive(tcpCon->socket)) {
        shutdown(tcpCon->socket, 0);
        WIN(closesocket)NIX(close)(tcpCon->socket);
    }
    
    // в неблокирующий режим 
    WIN(unsigned long block = 1;
        ioctlsocket(tcpCon->socket, FIONBIO, &block));  // На линуксе не компилируется
    NIX(const int flags = fcntl(tcpCon->socket, F_GETFL, 0);
        fcntl(tcpCon->socket, F_SETFL, flags | O_NONBLOCK));

    return tcpCon;
}

int TCP_Socket::Receive()
{
    if (!isOpen())
        return -1;

    int received_bytes = recv(socket, (char*)buffer.data() + totalSize, static_cast<int>(buffer.size() - totalSize), NIX(MSG_DONTWAIT)WIN(0));

    if (received_bytes > 0) {
        totalSize += received_bytes;
    }

    #ifdef _WIN32
    // WSAEWOULDBLOCK означает данных нет, не критично
    if (received_bytes == SOCKET_ERROR)
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            received_bytes = 0;
    #else
    if (received_bytes == SOCKET_ERROR)
        if (errno == EAGAIN)
            received_bytes = 0;
    #endif

    // Возвращаем только длину новых данных, чтобы сообщить что пришло что-то новое
    return received_bytes;
}

bool TCP_Socket::Send(const void* data, size_t size)
{
    // Если сокет закрыт вернуть false
    if (!isOpen()) return false;
    if (size > INT_MAX)
        return false;
    // Отправить сообщение
    int sendlen = send(socket, reinterpret_cast<const char*>(data), static_cast<int>(size), 0);
    if (sendlen != size ) return false;
    return true;
}