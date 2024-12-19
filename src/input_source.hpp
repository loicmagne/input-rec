#pragma once
#include <vector>
#include <atomic>
#include <thread>
#include <SDL3/SDL.h>
#include <fstream>
#include <filesystem>
#include "utils.hpp"

class gamepad_manager {
private:
	std::vector<SDL_Gamepad *> m_gamepads;
	std::ofstream m_file;
	std::filesystem::path m_file_path;
	rec_timer m_timer;
	std::atomic<bool> m_running{false};
	std::thread m_thread_loop;
	void init_gamepads();
	void add_gamepad(SDL_JoystickID joystickid);
	void remove_gamepad(SDL_JoystickID joystickid);
	int get_gamepad_idx(SDL_JoystickID joystickid);
	SDL_Gamepad *active_gamepad();
	void save_gamepad_state();
	void loop();

public:
	gamepad_manager();
	~gamepad_manager();
	void prepare_recording();
	void start_recording();
	void stop_recording();
	void close_recording(std::string recording_path);
};

bool initialize_rec_source();
