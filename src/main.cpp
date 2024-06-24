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

#include <atomic>
#include <iostream>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <SDL3/SDL.h>

#include "plugin-support.h"
#include "input_source.hpp"
#include "utils.hpp"
#include "globals.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

std::unique_ptr<rec_timer> REC_TIMER;

bool obs_module_load(void)
{
	REC_TIMER = std::make_unique<rec_timer>();

	if (!initialize_rec_source()) {
		obs_log(LOG_ERROR, "input-rec failed to load");
		return false;
	}

	obs_frontend_add_event_callback(
		[](enum obs_frontend_event event, void *private_data) {
			UNUSED_PARAMETER(private_data);
			switch (event) {
			case OBS_FRONTEND_EVENT_RECORDING_STARTED:
				obs_log(LOG_INFO,
					"OBS_FRONTEND_EVENT_RECORDING_STARTED received");
				REC_TIMER->start();
				break;
			case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
				obs_log(LOG_INFO,
					"OBS_FRONTEND_EVENT_RECORDING_STOPPING received");
				REC_TIMER->stop();
				break;
			default:
				break;
			}
		},
		nullptr);

	obs_log(LOG_INFO, "input-rec (version %s) loaded successfully",
		PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	REC_TIMER.reset();
	obs_log(LOG_INFO, "input-rec unloaded");
}
