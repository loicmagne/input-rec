#ifdef __linux__

#include "linux_evdev.hpp"
#include "writer/input_writer.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <poll.h>
#include <cstring>
#include <climits>
#include <algorithm>

// X11 for window info - include here to avoid polluting header
#include <X11/Xlib.h>
#include <X11/Xatom.h>

// Static member definition
KeyLookupLinux LinuxEvdevDevice::s_key_lookup;

// Helper to get active window from _NET_ACTIVE_WINDOW
static Window get_active_window(Display *display)
{
	if (!display) return None;

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

std::string LinuxEvdevDevice::get_active_window_title()
{
	Display *display = static_cast<Display *>(m_x_display);
	Window active = get_active_window(display);
	if (active == None) return "";

	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = nullptr;

	// Try _NET_WM_NAME first (UTF-8)
	Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
	Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);

	if (XGetWindowProperty(display, active, net_wm_name, 0, 256, False, utf8_string, &actual_type, &actual_format,
			       &nitems, &bytes_after, &prop) == Success &&
	    prop) {
		std::string title(reinterpret_cast<char *>(prop));
		XFree(prop);
		return title;
	}

	// Fallback to WM_NAME
	char *name = nullptr;
	if (XFetchName(display, active, &name) && name) {
		std::string title(name);
		XFree(name);
		return title;
	}

	return "";
}

