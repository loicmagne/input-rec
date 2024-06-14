/*
input-rec
Copyright (C) 2023 lm loic.magne@outlook.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>
#include <input_source.hpp>
#include <obs-frontend-api.h>
#include <atomic>
#include <iostream>
#include <SDL3/SDL.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

long double t {0.};
bool first_frame {true};
std::atomic<bool> is_recording {false};

bool obs_module_load(void) {
	if (!initialize_rec_source()) {
		obs_log(LOG_ERROR, "input-rec failed to load");
		return false;
	}

	obs_frontend_add_event_callback([](enum obs_frontend_event event, void *private_data) {
		UNUSED_PARAMETER(private_data);
		switch (event) {
		case OBS_FRONTEND_EVENT_RECORDING_STARTING:
			obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STARTING received");
			is_recording = true;
			break;
		case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
			obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STOPPING received");
			is_recording = false;
			first_frame = true;
			break;
		default:
			break;
		}
	}, nullptr);

	obs_add_tick_callback([](void *param, float seconds) {
		UNUSED_PARAMETER(param);
        if (is_recording) {
			if (first_frame) {
				t = 0.;
				first_frame = false;
			} else {
				t += seconds;
			}
			obs_log(LOG_INFO, "time: %Lf", t);
		}
	}, nullptr);
	obs_log(LOG_INFO, "input-rec (version %s) loaded successfully", PLUGIN_VERSION);

	obs_log(LOG_INFO, "SDL_Init succeeded!");

    SDL_Quit();
	return true;
}

void obs_module_unload(void) {
	obs_log(LOG_INFO, "input-rec unloaded");
}
