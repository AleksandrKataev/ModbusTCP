#include "ModbusTCP.h"
#include <iostream>
#include <array>

int ModbusTCP::connectToDevice(Address addr)
{
	D2(std::string("connectToDevice ") + addr.GetAddressString() + "\n");
	
	tcpConnect = TCP_Socket::connectTo(addr);
	if (!tcpConnect) {
		D1(std::string("Connection fault ") + addr.GetAddressString() + "\n");
		return -1;
	}

	// Обнуляем счетчик сообщений
	lastReqId = 0;

	return 0;
}

bool ModbusTCP::readDataReq(const mbFunc func, const uint16_t addr, const uint16_t len)
{
	D2(std::string("readDataReq ") + mbFuncToString(func) + " " + std::to_string(addr) + ":" + std::to_string(len) + "\n");

	// Проверяем что функция выбрана верно
	if (func != mbFunc::READ_COILS && func != mbFunc::READ_INPUT_BITS 
		&& func != mbFunc::READ_REGS && func != mbFunc::READ_INPUT_REGS
		&& func != mbFunc::READ_EXTRA_REGS)
	{
		D1(std::string("Read function ") + std::to_string(static_cast<uint8_t>(func)) + " not supported\n");
		return false;
	}

	auto req = getReq(func, addr, len);

	D3("ReadReq", req);

	if (tcpConnect)
		return tcpConnect->Send(req.data(), req.size());

	D1("Error! Connection not possible");
	return false;
}

bool ModbusTCP::writeDataReq(const mbFunc func, const uint16_t addr, const uint16_t len, const std::vector<uint16_t> data)
{
	D2(std::string("writeDataReq ") + mbFuncToString(func) + " " + std::to_string(addr) + ":" + std::to_string(len) + "\n");

	if (data.empty()) {
		D1(std::string("writeDataReq error. Data is empty"));
		return false;
	}

	std::vector<uint8_t> pack = getReq(func, addr, len);

	int limit = 0; // Ограничитель

	switch (func)
	{
	case ModbusTCP::mbFunc::WRITE_COIL:
		pack.push_back(data[0]?0xFF:0x00);
		pack.push_back(0);
		break;
	case ModbusTCP::mbFunc::WRITE_REG:
		pack.push_back(static_cast<uint8_t>(data[0]>>8));
		pack.push_back(static_cast<uint8_t>(data[0]));
		break;
	case ModbusTCP::mbFunc::WRITE_COILS: {
		limit = 1968;
		int n = 0;
		uint8_t byte = 0;
		for (auto d : data) {
			if (d)
				byte += 1 << n;

			if (--limit == 0)
				break;

			if (++n >= 8) {
				pack.push_back(byte);
				byte = 0;
				n = 0;
			}
		}
		pack.push_back(byte);
		break;
	}
	case ModbusTCP::mbFunc::WRITE_EXTRA_REGS:
		limit = 719-123;
	case ModbusTCP::mbFunc::WRITE_REGS:
		limit += 123;
		for (auto d : data) {
			pack.push_back(static_cast<uint8_t>(d >> 8));
			pack.push_back(static_cast<uint8_t>(d));
			if (--limit == 0)
				break;
		}
		break;
	default:
		D1(std::string("Write function ") + std::to_string(static_cast<uint8_t>(func)) + " not supported\n");
		return false;
	}

	D3("WriteReq", pack);
	if (tcpConnect) 
		return tcpConnect->Send(pack.data(), pack.size());

	D1("Error! Connection not possible");
	return false;
}

int ModbusTCP::checkResponse()
{
	auto dataLen = tcpConnect->Receive();
	if (dataLen < 0) {
		D1(std::string("Data receive error. \n"));
		tcpConnect->Close();
		return -1;
	}
	if (dataLen >= 0 && tcpConnect->totalSize>6) {	// 6 минимальный размер пакета
		if (tcpConnect->buffer[2] != 0 || tcpConnect->buffer[3] != 0) {
			// Какая-то ерунда чистим буфер на всякий. 
			D1(std::string("Receive incorrect packet"));
			tcpConnect->totalSize = 0;
		}

		auto messId = (static_cast<uint16_t>(tcpConnect->buffer[0]) << 8) + static_cast<uint16_t>(tcpConnect->buffer[1]);
		if (messId != lastReqId) {
			// Идентификатор сообщения сломался, пропускаем сообщение
			D1(std::string("Message ID is incorrect"));
			tcpConnect->totalSize = 0;
			return 0;
		}
		
		uint16_t messLen = (static_cast<uint16_t>(tcpConnect->buffer[4]) << 8) + static_cast<uint16_t>(tcpConnect->buffer[5]);
		if (tcpConnect->totalSize >= (6 + messLen)) { // Нужное количестов данных пришло
			
			D3("Receive data", tcpConnect->buffer, 6 + messLen);

			parseResponse();

			// Нескольких сообщений не предполагается, поэтому чистим буфер
			tcpConnect->totalSize = 0;
			return 1;
		}
	}
	return 0;
}