std::string LinuxEvdevDevice::get_active_window_executable()
{
	Display *display = static_cast<Display *>(m_x_display);
	Window active = get_active_window(display);
	if (active == None) return "";

	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = nullptr;

	// Get _NET_WM_PID
	Atom net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
	Atom cardinal = XInternAtom(display, "CARDINAL", False);

	if (XGetWindowProperty(display, active, net_wm_pid, 0, 1, False, cardinal, &actual_type, &actual_format,
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

void LinuxEvdevDevice::scan_and_open_devices()
{
	const char *input_dir = "/dev/input";
	DIR *dir = opendir(input_dir);
	if (!dir) {
		obs_log(LOG_ERROR, "[LinuxInput] Cannot open %s: %s", input_dir, strerror(errno));
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != nullptr) {
		// Only look at event* devices
		if (strncmp(entry->d_name, "event", 5) != 0) continue;

		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", input_dir, entry->d_name);

		int fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			obs_log(LOG_DEBUG, "[LinuxInput] Cannot open %s: %s", path, strerror(errno));
			continue;
		}

		struct libevdev *dev = nullptr;
		int rc = libevdev_new_from_fd(fd, &dev);
		if (rc < 0) {
			obs_log(LOG_DEBUG, "[LinuxInput] Failed to create evdev for %s: %s", path, strerror(-rc));
			close(fd);
			continue;
		}

		const char *name = libevdev_get_name(dev);
		bool is_keyboard = libevdev_has_event_type(dev, EV_KEY) && libevdev_has_event_code(dev, EV_KEY, KEY_A);
		bool is_mouse = libevdev_has_event_type(dev, EV_REL) &&
				(libevdev_has_event_code(dev, EV_REL, REL_X) || libevdev_has_event_code(dev, EV_REL, REL_Y));

		// Also check for mouse buttons
		if (!is_mouse && libevdev_has_event_type(dev, EV_KEY) && libevdev_has_event_code(dev, EV_KEY, BTN_LEFT)) {
			is_mouse = true;
		}

		if (is_keyboard || is_mouse) {
			EvdevDevice device;
			device.fd = fd;
			device.dev = dev;
			device.is_keyboard = is_keyboard;
			device.is_mouse = is_mouse;
			device.name = name ? name : "Unknown";

			m_devices.push_back(device);
			obs_log(LOG_INFO, "[LinuxInput] Opened %s: %s (keyboard=%d, mouse=%d)", path, device.name.c_str(),
				is_keyboard, is_mouse);
		} else {
			libevdev_free(dev);
			close(fd);
		}
	}

	closedir(dir);
	obs_log(LOG_INFO, "[LinuxInput] Opened %zu input devices", m_devices.size());
}

void LinuxEvdevDevice::process_event(const EvdevDevice &device, const struct input_event &ev)
{
	std::lock_guard<std::mutex> lock(m_state_mutex);

	if (ev.type == EV_REL) {
		// Relative motion (mouse)
		switch (ev.code) {
		case REL_X: m_mouse_delta_x += ev.value; break;
		case REL_Y: m_mouse_delta_y += ev.value; break;
		case REL_WHEEL: m_wheel_delta += static_cast<int16_t>(ev.value * 120); break;
		case REL_HWHEEL: m_hwheel_delta += static_cast<int16_t>(ev.value * 120); break;
		}
	} else if (ev.type == EV_ABS) {
		// Absolute position (touchpad, tablet) - compute deltas from position changes
		switch (ev.code) {
		case ABS_X:
			if (m_abs_x_valid) {
				m_mouse_delta_x += ev.value - m_last_abs_x;
			}
			m_last_abs_x = ev.value;
			m_abs_x_valid = true;
			break;
		case ABS_Y:
			if (m_abs_y_valid) {
				m_mouse_delta_y += ev.value - m_last_abs_y;
			}
			m_last_abs_y = ev.value;
			m_abs_y_valid = true;
			break;
		}
	} else if (ev.type == EV_KEY) {
		bool pressed = (ev.value != 0); // 1 = press, 0 = release, 2 = repeat

		// Reset touchpad tracking when finger is lifted
		if ((ev.code == BTN_TOUCH || ev.code == BTN_TOOL_FINGER) && !pressed) {
			m_abs_x_valid = false;
			m_abs_y_valid = false;
		}

		// Mouse buttons
		switch (ev.code) {
		case BTN_LEFT: m_mouse_buttons[0] = pressed; return;
		case BTN_RIGHT: m_mouse_buttons[1] = pressed; return;
		case BTN_MIDDLE: m_mouse_buttons[2] = pressed; return;
		case BTN_SIDE: m_mouse_buttons[3] = pressed; return;   // X1/Back
		case BTN_EXTRA: m_mouse_buttons[4] = pressed; return;  // X2/Forward
		case BTN_TOUCH: return; // Don't treat as keyboard key
		case BTN_TOOL_FINGER: return; // Don't treat as keyboard key
		}

		// Keyboard keys
		int index = s_key_lookup.to_index(ev.code);
		if (index >= 0) {
			m_key_states[index] = pressed;
		}
	}
}

void LinuxEvdevDevice::event_loop()
{
	obs_log(LOG_INFO, "[LinuxInput] Event loop starting");

	// Build poll fd array
	std::vector<struct pollfd> fds;
	for (const auto &device : m_devices) {
		struct pollfd pfd;
		pfd.fd = device.fd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		fds.push_back(pfd);
	}

	if (fds.empty()) {
		obs_log(LOG_ERROR, "[LinuxInput] No devices to poll");
		return;
	}

	m_initialized = true;
	obs_log(LOG_INFO, "[LinuxInput] Polling %zu devices", fds.size());

	while (m_running) {
		int ret = poll(fds.data(), fds.size(), 10); // 10ms timeout

		if (ret < 0) {
			if (errno != EINTR) {
				obs_log(LOG_ERROR, "[LinuxInput] poll error: %s", strerror(errno));
			}
			continue;
		}

		if (ret == 0) continue; // Timeout, no events

		// Process events from all ready devices
		for (size_t i = 0; i < fds.size(); i++) {
			if (!(fds[i].revents & POLLIN)) continue;

			struct input_event ev;
			int rc;
			while ((rc = libevdev_next_event(m_devices[i].dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS) {
				process_event(m_devices[i], ev);
			}

			// Handle sync events (dropped events)
			if (rc == LIBEVDEV_READ_STATUS_SYNC) {
				while ((rc = libevdev_next_event(m_devices[i].dev, LIBEVDEV_READ_FLAG_SYNC, &ev)) == LIBEVDEV_READ_STATUS_SYNC) {
					process_event(m_devices[i], ev);
				}
			}
		}
	}

	obs_log(LOG_INFO, "[LinuxInput] Event loop exiting");
}

LinuxEvdevDevice::LinuxEvdevDevice()
	: m_running(true),
	  m_initialized(false),
	  m_mouse_x(0),
	  m_mouse_y(0),
	  m_mouse_delta_x(0),
	  m_mouse_delta_y(0),
	  m_wheel_delta(0),
	  m_hwheel_delta(0),
	  m_mouse_flags(0),
	  m_mouse_buttons{},
	  m_last_abs_x(0),
	  m_last_abs_y(0),
	  m_abs_x_valid(false),
	  m_abs_y_valid(false),
	  m_key_states{},
	  m_x_display(nullptr)
{
	obs_log(LOG_INFO, "[LinuxInput] Starting input capture (evdev)");

	// Open X11 connection for window info
	m_x_display = XOpenDisplay(nullptr);
	if (!m_x_display) {
		obs_log(LOG_WARNING, "[LinuxInput] Cannot open X display - window info will be unavailable");
	}

	// Scan and open input devices
	scan_and_open_devices();

	if (m_devices.empty()) {
		obs_log(LOG_ERROR, "[LinuxInput] No input devices found! Make sure you're in the 'input' group.");
		obs_log(LOG_ERROR, "[LinuxInput] Run: sudo usermod -a -G input $USER  (then log out and back in)");
		return;
	}

	// Start event thread
	m_event_thread = std::thread(&LinuxEvdevDevice::event_loop, this);

	// Wait for initialization
	int attempts = 0;
	while (!m_initialized && attempts < 50) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		attempts++;
	}

	if (!m_initialized) {
		obs_log(LOG_ERROR, "[LinuxInput] Failed to initialize within timeout");
	}
}

LinuxEvdevDevice::~LinuxEvdevDevice()
{
	obs_log(LOG_INFO, "[LinuxInput] Stopping input capture");
	m_running = false;

	if (m_event_thread.joinable()) {
		m_event_thread.join();
	}

	// Close all evdev devices
	for (auto &device : m_devices) {
		if (device.dev) {
			libevdev_free(device.dev);
		}
		if (device.fd >= 0) {
			close(device.fd);
		}
	}
	m_devices.clear();

	// Close X11 connection
	if (m_x_display) {
		XCloseDisplay(static_cast<Display *>(m_x_display));
		m_x_display = nullptr;
	}
}

void LinuxEvdevDevice::write_header(InputWriter &writer)
{
	writer.begin_header();

	// Mouse
	writer.append_header(static_cast<int16_t>(0), "mouse_x");
	writer.append_header(static_cast<int16_t>(0), "mouse_y");
	writer.append_header(static_cast<int16_t>(0), "mouse_delta_x");
	writer.append_header(static_cast<int16_t>(0), "mouse_delta_y");
	writer.append_header(false, "mouse_left");
	writer.append_header(false, "mouse_right");
	writer.append_header(false, "mouse_middle");
	writer.append_header(false, "mouse_x1");
	writer.append_header(false, "mouse_x2");
	writer.append_header(static_cast<int16_t>(0), "wheel");
	writer.append_header(static_cast<int16_t>(0), "hwheel");

	// Mouse state indicators (compatibility with Windows)
	writer.append_header(static_cast<int16_t>(0), "mouse_flags");
	writer.append_header(false, "cursor_visible");
	writer.append_header(false, "cursor_clipped");

	// Active window context
	writer.append_header(std::string{}, "active_executable");
	writer.append_header(std::string{}, "active_window");

	// Keyboard
	for (size_t i = 0; i < NUM_TRACKED_KEYS_LINUX; i++) {
		writer.append_header(false, KeyLookupLinux::to_name(static_cast<int>(i)));
	}

	writer.end_header();
}

void LinuxEvdevDevice::write_state(InputWriter &writer)
{
	// Get active window info (only works for X11/XWayland apps on Wayland)
	std::string active_executable = m_x_display ? get_active_window_executable() : "";
	std::string active_window = m_x_display ? get_active_window_title() : "";

	std::lock_guard<std::mutex> lock(m_state_mutex);

	// Update accumulated position from deltas
	// (On Wayland, XQueryPointer doesn't work, so we track position ourselves)
	m_mouse_x += m_mouse_delta_x;
	m_mouse_y += m_mouse_delta_y;

	writer.begin_row();

	// Mouse position - accumulated from deltas (relative tracking)
	// Note: On Wayland, absolute screen position is not available
	writer.append_row(static_cast<int16_t>(std::clamp(m_mouse_x, -32768, 32767)));
	writer.append_row(static_cast<int16_t>(std::clamp(m_mouse_y, -32768, 32767)));

	// Mouse deltas (reset after reading)
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
	writer.append_row(true);  // cursor_visible - assume true on Linux
	writer.append_row(false); // cursor_clipped - no equivalent on Linux

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
