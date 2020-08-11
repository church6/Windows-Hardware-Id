#include <iostream>
#include <vector>
#include <string>
#include <Windows.h>
#include <Winioctl.h>
#include <sysinfoapi.h>
#include <urlmon.h>
#include <Shlwapi.h>

#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Shlwapi.lib")


class HardwareId {

private:


	struct RawSMBIOSData {
		BYTE	Used20CallingMethod;
		BYTE	SMBIOSMajorVersion;
		BYTE	SMBIOSMinorVersion;
		BYTE	DmiRevision;
		DWORD	Length;
		BYTE	SMBIOSTableData[1];
	};

	struct DMI_HEADER {
		BYTE Type;
		BYTE Length;
		WORD Handle;
	};

	struct MoboData {
		DMI_HEADER Header;
		UCHAR	Manufacturer;
		UCHAR	Product;
		UCHAR	Version;
		UCHAR	SerialNumber;
	};

	typedef struct SMBIOS {
		const char* Manufacturer;
		const char* Product;
		const char* Version;
		const char* SerialNumber;
		const char* UUID;
		const char* SKU;
		const char* Family;
	};


	const char* SMBIOSToString(const DMI_HEADER* Header, BYTE Entry) {
		auto HeaderString{ const_cast<char*>(reinterpret_cast<const char*>(Header)) };
		SIZE_T i{ 0 };

		if (Entry == 0) {
			return "null";
		}

		HeaderString += Header->Length;

		while (Entry > 1 && *HeaderString) {
			HeaderString += strlen(HeaderString);
			HeaderString++;
			Entry--;
		}

		if (!*HeaderString) {
			return "null";
		}

		SIZE_T Length{ strlen(HeaderString) };

		for (i = 0; i < Length; i++) {
			if (HeaderString[i] < 32 || HeaderString[i] == 127) {
				HeaderString[i] = '.';
			}
		}

		return HeaderString;
	}


	template <typename T>
	T GetHKLM(const char* SubKey, const char* Value) {
		DWORD Size{};

		RegGetValueA(HKEY_LOCAL_MACHINE, SubKey, Value, RRF_RT_ANY, nullptr, nullptr, &Size);
		static T* Buffer{ reinterpret_cast<T*>(VirtualAlloc(nullptr, Size, MEM_COMMIT, PAGE_READWRITE)) };
		RegGetValueA(HKEY_LOCAL_MACHINE, SubKey, Value, RRF_RT_ANY, nullptr, (PVOID)Buffer, &Size);

		return *Buffer;
	}

	const char* GetHKLM(const char* SubKey, const char* Value) {
		DWORD Size{};
		std::string Ret{};

		RegGetValueA(HKEY_LOCAL_MACHINE, SubKey, Value, RRF_RT_REG_SZ, nullptr, nullptr, &Size);
		Ret.resize(Size);
		RegGetValueA(HKEY_LOCAL_MACHINE, SubKey, Value, RRF_RT_REG_SZ, nullptr, &Ret[0], &Size);

		return Ret.c_str();
	}


