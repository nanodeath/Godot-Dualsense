#include "Managers/DualSenseManager.h"
#include <cstring>
#include <vector>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/engine.hpp>
#include "Adapter/GodotDeviceRegistry.h"
#include "API/GamepadDefs.h"
#include "GCore/Interfaces/Segregations/IGamepadHaptics.h"
#include "GCore/Interfaces/Segregations/IGamepadLightbar.h"
#include "GCore/Interfaces/Segregations/IGamepadRumbles.h"
#include "GCore/Interfaces/Segregations/IGamepadSensors.h"
#include "GImplementations/Libraries/DualSense/DualSenseLibrary.h"
#ifdef _WIN32
#include "Platforms/Windows/WindowsHardwarePolicy.h"
#endif
#ifdef __unix__
#include "Platforms/Linux/LinuxHardwarePolicy.h"
#endif
#include "GCore/Interfaces/IPlatformHardware.h"

using namespace godot;

DualSenseManager *DualSenseManager::singleton = nullptr;

DualSenseManager::DualSenseManager() {
    singleton = this;
}

DualSenseManager::~DualSenseManager() {
    if (singleton == this) {
        singleton = nullptr;
    }
}

void DualSenseManager::_ready() {
    UtilityFunctions::print("[DualSenseManager] Initialize GamepadCore...");

    // 1.Hardware
#ifdef _WIN32
    std::unique_ptr<IPlatformHardware> WindowsInstance = std::make_unique<FWindowsPlatform::FWindowsHardware>();
    IPlatformHardware::SetInstance(std::move(WindowsInstance));
#elif defined(__unix__)
    std::unique_ptr<IPlatformHardware> LinuxInstance = std::make_unique<FLinuxPlatform::FLinuxHardware>();
    IPlatformHardware::SetInstance(std::move(LinuxInstance));
#endif

    FGodotDeviceRegistry::Initialize();

    haptic_thread_running.store(true);
    haptic_thread = std::thread([this]() { haptic_worker_loop(); });

    UtilityFunctions::print("[DualSenseManager] GamepadCore initialized!");
}

void DualSenseManager::_process(double delta) {
    FGodotDeviceRegistry::DiscoverDevices(delta);
}

void DualSenseManager::_exit_tree() {
    haptic_thread_running.store(false);
    haptic_cv.notify_all();
    if (haptic_thread.joinable()) {
        haptic_thread.join();
    }
    FGodotDeviceRegistry::Shutdown();
}

void DualSenseManager::haptic_worker_loop() {
    while (haptic_thread_running.load()) {
        HapticJob job;
        {
            std::unique_lock<std::mutex> lock(haptic_mutex);
            haptic_cv.wait(lock, [this]() {
                return !haptic_queue.empty() || !haptic_thread_running.load();
            });
            if (!haptic_thread_running.load()) {
                return;
            }
            job = std::move(haptic_queue.front());
            haptic_queue.pop_front();
        }
        haptic_process_job(job);
    }
}

void DualSenseManager::haptic_enqueue(std::vector<std::uint8_t>&& data, int device_id, bool streaming) {
    {
        std::lock_guard<std::mutex> lock(haptic_mutex);
        // Cap the queue depth: if the worker is falling behind (e.g. BT
        // saturated during a bump-storm), drop oldest jobs rather than let
        // memory grow. Newer haptic events are more relevant than stale ones.
        while (haptic_queue.size() >= HAPTIC_QUEUE_MAX) {
            haptic_queue.pop_front();
        }
        haptic_queue.push_back(HapticJob{std::move(data), device_id, streaming});
    }
    haptic_cv.notify_one();
}