std::vector<uint8_t> ModbusTCP::getReq(const mbFunc func, const uint16_t addr, uint16_t len)
{
	std::vector<uint8_t> data;

	struct {
		uint16_t id = 0;		//Transaction identifier
		uint16_t zero = 0;		//Protocol Identifier
		uint16_t messLen = 6;	// Message length
		uint8_t devAddr = 0;	// Unit Identifier
		uint8_t fc = 0;			// Function Code
		uint16_t daddr = 0;		// Data Address of the first register
		uint16_t dlen = 0;		// The total number of registers
	} request;

	// Ограничители длины
	switch (func)
	{
	case ModbusTCP::mbFunc::WRITE_COILS:
		if (len > 1968)
			len = 1968;
		break;
	case ModbusTCP::mbFunc::WRITE_REGS:
		if (len > 123)
			len = 123;
		break;
	case ModbusTCP::mbFunc::WRITE_EXTRA_REGS:
		if (len > 719)
			len = 719;
		break;
	}

	request.id = htons(getNewReqID());
	request.devAddr = devAddr;
	request.fc = static_cast<uint8_t>(func);
	request.daddr = htons(addr);
	request.dlen = htons(len);

	switch (func)
	{
	case ModbusTCP::mbFunc::WRITE_COIL:
	case ModbusTCP::mbFunc::WRITE_REG:
		//request.messLen += 2; В одиночной записи нет длины
		break;
	case ModbusTCP::mbFunc::WRITE_COILS:
		request.messLen += 1/*num of bytes*/ + (len + 7) / 8;
		break;
	case ModbusTCP::mbFunc::WRITE_REGS:
		request.messLen += 1/*num of bytes*/ + len*2;
		break;
	default:
		break;
	}
	auto mesLen = request.messLen;
	request.messLen = htons(mesLen);

	std::copy(reinterpret_cast<uint8_t*>(&request), reinterpret_cast<uint8_t*>(&request) + sizeof(request), std::back_inserter(data));

	if (func == ModbusTCP::mbFunc::WRITE_COIL || func == ModbusTCP::mbFunc::WRITE_REG) {
		// Выкидываем длину
		data.pop_back(); data.pop_back();
	}	

	if (func == ModbusTCP::mbFunc::WRITE_COILS || func == ModbusTCP::mbFunc::WRITE_REGS || func == ModbusTCP::mbFunc::WRITE_EXTRA_REGS)
		data.push_back(static_cast<uint8_t>(mesLen-7));

	// Сразу запоминаем начало и размер данных для ответа
	response.firstElement = addr;
	response.countOfElement = len;

	return data;
}

void ModbusTCP::parseResponse()
{
	auto& resp = tcpConnect->buffer;
	uint16_t messLen = (static_cast<uint16_t>(resp[4]) << 8) + static_cast<uint16_t>(resp[5]);
	response.device = resp[6];
	response.func = static_cast<mbFunc>(resp[7]&0x7F);
	// Проверяем ошибку
	if (resp[7] & 0x80) {
		response.errcode = static_cast<errCode>(resp[8]);
	}
	else {
		response.errcode = errCode::NO_ERR;

		auto& data = response.data;
		data.clear();
		auto numOfByte = resp[8];

		switch (response.func)
		{
		case ModbusTCP::mbFunc::READ_COILS:
		case ModbusTCP::mbFunc::READ_INPUT_BITS:
			data.reserve(response.countOfElement);
			for (int i = 0; i < response.countOfElement; i++) {
				data.emplace_back((resp[9+i/8] >> (i % 8)) & 0x1);
			}
			break;
		case ModbusTCP::mbFunc::READ_REGS:
		case ModbusTCP::mbFunc::READ_INPUT_REGS:
			response.countOfElement = numOfByte / 2;
			data.reserve(response.countOfElement);
			for (int i = 0; i < response.countOfElement; i++) {
				uint16_t val = (static_cast<uint16_t>(resp[9 + i*2])<<8) + resp[10 + i*2];
				data.emplace_back(val);
			}
			break;
		case ModbusTCP::mbFunc::READ_EXTRA_REGS:
			response.countOfElement = (messLen - 3) / 2;
			data.reserve(response.countOfElement / 2);
			// Копируем данные, в лоб пока не стал, так как возможно понадобится переворачивать байты
			for (int i = 0; i < response.countOfElement; i++) {
				uint16_t val = (static_cast<uint16_t>(resp[9 + i*2]) << 8) + resp[10 + i*2];
				data.emplace_back(val);
			}
			break;
		case ModbusTCP::mbFunc::WRITE_COIL:
		case ModbusTCP::mbFunc::WRITE_REG:
		case ModbusTCP::mbFunc::WRITE_COILS:
		case ModbusTCP::mbFunc::WRITE_REGS:
		case ModbusTCP::mbFunc::WRITE_EXTRA_REGS:
			break;
		default:
			response.errcode = errCode::ILLEGAL_FUNCTION;
			D1("receive unknown function");
		}
	}
}

