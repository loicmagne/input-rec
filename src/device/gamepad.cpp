#include "gamepad.hpp"
#include <iostream>
#include <string>

constexpr int16_t AXIS_DEADZONE = 0;

std::string axis_to_string(SDL_GamepadAxis axis)
{
	switch (axis) {
	case SDL_GAMEPAD_AXIS_LEFTX: return "LEFTX";
	case SDL_GAMEPAD_AXIS_LEFTY: return "LEFTY";
	case SDL_GAMEPAD_AXIS_RIGHTX: return "RIGHTX";
	case SDL_GAMEPAD_AXIS_RIGHTY: return "RIGHTY";
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return "LEFT_TRIGGER";
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return "RIGHT_TRIGGER";
	default: return "UNKNOWN";
	}
}

std::string button_to_string(SDL_GamepadButton button)
{
	switch (button) {
	case SDL_GAMEPAD_BUTTON_SOUTH: return "SOUTH";
	case SDL_GAMEPAD_BUTTON_EAST: return "EAST";
	case SDL_GAMEPAD_BUTTON_WEST: return "WEST";
	case SDL_GAMEPAD_BUTTON_NORTH: return "NORTH";
	case SDL_GAMEPAD_BUTTON_BACK: return "BACK";
	case SDL_GAMEPAD_BUTTON_GUIDE: return "GUIDE";
	case SDL_GAMEPAD_BUTTON_START: return "START";
	case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "LEFT_STICK";
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "RIGHT_STICK";
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "LEFT_SHOULDER";
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "RIGHT_SHOULDER";
	case SDL_GAMEPAD_BUTTON_DPAD_UP: return "DPAD_UP";
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "DPAD_DOWN";
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "DPAD_LEFT";
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "DPAD_RIGHT";
	case SDL_GAMEPAD_BUTTON_MISC1: return "MISC1";
	case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "RIGHT_PADDLE1";
	case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "LEFT_PADDLE1";
	case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "RIGHT_PADDLE2";
	case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "LEFT_PADDLE2";
	case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "TOUCHPAD";
	case SDL_GAMEPAD_BUTTON_MISC2: return "MISC2";
	case SDL_GAMEPAD_BUTTON_MISC3: return "MISC3";
	case SDL_GAMEPAD_BUTTON_MISC4: return "MISC4";
	case SDL_GAMEPAD_BUTTON_MISC5: return "MISC5";
	case SDL_GAMEPAD_BUTTON_MISC6: return "MISC6";
	case SDL_GAMEPAD_BUTTON_COUNT: return "MAX";
	default: return "UNKNOWN";
	}
}

void GamepadDevice::add_gamepad(SDL_JoystickID joystickid)
{
	SDL_Gamepad *gamepad = SDL_OpenGamepad(joystickid);
	if (gamepad) {
		m_gamepads.push_back(gamepad);
	} else {
		std::cerr << "Failed to open gamepad: " << SDL_GetError() << std::endl;
	}
}

void GamepadDevice::remove_gamepad(SDL_JoystickID joystickid)
{
	int i = get_gamepad_idx(joystickid);
	if (m_gamepads[i]) {
		SDL_CloseGamepad(m_gamepads[i]);
		m_gamepads[i] = nullptr;
	}
}

int GamepadDevice::get_gamepad_idx(SDL_JoystickID joystickid)
{
	for (size_t i = 0; i < m_gamepads.size(); ++i) {
		if (m_gamepads[i]) {
			SDL_Joystick *joystick = SDL_GetGamepadJoystick(m_gamepads[i]);
			if (joystick && SDL_GetJoystickID(joystick) == joystickid) { return static_cast<int>(i); }
		}
	}
	return -1;
}

SDL_Gamepad *GamepadDevice::active_gamepad()
{
	for (auto gamepad : m_gamepads) {
		if (gamepad) { return gamepad; }
	}
	return nullptr;
}

GamepadDevice::GamepadDevice()
{
	/* Init SDL, see SDL/test/testcontroller.c */
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ROG_CHAKRAM, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_LINUX_DEADZONES, "1");

	/* Enable input debug logging */
	SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_DEBUG);
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return;
	}

	SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");

#ifdef DEBUG
	int count = 0;
	char **mappings = SDL_GetGamepadMappings(&count);
	int map_i;
	SDL_Log("Supported mappings:\n");
	for (map_i = 0; map_i < count; ++map_i) { SDL_Log("\t%s\n", mappings[map_i]); }
	SDL_Log("\n");
	SDL_free(mappings);
#endif

	/* The following delay is necessary to avoid a crash on Linux
	The crash occurs in a particular setting, on Ubuntu:
	- if the gamepad is connected to the computer before obs is launched
	- if the obs instance has input-overlay and input-rec installed
	- if the obs instance has one source for both input-overlay and input-rec
	Then opening OBS will segfault. The issue seems to be about SDL initialization
	and gamepad detection. A delay >1.5s avoid the crash.

	I don't think the issue comes from input-rec, because this crash has been
	reported on the input-overlay repo: https://github.com/univrsal/input-overlay/issues/426
	The issue also occurs when using the obs input-overlay plugin only, without input-rec.
	*/
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	// Init gamepads
	int count = 0;
	SDL_JoystickID *joystick_ids = SDL_GetGamepads(&count);
	if (joystick_ids) {
		for (int i = 0; i < count; ++i) {
			add_gamepad(joystick_ids[i]);
			std::cout << "Gamepad added: " << joystick_ids[i] << std::endl;
		}
		SDL_free(joystick_ids);
	} else {
		std::cerr << "Failed to get gamepads: " << SDL_GetError() << std::endl;
	}
}

GamepadDevice::~GamepadDevice()
{
	for (auto gamepad : m_gamepads) { SDL_CloseGamepad(gamepad); }
	SDL_Quit();
}

void GamepadDevice::write_header(InputWriter &writer)
{
	writer.begin_header();
	for (int i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i)
		writer.append_header(false, button_to_string((SDL_GamepadButton)i));
	for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i)
		writer.append_header(static_cast<int16_t>(0), axis_to_string((SDL_GamepadAxis)i));
	writer.end_header();
}

void GamepadDevice::write_state(InputWriter &writer)
{
	SDL_Gamepad *gamepad = active_gamepad();
	if (gamepad) {
		writer.begin_row();
		for (int i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
			const SDL_GamepadButton button = (SDL_GamepadButton)i;
			const bool pressed = SDL_GetGamepadButton(gamepad, button) == true;
			writer.append_row(pressed);
		}

		for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
			const SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
			int16_t value = SDL_GetGamepadAxis(gamepad, axis);
			// TODO: Use dead zone?
			// value = (value < AXIS_DEADZONE && value > -AXIS_DEADZONE) ? 0 : value;
			writer.append_row(value);
		}
		writer.end_row();
	}
}

void GamepadDevice::loop(InputWriter &writer)
{
	SDL_Event event;
	SDL_PumpEvents();
	while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) == 1) {
		switch (event.type) {
		case SDL_EVENT_GAMEPAD_ADDED:
			add_gamepad(event.gdevice.which);
			std::cout << "Gamepad added" << std::endl;
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			remove_gamepad(event.gdevice.which);
			std::cout << "Gamepad removed" << std::endl;
			break;
		default: break;
		}
	}
	write_state(writer);
	// TODO: delay should be handled by the writer, not the device
	SDL_Delay(2);
}