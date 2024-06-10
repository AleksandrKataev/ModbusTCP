#include "UDP_Socket.hxx"
#include <thread>

UDP_Socket::UDP_Socket() : Socket(UDP_BUFFER_SIZE) {
}

UDP_Socket::~UDP_Socket() {
	Close();
};

// port может быть равен 0, система сама выделит свободный
bool UDP_Socket::Open(uint16_t port) {

	socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (socket == OS_SOCKET_INVALID)
	{
		//printf("failed to create socket\n");
		return false;
	}
	
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	if (bind(socket, (const sockaddr*)&address, sizeof(sockaddr_in)) < 0)
	{
		//printf("failed to bind socket\n");
		return false;
	}

	#ifdef _WIN32
		u_long nonBlocking = 1;
		if (ioctlsocket(socket, FIONBIO, &nonBlocking) != 0)
		{
			//printf("failed to set non-blocking socket\n");
			return false;
		}
	#else
		int nonBlocking = 1;
		if (fcntl(socket, F_SETFL, O_NONBLOCK, nonBlocking) == -1)
		{
			//printf("failed to set non-blocking socket\n");
			return false;
		}
	#endif
	return true;
}

int UDP_Socket::Receive() {

	#ifdef _WIN32
		typedef int socklen_t;
	#endif

	if (!isOpen())
		return -1;

	sockaddr_in from;
	socklen_t fromLength = sizeof(from);

	int received_bytes = recvfrom(socket, (char*)buffer.data()+totalSize, static_cast<int>(buffer.size() - totalSize),
		0, (sockaddr*)&from, &fromLength);

	if (received_bytes > 0) {
		//после получения можем посмотреть, откуда пришло.
		unsigned int from_address = ntohl(from.sin_addr.s_addr);
		uint16_t from_port = ntohs(from.sin_port);
		addr = Address(from_address, from_port);
		totalSize += received_bytes;
	}

	#ifdef _WIN32
	// WSAEWOULDBLOCK означает данных нет, не критично
	// WSAECONNRESET конечная точка предлагает заткнуться, но мы не реагируем
	if (received_bytes == SOCKET_ERROR)
		if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAECONNRESET)
			received_bytes = 0;
	#else
		if (received_bytes == SOCKET_ERROR)
			if (errno == EAGAIN)
				received_bytes = 0;
	#endif

	// Возвращаем только длину новых данных, чтобы сообщить что пришло что-то новое
	return received_bytes;
}

bool UDP_Socket::Send(const void* data, size_t size) {

	if (!isOpen())
		return false;

	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(addr.GetAddress());
	address.sin_port = htons(addr.GetPort());

	int sent_bytes;
	for (int i = 0; i < 5; i++) {
		sent_bytes = sendto(socket, (const char*)data, (int)size, 0, (sockaddr*)&address, sizeof(sockaddr_in));
		// Если в силу перегрузки интерфейса не удалось отправить данные, это нормально, пытаемся еще раз
		if (sent_bytes != size && 
			#ifdef _WIN32
			WSAGetLastError() == WSAEWOULDBLOCK
			#else
			(errno == EWOULDBLOCK || errno == EAGAIN)
			#endif
			) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		break;
	}
	if (sent_bytes != size)
		return false;

	return true;
};

