#pragma once
#include <vector>
#include <mutex>
#include <thread>
#include <SDL3/SDL.h>
#include "input_device.hpp"
#include "writer/input_writer.hpp"

class GamepadDevice : public InputDevice {
private:
	std::vector<SDL_Gamepad *> m_gamepads;
    std::mutex m_gamepads_mutex;
    std::thread m_polling_thread;
    std::atomic<bool> m_should_poll;

	void add_gamepad(SDL_JoystickID joystickid);
	void remove_gamepad(SDL_JoystickID joystickid);
	int get_gamepad_idx(SDL_JoystickID joystickid);
	void polling_loop();
	void polling_iter();
	SDL_Gamepad *active_gamepad();

public:
	GamepadDevice();
	~GamepadDevice() override;

	void write_header(InputWriter &writer) override;
	void write_state(InputWriter &writer) override;
};