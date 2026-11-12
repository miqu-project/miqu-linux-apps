#include "sound_backend.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <regex>

namespace miqu {

static std::string run_cmd(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

static void run_cmd_async(const std::string& cmd) {
    std::string async_cmd = cmd + " >/dev/null 2>&1 &";
    std::system(async_cmd.c_str());
}

SoundState SoundBackend::get_state() {
    SoundState state;

    // 1. Output volume & mute
    std::string out_vol_str = run_cmd("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null");
    size_t pos = out_vol_str.find("Volume:");
    if (pos != std::string::npos) {
        try {
            float v = std::stof(out_vol_str.substr(pos + 7));
            state.output_volume = std::clamp(static_cast<int>(std::round(v * 100.0f)), 0, 100);
        } catch (...) {}
    }
    state.output_muted = (out_vol_str.find("[MUTED]") != std::string::npos);

    // 2. Input volume & mute
    std::string in_vol_str = run_cmd("wpctl get-volume @DEFAULT_AUDIO_SOURCE@ 2>/dev/null");
    pos = in_vol_str.find("Volume:");
    if (pos != std::string::npos) {
        try {
            float v = std::stof(in_vol_str.substr(pos + 7));
            state.input_volume = std::clamp(static_cast<int>(std::round(v * 100.0f)), 0, 100);
        } catch (...) {}
    }
    state.input_muted = (in_vol_str.find("[MUTED]") != std::string::npos);

    // 3. Audio Sinks and Sources from wpctl status
    std::string status = run_cmd("wpctl status 2>/dev/null");
    std::istringstream stream(status);
    std::string line;

    enum class Section { None, Sinks, Sources, Other };
    Section current_section = Section::None;

    std::regex dev_regex(R"(([*]?)\s*(\d+)\.\s+(.*?)(?:\s*\[vol:\s*([\d.]+)(\s*MUTED)?\]|\s*$))");

    while (std::getline(stream, line)) {
        if (line.find("Video") != std::string::npos || line.find("Settings") != std::string::npos) {
            break;
        }

        if (line.find("Sinks:") != std::string::npos) {
            current_section = Section::Sinks;
            continue;
        } else if (line.find("Sources:") != std::string::npos) {
            current_section = Section::Sources;
            continue;
        } else if (line.find("Filters:") != std::string::npos ||
                   line.find("Streams:") != std::string::npos) {
            current_section = Section::Other;
            continue;
        }

        if (current_section == Section::Sinks || current_section == Section::Sources) {
            std::smatch match;
            if (std::regex_search(line, match, dev_regex)) {
                SoundDevice dev;
                dev.is_default = (match[1].str() == "*");
                try {
                    dev.id = std::stoi(match[2].str());
                } catch (...) {
                    dev.id = 0;
                }
                dev.name = match[3].str();
                size_t endpos = dev.name.find_last_not_of(" \t");
                if (endpos != std::string::npos) {
                    dev.name = dev.name.substr(0, endpos + 1);
                }

                if (match[4].matched) {
                    try {
                        dev.volume = std::stof(match[4].str());
                    } catch (...) {
                        dev.volume = 0.5f;
                    }
                } else {
                    dev.volume = 0.5f;
                }
                dev.is_muted = match[5].matched;

                if (current_section == Section::Sinks) {
                    if (dev.is_default) {
                        state.default_sink_id = dev.id;
                        state.default_sink_name = dev.name;
                    }
                    state.output_devices.push_back(std::move(dev));
                } else {
                    if (dev.is_default) {
                        state.default_source_id = dev.id;
                        state.default_source_name = dev.name;
                    }
                    state.input_devices.push_back(std::move(dev));
                }
            }
        }
    }

    if (state.default_sink_name.empty() && !state.output_devices.empty()) {
        state.default_sink_name = state.output_devices.front().name;
        state.default_sink_id = state.output_devices.front().id;
    }
    if (state.default_source_name.empty() && !state.input_devices.empty()) {
        state.default_source_name = state.input_devices.front().name;
        state.default_source_id = state.input_devices.front().id;
    }

    return state;
}

void SoundBackend::set_output_volume(int percent) {
    int p = std::clamp(percent, 0, 100);
    float norm = p / 100.0f;
    std::string cmd = "wpctl set-volume -l 1.0 @DEFAULT_AUDIO_SINK@ " + std::to_string(norm);
    run_cmd_async(cmd);
}

void SoundBackend::set_output_mute(bool muted) {
    std::string cmd = "wpctl set-mute @DEFAULT_AUDIO_SINK@ " + std::string(muted ? "1" : "0");
    run_cmd_async(cmd);
}

void SoundBackend::toggle_output_mute() {
    std::string cmd = "wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle";
    run_cmd_async(cmd);
}

void SoundBackend::set_input_volume(int percent) {
    int p = std::clamp(percent, 0, 100);
    float norm = p / 100.0f;
    std::string cmd = "wpctl set-volume -l 1.0 @DEFAULT_AUDIO_SOURCE@ " + std::to_string(norm);
    run_cmd_async(cmd);
}

void SoundBackend::set_input_mute(bool muted) {
    std::string cmd = "wpctl set-mute @DEFAULT_AUDIO_SOURCE@ " + std::string(muted ? "1" : "0");
    run_cmd_async(cmd);
}

void SoundBackend::toggle_input_mute() {
    std::string cmd = "wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle";
    run_cmd_async(cmd);
}

void SoundBackend::set_default_sink(int id) {
    if (id <= 0) return;
    std::string cmd = "wpctl set-default " + std::to_string(id);
    run_cmd_async(cmd);
}

void SoundBackend::set_default_source(int id) {
    if (id <= 0) return;
    std::string cmd = "wpctl set-default " + std::to_string(id);
    run_cmd_async(cmd);
}

void SoundBackend::sync_item_state(const std::shared_ptr<SettingItem>& item, const SoundState& state) {
    if (!item) return;

    bool is_speaker = (item->id == "sound_speaker");
    if (is_speaker) {
        item->slider_val = std::clamp(state.output_volume / 100.0f, 0.0f, 1.0f);
        item->switch_val = state.output_muted;
        item->telemetry = {
            {"Active Output", state.default_sink_name},
            {"Output Volume", std::to_string(state.output_volume) + "%" + (state.output_muted ? " (MUTED)" : "")},
            {"Audio Server", "PipeWire 1.0 (WirePlumber)"},
            {"Audio Node ID", std::to_string(state.default_sink_id)},
            {"Backend CLI", "wpctl (WirePlumber Control)"}
        };
        item->telemetry_title = "Speaker & output specifications";
    } else {
        item->slider_val = std::clamp(state.input_volume / 100.0f, 0.0f, 1.0f);
        item->switch_val = state.input_muted;
        item->telemetry = {
            {"Active Input", state.default_source_name},
            {"Input Level (Gain)", std::to_string(state.input_volume) + "%" + (state.input_muted ? " (MUTED)" : "")},
            {"Audio Server", "PipeWire 1.0 (WirePlumber)"},
            {"Source Node ID", std::to_string(state.default_source_id)},
            {"Backend CLI", "wpctl (WirePlumber Control)"}
        };
        item->telemetry_title = "Microphone & input specifications";
    }
}

void SoundBackend::sync_item_state(const std::shared_ptr<SettingItem>& item) {
    auto state = get_state();
    sync_item_state(item, state);
}

} // namespace miqu
