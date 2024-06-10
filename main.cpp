#include "ModbusTCP.h"
#include "iostream"

void ReceiveResponse(ModbusTCP &);
bool SendReq(ModbusTCP&, const ModbusTCP::mbFunc func, const uint16_t addr, const uint16_t len);

int main(int argc, char* argv[]) {

	ModbusTCP mbDevice(1);

	mbDevice.debugLevel = 3;

	auto addr = Address(127, 0, 0, 1, 502);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);


	auto ret = mbDevice.connectToDevice(addr);
	if (ret) {
		std::cout << "connectToDevice return " << ret << std::endl;
		return -1;
	}
	
	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_COILS, 0, 16))
		ReceiveResponse(mbDevice);
	
	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_INPUT_BITS, 0, 16))
		ReceiveResponse(mbDevice);

	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_REGS, 0, 16))
		ReceiveResponse(mbDevice);

	if (SendReq(mbDevice, ModbusTCP::mbFunc::READ_INPUT_REGS, 0, 16))
		ReceiveResponse(mbDevice);

	return 0;
}

bool SendReq(ModbusTCP& mbDevice, const ModbusTCP::mbFunc func, const uint16_t addr, const uint16_t len) {
	std::cout << "\nSend data request " << ModbusTCP::mbFuncToString(func) << " " << addr << ":" << len << std::endl;
	if (!mbDevice.readDataReq(func, addr, len)) {
		std::cout << "readDataReq return err" << std::endl;
		return false;
	}
	return true;
}


void ReceiveResponse(ModbusTCP &mbDevice) {
	std::cout << "Wait response ";
	for (int i = 0; i < 100; i++) {
		std::cout << "." << std::flush;
		Sleep(10);
		auto ret = mbDevice.checkResponse();
		if (ret>0) {
			std::string data;
			for (auto d : mbDevice.response.data) {
				data += std::to_string(d);
				data += " ";
			}
			std::cout << std::dec << "Response:"
				<< " dev=" << (int)mbDevice.response.device
				<< " func=" << static_cast<int>(mbDevice.response.func) << "(" << ModbusTCP::mbFuncToString(mbDevice.response.func) << ")"
				<< " errCode=" << static_cast<int>(mbDevice.response.errcode) << "(" << ModbusTCP::mbErrToString(mbDevice.response.errcode) << ")"
				<< " firstElement=" << mbDevice.response.firstElement
				<< " countOfElement=" << mbDevice.response.countOfElement
				<< " Data=" << data
				<< std::endl;
			break;
		}
		if (ret < 0) {
			std::cout << "Connection error" << std::endl;
			break;
		}
	}
}