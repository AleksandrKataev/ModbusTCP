#pragma once
#include "Socket/TCP_Socket.hxx"
#include <chrono>
#include <atomic>

// Как показывает практика (на Windows) Быстрее всего выходит если использовать блокирующий режим.
// Поскольку в рамках одного соединения стандартом не предусмотрен асинхронный режим все сделано с блокирующим режимом

class ModbusTCP {
public:
	ModbusTCP(unsigned int timeout = 1000) : timeout(timeout) {};

	static const unsigned int COILS_LIMIT = 1968;
	static const unsigned int REGS_LIMIT = 123;
	static const unsigned int REGS_EX_LIMIT = 719;

	enum class mbFunc : uint8_t {
		UNKNOWN = 0,
		READ_COILS = 0x01,
		READ_INPUT_BITS = 0x02,
		READ_REGS = 0x03,
		READ_INPUT_REGS = 0x04,
		WRITE_COIL = 0x05,
		WRITE_REG = 0x06,
		WRITE_COILS = 0x0F,
		WRITE_REGS = 0x10,
		READ_EXTRA_REGS = 67,
		WRITE_EXTRA_REGS = 70,
	};

	enum class errCode : uint8_t {
		NO_ERR = 0,
		ILLEGAL_FUNCTION = 0x01, // Function Code not Supported
		ADDRESS_NOT_AVALIABLE = 0x02,  // Output Address not exists
		ILLEGAL_VALUE = 0x03,    // Output Value not in Range
		SERVER_FAILURE = 0x04,	// Slave Deive Fails to process request
		ACKNOWLEDGE = 0x05,      // Service Need Long Time to Execute
		SERVER_BUSY = 0x06,      // Server Was Unable to Accept MB Request PDU
		NEGATIVE_ACK = 0x07,
		MEM_PARITY_PROB = 0x08,
		GATEWAY_PROBLEMP = 0x0A, // Gateway Path not Available
		GATEWAY_PROBLEMF = 0x0B, // Target Device Failed to Response
		TIMEOUT = 0xFE,
		BAD_DATA = 0XFF,			// Bad Data lenght or Address
	};

	struct mbResponse {
		uint8_t device;
		mbFunc func;
		errCode errcode; 
		uint16_t firstElement;
		uint16_t countOfElement;
		std::vector<uint16_t> data;
	}; //response = { 0 };

	int debugLevel = 0;

	// Установка связи
	int connectToDevice(const Address addr);

	// Отключение (при выходе необязательно)
	int disconnect();

	// Запрос занных
	// Останавливает поток если предыдущий ответ еще не получен!
	bool readDataReq(const mbFunc, const uint8_t devAddr, const uint16_t firstReg, const uint16_t numOfReg);

	// Запрос на запись данных
	// Останавливает поток если предыдущий ответ еще не получен!
	// Длина определяется числом элементов data, для Coil 1 элемент = 1 coil.
	bool writeDataReq(const mbFunc, const uint8_t devAddr, const uint16_t addr, const std::vector<uint16_t> data);

	// Приемник ответов
	// <0 - ошибка связи
	// =0 - нет данных
	// >0 - данные получены
	int getResponse(mbResponse &response);

	// Проверка связи
	bool isConnected() const;

	static std::string mbFuncToString(mbFunc);
	static std::string mbErrToString(errCode);

	// Проверка поддержки функции
	static bool isValidFunc(const int func);

	void setTimeout(const unsigned int time) { timeout = time; }

private:
	// Данный сокет в целом нужен для слушанья порта, но без него не запускается установка активного соединения почему-то
	TCP_Socket *tcpConnect = nullptr;

	// Подготовить запрос
	std::vector<uint8_t> getReq(const mbFunc func, const uint8_t devAddr, const uint16_t addr, uint16_t len);

	// данные последнего запроса
	// Идентификатор сообщения
	uint16_t lastReqId = 0;
	uint16_t getNewReqID() { return ++lastReqId; }
	mbResponse lastReq;

	// разбор ответов
	mbResponse parseResponse();
	
	/// Контроль ожидания ответа
	// Время ожидания ответа, мс
	unsigned int timeout;

	// Блокировка при ожидании ответа от устройства
	//std::mutex reqMutex;
	std::atomic_bool waitResponse{false};
	std::mutex wrMutex;

	// Время отправки запроса
	std::chrono::time_point< std::chrono::system_clock> reqTime;

	// Запуск блокировки запросов до получения ответа.
	//void startWatchDog();
	
	/// Отладочные методы
	// Оталадка первого уровня (сообщения об ошибках)
	void D1(const std::string) const;
	// Оталадка второго уровня (сообщения об событиях)
	void D2(const std::string) const;
	// Оталадка третьего уровня (сообщения с данными)
	template<typename container>
	void D3(const std::string text, const container& data, size_t realLength = 0) const;
};