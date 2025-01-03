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

#include "plugin-support.h"
#include "obs_source.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	if (!initialize_rec_source()) {
		obs_log(LOG_ERROR, "input-rec failed to load");
		return false;
	}

	obs_log(LOG_INFO, "input-rec (version %s) loaded successfully", PLUGIN_VERSION);
	obs_log(LOG_INFO, "(￣▽￣)ノ");
	return true;
}

void obs_module_unload(void) { obs_log(LOG_INFO, "input-rec unloaded"); }