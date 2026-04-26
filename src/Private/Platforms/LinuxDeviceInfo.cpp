// Copyright (c) 2026 Rafael Valoto/Publisher. All rights reserved.
// Created for: WindowsDualsense_ds5w - Plugin to support DualSense controller on Windows.
//
// Linux HID device backend, mirroring src/Private/Platforms/WindowsDeviceInfo.cpp.
// Ported from src/GamepadCore/Examples/Platform_Linux/CommonsDeviceInfo.cpp,
// which uses SDL_hidapi; this implementation calls hidapi (libhidapi-hidraw)
// directly so the build doesn't depend on SDL2-dev. The hid_* and SDL_hid_*
// function signatures are 1:1.

#ifdef __unix__

#include "Platforms/Linux/LinuxDeviceInfo.h"
#include "Platforms/SonyHIDProtocol.h"
#include "GCore/Types/ECoreGamepad.h"
#include "GCore/Types/Structs/Config/GamepadCalibration.h"
#include "GCore/Types/Structs/Context/DeviceContext.h"
#include "GImplementations/Utils/GamepadSensors.h"
#include <hidapi.h>
#include <cstring>
#include <string>
#include <unordered_set>

void FLinuxDeviceInfo::Read(FDeviceContext* Context)
{
	if (!Context || !Context->Handle)
	{
		return;
	}
	hid_device* DeviceHandle = static_cast<hid_device*>(Context->Handle);
	if (!DeviceHandle)
	{
		return;
	}

	if (Context->ConnectionType == EDSDeviceConnection::Bluetooth && Context->DeviceType == EDSDeviceType::DualShock4)
	{
		const size_t InputReportLength = SonyHIDProtocol::DUALSHOCK4_BLUETOOTH_INPUT_LEN;
		if (hid_read(DeviceHandle, Context->BufferDS4, InputReportLength) < 0)
		{
			InvalidateHandle(Context);
		}
		return;
	}

	const size_t InputReportLength = (Context->ConnectionType == EDSDeviceConnection::Bluetooth)
	                                     ? SonyHIDProtocol::BLUETOOTH_INPUT_LEN
	                                     : SonyHIDProtocol::DUALSENSE_USB_INPUT_LEN;
	if (sizeof(Context->Buffer) < InputReportLength)
	{
		InvalidateHandle(Context);
		return;
	}

	if (hid_read(DeviceHandle, Context->Buffer, InputReportLength) < 0)
	{
		InvalidateHandle(Context);
	}
}

void FLinuxDeviceInfo::ProcessAudioHapitc(FDeviceContext* Context)
{
	if (!Context || !Context->Handle)
	{
		return;
	}

	hid_device* DeviceHandle = static_cast<hid_device*>(Context->Handle);

	constexpr size_t Report = SonyHIDProtocol::AUDIO_HAPTICS_OUTPUT_LEN;
	int BytesWritten = hid_write(DeviceHandle, Context->BufferAudio, Report);
	(void)BytesWritten;
}

bool FLinuxDeviceInfo::ConfigureFeatures(FDeviceContext* Context)
{
	hid_device* DeviceHandle = static_cast<hid_device*>(Context->Handle);

	unsigned char FeatureBuffer[SonyHIDProtocol::CALIBRATION_FEATURE_LEN] = {0};
	std::memset(FeatureBuffer, 0, sizeof(FeatureBuffer));

	FeatureBuffer[0] = SonyHIDProtocol::CALIBRATION_FEATURE_ID;
	if (hid_get_feature_report(DeviceHandle, FeatureBuffer, SonyHIDProtocol::CALIBRATION_FEATURE_LEN) <= 0)
	{
		return false;
	}

	using namespace FGamepadSensors;
	FGamepadCalibration Calibration;
	DualSenseCalibrationSensors(FeatureBuffer, Calibration);

	Context->Calibration = Calibration;
	return true;
}

