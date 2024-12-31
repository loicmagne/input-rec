#pragma once
#include <vector>
#include <SDL3/SDL.h>
#include "input_device.hpp"
#include "writer/input_writer.hpp"

class GamepadDevice : public InputDevice {
private:
	std::vector<SDL_Gamepad *> m_gamepads;

	void add_gamepad(SDL_JoystickID joystickid);
	void remove_gamepad(SDL_JoystickID joystickid);
	int get_gamepad_idx(SDL_JoystickID joystickid);
	SDL_Gamepad *active_gamepad();

public:
	GamepadDevice();
	~GamepadDevice() override;

	void write_header(InputWriter &writer) override;
	void write_state(InputWriter &writer) override;
	void loop(InputWriter &writer) override;
};