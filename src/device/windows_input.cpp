#ifdef _WIN32

#include "windows_input.hpp"
#include "writer/input_writer.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <vector>
#include <string>
#include <Psapi.h>

// Helper to convert wide string to UTF-8
static std::string wide_to_utf8(const std::wstring &wide)
{
	if (wide.empty())
		return "";

	int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (utf8_len == 0)
		return "";

	std::string utf8(utf8_len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8_len, nullptr, nullptr);
	return utf8;
}

// Helper to get active window title as UTF-8 string
static std::string get_active_window_title(HWND hwnd)
{
	if (!hwnd)
		return "";

	int len = GetWindowTextLengthW(hwnd);
	if (len == 0)
		return "";

	std::wstring title(len + 1, L'\0');
	GetWindowTextW(hwnd, &title[0], len + 1);
	title.resize(len);

	return wide_to_utf8(title);
}

// Helper to get executable name from window handle
static std::string get_window_executable(HWND hwnd)
{
	if (!hwnd)
		return "";

	// Get process ID from window
	DWORD process_id = 0;
	GetWindowThreadProcessId(hwnd, &process_id);
	if (process_id == 0)
		return "";

	// Open process to query its executable path
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
	if (!process)
		return "";

	// Get full executable path
	wchar_t path[MAX_PATH] = {};
	DWORD path_size = MAX_PATH;
	BOOL success = QueryFullProcessImageNameW(process, 0, path, &path_size);
	CloseHandle(process);

	if (!success)
		return "";

	// Extract just the filename from the path
	std::wstring full_path(path);
	size_t last_slash = full_path.find_last_of(L"\\/");
	std::wstring exe_name = (last_slash != std::wstring::npos) ? full_path.substr(last_slash + 1) : full_path;

	return wide_to_utf8(exe_name);
}

// Static member definition - lookup table built once at startup
KeyLookup WindowsInputDevice::s_key_lookup;

LRESULT CALLBACK WindowsInputDevice::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_INPUT) {
		WindowsInputDevice *device =
			reinterpret_cast<WindowsInputDevice *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		if (device) {
			device->process_raw_input(lparam);
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

void WindowsInputDevice::process_raw_input(LPARAM lparam)
{
	UINT size = 0;
	GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));

	if (size == 0)
		return;

	std::vector<BYTE> buffer(size);
	if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, buffer.data(), &size,
			    sizeof(RAWINPUTHEADER)) != size) {
		return;
	}

	RAWINPUT *raw = reinterpret_cast<RAWINPUT *>(buffer.data());

	if (raw->header.dwType == RIM_TYPEMOUSE) {
		process_mouse(raw->data.mouse);
	} else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
		process_keyboard(raw->data.keyboard);
	}
}

