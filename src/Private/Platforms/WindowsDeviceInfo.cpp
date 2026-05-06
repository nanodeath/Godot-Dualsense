// Copyright (c) 2025 Rafael Valoto/Publisher. All rights reserved.
// Created for: WindowsDualsense_ds5w - Plugin to support DualSense controller on Windows.
// Planned Release Year: 2025

#include "Platforms/Windows/WindowsDeviceInfo.h"
#include "Platforms/SonyHIDProtocol.h"
#include "GCore/Types/DSCoreTypes.h"
#include "GCore/Types/Structs/Config/GamepadCalibration.h"
#include "GCore/Types/Structs/Context/DeviceContext.h"
#include "GImplementations/Utils/GamepadSensors.h"
#include <filesystem>
#include <hidsdi.h>
#include <setupapi.h>

void FWindowsDeviceInfo::Detect(std::vector<FDeviceContext>& Devices)
{
	GUID HidGuid;
	HidD_GetHidGuid(&HidGuid);

	const HDEVINFO DeviceInfoSet = SetupDiGetClassDevs(&HidGuid, nullptr, nullptr,
	                                                   DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (DeviceInfoSet == INVALID_HANDLE_VALUE)
	{
		return;
	}

	SP_DEVICE_INTERFACE_DATA DeviceInterfaceData = {};
	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	std::unordered_map<int, std::string> DevicePaths;
	for (int DeviceIndex = 0; SetupDiEnumDeviceInterfaces(DeviceInfoSet, nullptr, &HidGuid, DeviceIndex,
	                                                      &DeviceInterfaceData);
	     DeviceIndex++)
	{
		DWORD RequiredSize = 0;
		SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, nullptr, 0, &RequiredSize, nullptr);

		const auto DetailDataBuffer = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(malloc(RequiredSize));
		if (!DetailDataBuffer)
		{
			continue;
		}

		DetailDataBuffer->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if (SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, DetailDataBuffer, RequiredSize,
		                                    nullptr, nullptr))
		{
			const HANDLE TempDeviceHandle = CreateFileW(
			    DetailDataBuffer->DevicePath,
			    GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, NULL, nullptr);

			if (TempDeviceHandle != INVALID_HANDLE_VALUE)
			{
				HIDD_ATTRIBUTES Attributes = {};
				Attributes.Size = sizeof(HIDD_ATTRIBUTES);

				if (HidD_GetAttributes(TempDeviceHandle, &Attributes))
				{
					if (
					    Attributes.VendorID == SonyHIDProtocol::SONY_VENDOR_ID &&
					    (Attributes.ProductID == SonyHIDProtocol::DUALSENSE_PID ||
					     Attributes.ProductID == SonyHIDProtocol::DUALSENSE_EDGE_PID ||
					     Attributes.ProductID == SonyHIDProtocol::DUALSHOCK4_V1_PID ||
					     Attributes.ProductID == SonyHIDProtocol::DUALSHOCK4_V2_PID))
					{
						FDeviceContext Context = {};
						wchar_t DeviceProductString[260];
						if (HidD_GetProductString(TempDeviceHandle, DeviceProductString, 260))
						{
							if (DevicePaths.find(DeviceIndex) == DevicePaths.end())
							{
								std::string FinalString = std::filesystem::path(DetailDataBuffer->DevicePath).string();
								Context.Path = FinalString;
								DevicePaths[DeviceIndex] = FinalString;
								switch (Attributes.ProductID)
								{
									case SonyHIDProtocol::DUALSHOCK4_V1_PID:
									case SonyHIDProtocol::DUALSHOCK4_V2_PID:
										Context.DeviceType = EDSDeviceType::DualShock4;
										break;
									case SonyHIDProtocol::DUALSENSE_EDGE_PID:
										Context.DeviceType = EDSDeviceType::DualSenseEdge;
										break;
									default: Context.DeviceType = EDSDeviceType::DualSense;
								}

								Context.IsConnected = true;
								Context.ConnectionType = EDSDeviceConnection::Usb;
								const std::string BtGuid = "{00001124-0000-1000-8000-00805f9b34fb}";
								if (FinalString.find(BtGuid) != std::string::npos ||
								    FinalString.find("bth") != std::string::npos ||
								    FinalString.find("BTHENUM") != std::string::npos)
								{
									Context.ConnectionType = EDSDeviceConnection::Bluetooth;
								}
							}
							Devices.push_back(Context);
						}
					}
				}
			}
			CloseHandle(TempDeviceHandle);
		}
		free(DetailDataBuffer);
	}
	SetupDiDestroyDeviceInfoList(DeviceInfoSet);
}

void FWindowsDeviceInfo::Read(FDeviceContext* Context)
{
	if (!Context)
	{
		return;
	}

	if (Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return;
	}

	if (!Context->IsConnected)
	{
		return;
	}

	DWORD BytesRead = 0;
	if (Context->ConnectionType == EDSDeviceConnection::Bluetooth && Context->DeviceType == EDSDeviceType::DualShock4)
	{
		constexpr size_t InputReportLength = SonyHIDProtocol::DUALSHOCK4_BLUETOOTH_INPUT_LEN;
		PollTick(Context->Handle, Context->BufferDS4, InputReportLength, BytesRead);
	}
	else
	{
		const size_t InputBufferSize = Context->ConnectionType == EDSDeviceConnection::Bluetooth
		                                   ? SonyHIDProtocol::BLUETOOTH_INPUT_LEN
		                                   : SonyHIDProtocol::DUALSENSE_USB_INPUT_LEN;
		PollTick(Context->Handle, Context->Buffer, InputBufferSize, BytesRead);
	}
}

