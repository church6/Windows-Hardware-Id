#include "hwid.h"

int main() {
	auto HWID{ HardwareId() };

	std::cout << HWID.Disk.SerialNumber << std::endl;
	std::cout << HWID.SMBIOS.Manufacturer << std::endl;
	std::cout << HWID.CPU.Architecture << std::endl;
	std::cout << HWID.Windows.ComputerHardwareId << std::endl;

	for (int i = 0; i < HWID.CPU.Features.size(); i++) {
		std::cout << HWID.CPU.Features.at(i) << std::endl;
	}

	getchar();
    return 0;
}