void FLinuxDeviceInfo::Write(FDeviceContext* Context)
{
	if (!Context || !Context->Handle)
	{
		return;
	}

	hid_device* DeviceHandle = static_cast<hid_device*>(Context->Handle);

	const size_t InReportLength = (Context->DeviceType == EDSDeviceType::DualShock4)
	                                  ? SonyHIDProtocol::DUALSHOCK4_USB_OUTPUT_LEN
	                                  : SonyHIDProtocol::DUALSENSE_USB_OUTPUT_LEN;
	const size_t OutputReportLength = (Context->ConnectionType == EDSDeviceConnection::Bluetooth)
	                                      ? SonyHIDProtocol::BLUETOOTH_OUTPUT_LEN
	                                      : InReportLength;

	int BytesWritten = hid_write(DeviceHandle, Context->BufferOutput, OutputReportLength);
	if (BytesWritten < 0)
	{
		InvalidateHandle(Context);
	}
}

void FLinuxDeviceInfo::Detect(std::vector<FDeviceContext>& Devices)
{
	Devices.clear();

	const std::unordered_set<std::uint16_t> SupportedPIDs = {
	    SonyHIDProtocol::DUALSHOCK4_V1_PID,
	    SonyHIDProtocol::DUALSHOCK4_V2_PID,
	    SonyHIDProtocol::DUALSENSE_PID,
	    SonyHIDProtocol::DUALSENSE_EDGE_PID};

	hid_device_info* Devs = hid_enumerate(SonyHIDProtocol::SONY_VENDOR_ID, 0);
	if (!Devs)
	{
		return;
	}

	for (hid_device_info* CurrentDevice = Devs; CurrentDevice != nullptr; CurrentDevice = CurrentDevice->next)
	{
		if (SupportedPIDs.contains(CurrentDevice->product_id))
		{
			FDeviceContext NewDeviceContext;
			NewDeviceContext.Path = std::string(CurrentDevice->path);

			switch (CurrentDevice->product_id)
			{
				case SonyHIDProtocol::DUALSHOCK4_V1_PID:
				case SonyHIDProtocol::DUALSHOCK4_V2_PID:
					NewDeviceContext.DeviceType = EDSDeviceType::DualShock4;
					break;
				case SonyHIDProtocol::DUALSENSE_EDGE_PID:
					NewDeviceContext.DeviceType = EDSDeviceType::DualSenseEdge;
					break;
				case SonyHIDProtocol::DUALSENSE_PID:
				default:
					NewDeviceContext.DeviceType = EDSDeviceType::DualSense;
					break;
			}

			NewDeviceContext.IsConnected = true;
			if (CurrentDevice->interface_number == -1)
			{
				NewDeviceContext.ConnectionType = EDSDeviceConnection::Bluetooth;
			}
			else
			{
				NewDeviceContext.ConnectionType = EDSDeviceConnection::Usb;
			}
			NewDeviceContext.Handle = nullptr;
			Devices.push_back(NewDeviceContext);
		}
	}
	hid_free_enumeration(Devs);
}

bool FLinuxDeviceInfo::CreateHandle(FDeviceContext* Context)
{
	if (!Context)
	{
		return false;
	}

	const char* Path = Context->Path.data();
	hid_device* Handle = hid_open_path(Path);
	if (Handle == nullptr)
	{
		return false;
	}

	hid_set_nonblocking(Handle, 1);
	Context->Handle = static_cast<FPlatformDeviceHandle>(Handle);

	ConfigureFeatures(Context);
	return true;
}

void FLinuxDeviceInfo::InvalidateHandle(FDeviceContext* Context)
{
	if (Context)
	{
		hid_device* DeviceHandle = static_cast<hid_device*>(Context->Handle);
		if (DeviceHandle != nullptr)
		{
			hid_close(DeviceHandle);
		}

		Context->Handle = INVALID_PLATFORM_HANDLE;
		Context->IsConnected = false;

		Context->Path.clear();
		std::memset(Context->Buffer, 0, sizeof(Context->Buffer));
		std::memset(Context->BufferDS4, 0, sizeof(Context->BufferDS4));
		std::memset(Context->BufferOutput, 0, sizeof(Context->BufferOutput));
		std::memset(Context->BufferAudio, 0, sizeof(Context->BufferAudio));
	}
}

#endif // __unix__