void DualSenseManager::haptic_process_job(const HapticJob &job) {
    const auto gamepad = FGodotDeviceRegistry::GetGamepad(job.device_id);
    if (!gamepad) return;
    IGamepadHaptics *haptics = gamepad->GetIGamepadHaptics();
    if (!haptics) return;
    const int64_t total = static_cast<int64_t>(job.data.size());
    if (total <= 0) return;

    constexpr int64_t CHUNK = 64;
    constexpr int64_t MIN_CHUNKS = 20;  // see set_audio_haptic comment
    std::vector<std::uint8_t> buf(CHUNK, 0);
    const std::uint8_t *src = job.data.data();
    int64_t chunks_sent = 0;
    for (int64_t offset = 0; offset < total; offset += CHUNK) {
        const int64_t take = (CHUNK < total - offset) ? CHUNK : (total - offset);
        std::memcpy(buf.data(), src + offset, static_cast<size_t>(take));
        if (take < CHUNK) std::memset(buf.data() + take, 0, static_cast<size_t>(CHUNK - take));
        haptics->AudioHapticUpdate(buf);
        chunks_sent++;
    }
    if (!job.streaming && chunks_sent < MIN_CHUNKS) {
        std::memset(buf.data(), 0, CHUNK);
        for (int64_t i = chunks_sent; i < MIN_CHUNKS; i++) {
            haptics->AudioHapticUpdate(buf);
        }
    }
}

void DualSenseManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_rumble", "left", "right", "device_id"), &DualSenseManager::set_rumble, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("set_lightbar", "color", "device_id"), &DualSenseManager::set_lightbar, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("set_trigger_off", "hand", "device_id"), &DualSenseManager::set_trigger_off, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("set_trigger_resistance", "hand", "start_zone", "strength", "device_id"), &DualSenseManager::set_trigger_resistance, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("set_trigger_weapon", "hand", "start_zone", "amplitude", "behavior", "trigger", "device_id"), &DualSenseManager::set_trigger_weapon, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("set_audio_haptic", "data", "device_id"), &DualSenseManager::set_audio_haptic, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("set_audio_haptic_streaming", "data", "device_id"), &DualSenseManager::set_audio_haptic_streaming, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("get_gyro", "device_id"), &DualSenseManager::get_gyro, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("get_accel", "device_id"), &DualSenseManager::get_accel, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("enable_motion_sensor", "enabled", "device_id"), &DualSenseManager::enable_motion_sensor, DEFVAL(1));
    ClassDB::bind_method(D_METHOD("reset_gyro_orientation", "device_id"), &DualSenseManager::reset_gyro_orientation, DEFVAL(1));
}

static std::uint8_t clamp_byte(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<std::uint8_t>(v);
}

static EDSGamepadHand to_hand(int hand) {
    return hand == 0 ? EDSGamepadHand::Left : EDSGamepadHand::Right;
}

void DualSenseManager::set_rumble(int left, int right, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        if (auto *rumbles = gamepad->GetIGamepadRumbles()) {
            rumbles->SetVibration(clamp_byte(left), clamp_byte(right));
        }
    }
}

void DualSenseManager::set_lightbar(Color color, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        if (auto *lightbar = gamepad->GetIGamepadLightbar()) {
            DSCoreTypes::FDSColor c{
                clamp_byte(static_cast<int>(color.r * 255.0f)),
                clamp_byte(static_cast<int>(color.g * 255.0f)),
                clamp_byte(static_cast<int>(color.b * 255.0f)),
                1
            };
            lightbar->SetLightbar(c);
        }
    }
}

void DualSenseManager::set_trigger_off(int hand, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(device_id)) {
        gamepad->StopTrigger(to_hand(hand));
    }
}

void DualSenseManager::set_trigger_resistance(int hand, int start_zone, int strength, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(device_id)) {
        gamepad->SetResistance(clamp_byte(start_zone), clamp_byte(strength), to_hand(hand));
    }
}

void DualSenseManager::set_trigger_weapon(int hand, int start_zone, int amplitude, int behavior, int trigger, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(device_id)) {
        gamepad->SetWeapon25(clamp_byte(start_zone), clamp_byte(amplitude), clamp_byte(behavior), clamp_byte(trigger), to_hand(hand));
    }
}

