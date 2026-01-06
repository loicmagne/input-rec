#pragma once
#ifdef _WIN32

#include <Windows.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <algorithm>
#include "input_device.hpp"

// Single source of truth for all tracked keys
struct TrackedKey {
	USHORT vk_code;
	const char *name;
};

// All tracked keys defined in one place
// To add/remove/reorder keys, only modify this array
static constexpr TrackedKey TRACKED_KEYS[] = {
	// Letters A-Z (indices 0-25)
	{0x41, "A"}, {0x42, "B"}, {0x43, "C"}, {0x44, "D"}, {0x45, "E"}, {0x46, "F"}, {0x47, "G"}, {0x48, "H"},
	{0x49, "I"}, {0x4A, "J"}, {0x4B, "K"}, {0x4C, "L"}, {0x4D, "M"}, {0x4E, "N"}, {0x4F, "O"}, {0x50, "P"},
	{0x51, "Q"}, {0x52, "R"}, {0x53, "S"}, {0x54, "T"}, {0x55, "U"}, {0x56, "V"}, {0x57, "W"}, {0x58, "X"},
	{0x59, "Y"}, {0x5A, "Z"},

	// Digits 0-9 (indices 26-35)
	{0x30, "0"}, {0x31, "1"}, {0x32, "2"}, {0x33, "3"}, {0x34, "4"},
	{0x35, "5"}, {0x36, "6"}, {0x37, "7"}, {0x38, "8"}, {0x39, "9"},

	// F1-F12 (indices 36-47)
	{VK_F1, "F1"}, {VK_F2, "F2"}, {VK_F3, "F3"}, {VK_F4, "F4"}, {VK_F5, "F5"}, {VK_F6, "F6"},
	{VK_F7, "F7"}, {VK_F8, "F8"}, {VK_F9, "F9"}, {VK_F10, "F10"}, {VK_F11, "F11"}, {VK_F12, "F12"},

	// Modifiers (indices 48-53)
	// Note: VK_SHIFT/VK_CONTROL/VK_MENU are generic, VK_L*/VK_R* are specific
	// Raw Input typically sends the generic ones, so we include both as aliases
	{VK_LSHIFT, "LSHIFT"}, {VK_RSHIFT, "RSHIFT"},
	{VK_LCONTROL, "LCTRL"}, {VK_RCONTROL, "RCTRL"},
	{VK_LMENU, "LALT"}, {VK_RMENU, "RALT"},

	// Arrows (indices 54-57)
	{VK_LEFT, "LEFT"}, {VK_UP, "UP"}, {VK_RIGHT, "RIGHT"}, {VK_DOWN, "DOWN"},

	// Special keys (indices 58+)
	{VK_SPACE, "SPACE"}, {VK_RETURN, "ENTER"}, {VK_ESCAPE, "ESCAPE"}, {VK_TAB, "TAB"},
	{VK_BACK, "BACKSPACE"}, {VK_DELETE, "DELETE"}, {VK_INSERT, "INSERT"},
	{VK_HOME, "HOME"}, {VK_END, "END"}, {VK_PRIOR, "PAGEUP"}, {VK_NEXT, "PAGEDOWN"},
	{VK_CAPITAL, "CAPSLOCK"}, {VK_NUMLOCK, "NUMLOCK"}, {VK_SCROLL, "SCROLLLOCK"},
	{VK_SNAPSHOT, "PRINTSCREEN"}, {VK_PAUSE, "PAUSE"}, {VK_APPS, "MENU"},
};

static constexpr size_t NUM_TRACKED_KEYS = sizeof(TRACKED_KEYS) / sizeof(TRACKED_KEYS[0]);

// TRACKED_KEYS is a mapping from index -> (vk_code, name)
// But when we receive a vk_code from Raw Input, we need to map it back to an index in TRACKED_KEYS
// which is what this next class does. O(1) lookup table built from TRACKED_KEYS
class KeyLookup {
private:
	int8_t m_table[256];

public:
	KeyLookup()
	{
		std::fill_n(m_table, 256, static_cast<int8_t>(-1));
		for (size_t i = 0; i < NUM_TRACKED_KEYS; i++) {
			USHORT vk = TRACKED_KEYS[i].vk_code;
			if (vk < 256) {
				m_table[vk] = static_cast<int8_t>(i);
			}
		}
		// Add aliases for generic modifier keys -> map to left variant
		m_table[VK_SHIFT] = m_table[VK_LSHIFT];
		m_table[VK_CONTROL] = m_table[VK_LCONTROL];
		m_table[VK_MENU] = m_table[VK_LMENU];
	}

	int to_index(USHORT vk) const { return vk < 256 ? m_table[vk] : -1; }

	static const char *to_name(int index)
	{
		return (index >= 0 && static_cast<size_t>(index) < NUM_TRACKED_KEYS) ? TRACKED_KEYS[index].name
										     : "UNKNOWN";
	}
};

class WindowsInputDevice : public InputDevice {
private:
	// Message-only window for receiving WM_INPUT
	HWND m_hwnd;
	ATOM m_window_class;
	std::thread m_message_thread;
	std::atomic<bool> m_running;
	std::atomic<bool> m_initialized;

	// Thread-safe state
	std::mutex m_state_mutex;

	// Mouse state
	int32_t m_mouse_delta_x;
	int32_t m_mouse_delta_y;
	int16_t m_wheel_delta;
	int16_t m_hwheel_delta;
	int16_t m_mouse_flags; // Last raw input mouse flags (MOUSE_MOVE_ABSOLUTE, etc.)
	std::array<bool, 5> m_mouse_buttons;

	// Keyboard state
	std::array<bool, NUM_TRACKED_KEYS> m_key_states;

	// Key lookup table (built once at startup)
	static KeyLookup s_key_lookup;

	void message_loop();
	void process_raw_input(LPARAM lparam);
	void process_mouse(const RAWMOUSE &mouse);
	void process_keyboard(const RAWKEYBOARD &keyboard);

	static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
	WindowsInputDevice();
	~WindowsInputDevice() override;

	void write_header(InputWriter &writer) override;
	void write_state(InputWriter &writer) override;
};

#endif // _WIN32
