// Copyright (c) 2026 Rafael Valoto/Publisher. All rights reserved.
// Created for: WindowsDualsense_ds5w - Plugin to support DualSense controller on Windows.
#pragma once
#include "GCore/Templates/TGenericHardwareInfo.h"
#include "GCore/Types/Structs/Context/DeviceContext.h"
#include "Platforms/Linux/LinuxDeviceInfo.h"


namespace FLinuxPlatform
{
	struct FLinuxHardwarePolicy;
	using FLinuxHardware = GamepadCore::TGenericHardwareInfo<FLinuxHardwarePolicy>;

	struct FLinuxHardwarePolicy
	{
		FLinuxHardwarePolicy() = default;

		void Read(FDeviceContext* Context)
		{
			FLinuxDeviceInfo::Read(Context);
		}

		void Write(FDeviceContext* Context)
		{
			FLinuxDeviceInfo::Write(Context);
		}

		void Detect(std::vector<FDeviceContext>& Devices)
		{
			FLinuxDeviceInfo::Detect(Devices);
		}

		bool CreateHandle(FDeviceContext* Context)
		{
			return FLinuxDeviceInfo::CreateHandle(Context);
		}

		void InvalidateHandle(FDeviceContext* Context)
		{
			FLinuxDeviceInfo::InvalidateHandle(Context);
		}

		void ProcessAudioHaptic(FDeviceContext* Context)
		{
			FLinuxDeviceInfo::ProcessAudioHapitc(Context);
		}
	};
}
