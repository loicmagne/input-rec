#include <iostream>
#include <chrono>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <random>

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "input_source.hpp"
#include "plugin-support.h"

namespace fs = std::filesystem;

constexpr int16_t AXIS_DEADZONE = 0;

gamepad_manager::gamepad_manager() : m_timer{}
{
	/* Init SDL, see SDL/test/testcontroller.c */
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ROG_CHAKRAM, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	SDL_SetHint(SDL_HINT_JOYSTICK_LINUX_DEADZONES, "1");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) != 0) {
		obs_log(LOG_ERROR, "SDL_Init Error: %s", SDL_GetError());
	}

	SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");

#ifdef DEBUG
	int count = 0;
	char **mappings = SDL_GetGamepadMappings(&count);
	int map_i;
	SDL_Log("Supported mappings:\n");
	for (map_i = 0; map_i < count; ++map_i) {
		SDL_Log("\t%s\n", mappings[map_i]);
	}
	SDL_Log("\n");
	SDL_free(mappings);
#endif

	init_gamepads();
}

void gamepad_manager::init_gamepads()
{
	int count = 0;
	SDL_JoystickID *joystick_ids = SDL_GetGamepads(&count);
	if (joystick_ids) {
		for (int i = 0; i < count; ++i) {
			add_gamepad(joystick_ids[i]);
		}
		SDL_free(joystick_ids);
	} else {
		std::cerr << "Failed to get gamepads: " << SDL_GetError() << std::endl;
	}
}

void gamepad_manager::add_gamepad(SDL_JoystickID joystickid)
{
	SDL_Gamepad *gamepad = SDL_OpenGamepad(joystickid);
	if (gamepad) {
		m_gamepads.push_back(gamepad);
	} else {
		std::cerr << "Failed to open gamepad: " << SDL_GetError() << std::endl;
	}
}

int gamepad_manager::get_gamepad_idx(SDL_JoystickID joystickid)
{
	for (size_t i = 0; i < m_gamepads.size(); ++i) {
		if (m_gamepads[i]) {
			SDL_Joystick *joystick = SDL_GetGamepadJoystick(m_gamepads[i]);
			if (joystick && SDL_GetJoystickID(joystick) == joystickid) {
				return static_cast<int>(i);
			}
		}
	}
	return -1;
}

void gamepad_manager::remove_gamepad(SDL_JoystickID joystickid)
{
	int i = get_gamepad_idx(joystickid);
	if (m_gamepads[i]) {
		SDL_CloseGamepad(m_gamepads[i]);
		m_gamepads[i] = nullptr;
	}
}

SDL_Gamepad *gamepad_manager::active_gamepad()
{
	for (auto gamepad : m_gamepads) {
		if (gamepad) {
			return gamepad;
		}
	}
	return nullptr;
}

void gamepad_manager::save_gamepad_state()
{
	SDL_Gamepad *gamepad = active_gamepad();
	// Print number of gamepads
	if (gamepad) {
		auto dt = m_timer.elapsed();
		if (!dt)
			return;

		int i;
		m_file << *dt << ",";
		for (i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
			const SDL_GamepadButton button = (SDL_GamepadButton)i;
			const bool pressed = SDL_GetGamepadButton(gamepad, button) == true;
			m_file << pressed << ",";
		}

		for (i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
			const SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
			int16_t value = SDL_GetGamepadAxis(gamepad, axis);
			value = (value < AXIS_DEADZONE && value > -AXIS_DEADZONE) ? 0 : value;
			m_file << value << ",";
		}

		m_file << std::endl;
	}
}

gamepad_manager::~gamepad_manager()
{
	m_running = false;
	if (m_thread_loop.joinable()) {
		m_thread_loop.join();
	}
	for (auto gamepad : m_gamepads) {
		SDL_CloseGamepad(gamepad);
	}
	m_file.close();
	SDL_Quit();
}

void gamepad_manager::loop()
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
		default:
			break;
		}
	}
	save_gamepad_state();
	SDL_Delay(2);
}

void gamepad_manager::prepare_recording()
{
	// Create tmp file with random name
	m_file_path = fs::temp_directory_path() /
		      fs::path("obs_input_rec_" + std::to_string(std::random_device{}()) + ".csv");
	m_file.open(m_file_path, std::ios::trunc);

	if (!m_file.is_open()) {
		std::cerr << "Failed to open file: " << m_file_path << std::endl;
		return;
	}

	m_file << "time,";
	for (int i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
		m_file << button_to_string((SDL_GamepadButton)i) << ",";
	}
	for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
		m_file << axis_to_string((SDL_GamepadAxis)i) << ",";
	}
	m_file << std::endl;
}

void gamepad_manager::start_recording()
{
	m_timer.start();
	m_running = true;

	m_thread_loop = std::thread([this]() {
		while (m_running) {
			loop();
		}
	});
}

void gamepad_manager::stop_recording()
{
	m_timer.stop();
	m_running = false;
	if (m_thread_loop.joinable())
		m_thread_loop.join();
}

void gamepad_manager::close_recording(std::string recording_path)
{
	// Close file and move it to the recording path with a .csv extension
	m_file.close();
	fs::path recording_csv = fs::path(recording_path).replace_extension(".csv");
	fs::rename(m_file_path, recording_csv);
}

class rec_source {
private:
	obs_data_t *m_settings;
	obs_source_t *m_source;
	gamepad_manager m_gamepad_manager;

public:
	rec_source(obs_data_t *settings, obs_source_t *source)
		: m_settings{settings},
		  m_source{source},
		  m_gamepad_manager{}
	{

		obs_frontend_add_event_callback(
			[](enum obs_frontend_event event, void *private_data) {
				gamepad_manager *current_gpm = static_cast<gamepad_manager *>(private_data);
				switch (event) {
				case OBS_FRONTEND_EVENT_RECORDING_STARTING: {
					obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STARTING received");
					current_gpm->prepare_recording();
					break;
				}
				case OBS_FRONTEND_EVENT_RECORDING_STARTED: {
					obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STARTED received");
					current_gpm->start_recording();
					break;
				}
				case OBS_FRONTEND_EVENT_RECORDING_STOPPING: {
					obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STOPPING received");
					current_gpm->stop_recording();
					break;
				}
				case OBS_FRONTEND_EVENT_RECORDING_STOPPED: {
					obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STOPPED received");
					// Get last recording path
					std::string recording_path{obs_frontend_get_last_recording()};
					current_gpm->close_recording(recording_path);
					break;
				}
				default:
					break;
				}
			},
			// pass gamepad_manager instance as private_data
			&m_gamepad_manager);
	}

	void tick(float seconds) { UNUSED_PARAMETER(seconds); }
};

bool initialize_rec_source()
{
	obs_source_info source_info = {};
	source_info.id = "input_recording_source";
	source_info.type = OBS_SOURCE_TYPE_INPUT;
	source_info.output_flags = OBS_SOURCE_VIDEO;
	source_info.icon_type = OBS_ICON_TYPE_GAME_CAPTURE;
	source_info.get_name = [](void *) {
		return obs_module_text("InputRecording");
		;
	};
	source_info.get_width = [](void *) -> uint32_t {
		return 0;
	};
	source_info.get_height = [](void *) -> uint32_t {
		return 0;
	};
	source_info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		return static_cast<void *>(new rec_source(settings, source));
	};
	source_info.destroy = [](void *data) {
		delete static_cast<rec_source *>(data);
	};
	source_info.video_tick = [](void *data, float seconds) {
		static_cast<rec_source *>(data)->tick(seconds);
	};
	obs_register_source(&source_info);
	return true;
}
