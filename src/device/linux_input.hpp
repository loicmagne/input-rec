#pragma once
#ifdef __linux__

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <algorithm>
#include "input_device.hpp"

// Single source of truth for all tracked keys (matches Windows column names)
struct TrackedKeyLinux {
	KeySym keysym;
	const char *name;
};

// All tracked keys defined in one place - same order as Windows for column parity
// To add/remove/reorder keys, only modify this array
static constexpr TrackedKeyLinux TRACKED_KEYS_LINUX[] = {
	// Letters A-Z (indices 0-25) - use lowercase keysyms
	{XK_a, "A"}, {XK_b, "B"}, {XK_c, "C"}, {XK_d, "D"}, {XK_e, "E"}, {XK_f, "F"}, {XK_g, "G"}, {XK_h, "H"},
	{XK_i, "I"}, {XK_j, "J"}, {XK_k, "K"}, {XK_l, "L"}, {XK_m, "M"}, {XK_n, "N"}, {XK_o, "O"}, {XK_p, "P"},
	{XK_q, "Q"}, {XK_r, "R"}, {XK_s, "S"}, {XK_t, "T"}, {XK_u, "U"}, {XK_v, "V"}, {XK_w, "W"}, {XK_x, "X"},
	{XK_y, "Y"}, {XK_z, "Z"},

	// Digits 0-9 (indices 26-35)
	{XK_0, "0"}, {XK_1, "1"}, {XK_2, "2"}, {XK_3, "3"}, {XK_4, "4"},
	{XK_5, "5"}, {XK_6, "6"}, {XK_7, "7"}, {XK_8, "8"}, {XK_9, "9"},

	// F1-F12 (indices 36-47)
	{XK_F1, "F1"}, {XK_F2, "F2"}, {XK_F3, "F3"}, {XK_F4, "F4"}, {XK_F5, "F5"}, {XK_F6, "F6"},
	{XK_F7, "F7"}, {XK_F8, "F8"}, {XK_F9, "F9"}, {XK_F10, "F10"}, {XK_F11, "F11"}, {XK_F12, "F12"},

	// Modifiers (indices 48-53)
	{XK_Shift_L, "LSHIFT"}, {XK_Shift_R, "RSHIFT"},
	{XK_Control_L, "LCTRL"}, {XK_Control_R, "RCTRL"},
	{XK_Alt_L, "LALT"}, {XK_Alt_R, "RALT"},

	// Arrows (indices 54-57)
	{XK_Left, "LEFT"}, {XK_Up, "UP"}, {XK_Right, "RIGHT"}, {XK_Down, "DOWN"},

	// Special keys (indices 58+)
	{XK_space, "SPACE"}, {XK_Return, "ENTER"}, {XK_Escape, "ESCAPE"}, {XK_Tab, "TAB"},
	{XK_BackSpace, "BACKSPACE"}, {XK_Delete, "DELETE"}, {XK_Insert, "INSERT"},
	{XK_Home, "HOME"}, {XK_End, "END"}, {XK_Page_Up, "PAGEUP"}, {XK_Page_Down, "PAGEDOWN"},
	{XK_Caps_Lock, "CAPSLOCK"}, {XK_Num_Lock, "NUMLOCK"}, {XK_Scroll_Lock, "SCROLLLOCK"},
	{XK_Print, "PRINTSCREEN"}, {XK_Pause, "PAUSE"}, {XK_Menu, "MENU"},
};

static constexpr size_t NUM_TRACKED_KEYS_LINUX = sizeof(TRACKED_KEYS_LINUX) / sizeof(TRACKED_KEYS_LINUX[0]);

// Lookup class to convert X11 keycodes to our tracked key indices
// Note: Unlike Windows VK codes which are fixed, X11 keycodes are server-dependent
// We need to use keysyms via XkbKeycodeToKeysym at runtime
class KeyLookupLinux {
private:
	// Map from keysym to index (keysyms can be large, so we use a simple array for common ones)
	// For keysyms outside this range, we do linear search
	static constexpr size_t FAST_LOOKUP_SIZE = 0x10000; // Covers most Latin keysyms
	int16_t m_fast_table[FAST_LOOKUP_SIZE];

public:
	KeyLookupLinux()
	{
		std::fill_n(m_fast_table, FAST_LOOKUP_SIZE, static_cast<int16_t>(-1));
		for (size_t i = 0; i < NUM_TRACKED_KEYS_LINUX; i++) {
			KeySym ks = TRACKED_KEYS_LINUX[i].keysym;
			if (ks < FAST_LOOKUP_SIZE) {
				m_fast_table[ks] = static_cast<int16_t>(i);
			}
		}
		// Also map uppercase letters to same indices as lowercase
		for (size_t i = 0; i < 26; i++) {
			KeySym upper = XK_A + i;
			if (upper < FAST_LOOKUP_SIZE) {
				m_fast_table[upper] = static_cast<int16_t>(i);
			}
		}
	}

	int to_index(KeySym keysym) const
	{
		if (keysym < FAST_LOOKUP_SIZE) {
			return m_fast_table[keysym];
		}
		// Fallback: linear search for keysyms outside fast range
		for (size_t i = 0; i < NUM_TRACKED_KEYS_LINUX; i++) {
			if (TRACKED_KEYS_LINUX[i].keysym == keysym) {
				return static_cast<int>(i);
			}
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

class LinuxInputDevice : public InputDevice {
private:
	// X11 connection (separate from main thread for thread safety)
	Display *m_display;
	int m_xi_opcode;
	int m_xi_event_base;
	int m_xi_error_base;

	std::thread m_event_thread;
	std::atomic<bool> m_running;
	std::atomic<bool> m_initialized;

	// Thread-safe state
	std::mutex m_state_mutex;

	// Mouse state
	int32_t m_mouse_delta_x;
	int32_t m_mouse_delta_y;
	int16_t m_wheel_delta;
	int16_t m_hwheel_delta;
	int16_t m_mouse_flags; // 0 = relative, 1 = absolute (for compatibility with Windows)
	std::array<bool, 5> m_mouse_buttons;

	// Keyboard state
	std::array<bool, NUM_TRACKED_KEYS_LINUX> m_key_states;

	// Key lookup table (built once at startup)
	static KeyLookupLinux s_key_lookup;

	void event_loop();
	void process_raw_motion(XIRawEvent *event);
	void process_raw_button(XIRawEvent *event, bool pressed);
	void process_raw_key(XIRawEvent *event, bool pressed);

	// Helper functions for window info
	std::string get_active_window_title();
	std::string get_active_window_executable();
	bool is_cursor_visible();
	bool is_pointer_grabbed();

public:
	LinuxInputDevice();
	~LinuxInputDevice() override;

	void write_header(InputWriter &writer) override;
	void write_state(InputWriter &writer) override;
};

#endif // __linux__
