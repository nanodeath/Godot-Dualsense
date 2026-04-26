#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/color.hpp>
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

		// Motion API. Axes are controller-local; consumer applies game-specific remap.
		Vector3 get_gyro(int device_id);
		Vector3 get_accel(int device_id);
		Quaternion get_orientation(int device_id);
		void enable_motion_sensor(bool enabled, int device_id);
		void reset_gyro_orientation(int device_id);
	private:
		static DualSenseManager *singleton;

	protected:
		static void _bind_methods();
	};
}