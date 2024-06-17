#pragma once
#include <SDL3/SDL.h>
#include <chrono>
#include <iostream>
#include <atomic>
#include <optional>

std::string axis_to_string(SDL_GamepadAxis axis);
std::string button_to_string(SDL_GamepadButton button);

class rec_timer
{
private:
    std::chrono::high_resolution_clock::time_point m_start;
    std::atomic<bool> m_running{false};
public:
    rec_timer();
    void start();
    void stop();
    std::optional<int64_t> elapsed();
};
