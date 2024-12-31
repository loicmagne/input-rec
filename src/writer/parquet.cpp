#include "parquet.hpp"
#include <random>
#include <iostream>

namespace fs = std::filesystem;

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
	duckdb_query(m_con, ("COPY inputs TO '" + parquet_path.string() + "' (FORMAT 'parquet')").c_str(), NULL);

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
	if (u == ',') m_create_statement.pop_back();
	m_create_statement += ");";
	duckdb_query(m_con, m_create_statement.c_str(), NULL);
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
	if (u == ',') m_create_statement.pop_back();
	m_create_statement += ");";
	duckdb_query(m_con, m_create_statement.c_str(), NULL);
}

void ParquetWriter::append_header(const bool &value, const std::string &name)
{
	m_create_statement += name + " BOOLEAN,";
}
void ParquetWriter::append_header(const int16_t &value, const std::string &name)
{
	m_create_statement += name + " SMALLINT,";
}
void ParquetWriter::append_header(const int64_t &value, const std::string &name)
{
	m_create_statement += name + " BIGINT,";
}

void ParquetWriter::append_row(const bool &value) { m_create_statement += value ? "TRUE," : "FALSE,"; }
void ParquetWriter::append_row(const int16_t &value) { m_create_statement += std::to_string(value) + ","; }
void ParquetWriter::append_row(const int64_t &value) { m_create_statement += std::to_string(value) + ","; }