void DualSenseManager::set_audio_haptic(const PackedByteArray &data, int device_id) {
    // Enqueue the burst on the worker thread and return immediately. Each
    // chunk is one hid_write (~2 ms over BT), so a 20-chunk burst would
    // block the caller ~40 ms — enough to stall input polling when bumps
    // fire rapidly. The worker pads short bursts with trailing silence
    // (MIN_CHUNKS) so they cross the controller's playback threshold.
    const int64_t total = data.size();
    if (total <= 0) return;
    std::vector<std::uint8_t> buf(static_cast<size_t>(total));
    std::memcpy(buf.data(), data.ptr(), static_cast<size_t>(total));
    haptic_enqueue(std::move(buf), device_id, /*streaming=*/false);
}

void DualSenseManager::set_audio_haptic_streaming(const PackedByteArray &data, int device_id) {
    // Pad-free variant for continuous textures (gravel, engine hum). Caller
    // is expected to invoke this every ~100 ms so the controller's buffer
    // stays above the playback threshold without per-call silence padding.
    const int64_t total = data.size();
    if (total <= 0) return;
    std::vector<std::uint8_t> buf(static_cast<size_t>(total));
    std::memcpy(buf.data(), data.ptr(), static_cast<size_t>(total));
    haptic_enqueue(std::move(buf), device_id, /*streaming=*/true);
}

Vector3 DualSenseManager::get_gyro(int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        if (auto *sensors = gamepad->GetIGamepadSensors()) {
            const auto v = sensors->GetGyro();
            return Vector3(v.X, v.Y, v.Z);
        }
    }
    return Vector3();
}

Vector3 DualSenseManager::get_accel(int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        if (auto *sensors = gamepad->GetIGamepadSensors()) {
            const auto v = sensors->GetAccel();
            return Vector3(v.X, v.Y, v.Z);
        }
    }
    return Vector3();
}

void DualSenseManager::enable_motion_sensor(bool enabled, int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        if (auto *sensors = gamepad->GetIGamepadSensors()) {
            sensors->EnableMotionSensor(enabled);
        }
    }
}

void DualSenseManager::reset_gyro_orientation(int device_id) {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(device_id)) {
        if (auto *sensors = gamepad->GetIGamepadSensors()) {
            sensors->ResetGyroOrientation();
        }
    }
}

void DualSenseManager::test_rumble() {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(1)) {
        UtilityFunctions::print("test_rumble vibration...");
        if (auto *rumbles = gamepad->GetIGamepadRumbles()) {
            rumbles->SetVibration(255, 255);
        }
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::test_lightbar() {
    if (const auto gamepad = FGodotDeviceRegistry::GetGamepad(1)) {
        if (auto *lightbar = gamepad->GetIGamepadLightbar()) {
            lightbar->SetLightbar({255, 0, 0, 0});
        }
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}

void DualSenseManager::test_weapon() {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(1)) {
        UtilityFunctions::print("test_weapon (weapon 0x25) effect...");
        gamepad->SetWeapon25(
            GamepadDefs::POS_START,
            GamepadDefs::AMP_HIGH,
            GamepadDefs::SUSTAINED,
            GamepadDefs::TriggerForceMask::MASK_FORCE_HIGH,
            static_cast<EDSGamepadHand>(GamepadDefs::GamepadHand::RIGHT_HAND)
        );
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}
void DualSenseManager::test_custom_trigger() {
    if (const auto gamepad = FGodotDeviceRegistry::GetTriggerGamepad(1)) {
        UtilityFunctions::print("test_trigger_custom (machine 0x27) effect...");
        // machine effects...
        // 27 02 02 3a 0a 05
        // 27 40 01 3a 0a 05
        // 27 80 02 32 19 02
        std::vector<std::uint8_t> Buffer = {0};
        Buffer.resize(10);
        Buffer[0] = 0x27;
        Buffer[1] = 0x80;
        Buffer[2] = 0x02;
        Buffer[3] = 0x32;
        Buffer[4] = 0x19;
        Buffer[5] = 0x02;
        Buffer[6] = 0;
        Buffer[7] = 0;
        Buffer[8] = 0;
        Buffer[9] = 0;
        gamepad->SetCustomTrigger(EDSGamepadHand::Left, Buffer);
    } else {
        UtilityFunctions::print("Not found gamepad");
    }
}
