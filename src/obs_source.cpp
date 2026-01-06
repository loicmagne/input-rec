#include <obs-module.h>
#include <obs-frontend-api.h>
#include <cstring>

#include "device/input_device.hpp"
#include "device/gamepad.hpp"
#ifdef _WIN32
#include "device/windows_input.hpp"
#elif defined(__linux__)
#include "device/linux_evdev.hpp"
#endif
#include "writer/input_writer.hpp"
#include "writer/csv.hpp"
#include "writer/parquet.hpp"
#include "writer/debug.hpp"
#include "obs_source.hpp"
#include "plugin-support.h"

// Settings keys
#define SETTING_INPUT_DEVICE "input_device"
#define SETTING_OUTPUT_FORMAT "output_format"

// Settings values
#define DEVICE_GAMEPAD "gamepad"
#define DEVICE_MOUSE_KEYBOARD "mouse_keyboard"
#define FORMAT_PARQUET "parquet"
#define FORMAT_CSV "csv"
#define FORMAT_DEBUG "debug"

class RecSource {
private:
	obs_data_t *m_settings;
	obs_source_t *m_source;
	std::unique_ptr<InputWriter> m_input_writer;
	obs_frontend_event_cb m_event_callback;

	static std::unique_ptr<InputDevice> create_device(const char *type)
	{
#ifdef _WIN32
		if (strcmp(type, DEVICE_MOUSE_KEYBOARD) == 0) {
			return std::make_unique<WindowsInputDevice>();
		}
#elif defined(__linux__)
		if (strcmp(type, DEVICE_MOUSE_KEYBOARD) == 0) {
			return std::make_unique<LinuxEvdevDevice>();
		}
#endif
		// Default to gamepad
		return std::make_unique<GamepadDevice>();
	}

	static std::unique_ptr<InputWriter> create_writer(const char *format, std::unique_ptr<InputDevice> device)
	{
		if (strcmp(format, FORMAT_CSV) == 0) {
			return std::make_unique<CSVWriter>(std::move(device));
		}
		if (strcmp(format, FORMAT_DEBUG) == 0) {
			return std::make_unique<DebugWriter>(std::move(device));
		}
		// Default to parquet
		return std::make_unique<ParquetWriter>(std::move(device));
	}

public:
	RecSource(obs_data_t *settings, obs_source_t *source) : m_settings{settings}, m_source{source}
	{
		m_event_callback = [](enum obs_frontend_event event, void *private_data) {
			RecSource *rec_source = static_cast<RecSource *>(private_data);
			switch (event) {
			case OBS_FRONTEND_EVENT_RECORDING_STARTING: {
				obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STARTING received");
				rec_source->prepare_recording();
				break;
			}
			case OBS_FRONTEND_EVENT_RECORDING_STARTED: {
				obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STARTED received");
				rec_source->m_input_writer->start_recording();
				break;
			}
			case OBS_FRONTEND_EVENT_RECORDING_STOPPING: {
				obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STOPPING received");
				rec_source->m_input_writer->stop_recording();
				break;
			}
			case OBS_FRONTEND_EVENT_RECORDING_STOPPED: {
				obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STOPPED received");
				std::string recording_path{obs_frontend_get_last_recording()};
				rec_source->m_input_writer->close_recording(recording_path);
				rec_source->m_input_writer.reset();
				break;
			}
			default:
				break;
			}
		};
		obs_frontend_add_event_callback(m_event_callback, this);
	}

	~RecSource() { obs_frontend_remove_event_callback(m_event_callback, this); }

	void prepare_recording()
	{
		const char *device_type = obs_data_get_string(m_settings, SETTING_INPUT_DEVICE);
		const char *output_format = obs_data_get_string(m_settings, SETTING_OUTPUT_FORMAT);

		obs_log(LOG_INFO, "Creating writer with device=%s, format=%s", device_type, output_format);

		auto device = create_device(device_type);
		m_input_writer = create_writer(output_format, std::move(device));
		m_input_writer->prepare_recording();
	}

	void tick(float seconds) { UNUSED_PARAMETER(seconds); }

	bool is_recording() const { return m_input_writer != nullptr; }
};

static void rec_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, SETTING_INPUT_DEVICE, DEVICE_GAMEPAD);
	obs_data_set_default_string(settings, SETTING_OUTPUT_FORMAT, FORMAT_PARQUET);
}

static obs_properties_t *rec_source_properties(void *data)
{
	RecSource *source = static_cast<RecSource *>(data);
	obs_properties_t *props = obs_properties_create();

	// Check if recording is active to disable options
	bool recording = source && source->is_recording();

	// Input device dropdown
	obs_property_t *device_list = obs_properties_add_list(props, SETTING_INPUT_DEVICE,
							      obs_module_text("InputDevice"), OBS_COMBO_TYPE_LIST,
							      OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(device_list, obs_module_text("Gamepad"), DEVICE_GAMEPAD);
#if defined(_WIN32) || defined(__linux__)
	obs_property_list_add_string(device_list, obs_module_text("MouseKeyboard"), DEVICE_MOUSE_KEYBOARD);
#endif
	obs_property_set_enabled(device_list, !recording);

	// Output format dropdown
	obs_property_t *format_list = obs_properties_add_list(props, SETTING_OUTPUT_FORMAT,
							      obs_module_text("OutputFormat"), OBS_COMBO_TYPE_LIST,
							      OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(format_list, "Parquet", FORMAT_PARQUET);
	obs_property_list_add_string(format_list, "CSV", FORMAT_CSV);
	obs_property_list_add_string(format_list, obs_module_text("DebugLog"), FORMAT_DEBUG);
	obs_property_set_enabled(format_list, !recording);

	return props;
}

bool initialize_rec_source()
{
	obs_source_info source_info = {};
	source_info.id = "input_recording_source";
	source_info.type = OBS_SOURCE_TYPE_INPUT;
	source_info.output_flags = OBS_SOURCE_VIDEO;
	source_info.icon_type = OBS_ICON_TYPE_GAME_CAPTURE;
	source_info.get_name = [](void *) { return obs_module_text("InputRecording"); };
	source_info.get_width = [](void *) -> uint32_t { return 0; };
	source_info.get_height = [](void *) -> uint32_t { return 0; };
	source_info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		return static_cast<void *>(new RecSource(settings, source));
	};
	source_info.destroy = [](void *data) { delete static_cast<RecSource *>(data); };
	source_info.video_tick = [](void *data, float seconds) { static_cast<RecSource *>(data)->tick(seconds); };
	source_info.get_defaults = rec_source_defaults;
	source_info.get_properties = rec_source_properties;
	obs_register_source(&source_info);
	return true;
}
