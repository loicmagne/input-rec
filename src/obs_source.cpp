#include <obs-module.h>
#include <obs-frontend-api.h>

#include "device/input_device.hpp"
#include "device/gamepad.hpp"
#include "writer/input_writer.hpp"
#include "writer/csv.hpp"
#include "writer/parquet.hpp"
#include "obs_source.hpp"
#include "plugin-support.h"

class RecSource {
private:
	obs_data_t *m_settings;
	obs_source_t *m_source;
	std::unique_ptr<InputWriter> m_input_writer;

public:
	RecSource(obs_data_t *settings, obs_source_t *source)
		: m_settings{settings},
		  m_source{source},
		  m_input_writer{std::make_unique<ParquetWriter>(std::make_unique<GamepadDevice>())}
	{
		obs_frontend_add_event_callback(
			[](enum obs_frontend_event event, void *private_data) {
				InputWriter *current_writer = static_cast<InputWriter *>(private_data);
				switch (event) {
				case OBS_FRONTEND_EVENT_RECORDING_STARTING: {
					obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STARTING received");
					current_writer->prepare_recording();
					break;
				}
				case OBS_FRONTEND_EVENT_RECORDING_STARTED: {
					obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STARTED received");
					current_writer->start_recording();
					break;
				}
				case OBS_FRONTEND_EVENT_RECORDING_STOPPING: {
					obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STOPPING received");
					current_writer->stop_recording();
					break;
				}
				case OBS_FRONTEND_EVENT_RECORDING_STOPPED: {
					obs_log(LOG_INFO, "OBS_FRONTEND_EVENT_RECORDING_STOPPED received");
					// Get last recording path
					std::string recording_path{obs_frontend_get_last_recording()};
					current_writer->close_recording(recording_path);
					break;
				}
				default: break;
				}
			},
			m_input_writer.get());
	}

	void tick(float seconds) { UNUSED_PARAMETER(seconds); }
};

bool initialize_rec_source()
{
	obs_source_info source_info = {};
	source_info.id = "input_recording_source";
	source_info.type = OBS_SOURCE_TYPE_INPUT;
	source_info.output_flags = OBS_SOURCE_VIDEO;
	source_info.icon_type = OBS_ICON_TYPE_GAME_CAPTURE;
	source_info.get_name = [](void *) {
		return obs_module_text("InputRecording");
		;
	};
	source_info.get_width = [](void *) -> uint32_t { return 0; };
	source_info.get_height = [](void *) -> uint32_t { return 0; };
	source_info.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
		return static_cast<void *>(new RecSource(settings, source));
	};
	source_info.destroy = [](void *data) { delete static_cast<RecSource *>(data); };
	source_info.video_tick = [](void *data, float seconds) { static_cast<RecSource *>(data)->tick(seconds); };
	obs_register_source(&source_info);
	return true;
}
