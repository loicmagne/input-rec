#include "parquet.hpp"
#include "plugin-support.h"
#include <obs-module.h>
#include <random>
#include <iostream>

namespace fs = std::filesystem;

// Helper to run a DuckDB query with error logging
static bool run_query(duckdb_connection con, const std::string &sql)
{
	duckdb_result result;
	if (duckdb_query(con, sql.c_str(), &result) == DuckDBError) {
		const char *error = duckdb_result_error(&result);
		obs_log(LOG_ERROR, "[ParquetWriter] DuckDB error: %s", error ? error : "unknown");
		obs_log(LOG_ERROR, "[ParquetWriter] Failed SQL: %.200s...", sql.c_str());
		duckdb_destroy_result(&result);
		return false;
	}
	duckdb_destroy_result(&result);
	return true;
}

ParquetWriter::ParquetWriter(std::unique_ptr<InputDevice> device) : InputWriter{std::move(device)} {}

void ParquetWriter::prepare_recording()
{
	if (duckdb_open(NULL, &m_db) == DuckDBError) {
		std::cerr << "Failed to open database" << std::endl;
		return;
	}
	if (duckdb_connect(m_db, &m_con) == DuckDBError) {
		std::cerr << "Failed to connect to database" << std::endl;
		return;
	}

	// Create table
	m_device->write_header(*this);
}

void ParquetWriter::close_recording(std::string recording_path)
{
	// export to parquet file
	fs::path parquet_path = fs::path(recording_path).replace_extension(".parquet");

	// Convert path to use forward slashes (DuckDB/SQL compatible)
	std::string path_str = parquet_path.string();
	for (char &c : path_str) {
		if (c == '\\')
			c = '/';
	}

	std::string copy_sql = "COPY inputs TO '" + path_str + "' (FORMAT 'parquet')";
	if (!run_query(m_con, copy_sql)) {
		obs_log(LOG_ERROR, "[ParquetWriter] Failed to export parquet file");
	} else {
		obs_log(LOG_INFO, "[ParquetWriter] Exported to %s", path_str.c_str());
	}

	// close connection and database
	duckdb_disconnect(&m_con);
	duckdb_close(&m_db);
}

void ParquetWriter::begin_header()
{
	m_create_statement = "CREATE TABLE inputs (";
	append_header(static_cast<int64_t>(0), "time");
}
void ParquetWriter::end_header()
{
	char u = m_create_statement.back();
	if (u == ',')
		m_create_statement.pop_back();
	m_create_statement += ");";
	if (!run_query(m_con, m_create_statement)) {
		obs_log(LOG_ERROR, "[ParquetWriter] Failed to create table");
	}
}

void ParquetWriter::begin_row()
{
	m_create_statement = "INSERT INTO inputs VALUES (";
	auto dt = m_timer.elapsed();
	if (dt) m_create_statement += std::to_string(*dt) + ",";
}
void ParquetWriter::end_row()
{
	char u = m_create_statement.back();
	if (u == ',')
		m_create_statement.pop_back();
	m_create_statement += ");";
	run_query(m_con, m_create_statement);
}

void ParquetWriter::append_header(const bool &value, const std::string &name)
{
	// Quote column names to handle digits (0-9) and reserved words
	m_create_statement += "\"" + name + "\" BOOLEAN,";
}
void ParquetWriter::append_header(const int16_t &value, const std::string &name)
{
	m_create_statement += "\"" + name + "\" SMALLINT,";
}
void ParquetWriter::append_header(const int64_t &value, const std::string &name)
{
	m_create_statement += "\"" + name + "\" BIGINT,";
}
void ParquetWriter::append_header(const std::string &value, const std::string &name)
{
	m_create_statement += "\"" + name + "\" VARCHAR,";
}

void ParquetWriter::append_row(const bool &value) { m_create_statement += value ? "TRUE," : "FALSE,"; }
void ParquetWriter::append_row(const int16_t &value) { m_create_statement += std::to_string(value) + ","; }
void ParquetWriter::append_row(const int64_t &value) { m_create_statement += std::to_string(value) + ","; }
void ParquetWriter::append_row(const std::string &value)
{
	// Escape single quotes for SQL
	std::string escaped;
	escaped.reserve(value.size() + 2);
	for (char c : value) {
		if (c == '\'')
			escaped += "''";
		else
			escaped += c;
	}
	m_create_statement += "'" + escaped + "',";
}
