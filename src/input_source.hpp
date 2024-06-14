#include <vector>
#include <SDL3/SDL.h>


bool initialize_rec_source();

class gamepad_manager
{
private:
    std::vector<SDL_Gamepad*> m_gamepads;
public:
    gamepad_manager(/* args */);
    ~gamepad_manager();
    void init_gamepads();
    void add_gamepad(SDL_JoystickID joystickid);
    void loop();
    int get_gamepad_idx(SDL_JoystickID joystickid);
    void remove_gamepad(SDL_JoystickID joystickid);
    SDL_Gamepad* active_gamepad();
    void save_gamepad_state();
};
