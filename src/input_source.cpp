#include <input_source.hpp>
#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>

class input_rec {
private:
    obs_data_t *m_settings;
    obs_source_t *m_source;
    long long m_counter {0};
public:
    input_rec(obs_data_t *settings, obs_source_t *source):
        m_settings {settings},
        m_source {source}
    {}

    void tick(float seconds) {
        UNUSED_PARAMETER(seconds);
        if (obs_frontend_recording_active()) {
            m_counter++;
        }
    }
};


bool initialize_rec_source() {
    obs_source_info source_info = {};
    source_info.id = "input_recording_source";
    source_info.type = OBS_SOURCE_TYPE_INPUT;
    source_info.output_flags = OBS_SOURCE_VIDEO;
    source_info.icon_type = OBS_ICON_TYPE_GAME_CAPTURE;
    source_info.get_name = [](void *) { return obs_module_text("InputRecording");; };
    source_info.get_width = [](void *) -> uint32_t { return 0; };
    source_info.get_height = [](void *) -> uint32_t{ return 0; };
    source_info.create = [](obs_data_t *settings, obs_source_t *source) -> void* { return static_cast<void*>(new input_rec(settings, source)); };
    source_info.destroy = [](void *data) {delete static_cast<input_rec *>(data);};
    source_info.video_tick = [](void *data, float seconds) { static_cast<input_rec *>(data)->tick(seconds); };
    obs_register_source(&source_info);
    return true;
}