void FWindowsDeviceInfo::Write(FDeviceContext* Context)
{
	if (Context->Handle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	size_t InReportLength = Context->DeviceType == EDSDeviceType::DualShock4
	                            ? SonyHIDProtocol::DUALSHOCK4_USB_OUTPUT_LEN
	                            : SonyHIDProtocol::DUALSENSE_USB_OUTPUT_LEN;
	size_t OutputReportLength = Context->ConnectionType == EDSDeviceConnection::Bluetooth
	                                ? SonyHIDProtocol::BLUETOOTH_OUTPUT_LEN
	                                : InReportLength;

	DWORD BytesWritten = 0;
	if (!WriteFile(Context->Handle, Context->GetRawOutputBuffer(), OutputReportLength, &BytesWritten, nullptr))
	{
	}
}

bool FWindowsDeviceInfo::CreateHandle(FDeviceContext* DeviceContext)
{
	std::string Source = DeviceContext->Path;
	std::wstring MyStdString = std::filesystem::path(Source).wstring();
	const HANDLE DeviceHandle = CreateFileW(
	    MyStdString.data(),
	    GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, NULL, nullptr);

	if (DeviceHandle == INVALID_PLATFORM_HANDLE)
	{
		DeviceContext->Handle = DeviceHandle;
		return false;
	}

	DeviceContext->Handle = DeviceHandle;
	ConfigureFeatures(DeviceContext);
	return true;
}

void FWindowsDeviceInfo::InvalidateHandle(FDeviceContext* Context)
{
	if (!Context)
	{
		return;
	}

	if (Context->Handle != INVALID_PLATFORM_HANDLE)
	{
		CloseHandle(Context->Handle);
		Context->Handle = INVALID_PLATFORM_HANDLE;
		Context->IsConnected = false;
		Context->Path.clear();

		// BufferOutput is 78 bytes (private member; size matches GamepadCore's
		// FDeviceContext::BufferOutput[78] declaration).
		ZeroMemory(Context->GetRawOutputBuffer(), 78);
		ZeroMemory(Context->BufferHapitcs, sizeof(Context->BufferHapitcs));
		ZeroMemory(Context->Buffer, sizeof(Context->Buffer));
		ZeroMemory(Context->BufferDS4, sizeof(Context->BufferDS4));
	}
}

void FWindowsDeviceInfo::InvalidateHandle(HANDLE Handle)
{
	if (Handle != INVALID_PLATFORM_HANDLE)
	{
		CloseHandle(Handle);
	}
}

EPollResult FWindowsDeviceInfo::PollTick(HANDLE Handle, unsigned char* Buffer, std::int32_t Length, DWORD& OutBytesRead)
{
	std::int32_t Err = ERROR_SUCCESS;
	PingOnce(Handle, &Err);

	OutBytesRead = 0;
	if (!ReadFile(Handle, Buffer, Length, &OutBytesRead, nullptr))
	{
		return EPollResult::Disconnected;
	}

	return EPollResult::ReadOk;
}

bool FWindowsDeviceInfo::PingOnce(HANDLE Handle, std::int32_t* OutLastError)
{
	FILE_STANDARD_INFO Info{};
	if (!GetFileInformationByHandleEx(Handle, FileStandardInfo, &Info, sizeof(Info)))
	{
		if (OutLastError)
		{
			*OutLastError = GetLastError();
		}
		return false;
	}
	if (OutLastError)
	{
		*OutLastError = ERROR_SUCCESS;
	}
	return true;
}

void FWindowsDeviceInfo::ProcessAudioHapitc(FDeviceContext* Context)
{
	if (!Context || !Context->Handle)
	{
		return;
	}

	if (Context->Handle == INVALID_PLATFORM_HANDLE)
	{
		return;
	}

	if (Context->ConnectionType != EDSDeviceConnection::Bluetooth)
	{
		return;
	}

	unsigned long BytesWritten = 0;
	constexpr size_t BufferSize = SonyHIDProtocol::AUDIO_HAPTICS_OUTPUT_LEN;
	if (!WriteFile(Context->Handle, Context->BufferHapitcs, BufferSize, &BytesWritten, nullptr))
	{
		const unsigned long Error = GetLastError();
		if (Error != ERROR_IO_PENDING)
		{
		}
	}
}

void FWindowsDeviceInfo::ConfigureFeatures(FDeviceContext* Context)
{
	unsigned char FeatureBuffer[SonyHIDProtocol::CALIBRATION_FEATURE_LEN] = {0};
	std::memset(FeatureBuffer, 0, sizeof(FeatureBuffer));

	FeatureBuffer[0] = SonyHIDProtocol::CALIBRATION_FEATURE_ID;
	if (!HidD_GetFeature(Context->Handle, FeatureBuffer, SonyHIDProtocol::CALIBRATION_FEATURE_LEN))
	{
		const unsigned long Error = GetLastError();
		return;
	}

	FGamepadCalibration Calibration;
	using namespace FGamepadSensors;
	DualSenseCalibrationSensors(FeatureBuffer, Calibration);

	Context->Calibration = Calibration;
}
