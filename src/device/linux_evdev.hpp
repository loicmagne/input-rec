#pragma once
#ifdef __linux__

#include <linux/input.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <vector>
#include <string>
#include "input_device.hpp"

// Forward declaration
struct libevdev;

// Single source of truth for all tracked keys (matches Windows column names)
// Uses Linux evdev keycodes (KEY_* from linux/input-event-codes.h)
struct TrackedKeyLinux {
	unsigned int keycode;
	const char *name;
};

// All tracked keys defined in one place - same order as Windows for column parity
static constexpr TrackedKeyLinux TRACKED_KEYS_LINUX[] = {
	// Letters A-Z (indices 0-25)
	{KEY_A, "A"}, {KEY_B, "B"}, {KEY_C, "C"}, {KEY_D, "D"}, {KEY_E, "E"}, {KEY_F, "F"}, {KEY_G, "G"}, {KEY_H, "H"},
	{KEY_I, "I"}, {KEY_J, "J"}, {KEY_K, "K"}, {KEY_L, "L"}, {KEY_M, "M"}, {KEY_N, "N"}, {KEY_O, "O"}, {KEY_P, "P"},
	{KEY_Q, "Q"}, {KEY_R, "R"}, {KEY_S, "S"}, {KEY_T, "T"}, {KEY_U, "U"}, {KEY_V, "V"}, {KEY_W, "W"}, {KEY_X, "X"},
	{KEY_Y, "Y"}, {KEY_Z, "Z"},

	// Digits 0-9 (indices 26-35)
	{KEY_0, "0"}, {KEY_1, "1"}, {KEY_2, "2"}, {KEY_3, "3"}, {KEY_4, "4"},
	{KEY_5, "5"}, {KEY_6, "6"}, {KEY_7, "7"}, {KEY_8, "8"}, {KEY_9, "9"},

	// F1-F12 (indices 36-47)
	{KEY_F1, "F1"}, {KEY_F2, "F2"}, {KEY_F3, "F3"}, {KEY_F4, "F4"}, {KEY_F5, "F5"}, {KEY_F6, "F6"},
	{KEY_F7, "F7"}, {KEY_F8, "F8"}, {KEY_F9, "F9"}, {KEY_F10, "F10"}, {KEY_F11, "F11"}, {KEY_F12, "F12"},

	// Modifiers (indices 48-53)
	{KEY_LEFTSHIFT, "LSHIFT"}, {KEY_RIGHTSHIFT, "RSHIFT"},
	{KEY_LEFTCTRL, "LCTRL"}, {KEY_RIGHTCTRL, "RCTRL"},
	{KEY_LEFTALT, "LALT"}, {KEY_RIGHTALT, "RALT"},

	// Arrows (indices 54-57)
	{KEY_LEFT, "LEFT"}, {KEY_UP, "UP"}, {KEY_RIGHT, "RIGHT"}, {KEY_DOWN, "DOWN"},

	// Special keys (indices 58+)
	{KEY_SPACE, "SPACE"}, {KEY_ENTER, "ENTER"}, {KEY_ESC, "ESCAPE"}, {KEY_TAB, "TAB"},
	{KEY_BACKSPACE, "BACKSPACE"}, {KEY_DELETE, "DELETE"}, {KEY_INSERT, "INSERT"},
	{KEY_HOME, "HOME"}, {KEY_END, "END"}, {KEY_PAGEUP, "PAGEUP"}, {KEY_PAGEDOWN, "PAGEDOWN"},
	{KEY_CAPSLOCK, "CAPSLOCK"}, {KEY_NUMLOCK, "NUMLOCK"}, {KEY_SCROLLLOCK, "SCROLLLOCK"},
	{KEY_SYSRQ, "PRINTSCREEN"}, {KEY_PAUSE, "PAUSE"}, {KEY_COMPOSE, "MENU"},
};

static constexpr size_t NUM_TRACKED_KEYS_LINUX = sizeof(TRACKED_KEYS_LINUX) / sizeof(TRACKED_KEYS_LINUX[0]);

// Lookup class to convert evdev keycodes to our tracked key indices
class KeyLookupLinux {
private:
	static constexpr size_t LOOKUP_SIZE = 256; // Covers all standard keycodes
	int16_t m_table[LOOKUP_SIZE];

public:
	KeyLookupLinux()
	{
		std::fill_n(m_table, LOOKUP_SIZE, static_cast<int16_t>(-1));
		for (size_t i = 0; i < NUM_TRACKED_KEYS_LINUX; i++) {
			unsigned int kc = TRACKED_KEYS_LINUX[i].keycode;
			if (kc < LOOKUP_SIZE) {
				m_table[kc] = static_cast<int16_t>(i);
			}
		}
	}

	int to_index(unsigned int keycode) const
	{
		if (keycode < LOOKUP_SIZE) {
			return m_table[keycode];
		}
		return -1;
	}

	static const char *to_name(int index)
	{
		return (index >= 0 && static_cast<size_t>(index) < NUM_TRACKED_KEYS_LINUX)
			       ? TRACKED_KEYS_LINUX[index].name
			       : "UNKNOWN";
	}
};

// Info about an opened evdev device
struct EvdevDevice {
	int fd;
	struct libevdev *dev;
	bool is_keyboard;
	bool is_mouse;
	std::string name;
};

class LinuxEvdevDevice : public InputDevice {
private:
	std::vector<EvdevDevice> m_devices;
	std::thread m_event_thread;
	std::atomic<bool> m_running;
	std::atomic<bool> m_initialized;

	// Thread-safe state
	std::mutex m_state_mutex;

	// Mouse state
	int32_t m_mouse_x;
	int32_t m_mouse_y;
	int32_t m_mouse_delta_x;
	int32_t m_mouse_delta_y;
	int16_t m_wheel_delta;
	int16_t m_hwheel_delta;
	int16_t m_mouse_flags;
	std::array<bool, 5> m_mouse_buttons;

	// For touchpad absolute->relative conversion
	int32_t m_last_abs_x;
	int32_t m_last_abs_y;
	bool m_abs_x_valid;
	bool m_abs_y_valid;

	// Keyboard state
	std::array<bool, NUM_TRACKED_KEYS_LINUX> m_key_states;

	// Key lookup table
	static KeyLookupLinux s_key_lookup;

	void event_loop();
	void scan_and_open_devices();
	void process_event(const EvdevDevice &device, const struct input_event &ev);

	// X11 connection for window info (works through XWayland)
	void *m_x_display; // Display* but avoiding X11 headers here
	std::string get_active_window_title();
	std::string get_active_window_executable();

public:
	LinuxEvdevDevice();
	~LinuxEvdevDevice() override;

	void write_header(InputWriter &writer) override;
	void write_state(InputWriter &writer) override;
};

#endif // __linux__