// Ошибки
void ModbusTCP::D1(const std::string text)
{
	if (debugLevel >= 1)
		std::cout << text;
}

// События
void ModbusTCP::D2(const std::string text)
{
	if (debugLevel >= 2)
		std::cout << text;
}

// Данные
template<typename container>
void ModbusTCP::D3(const std::string text, const container& data, size_t realLength)
{
	if (debugLevel >= 3) {
		size_t sz = realLength ? realLength : data.size();

		std::cout << text << "(" << sz << "): ";
		for (int i = 0; i < sz && i < 100; i++) {
			std::cout << std::hex << (int)data[i] << " ";
			if (i == 99)
				std::cout << "...";
		}
		std::cout << std::endl;
	}
}

std::string ModbusTCP::mbFuncToString(mbFunc f)
{
	switch (f)
	{
	case ModbusTCP::mbFunc::READ_COILS:
		return "READ_COILS";
	case ModbusTCP::mbFunc::READ_INPUT_BITS:
		return "READ_INPUT_BITS";
	case ModbusTCP::mbFunc::READ_REGS:
		return "READ_REGS";
	case ModbusTCP::mbFunc::READ_INPUT_REGS:
		return "READ_INPUT_REGS";
	case ModbusTCP::mbFunc::WRITE_COIL:
		return "WRITE_COIL";
	case ModbusTCP::mbFunc::WRITE_REG:
		return "WRITE_REG";
	case ModbusTCP::mbFunc::WRITE_COILS:
		return "WRITE_COILS";
	case ModbusTCP::mbFunc::WRITE_REGS:
		return "WRITE_REGS";
	case ModbusTCP::mbFunc::READ_EXTRA_REGS:
		return "READ_EXTRA_REGS";
	case ModbusTCP::mbFunc::WRITE_EXTRA_REGS:
		return "WRITE_EXTRA_REGS";
	default:
		return "UNKNOWN_FUNCTION";
	}
}

std::string ModbusTCP::mbErrToString(errCode err) {
	switch (err)
	{
	case ModbusTCP::errCode::NO_ERR:
		return "NO_ERR";
		break;
	case ModbusTCP::errCode::ILLEGAL_FUNCTION:
		return "ILLEGAL_FUNCTION";
		break;
	case ModbusTCP::errCode::ADDRESS_NOT_AVALIABLE:
		return "ADDRESS_NOT_AVALIABLE";
		break;
	case ModbusTCP::errCode::ILLEGAL_VALUE:
		return "ILLEGAL_VALUE";
		break;
	case ModbusTCP::errCode::SERVER_FAILURE:
		return "SERVER_FAILURE";
		break;
	case ModbusTCP::errCode::ACKNOWLEDGE:
		return "ACKNOWLEDGE";
		break;
	case ModbusTCP::errCode::SERVER_BUSY:
		return "SERVER_BUSY";
		break;
	case ModbusTCP::errCode::NEGATIVE_ACK:
		return "NEGATIVE_ACK";
		break;
	case ModbusTCP::errCode::MEM_PARITY_PROB:
		return "MEM_PARITY_PROB";
		break;
	case ModbusTCP::errCode::GATEWAY_PROBLEMP:
		return "GATEWAY_PROBLEMP";
		break;
	case ModbusTCP::errCode::GATEWAY_PROBLEMF:
		return "GATEWAY_PROBLEMF";
		break;
	case ModbusTCP::errCode::BAD_DATA:
		return "BAD_DATA";
		break;
	default:
		return "UNKNOWN_ERROR";
		break;
	}
}