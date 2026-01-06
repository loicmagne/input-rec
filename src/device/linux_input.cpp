#ifdef __linux__

#include "linux_input.hpp"
#include "writer/input_writer.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <cstring>
#include <climits>
#include <unistd.h>
#include <algorithm>

// Static member definition - lookup table built once at startup
KeyLookupLinux LinuxInputDevice::s_key_lookup;

// Helper to get active window from _NET_ACTIVE_WINDOW
static Window get_active_window(Display *display)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = nullptr;

	Window root = DefaultRootWindow(display);
	Atom net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);

	if (XGetWindowProperty(display, root, net_active_window, 0, 1, False, XA_WINDOW, &actual_type, &actual_format,
			       &nitems, &bytes_after, &prop) == Success &&
	    prop) {
		Window active = *(Window *)prop;
		XFree(prop);
		return active;
	}
	return None;
}

std::string LinuxInputDevice::get_active_window_title()
{
	Window active = get_active_window(m_display);
	if (active == None)
		return "";

	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = nullptr;

	// Try _NET_WM_NAME first (UTF-8)
	Atom utf8_string = XInternAtom(m_display, "UTF8_STRING", False);
	Atom net_wm_name = XInternAtom(m_display, "_NET_WM_NAME", False);

	if (XGetWindowProperty(m_display, active, net_wm_name, 0, 256, False, utf8_string, &actual_type, &actual_format,
			       &nitems, &bytes_after, &prop) == Success &&
	    prop) {
		std::string title(reinterpret_cast<char *>(prop));
		XFree(prop);
		return title;
	}

	// Fallback to WM_NAME
	char *name = nullptr;
	if (XFetchName(m_display, active, &name) && name) {
		std::string title(name);
		XFree(name);
		return title;
	}

	return "";
}

std::string LinuxInputDevice::get_active_window_executable()
{
	Window active = get_active_window(m_display);
	if (active == None)
		return "";

	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = nullptr;

	// Get _NET_WM_PID
	Atom net_wm_pid = XInternAtom(m_display, "_NET_WM_PID", False);
	Atom cardinal = XInternAtom(m_display, "CARDINAL", False);

	if (XGetWindowProperty(m_display, active, net_wm_pid, 0, 1, False, cardinal, &actual_type, &actual_format,
			       &nitems, &bytes_after, &prop) == Success &&
	    prop) {
		pid_t pid = static_cast<pid_t>(*(unsigned long *)prop);
		XFree(prop);

		// Read /proc/[pid]/exe symlink
		char path[64];
		snprintf(path, sizeof(path), "/proc/%d/exe", pid);

		char exe_path[PATH_MAX];
		ssize_t len = readlink(path, exe_path, sizeof(exe_path) - 1);
		if (len != -1) {
			exe_path[len] = '\0';
			std::string full_path(exe_path);
			size_t last_slash = full_path.rfind('/');
			return (last_slash != std::string::npos) ? full_path.substr(last_slash + 1) : full_path;
		}
	}

	return "";
}

bool LinuxInputDevice::is_cursor_visible()
{
	// X11 doesn't have a direct API to query cursor visibility
	// We could use XFixes to track cursor changes, but for simplicity
	// we'll return true (visible) by default
	// Games that hide cursor typically also grab the pointer
	return true;
}

