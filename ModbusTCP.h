#pragma once
#include "Socket/TCP_Socket.hxx"

	class ModbusTCP {
	public:
		ModbusTCP(uint8_t device = 1) : devAddr(device) { };

		enum class mbFunc : uint8_t {
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
			BAD_DATA = 0XFF,			// Bad Data lenght or Address
		};

		struct mbResponse {
			uint8_t device;
			mbFunc func;
			errCode errcode; 
			uint16_t firstElement;
			uint16_t countOfElement;
			std::vector<uint16_t> data;
		} response = { 0 };

		int debugLevel = 0;

		// ��������� �����
		int connectToDevice(const Address addr);

		// ������ ������
		bool readDataReq(const mbFunc mbFunction, const uint16_t firstReg, const uint16_t numOfReg);

		// ������ �� ������ ������
		bool writeDataReq(const mbFunc func, const uint16_t addr, const uint16_t len, const std::vector<uint16_t> data);

		// �������� �������
		// <0 - ������ �����
		// =0 - ��� ������
		// >0 - ������ ��������
		int checkResponse();

		static std::string mbFuncToString(mbFunc);
		static std::string mbErrToString(errCode);
	private:
		// ������ ����� � ����� ����� ��� �������� �����, �� ��� ���� �� ����������� ��������� ��������� ���������� ������-��
		TCP_Socket *tcpConnect;

		// ����������� ������
		std::vector<uint8_t> getReq(const mbFunc func, const uint16_t addr, const uint16_t len);

		// ����� ����������
		uint8_t devAddr = 1;

		// ������������� ���������
		uint16_t lastReqId = 0;
		uint16_t getNewReqID() { return ++lastReqId; }

		// ������ �������
		void parseResponse();

		// �������� ������� ������ (��������� �� �������)
		void D1(const std::string);
		// �������� ������� ������ (��������� �� ��������)
		void D2(const std::string);
		// �������� �������� ������ (��������� � �������)
		template<typename container>
		void D3(const std::string text, const container& data, size_t realLength = 0);

	};