#include <input_source.hpp>
#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <vector>
#include <thread>
#include <iostream>
#include <SDL3/SDL.h>
#include <chrono>
#include <atomic>
typedef std::chrono::high_resolution_clock Clock;

constexpr int16_t AXIS_DEADZONE = 0;

std::string axis_to_string(SDL_GamepadAxis axis) {
    switch (axis)
    {
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

std::string button_to_string(SDL_GamepadButton button) {
    switch (button)
    {
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

gamepad_manager::gamepad_manager(/* args */) {
	/* Init SDL, see SDL/test/testcontroller.c */
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

void gamepad_manager::save_gamepad_state() {
    SDL_Gamepad* gamepad = active_gamepad();
    // Print number of gamepads
    if (gamepad) {
        auto now = Clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto epoch = now_ms.time_since_epoch();
        auto t = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);
        std::cout << SDL_GAMEPAD_BUTTON_MAX << " " << SDL_GAMEPAD_BUTTON_TOUCHPAD << std::endl;
        std::cout << t.count() << " ";

        int i;
        for (i = 0; i < SDL_GAMEPAD_BUTTON_TOUCHPAD; ++i) {
            const SDL_GamepadButton button = (SDL_GamepadButton)i;
            const bool pressed = SDL_GetGamepadButton(gamepad, button) == SDL_PRESSED;
            std::cout << "(" << button_to_string(button) << "," << pressed << ") ";

        }

        for (i = 0; i < SDL_GAMEPAD_AXIS_MAX; ++i) {
            const SDL_GamepadAxis axis = (SDL_GamepadAxis)i;
            int16_t value = SDL_GetGamepadAxis(gamepad, axis);
            value = (value < AXIS_DEADZONE && value > -AXIS_DEADZONE) ? 0 : value;
            std::cout << "(" << axis_to_string(axis) << "," << value << ") ";
        }

        std::cout << std::endl;
    }

}

gamepad_manager::~gamepad_manager() {
    for (auto gamepad : m_gamepads) {
        SDL_CloseGamepad(gamepad);
    }
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
        }
    }
    save_gamepad_state();
    SDL_Delay(5);
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