bool LinuxInputDevice::is_pointer_grabbed()
{
	// Try to grab the pointer - if it fails, someone else has it
	Window root = DefaultRootWindow(m_display);
	int result = XGrabPointer(m_display, root, False, 0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

	if (result == GrabSuccess) {
		XUngrabPointer(m_display, CurrentTime);
		XFlush(m_display);
		return false;
	}

	return (result == AlreadyGrabbed || result == GrabFrozen);
}

void LinuxInputDevice::process_raw_motion(XIRawEvent *event)
{
	std::lock_guard<std::mutex> lock(m_state_mutex);

	// Raw motion provides unaccelerated deltas
	double *raw_values = event->raw_values;
	int valuator_count = 0;

	// Check which valuators are present in the mask
	for (int i = 0; i < event->valuators.mask_len * 8 && valuator_count < 2; i++) {
		if (XIMaskIsSet(event->valuators.mask, i)) {
			if (valuator_count == 0) {
				// X axis
				m_mouse_delta_x += static_cast<int32_t>(raw_values[valuator_count]);
			} else if (valuator_count == 1) {
				// Y axis
				m_mouse_delta_y += static_cast<int32_t>(raw_values[valuator_count]);
			}
			valuator_count++;
		}
	}
}

void LinuxInputDevice::process_raw_button(XIRawEvent *event, bool pressed)
{
	std::lock_guard<std::mutex> lock(m_state_mutex);

	int button = event->detail;

	// Map X11 button numbers to our array indices
	// Button 1 = left, 2 = middle, 3 = right, 4/5 = wheel, 6/7 = hwheel, 8 = back, 9 = forward
	switch (button) {
	case 1:
		m_mouse_buttons[0] = pressed; // Left
		break;
	case 2:
		m_mouse_buttons[2] = pressed; // Middle
		break;
	case 3:
		m_mouse_buttons[1] = pressed; // Right
		break;
	case 4: // Wheel up
		if (pressed)
			m_wheel_delta += 120;
		break;
	case 5: // Wheel down
		if (pressed)
			m_wheel_delta -= 120;
		break;
	case 6: // Horizontal wheel left
		if (pressed)
			m_hwheel_delta -= 120;
		break;
	case 7: // Horizontal wheel right
		if (pressed)
			m_hwheel_delta += 120;
		break;
	case 8:
		m_mouse_buttons[3] = pressed; // Back (X1)
		break;
	case 9:
		m_mouse_buttons[4] = pressed; // Forward (X2)
		break;
	}
}

void LinuxInputDevice::process_raw_key(XIRawEvent *event, bool pressed)
{
	// Convert keycode to keysym
	// Note: We need to use XkbKeycodeToKeysym for proper mapping
	KeySym keysym = XkbKeycodeToKeysym(m_display, event->detail, 0, 0);

	int index = s_key_lookup.to_index(keysym);
	if (index < 0)
		return;

	std::lock_guard<std::mutex> lock(m_state_mutex);
	m_key_states[index] = pressed;
}

void LinuxInputDevice::event_loop()
{
	// Open a separate display connection for this thread
	m_display = XOpenDisplay(nullptr);
	if (!m_display) {
		obs_log(LOG_ERROR, "[LinuxInput] Failed to open X display");
		return;
	}

	// Check for XInput2 extension
	if (!XQueryExtension(m_display, "XInputExtension", &m_xi_opcode, &m_xi_event_base, &m_xi_error_base)) {
		obs_log(LOG_ERROR, "[LinuxInput] XInput extension not available");
		XCloseDisplay(m_display);
		m_display = nullptr;
		return;
	}

	// Check XInput2 version (need 2.0+)
	int major = 2, minor = 0;
	if (XIQueryVersion(m_display, &major, &minor) != Success) {
		obs_log(LOG_ERROR, "[LinuxInput] XInput2 not available");
		XCloseDisplay(m_display);
		m_display = nullptr;
		return;
	}
	obs_log(LOG_INFO, "[LinuxInput] XInput version %d.%d", major, minor);

	// Select raw events on root window
	Window root = DefaultRootWindow(m_display);

	XIEventMask mask;
	unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8] = {0};
	mask.deviceid = XIAllMasterDevices;
	mask.mask_len = sizeof(mask_bytes);
	mask.mask = mask_bytes;

	XISetMask(mask_bytes, XI_RawMotion);
	XISetMask(mask_bytes, XI_RawButtonPress);
	XISetMask(mask_bytes, XI_RawButtonRelease);
	XISetMask(mask_bytes, XI_RawKeyPress);
	XISetMask(mask_bytes, XI_RawKeyRelease);

	if (XISelectEvents(m_display, root, &mask, 1) != Success) {
		obs_log(LOG_ERROR, "[LinuxInput] Failed to select XInput events");
		XCloseDisplay(m_display);
		m_display = nullptr;
		return;
	}

	obs_log(LOG_INFO, "[LinuxInput] Raw input registered successfully");
	m_initialized = true;

	// Event loop
	while (m_running) {
		// Use XPending to check for events without blocking indefinitely
		while (XPending(m_display) > 0) {
			XEvent event;
			XNextEvent(m_display, &event);

			// Handle XInput2 events
			if (event.xcookie.type == GenericEvent && event.xcookie.extension == m_xi_opcode) {
				if (XGetEventData(m_display, &event.xcookie)) {
					XIRawEvent *raw_event = static_cast<XIRawEvent *>(event.xcookie.data);

					switch (event.xcookie.evtype) {
					case XI_RawMotion:
						process_raw_motion(raw_event);
						break;
					case XI_RawButtonPress:
						process_raw_button(raw_event, true);
						break;
					case XI_RawButtonRelease:
						process_raw_button(raw_event, false);
						break;
					case XI_RawKeyPress:
						process_raw_key(raw_event, true);
						break;
					case XI_RawKeyRelease:
						process_raw_key(raw_event, false);
						break;
					}

					XFreeEventData(m_display, &event.xcookie);
				}
			}
		}

		// Small sleep to prevent busy-waiting when no events
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// Cleanup
	XCloseDisplay(m_display);
	m_display = nullptr;
}

LinuxInputDevice::LinuxInputDevice()
	: m_display(nullptr),
	  m_xi_opcode(0),
	  m_xi_event_base(0),
	  m_xi_error_base(0),
	  m_running(true),
	  m_initialized(false),
	  m_mouse_delta_x(0),
	  m_mouse_delta_y(0),
	  m_wheel_delta(0),
	  m_hwheel_delta(0),
	  m_mouse_flags(0),
	  m_mouse_buttons{},
	  m_key_states{}
{
	obs_log(LOG_INFO, "[LinuxInput] Starting input capture");
	m_event_thread = std::thread(&LinuxInputDevice::event_loop, this);

	// Wait for initialization (with timeout)
	int attempts = 0;
	while (!m_initialized && attempts < 50) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		attempts++;
	}

	if (!m_initialized) {
		obs_log(LOG_ERROR, "[LinuxInput] Failed to initialize within timeout");
	}
}

