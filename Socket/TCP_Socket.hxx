// класс TCP_Socket является ячейкой массива соединений tcpConnections
// Для организации сервера нужно созадть экземпряр класса и вызвать Open(...), 
//	далее все входящие соединения будут добавляться в tcpConnections самостоятельно.
// В tcpConnections можно добавить активное соединение, методом connectTo(...)
// Для чисто клиентского режима, создавть экземпляр не нужно. Метод connectTo(...) вернет указатель на новое соединение.
// Соединения из tcpConnections не удаляются. При попытке создать новое с тем же адресом, будет переподключено сущестующее соединение
#pragma once
#include <stdint.h>
#include "Socket.hxx"
#include <thread>
#include <list>
#include <mutex>

const uint16_t TCP_BUFFER_SIZE = 0xFFFF;

class TCP_Socket : public Socket
{
public:
	// Стандартный конструктор для сервера
	TCP_Socket();
	// Конструктор для добавления соединений
	TCP_Socket(const Address);
	TCP_Socket(const OS_socket_hndl, const Address);
	~TCP_Socket();

	virtual int Receive()  final;
	virtual bool Send(const void* data, size_t size) final;

	// Открываем порт TCP
	virtual bool Open(uint16_t port) final;

	//bool enableKeepAlive(OS_socket_hndl socket);

	// Добавление и удаление соединений происходит в разных потоках, поэтому нужен mutex
	// Для случая когда tcpConnections перебираются для анализа принятых данные и возникает необходимость 
	//	передать данные остается использовать recursive_mutex
	static std::recursive_mutex tcpConns_mutex;
	// Перечень всех подключений
	static std::list<TCP_Socket> tcpConnections;

	// Добавить активное подключение в общий список
	static TCP_Socket* connectTo(Address);

private:
	std::thread AcceptLoopThread;
};

#define SCAN_TCP_CONNECTIONS(varName) std::lock_guard<std::recursive_mutex> lck(TCP_Socket::tcpConns_mutex); \
	for (auto& varName : TCP_Socket::tcpConnections)
