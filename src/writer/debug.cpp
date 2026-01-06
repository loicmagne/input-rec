#include "debug.hpp"
#include "plugin-support.h"
#include <obs-module.h>

DebugWriter::DebugWriter(std::unique_ptr<InputDevice> device)
	: InputWriter{std::move(device)},
	  m_current_button_idx{0},
	  m_current_axis_idx{0},
	  m_current_string_idx{0}
{
}

void DebugWriter::prepare_recording()
{
	m_button_names.clear();
	m_prev_button_states.clear();
	m_axis_names.clear();
	m_prev_axis_values.clear();
	m_string_names.clear();
	m_prev_string_values.clear();
	m_current_button_idx = 0;
	m_current_axis_idx = 0;
	m_current_string_idx = 0;

	obs_log(LOG_INFO, "[DebugWriter] Recording started - logging input changes");
	m_device->write_header(*this);
}

void DebugWriter::close_recording(std::string recording_path)
{
	obs_log(LOG_INFO, "[DebugWriter] Recording stopped");
}

void DebugWriter::begin_header() {}

void DebugWriter::end_header()
{
	// Initialize previous states
	m_prev_button_states.resize(m_button_names.size(), false);
	m_prev_axis_values.resize(m_axis_names.size(), 0);
	m_prev_string_values.resize(m_string_names.size(), "");

	obs_log(LOG_INFO, "[DebugWriter] Tracking %zu buttons, %zu axes, %zu strings", m_button_names.size(),
		m_axis_names.size(), m_string_names.size());
}

void DebugWriter::begin_row()
{
	m_current_button_idx = 0;
	m_current_axis_idx = 0;
	m_current_string_idx = 0;
}

void DebugWriter::end_row() {}

void DebugWriter::append_header(const bool &value, const std::string &name)
{
	m_button_names.push_back(name);
}

void DebugWriter::append_header(const int16_t &value, const std::string &name)
{
	m_axis_names.push_back(name);
}

void DebugWriter::append_header(const int64_t &value, const std::string &name)
{
	// Time column - ignore
}

void DebugWriter::append_row(const bool &value)
{
	if (m_current_button_idx < m_prev_button_states.size()) {
		bool prev = m_prev_button_states[m_current_button_idx];
		if (value != prev) {
			const char *action = value ? "PRESSED" : "RELEASED";
			obs_log(LOG_INFO, "[DebugWriter] %s %s", m_button_names[m_current_button_idx].c_str(), action);
			m_prev_button_states[m_current_button_idx] = value;
		}
	}
	m_current_button_idx++;
}

void DebugWriter::append_row(const int16_t &value)
{
	if (m_current_axis_idx < m_prev_axis_values.size()) {
		int16_t prev = m_prev_axis_values[m_current_axis_idx];
		const std::string &name = m_axis_names[m_current_axis_idx];

		// Determine threshold based on field type
		bool should_log = false;

		if (name == "wheel" || name == "hwheel") {
			// Log any wheel movement (non-zero)
			should_log = (value != 0);
		} else if (name == "mouse_delta_x" || name == "mouse_delta_y") {
			// Log mouse movement when delta > 20 (to avoid spam)
			should_log = (value > 20 || value < -20);
		} else if (name == "mouse_x" || name == "mouse_y") {
			// Log significant position changes (> 100 pixels)
			int16_t diff = value - prev;
			should_log = (diff > 100 || diff < -100);
		} else {
			// Gamepad axes: log significant changes (> 5000)
			int16_t diff = value - prev;
			should_log = (diff > 5000 || diff < -5000);
		}

		if (should_log) {
			obs_log(LOG_INFO, "[DebugWriter] %s = %d", name.c_str(), value);
			m_prev_axis_values[m_current_axis_idx] = value;
		}
	}
	m_current_axis_idx++;
}

void DebugWriter::append_row(const int64_t &value)
{
	// Time value - ignore
}

void DebugWriter::append_header(const std::string &value, const std::string &name)
{
	m_string_names.push_back(name);
}

void DebugWriter::append_row(const std::string &value)
{
	if (m_current_string_idx < m_prev_string_values.size()) {
		const std::string &prev = m_prev_string_values[m_current_string_idx];
		if (value != prev) {
			obs_log(LOG_INFO, "[DebugWriter] %s = \"%s\"", m_string_names[m_current_string_idx].c_str(),
				value.c_str());
			m_prev_string_values[m_current_string_idx] = value;
		}
	}
	m_current_string_idx++;
}
