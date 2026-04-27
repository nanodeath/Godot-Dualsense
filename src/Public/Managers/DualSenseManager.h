#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/vector3.hpp>
using namespace godot;

namespace godot {
	class DualSenseManager : public Node {
		GDCLASS(DualSenseManager, Node)
	public:
		DualSenseManager();
		~DualSenseManager();

		static DualSenseManager *get_singleton() { return singleton; }

		virtual void _ready() override;
		virtual void _process(double delta) override;
		virtual void _exit_tree() override;

		static void test_rumble();
		static void test_weapon();
		static void test_lightbar();
		static void test_custom_trigger();

		// GDScript-callable API. device_id defaults to 1 (first connected DualSense).
		// Hand: 0 = Left (L2), 1 = Right (R2).
		void set_rumble(int left, int right, int device_id);
		void set_lightbar(Color color, int device_id);
		void set_trigger_off(int hand, int device_id);
		void set_trigger_resistance(int hand, int start_zone, int strength, int device_id);
		void set_trigger_weapon(int hand, int start_zone, int amplitude, int behavior, int trigger, int device_id);
		void set_audio_haptic(const PackedByteArray &data, int device_id);
		void set_audio_haptic_streaming(const PackedByteArray &data, int device_id);

		// Motion API. Axes are controller-local; consumer applies game-specific remap.
		Vector3 get_gyro(int device_id);
		Vector3 get_accel(int device_id);
		Quaternion get_orientation(int device_id);
		void enable_motion_sensor(bool enabled, int device_id);
		void reset_gyro_orientation(int device_id);
	private:
		static DualSenseManager *singleton;

		// Audio-haptic writes happen on a worker thread. Each call enqueues
		// a job and returns immediately, so the physics loop never blocks on
		// hid_write (which can stall ~40 ms over BT for a 20-chunk burst).
		// Without this, sustained bump-haptic stalled input polling for
		// seconds after a heavy bump pass.
		struct HapticJob {
			std::vector<std::uint8_t> data;
			int device_id;
			bool streaming;
		};
		std::thread haptic_thread;
		std::mutex haptic_mutex;
		std::condition_variable haptic_cv;
		std::deque<HapticJob> haptic_queue;
		std::atomic<bool> haptic_thread_running{false};
		static constexpr size_t HAPTIC_QUEUE_MAX = 8;

		void haptic_worker_loop();
		void haptic_enqueue(std::vector<std::uint8_t>&& data, int device_id, bool streaming);
		static void haptic_process_job(const HapticJob &job);

	protected:
		static void _bind_methods();
	};
}