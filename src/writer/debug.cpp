#include "debug.hpp"
#include "plugin-support.h"
#include <obs-module.h>

DebugWriter::DebugWriter(std::unique_ptr<InputDevice> device)
	: InputWriter{std::move(device)},
	  m_current_button_idx{0},
	  m_current_axis_idx{0}
{
}

void DebugWriter::prepare_recording()
{
	m_button_names.clear();
	m_prev_button_states.clear();
	m_axis_names.clear();
	m_prev_axis_values.clear();
	m_current_button_idx = 0;
	m_current_axis_idx = 0;

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

	obs_log(LOG_INFO, "[DebugWriter] Tracking %zu buttons, %zu axes", m_button_names.size(), m_axis_names.size());
}

void DebugWriter::begin_row()
{
	m_current_button_idx = 0;
	m_current_axis_idx = 0;
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
	// Optionally log significant axis changes
	if (m_current_axis_idx < m_prev_axis_values.size()) {
		int16_t prev = m_prev_axis_values[m_current_axis_idx];
		int16_t diff = value - prev;
		if (diff > 5000 || diff < -5000) {
			obs_log(LOG_INFO, "[DebugWriter] %s = %d",
				m_axis_names[m_current_axis_idx].c_str(), value);
			m_prev_axis_values[m_current_axis_idx] = value;
		}
	}
	m_current_axis_idx++;
}

void DebugWriter::append_row(const int64_t &value)
{
	// Time value - ignore
}
