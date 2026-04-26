// Copyright (c) 2026 Rafael Valoto/Publisher. All rights reserved.
// Created for: WindowsDualsense_ds5w - Plugin to support DualSense controller on Windows.
#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief HID protocol constants for Sony DualSense and DualShock 4 controllers.
 *
 * Public hardware identifiers and report sizes, reverse-engineered by the
 * community and documented in the Linux hid-playstation kernel driver,
 * psdevwiki.com, and the ds4drv / ds5w projects. They describe the HID
 * framing used to talk to the hardware and are referenced by every platform
 * backend (Windows, Linux, …) so duplicating the magic numbers per-backend
 * isn't necessary.
 */
namespace SonyHIDProtocol
{
	// USB Vendor ID — Sony Interactive Entertainment.
	constexpr std::uint16_t SONY_VENDOR_ID = 0x054C;

	// Product IDs.
	constexpr std::uint16_t DUALSHOCK4_V1_PID = 0x05C4;   // PS4 launch model
	constexpr std::uint16_t DUALSHOCK4_V2_PID = 0x09CC;   // PS4 Slim model
	constexpr std::uint16_t DUALSENSE_PID = 0x0CE6;       // PS5 controller
	constexpr std::uint16_t DUALSENSE_EDGE_PID = 0x0DF2;

	// Input report lengths (bytes).
	constexpr std::size_t DUALSHOCK4_BLUETOOTH_INPUT_LEN = 547;  // extended sensor mode
	constexpr std::size_t DUALSENSE_USB_INPUT_LEN = 64;
	constexpr std::size_t BLUETOOTH_INPUT_LEN = 78;              // DualSense over BT

	// Output report lengths (bytes).
	constexpr std::size_t DUALSHOCK4_USB_OUTPUT_LEN = 32;
	constexpr std::size_t DUALSENSE_USB_OUTPUT_LEN = 74;
	constexpr std::size_t BLUETOOTH_OUTPUT_LEN = 78;             // DualSense over BT

	// Audio-haptics output buffer (DualSense only).
	constexpr std::size_t AUDIO_HAPTICS_OUTPUT_LEN = 142;

	// Calibration feature report — returns gyro/accel offsets.
	constexpr std::uint8_t CALIBRATION_FEATURE_ID = 0x05;
	constexpr std::size_t CALIBRATION_FEATURE_LEN = 41;
}
