#include "utils.hpp"

rec_timer::rec_timer() : m_start(std::chrono::high_resolution_clock::now())
{
	std::cout << "Timer created" << std::endl;
}

rec_timer::~rec_timer()
{
	std::cout << "Timer destroyed" << std::endl;
}

void rec_timer::start()
{
	m_start = std::chrono::high_resolution_clock::now();
	m_running = true;

	std::cout << "Timer started" << std::endl;
}

void rec_timer::stop()
{
	m_running = false;
}

std::optional<int64_t> rec_timer::elapsed()
{
	if (!m_running)
		return std::nullopt;
	auto now = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
		now - m_start);
	auto dt = duration.count();
	return dt;
}

std::string axis_to_string(SDL_GamepadAxis axis)
{
	switch (axis) {
	case SDL_GAMEPAD_AXIS_LEFTX:
		return "LEFTX";
	case SDL_GAMEPAD_AXIS_LEFTY:
		return "LEFTY";
	case SDL_GAMEPAD_AXIS_RIGHTX:
		return "RIGHTX";
	case SDL_GAMEPAD_AXIS_RIGHTY:
		return "RIGHTY";
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
		return "LEFT_TRIGGER";
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
		return "RIGHT_TRIGGER";
	default:
		return "UNKNOWN";
	}
}

std::string button_to_string(SDL_GamepadButton button)
{
	switch (button) {
	case SDL_GAMEPAD_BUTTON_SOUTH:
		return "SOUTH";
	case SDL_GAMEPAD_BUTTON_EAST:
		return "EAST";
	case SDL_GAMEPAD_BUTTON_WEST:
		return "WEST";
	case SDL_GAMEPAD_BUTTON_NORTH:
		return "NORTH";
	case SDL_GAMEPAD_BUTTON_BACK:
		return "BACK";
	case SDL_GAMEPAD_BUTTON_GUIDE:
		return "GUIDE";
	case SDL_GAMEPAD_BUTTON_START:
		return "START";
	case SDL_GAMEPAD_BUTTON_LEFT_STICK:
		return "LEFT_STICK";
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
		return "RIGHT_STICK";
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		return "LEFT_SHOULDER";
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		return "RIGHT_SHOULDER";
	case SDL_GAMEPAD_BUTTON_DPAD_UP:
		return "DPAD_UP";
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
		return "DPAD_DOWN";
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		return "DPAD_LEFT";
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		return "DPAD_RIGHT";
	case SDL_GAMEPAD_BUTTON_MISC1:
		return "MISC1";
	case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
		return "RIGHT_PADDLE1";
	case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
		return "LEFT_PADDLE1";
	case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
		return "RIGHT_PADDLE2";
	case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
		return "LEFT_PADDLE2";
	case SDL_GAMEPAD_BUTTON_TOUCHPAD:
		return "TOUCHPAD";
	case SDL_GAMEPAD_BUTTON_MISC2:
		return "MISC2";
	case SDL_GAMEPAD_BUTTON_MISC3:
		return "MISC3";
	case SDL_GAMEPAD_BUTTON_MISC4:
		return "MISC4";
	case SDL_GAMEPAD_BUTTON_MISC5:
		return "MISC5";
	case SDL_GAMEPAD_BUTTON_MISC6:
		return "MISC6";
	case SDL_GAMEPAD_BUTTON_MAX:
		return "MAX";
	default:
		return "UNKNOWN";
	}
}
