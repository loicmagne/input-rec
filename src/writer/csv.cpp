#include "csv.hpp"
#include <random>
#include <iostream>

namespace fs = std::filesystem;

CSVWriter::CSVWriter(std::unique_ptr<InputDevice> device) : InputWriter{std::move(device)} {}

void CSVWriter::prepare_recording()
{
	// Create tmp file with random name
	m_file_path = fs::temp_directory_path() /
		      fs::path("obs_input_rec_" + std::to_string(std::random_device{}()) + ".csv");
	m_file.open(m_file_path, std::ios::trunc);

	if (!m_file.is_open()) {
		std::cerr << "Failed to open file: " << m_file_path << std::endl;
		return;
	}

	// Write header
	m_device->write_header(*this);
}

void CSVWriter::close_recording(std::string recording_path)
{
	// Close file and move it to the recording path with a .csv extension
	m_file.close();
	fs::path recording_csv = fs::path(recording_path).replace_extension(".csv");
	fs::rename(m_file_path, recording_csv);
}

void CSVWriter::begin_header() { append_header(static_cast<int64_t>(0), "time"); }
void CSVWriter::end_header() { m_file << std::endl; }

void CSVWriter::begin_row()
{
	auto dt = m_timer.elapsed();
	if (dt) m_file << *dt << ",";
}
void CSVWriter::end_row() { m_file << std::endl; }

void CSVWriter::append_header(const bool &value, const std::string &name) { m_file << name << ","; }
void CSVWriter::append_header(const int16_t &value, const std::string &name) { m_file << name << ","; }
void CSVWriter::append_header(const int64_t &value, const std::string &name) { m_file << name << ","; }

void CSVWriter::append_row(const bool &value) { m_file << value << ","; }
void CSVWriter::append_row(const int16_t &value) { m_file << value << ","; }
void CSVWriter::append_row(const int64_t &value) { m_file << value << ","; }
