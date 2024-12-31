#pragma once
#include <fstream>
#include <filesystem>
#include <string>
#include <duckdb.h>
#include "input_writer.hpp"

class ParquetWriter : public InputWriter {
private:
	duckdb_database m_db;
	duckdb_connection m_con;
	std::string m_create_statement;

public:
	ParquetWriter(std::unique_ptr<InputDevice> device);

	void prepare_recording() override;
	void close_recording(std::string recording_path) override;

	void begin_header() override;
	void end_header() override;
	void begin_row() override;
	void end_row() override;

	void append_header(const bool &value, const std::string &name) override;
	void append_header(const int16_t &value, const std::string &name) override;
	void append_header(const int64_t &value, const std::string &name) override;

	void append_row(const bool &value) override;
	void append_row(const int16_t &value) override;
	void append_row(const int64_t &value) override;
};