void WindowsInputDevice::process_mouse(const RAWMOUSE &mouse)
{
	std::lock_guard<std::mutex> lock(m_state_mutex);

	// Store raw input flags (MOUSE_MOVE_ABSOLUTE, MOUSE_VIRTUAL_DESKTOP, etc.)
	m_mouse_flags = static_cast<int16_t>(mouse.usFlags);

	// Accumulate raw relative movement (from device)
	if (!(mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
		m_mouse_delta_x += mouse.lLastX;
		m_mouse_delta_y += mouse.lLastY;
	}

	// Handle buttons
	if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
		m_mouse_buttons[0] = true;
	if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
		m_mouse_buttons[0] = false;

	if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
		m_mouse_buttons[1] = true;
	if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
		m_mouse_buttons[1] = false;

	if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
		m_mouse_buttons[2] = true;
	if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
		m_mouse_buttons[2] = false;

	if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
		m_mouse_buttons[3] = true;
	if (mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
		m_mouse_buttons[3] = false;

	if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
		m_mouse_buttons[4] = true;
	if (mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
		m_mouse_buttons[4] = false;

	// Handle wheel
	if (mouse.usButtonFlags & RI_MOUSE_WHEEL) {
		m_wheel_delta += static_cast<SHORT>(mouse.usButtonData);
	}
	if (mouse.usButtonFlags & RI_MOUSE_HWHEEL) {
		m_hwheel_delta += static_cast<SHORT>(mouse.usButtonData);
	}
}

void WindowsInputDevice::process_keyboard(const RAWKEYBOARD &keyboard)
{
	int index = s_key_lookup.to_index(keyboard.VKey);
	if (index < 0)
		return;

	std::lock_guard<std::mutex> lock(m_state_mutex);

	// RI_KEY_BREAK means key up, otherwise key down
	bool pressed = !(keyboard.Flags & RI_KEY_BREAK);
	m_key_states[index] = pressed;
}

void WindowsInputDevice::message_loop()
{
	const wchar_t CLASS_NAME[] = L"InputRecRawInputWindow";

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.lpfnWndProc = window_proc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = CLASS_NAME;

	m_window_class = RegisterClassExW(&wc);
	if (!m_window_class) {
		obs_log(LOG_ERROR, "[WindowsInput] Failed to register window class: %lu", GetLastError());
		return;
	}

	// HWND_MESSAGE creates a message-only window (invisible, no UI)
	m_hwnd = CreateWindowExW(0, CLASS_NAME, L"InputRecRawInput", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
				 GetModuleHandle(nullptr), nullptr);

	if (!m_hwnd) {
		obs_log(LOG_ERROR, "[WindowsInput] Failed to create window: %lu", GetLastError());
		UnregisterClassW(CLASS_NAME, GetModuleHandle(nullptr));
		m_window_class = 0;
		return;
	}

	// Store pointer to this object in the window for use in window_proc
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	// Register for raw input (mouse and keyboard)
	RAWINPUTDEVICE rid[2] = {};

	// Mouse
	rid[0].usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
	rid[0].usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
	rid[0].dwFlags = RIDEV_INPUTSINK;
	rid[0].hwndTarget = m_hwnd;

	// Keyboard
	rid[1].usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
	rid[1].usUsage = 0x06;     // HID_USAGE_GENERIC_KEYBOARD
	rid[1].dwFlags = RIDEV_INPUTSINK;
	rid[1].hwndTarget = m_hwnd;

	if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
		obs_log(LOG_ERROR, "[WindowsInput] Failed to register raw input devices: %lu", GetLastError());
		DestroyWindow(m_hwnd);
		UnregisterClassW(CLASS_NAME, GetModuleHandle(nullptr));
		m_hwnd = nullptr;
		m_window_class = 0;
		return;
	}

	obs_log(LOG_INFO, "[WindowsInput] Raw input registered successfully");
	m_initialized = true;

	// Message loop
	MSG msg;
	while (m_running) {
		DWORD result = MsgWaitForMultipleObjects(0, nullptr, FALSE, 100, QS_ALLINPUT);

		if (result == WAIT_OBJECT_0) {
			while (PeekMessage(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	// Cleanup
	DestroyWindow(m_hwnd);
	UnregisterClassW(CLASS_NAME, GetModuleHandle(nullptr));
	m_hwnd = nullptr;
	m_window_class = 0;
}

WindowsInputDevice::WindowsInputDevice()
	: m_hwnd(nullptr),
	  m_window_class(0),
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
	obs_log(LOG_INFO, "[WindowsInput] Starting input capture");
	m_message_thread = std::thread(&WindowsInputDevice::message_loop, this);

	// Wait for initialization (with timeout)
	int attempts = 0;
	while (!m_initialized && attempts < 50) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		attempts++;
	}

	if (!m_initialized) {
		obs_log(LOG_ERROR, "[WindowsInput] Failed to initialize within timeout");
	}
}

WindowsInputDevice::~WindowsInputDevice()
{
	obs_log(LOG_INFO, "[WindowsInput] Stopping input capture");
	m_running = false;
	if (m_message_thread.joinable()) {
		m_message_thread.join();
	}
}

void WindowsInputDevice::write_header(InputWriter &writer)
{
	writer.begin_header();

	// Mouse - screen cursor position (from GetCursorPos)
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

	// Mouse state indicators
	writer.append_header(static_cast<int16_t>(0), "mouse_flags"); // Raw input flags (MOUSE_MOVE_ABSOLUTE, etc.)
	writer.append_header(false, "cursor_visible");                // Is cursor visible?
	writer.append_header(false, "cursor_clipped");                // Is cursor confined to a region?

	// Active window context
	writer.append_header(std::string{}, "active_executable"); // e.g., "obs64.exe"
	writer.append_header(std::string{}, "active_window");     // e.g., "OBS 32.0.0 - Profile: ..."

	// Keyboard - names come from the same TRACKED_KEYS array
	for (size_t i = 0; i < NUM_TRACKED_KEYS; i++) {
		writer.append_header(false, KeyLookup::to_name(static_cast<int>(i)));
	}

	writer.end_header();
}

void WindowsInputDevice::write_state(InputWriter &writer)
{
	// Get current cursor position (outside of lock since it's a separate API call)
	POINT cursor_pos = {0, 0};
	GetCursorPos(&cursor_pos);

	// Check if cursor is visible
	CURSORINFO cursor_info = {};
	cursor_info.cbSize = sizeof(CURSORINFO);
	bool cursor_visible = false;
	if (GetCursorInfo(&cursor_info)) {
		cursor_visible = (cursor_info.flags & CURSOR_SHOWING) != 0;
	}

	// Check if cursor is clipped to a region
	RECT clip_rect = {};
	bool cursor_clipped = false;
	if (GetClipCursor(&clip_rect)) {
		// If clip rect doesn't match full virtual screen, cursor is clipped
		int screen_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
		int screen_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
		int screen_right = screen_left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
		int screen_bottom = screen_top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
		cursor_clipped = (clip_rect.left > screen_left || clip_rect.top > screen_top ||
				  clip_rect.right < screen_right || clip_rect.bottom < screen_bottom);
	}

	// Get active window info
	HWND fg_hwnd = GetForegroundWindow();
	std::string active_executable = get_window_executable(fg_hwnd);
	std::string active_window = get_active_window_title(fg_hwnd);

	std::lock_guard<std::mutex> lock(m_state_mutex);

	writer.begin_row();

	// Mouse screen position (from GetCursorPos)
	writer.append_row(static_cast<int16_t>(std::clamp(static_cast<int>(cursor_pos.x), -32768, 32767)));
	writer.append_row(static_cast<int16_t>(std::clamp(static_cast<int>(cursor_pos.y), -32768, 32767)));

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
	writer.append_row(cursor_clipped);

	// Active window context
	writer.append_row(active_executable);
	writer.append_row(active_window);

	// Keyboard
	for (size_t i = 0; i < NUM_TRACKED_KEYS; i++) {
		writer.append_row(m_key_states[i]);
	}

	writer.end_row();
}

#endif // _WIN32
