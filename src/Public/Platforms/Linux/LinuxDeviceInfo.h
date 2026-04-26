// Copyright (c) 2026 Rafael Valoto/Publisher. All rights reserved.
// Created for: WindowsDualsense_ds5w - Plugin to support DualSense controller on Windows.
#pragma once

#include "GCore/Types/Structs/Context/DeviceContext.h"
#include <vector>

/**
 * @brief Linux HID backend for the GodotDualsense GDExtension.
 *
 * Mirrors FWindowsDeviceInfo, providing device discovery, handle creation,
 * and read/write paths for DualSense and DualShock4 controllers via hidapi
 * (libhidapi-hidraw).
 */
class FLinuxDeviceInfo
{
public:
	virtual ~FLinuxDeviceInfo() = default;

	/**
	 * Processes audio haptic feedback using the given device context.
	 *
	 * Writes the audio buffer report to the connected device, enabling
	 * the DualSense's voice-coil HD-haptic actuators.
	 *
	 * @param Context Pointer to the FDeviceContext object whose handle and
	 *                BufferAudio will be transmitted.
	 */
	static void ProcessAudioHapitc(FDeviceContext* Context);

	/**
	 * @brief Configures HID feature reports for a freshly opened device.
	 *
	 * Issues a feature report 0x05 read so the device's calibration data
	 * (gyro/accelerometer offsets) is captured into the context.
	 *
	 * @param Context Device context to populate. Must already have a valid
	 *                handle.
	 * @return True if the feature report was read and parsed; false otherwise.
	 */
	static bool ConfigureFeatures(FDeviceContext* Context);

	/**
	 * Reads an input report from the device into the context's buffer.
	 *
	 * Selects the right buffer (Buffer for DualSense, BufferDS4 for DualShock4
	 * Bluetooth) and report length based on the connection type. Invalidates
	 * the handle on read failure.
	 *
	 * @param Context Device context whose Handle will be read from.
	 */
	static void Read(FDeviceContext* Context);

	/**
	 * Writes the pending output report to the device.
	 *
	 * Picks the output report length from device + connection type and pushes
	 * the BufferOutput contents over HID. Invalidates the handle on write
	 * failure.
	 *
	 * @param Context Device context whose Handle will be written to.
	 */
	static void Write(FDeviceContext* Context);

	/**
	 * Enumerates connected Sony controllers and populates the device list.
	 *
	 * Scans HID for Sony VID 0x054C and filters by the supported PIDs
	 * (DualShock4 v1/v2, DualSense, DualSense Edge). Existing entries in
	 * Devices are cleared.
	 *
	 * @param Devices Output vector of detected device contexts.
	 */
	static void Detect(std::vector<FDeviceContext>& Devices);

	/**
	 * Opens a HID handle for the device described by Context.
	 *
	 * Sets non-blocking mode and runs ConfigureFeatures on success.
	 *
	 * @param Context Device context whose Path will be opened.
	 * @return True if the handle was opened; false otherwise.
	 */
	static bool CreateHandle(FDeviceContext* Context);

	/**
	 * Closes the device handle and clears the context's buffers.
	 *
	 * Marks the device as disconnected so the registry can drop it on the
	 * next discovery pass.
	 *
	 * @param Context Device context to invalidate.
	 */
	static void InvalidateHandle(FDeviceContext* Context);
};
