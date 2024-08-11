#include "ModbusTCP.h"
#include "iostream"
#include <thread>

void ReceiveResponse(ModbusTCP &);
bool SendReq(ModbusTCP& mbDevice, const ModbusTCP::mbFunc func, const uint8_t devaddr, const uint16_t fitstReg, const uint16_t len);
bool SendWReq(ModbusTCP& mbDevice, const ModbusTCP::mbFunc func, const uint8_t devaddr, const uint16_t fitstReg, const std::vector<uint16_t>& data);

class WaitableTimer
{
public:

	WaitableTimer()
	{
		m_timer = ::CreateWaitableTimer(NULL, FALSE, NULL);
		if (!m_timer)
			throw std::runtime_error("Failed to create waitable time (CreateWaitableTimer), error:" + std::to_string(::GetLastError()));
	}

	~WaitableTimer()
	{
		::CloseHandle(m_timer);
		m_timer = NULL;
	}

	void SetAndWait(unsigned relativeTime100Ns)
	{
		LARGE_INTEGER dueTime = { 0 };
		dueTime.QuadPart = static_cast<LONGLONG>(relativeTime100Ns) * -1;

		BOOL res = ::SetWaitableTimer(m_timer, &dueTime, 0, NULL, NULL, FALSE);
		if (!res)
			throw std::runtime_error("SetAndWait: failed set waitable time (SetWaitableTimer), error:" + std::to_string(::GetLastError()));

		DWORD waitRes = ::WaitForSingleObject(m_timer, INFINITE);
		if (waitRes == WAIT_FAILED)
			throw std::runtime_error("SetAndWait: failed wait for waitable time (WaitForSingleObject)" + std::to_string(::GetLastError()));
	}

private:
	HANDLE m_timer;
};

int main(int argc, char* argv[]) 
{
	std::string ip = "127.0.0.1";

	if (argc > 1)
		ip = argv[1];

	auto addr = Address(ip,502);

	ModbusTCP mbDevice;

	mbDevice.debugLevel = 3;

	auto ret = mbDevice.connectToDevice(addr);
	if (ret) {
		std::cout << "connectToDevice return " << ret << std::endl;
		return -1;
	}
	
	// Проверка типов
	std::cout << "Type check" << std::endl;
	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_COILS, 1, 0, 127))
		ReceiveResponse(mbDevice);
	
	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_INPUT_BITS, 1, 0, 127))
		ReceiveResponse(mbDevice);
	
	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_REGS, 1, 0, 123))
		ReceiveResponse(mbDevice);
	
	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_INPUT_REGS, 1, 0, 123))
		ReceiveResponse(mbDevice);
	
	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_EXTRA_REGS, 1, 0, 700))
		ReceiveResponse(mbDevice);

	// Тест на запись
	std::cout << "Write check" << std::endl;
	std::vector<uint16_t> data;// = { 1,1,1,0,0,0,2,3,4,5 };
	for (int i = 0; i < 2000; i++)
		data.push_back(i);

	if (SendWReq(mbDevice, ModbusTCP::mbFunc::WRITE_COIL, 0, 1, data))
		ReceiveResponse(mbDevice);

	if (SendWReq(mbDevice, ModbusTCP::mbFunc::WRITE_COILS, 10, 2000, data))
		ReceiveResponse(mbDevice);

	if (SendWReq(mbDevice, ModbusTCP::mbFunc::WRITE_REG, 0, 1, data))
		ReceiveResponse(mbDevice);

	if (SendWReq(mbDevice, ModbusTCP::mbFunc::WRITE_REGS, 10, 200, data))
		ReceiveResponse(mbDevice);

	if (SendWReq(mbDevice, ModbusTCP::mbFunc::WRITE_EXTRA_REGS, 0, 10, data))
		ReceiveResponse(mbDevice);

	// Тест быстродействия вычитка 
	//std::cout << "Perfomance test (send 500 req READ_REGS)" << std::endl;
	//auto start = time(nullptr);
	//for (int i = 0; i < 500; i++) {
	//	if (mbDevice.readDataReq(ModbusTCP::mbFunc::READ_REGS, 0, 127)) {
	//		while (mbDevice.checkResponse() == 0);
	//		//for (int i = 0; i < 10; i++) {
	//		//	std::this_thread::sleep_for(std::chrono::microseconds(5000));
	//		//	auto ret = mbDevice.checkResponse();
	//		//	if (ret > 0)
	//		//		break;
	//		//}
	//	}
	//}
	//auto end = time(nullptr);
	//std::cout << "time=" << end - start << "s, speed ~ " << 127*500/(end - start) << " words/s" << std::endl;
	//
	//std::cout << "Perfomance test (send 500 req READ_EXTRA_REGS)" << std::endl;
	//start = time(nullptr);
	//for (int i = 0; i < 500; i++) {
	//	if (mbDevice.readDataReq(ModbusTCP::mbFunc::READ_EXTRA_REGS, 0, 719)) {
	//		while (mbDevice.checkResponse() == 0);
	//		//for (int i = 0; i < 10; i++) {
	//		//	std::this_thread::sleep_for(std::chrono::microseconds(5000));
	//		//	auto ret = mbDevice.checkResponse();
	//		//	if (ret > 0)
	//		//		break;
	//		//}
	//	}
	//}
	//end = time(nullptr);
	//std::cout << "time=" << end - start << "s, speed ~ " << (719 * 500) / (end - start) << " words/s" << std::endl;

	return 0;
}

bool SendWReq(ModbusTCP& mbDevice, const ModbusTCP::mbFunc func, const uint8_t devaddr, const uint16_t fitstReg, const std::vector<uint16_t> &data) {
	std::cout << "\nSend write request " << ModbusTCP::mbFuncToString(func) << " " << fitstReg << ":" << data.size() << std::endl;
	if (!mbDevice.writeDataReq(func, devaddr, fitstReg, data)) {
		std::cout << "readDataReq return err" << std::endl;
		return false;
	}
	return true;
}

bool SendReq(ModbusTCP& mbDevice, const ModbusTCP::mbFunc func, const uint8_t devaddr, const uint16_t firstReg, const uint16_t len) {
	std::cout << "\nSend data request " << ModbusTCP::mbFuncToString(func) << " " << devaddr << ":" << len << std::endl;
	if (!mbDevice.readDataReq(func, devaddr, firstReg, len)) {
		std::cout << "readDataReq return err" << std::endl;
		return false;
	}
	return true;
}

void ReceiveResponse(ModbusTCP &mbDevice) {
	//std::cout << "Wait response ";
	ModbusTCP::mbResponse resp;
	auto ret = mbDevice.getResponse(resp);
	if (ret>0) {
		std::string data;
		for (auto d : resp.data) {
			data += std::to_string(d);
			data += " ";
		}
		std::cout << std::dec << "Response:"
			<< " dev=" << (int)resp.device
			<< " func=" << static_cast<int>(resp.func) << "(" << ModbusTCP::mbFuncToString(resp.func) << ")"
			<< " errCode=" << static_cast<int>(resp.errcode) << "(" << ModbusTCP::mbErrToString(resp.errcode) << ")"
			<< " firstElement=" << resp.firstElement
			<< " countOfElement=" << resp.countOfElement
			<< "\nData=" << data
			<< std::endl;
		resp.data.clear();
	}
	if (ret < 0) {
		std::cout << "Connection error" << std::endl;
	}
}