	STORAGE_DEVICE_DESCRIPTOR* QueryDiskInformation() {
		DWORD IoRetBytes{ NULL };
		STORAGE_DESCRIPTOR_HEADER Header{ NULL };
		STORAGE_PROPERTY_QUERY QueryInfo{ StorageDeviceProperty, PropertyStandardQuery };

		HANDLE hDisk{ CreateFileW(L"\\\\.\\PhysicalDrive0", 0, 0, nullptr, OPEN_EXISTING, 0, nullptr) };

		if (hDisk != INVALID_HANDLE_VALUE) {
			if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &QueryInfo, sizeof(QueryInfo),&Header, sizeof(Header), &IoRetBytes, nullptr)) {
				if (auto DiskInfo{ static_cast<STORAGE_DEVICE_DESCRIPTOR*>(VirtualAlloc(nullptr, Header.Size, MEM_COMMIT, PAGE_READWRITE)) }) {
					DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &QueryInfo, sizeof(QueryInfo), DiskInfo, Header.Size, &IoRetBytes, nullptr);
					return DiskInfo;
				}
			}
		}

		return nullptr;
	}


	void Init() {

		//
		// SMBIOS
		//

		DWORD RequiredSize{ GetSystemFirmwareTable('RSMB', 0, NULL, 0) };

		if (auto FirmwareTable{ static_cast<RawSMBIOSData*>( malloc( GetSystemFirmwareTable('RSMB', 0, nullptr, 0) ) ) }) {
			GetSystemFirmwareTable('RSMB', 0, FirmwareTable, RequiredSize);

			for (int i = 0; i < FirmwareTable->Length; i++) {
				auto Header = (DMI_HEADER*)&FirmwareTable->SMBIOSTableData;

				if (Header->Type == 1) {
					this->SMBIOS.Manufacturer = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x4]);
					this->SMBIOS.Product = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x5]);
					this->SMBIOS.Version = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x6]);
					this->SMBIOS.SerialNumber = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x7]);
					this->SMBIOS.SKU = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x19]);
					this->SMBIOS.Family = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x1a]);

					free(FirmwareTable);
					break;
				}

				Header += Header->Length;

				while (*reinterpret_cast<USHORT*>(Header) != 0) {
					Header++;
				} Header += 2;
			}
		}
		

		//
		// Disk
		//

		if (auto Disk{ QueryDiskInformation() }) {
			this->Disk.SerialNumber = reinterpret_cast<char*>(Disk) + Disk->SerialNumberOffset;
			this->Disk.Vendor = reinterpret_cast<char*>(Disk) + Disk->VendorIdOffset;
			this->Disk.Product = reinterpret_cast<char*>(Disk) + Disk->ProductIdOffset;
		}


		//
		// Processor
		//

		SYSTEM_INFO ProcessorInfo{ 0 };
		GetSystemInfo(&ProcessorInfo);

		this->CPU.Architecture = ProcessorInfo.wProcessorArchitecture;
		this->CPU.ProcessorLevel = ProcessorInfo.wProcessorLevel;
		this->CPU.ActiveProcessorMask = ProcessorInfo.dwActiveProcessorMask;

		static std::vector <DWORD> CPUFeatures({ 25, 24, 26, 27, 18, 7, 16, 2, 14, 15, 23, 1, 0, 3, 12, 9, 8, 22, 20, 13, 21, 6, 10, 17, 29, 30, 31, 34 });

		for (int i = 0; i < CPUFeatures.size(); i++) {
			if (IsProcessorFeaturePresent(CPUFeatures.at(i))) {
				this->CPU.Features->push_back(CPUFeatures.at(i));
				this->CPU.Hash += CPUFeatures.at(i) * (CPUFeatures.at(i) + this->CPU.Architecture);
			} else {
				CPUFeatures.erase(CPUFeatures.begin() + i);
			}
		} this->CPU.Features = &CPUFeatures;


		//
		// Windows Registry Identifiers
		//

		this->Windows.MachineGUID = GetHKLM("SOFTWARE\\Microsoft\\Cryptography", "MachineGuid");
		this->Windows.ComputerHardwareId = GetHKLM("SYSTEM\\CurrentControlSet\\Control\\SystemInformation", "ComputerHardwareId");
		this->Windows.SQMClientMachineId = GetHKLM("SOFTWARE\\Microsoft\\SQMClient", "MachineId");
		this->Windows.BuildLab = GetHKLM("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "BuildLab");
		this->Windows.InstallTime = GetHKLM<DWORD64>("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "InstallTime");
		this->Windows.InstallDate = GetHKLM<DWORD64>("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "InstallDate");
		this->Windows.BuildGUID = GetHKLM<DWORD64>("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "BuildGUID");

		GenerateHWID();
	};


public:


	DWORD64 Hash{ 0 };


	struct {

		const char* SerialNumber;
		const char* Vendor;
		const char* Product;

	} Disk;


	struct {

		const char* Manufacturer;
		const char* Product;
		const char* Version;
		const char* SerialNumber;
		const char* SKU;
		const char* Family;

	} SMBIOS;


	struct {

		std::vector <DWORD>* Features{ new std::vector <DWORD> };
		WORD Architecture;
		WORD ProcessorLevel;
		DWORD64 ActiveProcessorMask;
		DWORD64 Hash;

	} CPU;


	struct {

		const char* MachineGUID;
		const char* ComputerHardwareId;
		const char* SQMClientMachineId;
		const char* BuildLab;
		DWORD64 InstallTime;
		DWORD64 InstallDate;
		DWORD64 BuildGUID;

	} Windows;


	DWORD64 GenerateHWID() {
		this->Hash =
			this->Hash ?
			this->Hash :
			this->CPU.Hash * std::stoull(this->Disk.SerialNumber) * this->SMBIOS.Product[0, 3] * this->SMBIOS.Manufacturer[0, 3];

		return this->Hash;
	}


	std::unique_ptr<HardwareId> Pointer() {
		return std::make_unique<HardwareId>(this);
	}


	HardwareId() {
		int Flag{ 0 };

		do {

			Init();
			++Flag;

		} while(!this->Hash && Flag != 10);
	}
};


DWORD64 GetWhitelistedHWID() {
	const char Buffer[256]{ 0 };
	DWORD BytesRead{ 0 };
	char cPath[256]{ 0 };
	std::string Path{ 0 };

	GetTempPathA(MAX_PATH + 1, cPath);

	URLDownloadToFileA(
		nullptr, 
		"http://m.uploadedit.com/busd/1597102075361.txt", 
		(Path = std::string(cPath).append("\\zList.txt")).c_str(), 
		NULL, 
		nullptr
	);
	
	HANDLE hFile{ CreateFileA(Path.c_str(), FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr) };
	ReadFile(hFile, (PVOID)&Buffer[0], 256, &BytesRead, nullptr);

	CloseHandle(hFile);
	DeleteFileA(Path.c_str());
	return strtoul(Buffer, nullptr, 16);
}


int main() {
    if (auto Hash{ HardwareId().Hash }) {
	    if (auto Whitelisted{ GetWhitelistedHWID() }) {
		    if (Hash != GetWhitelistedHWID()) {
			   std::cout << "Invalid License: " << std::hex << std::uppercase << Hash << " " << Whitelisted;
		    } else {
		 	   std::cout << "Valid License: " << std::hex << std::uppercase << Hash << " " << Whitelisted;
		    }
	    }
    }

    getchar();
    return 0;
}
