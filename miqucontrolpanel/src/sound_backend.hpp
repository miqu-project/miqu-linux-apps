#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <memory>

namespace miqu {

struct SoundDevice {
    int id = 0;
    std::string name;
    float volume = 0.5f;
    bool is_default = false;
    bool is_muted = false;
};

struct SoundState {
    int output_volume = 50; // 0 - 100
    bool output_muted = false;
    int default_sink_id = 0;
    std::string default_sink_name = "Default Output";

    int input_volume = 50;  // 0 - 100
    bool input_muted = false;
    int default_source_id = 0;
    std::string default_source_name = "Default Input";

    std::vector<SoundDevice> output_devices;
    std::vector<SoundDevice> input_devices;
};

class SoundBackend {
public:
    static SoundState get_state();

    static void set_output_volume(int percent);
    static void set_output_mute(bool muted);
    static void toggle_output_mute();

    static void set_input_volume(int percent);
    static void set_input_mute(bool muted);
    static void toggle_input_mute();

    static void set_default_sink(int id);
    static void set_default_source(int id);

    static void sync_item_state(const std::shared_ptr<SettingItem>& item, const SoundState& state);
    static void sync_item_state(const std::shared_ptr<SettingItem>& item);
};

} // namespace miqu
