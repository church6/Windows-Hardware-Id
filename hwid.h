#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <Windows.h>


struct DMI_HEADER {
	BYTE Type;
	BYTE Length;
	WORD Handle;
};

struct RawSMBIOSData {
	BYTE	Used20CallingMethod;
	BYTE	SMBIOSMajorVersion;
	BYTE	SMBIOSMinorVersion;
	BYTE	DmiRevision;
	DWORD	Length;
	BYTE	SMBIOSTableData[1];
};


class HardwareId {

private:


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
		T Null{};
		DWORD Size{};

		RegGetValueA(HKEY_LOCAL_MACHINE, SubKey, Value, RRF_RT_ANY, nullptr, nullptr, &Size);
		static T* Buffer{ reinterpret_cast<T*>(VirtualAlloc(nullptr, Size, MEM_COMMIT, PAGE_READWRITE)) };
		RegGetValueA(HKEY_LOCAL_MACHINE, SubKey, Value, RRF_RT_ANY, nullptr, (PVOID)Buffer, &Size);

		return Buffer ? *Buffer : Null;
	}


	const char* GetHKLM(const char* SubKey, const char* Value) {
		DWORD Size{};
		std::string Ret{};

		RegGetValueA(HKEY_LOCAL_MACHINE, SubKey, Value, RRF_RT_REG_SZ, nullptr, nullptr, &Size);
		Ret.resize(Size);
		RegGetValueA(HKEY_LOCAL_MACHINE, SubKey, Value, RRF_RT_REG_SZ, nullptr, &Ret[0], &Size);

		return Ret.c_str();
	}


	void GetHardwareId() {


		//
		// Disk
		//


		STORAGE_DEVICE_DESCRIPTOR* DiskInfo{ QueryDisk() };
		
		this->Disk.SerialNumber = reinterpret_cast<char*>(DiskInfo) + DiskInfo->SerialNumberOffset;
		this->Disk.Vendor = reinterpret_cast<char*>(DiskInfo) + DiskInfo->VendorIdOffset;
		this->Disk.Product = reinterpret_cast<char*>(DiskInfo) + DiskInfo->ProductIdOffset;


		//
		// SMBIOS
		//


		RawSMBIOSData* FirmwareTable{ QuerySMBIOS() };
		DMI_HEADER* Header{ reinterpret_cast<DMI_HEADER*>(FirmwareTable->SMBIOSTableData) };

		this->SMBIOS.Manufacturer = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x4]);
		this->SMBIOS.Product = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x5]);
		this->SMBIOS.Version = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x6]);
		this->SMBIOS.SerialNumber = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x7]);
		this->SMBIOS.SKU = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x19]);
		this->SMBIOS.Family = SMBIOSToString(Header, reinterpret_cast<BYTE*>(Header)[0x1a]);

		free(FirmwareTable);


		//
		// Processor
		//


		SYSTEM_INFO ProcessorInfo{ QueryProcessor(this->CPU.Features) };

		this->CPU.Architecture = ProcessorInfo.wProcessorArchitecture;
		this->CPU.ProcessorLevel = ProcessorInfo.wProcessorLevel;
		this->CPU.ActiveProcessorMask = ProcessorInfo.dwActiveProcessorMask;


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
	}


public:


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

		WORD Architecture;
		WORD ProcessorLevel;
		DWORD64 ActiveProcessorMask;
		std::vector <DWORD> Features;

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


	STORAGE_DEVICE_DESCRIPTOR* QueryDisk() {
		DWORD IoRetBytes{ NULL };
		STORAGE_DESCRIPTOR_HEADER StorageHeader{ NULL };
		STORAGE_PROPERTY_QUERY QueryInfo{ StorageDeviceProperty, PropertyStandardQuery };

		HANDLE hDisk{ CreateFileW(L"\\\\.\\PhysicalDrive0", 0, 0, nullptr, OPEN_EXISTING, 0, nullptr) };

		if (hDisk != INVALID_HANDLE_VALUE) {
			if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &QueryInfo, sizeof(QueryInfo),&StorageHeader, sizeof(StorageHeader), &IoRetBytes, nullptr)) {
				if (auto DiskInfo{ static_cast<STORAGE_DEVICE_DESCRIPTOR*>(malloc(StorageHeader.Size)) }) {
					if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &QueryInfo, sizeof(QueryInfo), DiskInfo, StorageHeader.Size, &IoRetBytes, nullptr)) {
						CloseHandle(hDisk);
						return DiskInfo;
					}
				}
			}

			CloseHandle(hDisk);
		}

		return nullptr;
	}


	RawSMBIOSData* QuerySMBIOS() {
		DWORD RequiredSize{ GetSystemFirmwareTable('RSMB', 0, NULL, 0) };

		if (auto FirmwareTable{ static_cast<RawSMBIOSData*>(malloc(GetSystemFirmwareTable('RSMB', 0, nullptr, 0))) }) {
			GetSystemFirmwareTable('RSMB', 0, FirmwareTable, RequiredSize);

			for (int i = 0; i < FirmwareTable->Length; i++) {
				static auto Header{ reinterpret_cast<DMI_HEADER*>(&FirmwareTable->SMBIOSTableData) };

				if (Header->Type == 1) {
					return FirmwareTable;
				}

				Header += Header->Length;

				while (*reinterpret_cast<USHORT*>(Header) != 0) {
					Header++;
				} Header += 2;
			}
		}

		return nullptr;
	}


	SYSTEM_INFO QueryProcessor(std::vector <DWORD> &OutFeatures) {
		SYSTEM_INFO ProcessorInfo{ 0 };
		GetSystemInfo(&ProcessorInfo);

		std::vector <DWORD> CPUFeatures({ 
			25, 24, 26, 27, 18, 7, 16, 2, 14, 15, 23, 1, 0, 3, 12, 9, 8, 22, 20, 13, 21, 6, 10, 17, 29, 30, 31, 34 
		});

		for (int i = 0; i < CPUFeatures.size(); i++) {
			if (IsProcessorFeaturePresent(CPUFeatures.at(i))) {
				OutFeatures.push_back(CPUFeatures.at(i));
			} else {
				CPUFeatures.erase(CPUFeatures.begin() + i);
			}
		} 

		return ProcessorInfo;
	}


	std::unique_ptr<HardwareId> Pointer() {
		return std::make_unique<HardwareId>(*this);
	}


	HardwareId() {
		GetHardwareId();
	}
};