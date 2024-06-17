#pragma once
#include <vector>
#include <SDL3/SDL.h>
#include <fstream>

class gamepad_manager
{
private:
    std::vector<SDL_Gamepad*> m_gamepads;
    std::ofstream m_file;
public:
    gamepad_manager();
    ~gamepad_manager();
    void init_gamepads();
    void add_gamepad(SDL_JoystickID joystickid);
    void remove_gamepad(SDL_JoystickID joystickid);
    int get_gamepad_idx(SDL_JoystickID joystickid);
    void loop();
    SDL_Gamepad* active_gamepad();
    void setup_file();
    void save_gamepad_state();
};

bool initialize_rec_source();
