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

#include <duckdb.h>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include "plugin-support.h"
#include "obs_source.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	// Dummy duckdb code, open a database and close it
	duckdb_database db;
	duckdb_connection con;
	if (duckdb_open(NULL, &db) == DuckDBError) obs_log(LOG_ERROR, "Failed to open database");
	if (duckdb_connect(db, &con) == DuckDBError) obs_log(LOG_ERROR, "Failed to connect to database");
	duckdb_query(con, "SELECT 42", NULL);
	duckdb_disconnect(&con);
	duckdb_close(&db);

	if (!initialize_rec_source()) {
		obs_log(LOG_ERROR, "input-rec failed to load");
		return false;
	}

	obs_log(LOG_INFO, "input-rec (version %s) loaded successfully", PLUGIN_VERSION);
	obs_log(LOG_INFO, "input-rec version UwU");
	return true;
}

void obs_module_unload(void) { obs_log(LOG_INFO, "input-rec unloaded"); }