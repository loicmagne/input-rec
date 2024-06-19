
#include <thread>
#include <iostream>
#include <chrono>
#include <atomic>
#include <fstream>
#include <filesystem>

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "input_source.hpp"
#include "plugin-support.h"
#include "utils.hpp"
#include "globals.hpp"

constexpr int16_t AXIS_DEADZONE = 0;

std::filesystem::path home_path() {
    const std::filesystem::path default_path {"/tmp"};
    char* path;
#ifdef _WIN32
    path = getenv("USERPROFILE");
    if (path == nullptr) path = getenv("HOMEPATH"); // Alternative on Windows
#else
    path = getenv("HOME");
#endif
    return (path != nullptr) ? std::filesystem::path(path) : default_path;
}

gamepad_manager::gamepad_manager():
    m_file(home_path()/"input_recording.csv", std::ios::out)
{
	/* Init SDL, see SDL/test/testcontroller.c */
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ROG_CHAKRAM, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_LINUX_DEADZONES, "1");
	if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) != 0) {
		obs_log(LOG_ERROR, "SDL_Init Error: %s", SDL_GetError());
    }

    SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");
    if (true) {
        int count = 0;
        char **mappings = SDL_GetGamepadMappings(&count);
        int map_i;
        SDL_Log("Supported mappings:\n");
        for (map_i = 0; map_i < count; ++map_i) {
            SDL_Log("\t%s\n", mappings[map_i]);
        }
        SDL_Log("\n");
        SDL_free(mappings);
    }
    init_gamepads();
    setup_file();
}

void gamepad_manager::init_gamepads() {
    int count = 0;
    SDL_JoystickID* joystick_ids = SDL_GetGamepads(&count);
    if (joystick_ids) {
        for (int i = 0; i < count; ++i) {
            add_gamepad(joystick_ids[i]);
        }
        SDL_free(joystick_ids);
    } else {
        std::cerr << "Failed to get gamepads: " << SDL_GetError() << std::endl;
    }
}

void gamepad_manager::add_gamepad(SDL_JoystickID joystickid) {
    SDL_Gamepad* gamepad = SDL_OpenGamepad(joystickid);
    if (gamepad) {
        m_gamepads.push_back(gamepad);
    } else {
        std::cerr << "Failed to open gamepad: " << SDL_GetError() << std::endl;
    }
}

void gamepad_manager::remove_gamepad(SDL_JoystickID joystickid) {
    int i = get_gamepad_idx(joystickid);
    if (m_gamepads[i]) {
        SDL_CloseGamepad(m_gamepads[i]);
        m_gamepads[i] = nullptr;
    }
}

SDL_Gamepad* gamepad_manager::active_gamepad() {
    for (auto gamepad : m_gamepads) {
        if (gamepad) {
            return gamepad;
        }
    }
    return nullptr;
}

void gamepad_manager::setup_file() {
    m_file << "time,";
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
        m_file << button_to_string((SDL_GamepadButton)i) << ", ";
    }
    for (int i = 0; i < SDL_GAMEPAD_AXIS_MAX; ++i) {
        m_file << axis_to_string((SDL_GamepadAxis)i) << ", ";
    }
    m_file << std::endl;
}

void gamepad_manager::save_gamepad_state() {
    SDL_Gamepad* gamepad = active_gamepad();
    // Print number of gamepads
    if (gamepad) {
        auto dt = REC_TIMER.elapsed();
        if (!dt) return;
        std::cout << *dt << std::endl;

        int i;
        m_file << *dt << ",";
        for (i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
            const SDL_GamepadButton button = (SDL_GamepadButton)i;
            const bool pressed = SDL_GetGamepadButton(gamepad, button) == SDL_PRESSED;
            m_file << pressed << ",";
        }

        for (i = 0; i < SDL_GAMEPAD_AXIS_MAX; ++i) {
            const SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
            int16_t value = SDL_GetGamepadAxis(gamepad, axis);
            value = (value < AXIS_DEADZONE && value > -AXIS_DEADZONE) ? 0 : value;
            m_file << value << ",";
        }

        m_file << std::endl;
    }
}

gamepad_manager::~gamepad_manager() {
    for (auto gamepad : m_gamepads) {
        SDL_CloseGamepad(gamepad);
    }
    m_file.close();
    SDL_Quit();
}

void gamepad_manager::loop() {
    SDL_Event event;
    SDL_PumpEvents();
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST) == 1) {
        switch (event.type)
        {
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

int gamepad_manager::get_gamepad_idx(SDL_JoystickID joystickid) {
    for (size_t i = 0; i < m_gamepads.size(); ++i) {
        if (m_gamepads[i]) {
            SDL_Joystick *joystick = SDL_GetGamepadJoystick(m_gamepads[i]);
            if (joystick && SDL_GetJoystickInstanceID(joystick) == joystickid) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

class input_rec {
private:
    obs_data_t *m_settings;
    obs_source_t *m_source;
    gamepad_manager m_gamepad_manager;
    std::thread m_thread;
    std::atomic<bool> m_running{true};
public:
    input_rec(obs_data_t *settings, obs_source_t *source):
        m_settings {settings},
        m_source {source},
        m_gamepad_manager {}
    {
        m_thread = std::thread([this](){
            while (m_running) {
                m_gamepad_manager.loop();
            }
        });
    }

    ~input_rec() {
        std::cout << "Destroying input_rec" << std::endl;
        m_running = false;
        if (m_thread.joinable()) m_thread.join();
    }

    void tick(float seconds) {
        UNUSED_PARAMETER(seconds);
    }
};


bool initialize_rec_source() {
    obs_source_info source_info = {};
    source_info.id = "input_recording_source";
    source_info.type = OBS_SOURCE_TYPE_INPUT;
    source_info.output_flags = OBS_SOURCE_VIDEO;
    source_info.icon_type = OBS_ICON_TYPE_GAME_CAPTURE;
    source_info.get_name = [](void *) { return obs_module_text("InputRecording");; };
    source_info.get_width = [](void *) -> uint32_t { return 0; };
    source_info.get_height = [](void *) -> uint32_t{ return 0; };
    source_info.create = [](obs_data_t *settings, obs_source_t *source) -> void* { return static_cast<void*>(new input_rec(settings, source)); };
    source_info.destroy = [](void *data) {delete static_cast<input_rec *>(data);};
    source_info.video_tick = [](void *data, float seconds) { static_cast<input_rec *>(data)->tick(seconds); };
    obs_register_source(&source_info);
    return true;
}