LinuxInputDevice::~LinuxInputDevice()
{
	obs_log(LOG_INFO, "[LinuxInput] Stopping input capture");
	m_running = false;
	if (m_event_thread.joinable()) {
		m_event_thread.join();
	}
}

void LinuxInputDevice::write_header(InputWriter &writer)
{
	writer.begin_header();

	// Mouse - screen cursor position
	writer.append_header(static_cast<int16_t>(0), "mouse_x");
	writer.append_header(static_cast<int16_t>(0), "mouse_y");
	// Mouse - raw device deltas (accumulated between samples)
	writer.append_header(static_cast<int16_t>(0), "mouse_delta_x");
	writer.append_header(static_cast<int16_t>(0), "mouse_delta_y");
	// Mouse buttons and wheel
	writer.append_header(false, "mouse_left");
	writer.append_header(false, "mouse_right");
	writer.append_header(false, "mouse_middle");
	writer.append_header(false, "mouse_x1");
	writer.append_header(false, "mouse_x2");
	writer.append_header(static_cast<int16_t>(0), "wheel");
	writer.append_header(static_cast<int16_t>(0), "hwheel");

	// Mouse state indicators (same as Windows for compatibility)
	writer.append_header(static_cast<int16_t>(0), "mouse_flags");
	writer.append_header(false, "cursor_visible");
	writer.append_header(false, "cursor_clipped");

	// Active window context
	writer.append_header(std::string{}, "active_executable");
	writer.append_header(std::string{}, "active_window");

	// Keyboard - names come from the same TRACKED_KEYS_LINUX array
	for (size_t i = 0; i < NUM_TRACKED_KEYS_LINUX; i++) {
		writer.append_header(false, KeyLookupLinux::to_name(static_cast<int>(i)));
	}

	writer.end_header();
}

void LinuxInputDevice::write_state(InputWriter &writer)
{
	// Get current cursor position
	Window root_return, child_return;
	int root_x, root_y, win_x, win_y;
	unsigned int mask_return;

	int cursor_x = 0, cursor_y = 0;
	if (m_display && XQueryPointer(m_display, DefaultRootWindow(m_display), &root_return, &child_return, &root_x,
				       &root_y, &win_x, &win_y, &mask_return)) {
		cursor_x = root_x;
		cursor_y = root_y;
	}

	// Get cursor state
	bool cursor_visible = is_cursor_visible();
	bool cursor_grabbed = m_display ? is_pointer_grabbed() : false;

	// Get active window info
	std::string active_executable = m_display ? get_active_window_executable() : "";
	std::string active_window = m_display ? get_active_window_title() : "";

	std::lock_guard<std::mutex> lock(m_state_mutex);

	writer.begin_row();

	// Mouse screen position
	writer.append_row(static_cast<int16_t>(std::clamp(cursor_x, -32768, 32767)));
	writer.append_row(static_cast<int16_t>(std::clamp(cursor_y, -32768, 32767)));

	// Mouse raw deltas (reset after reading)
	writer.append_row(static_cast<int16_t>(std::clamp(m_mouse_delta_x, -32768, 32767)));
	writer.append_row(static_cast<int16_t>(std::clamp(m_mouse_delta_y, -32768, 32767)));
	m_mouse_delta_x = 0;
	m_mouse_delta_y = 0;

	// Mouse buttons
	for (int i = 0; i < 5; i++) {
		writer.append_row(m_mouse_buttons[i]);
	}

	// Wheel (reset after reading)
	writer.append_row(m_wheel_delta);
	writer.append_row(m_hwheel_delta);
	m_wheel_delta = 0;
	m_hwheel_delta = 0;

	// Mouse state indicators
	writer.append_row(m_mouse_flags);
	writer.append_row(cursor_visible);
	writer.append_row(cursor_grabbed); // cursor_clipped maps to grabbed on Linux

	// Active window context
	writer.append_row(active_executable);
	writer.append_row(active_window);

	// Keyboard
	for (size_t i = 0; i < NUM_TRACKED_KEYS_LINUX; i++) {
		writer.append_row(m_key_states[i]);
	}

	writer.end_row();
}

#endif // __linux__
