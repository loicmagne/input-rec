#pragma once
#include "input_writer.hpp"
#include <vector>
#include <string>

class DebugWriter : public InputWriter {
private:
	std::vector<std::string> m_button_names;
	std::vector<bool> m_prev_button_states;
	std::vector<std::string> m_axis_names;
	std::vector<int16_t> m_prev_axis_values;
	size_t m_current_button_idx;
	size_t m_current_axis_idx;

public:
	DebugWriter(std::unique_ptr<InputDevice> device);

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
