#pragma once
#include <variant>
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>
#include <memory>
#include <string>
#include <iostream>
#include <iomanip>

#include "device/input_device.hpp"

class RecTimer {
private:
	std::chrono::high_resolution_clock::time_point m_start{};

public:
	std::atomic<bool> running{false};
	void start()
	{
		m_start = std::chrono::high_resolution_clock::now();
		running = true;
	}

	void stop()
	{
		// print elapsed time in min:sec:ms
		auto dt = elapsed();
		if (dt) {
			// Convert microseconds to appropriate units
			auto ms = (*dt / 1000) % 1000; // Convert microseconds to milliseconds
			auto s = (*dt / 1000000) % 60; // Convert microseconds to seconds
			auto m = (*dt / 60000000);     // Convert microseconds to minutes

			// Format with leading zeros and proper width
			std::cout << "[input-rec] Elapsed time: " << std::setfill('0') << std::setw(2) << m << ":"
				  << std::setfill('0') << std::setw(2) << s << ":" << std::setfill('0') << std::setw(3)
				  << ms << std::endl;
		}

		running = false;
	}

	std::optional<int64_t> elapsed()
	{
		if (!running) return std::nullopt;
		auto now = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - m_start);
		auto dt = duration.count();
		return dt;
	}
};

using SupportedTypes = std::variant<bool, int16_t, int64_t>;
class InputWriter {
protected:
	RecTimer m_timer;
	std::thread m_thread_loop;
	/* dependency injection of an InputDevice
	each InputWriter have a reference to an InputDevice
	which they use to get the state of an input device */
	std::unique_ptr<InputDevice> m_device;

public:
	InputWriter(std::unique_ptr<InputDevice> device) : m_device{std::move(device)}, m_timer{} {}

	virtual ~InputWriter() = default;

	// Start/end a recording session
	virtual void prepare_recording() = 0;
	virtual void close_recording(std::string recording_path) = 0;

	// Start/Stop recording implementation are the same for all InputWriter
	void start_recording()
	{
		m_timer.start();

		m_thread_loop = std::thread([this]() {
			while (m_timer.running) m_device->loop(*this);
		});
	}
	void stop_recording()
	{
		m_timer.stop();

		if (m_thread_loop.joinable()) m_thread_loop.join();
	}

	// Start/end a header or row
	virtual void begin_header() = 0;
	virtual void end_header() = 0;
	virtual void begin_row() = 0;
	virtual void end_row() = 0;

	// Overloaded functions to append data
	virtual void append_header(const bool &value, const std::string &name) = 0;
	virtual void append_header(const int16_t &value, const std::string &name) = 0;
	virtual void append_header(const int64_t &value, const std::string &name) = 0;

	virtual void append_row(const bool &value) = 0;
	virtual void append_row(const int16_t &value) = 0;
	virtual void append_row(const int64_t &value) = 0;

	/* The following functions are just there to check that the overloads of
	append_header and append_row cover all the supported types, if it isn't the
	case, the program will not compile */

	void _append_header_check(const SupportedTypes &value, const std::string &name)
	{
		std::visit([this, &name](auto &&arg) { this->append_header(arg, name); }, value);
	}

	void _append_row_check(const SupportedTypes &value)
	{
		std::visit([this](auto &&arg) { this->append_row(arg); }, value);
	}
};
