#include "modules/keyboard/keyboard_backend.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

namespace miqu {

struct PipeCloser {
    void operator()(FILE* fp) const {
        if (fp) pclose(fp);
    }
};

std::string KeyboardBackend::run_miquctl(const std::string& args) {
    std::string result;
    std::string cmd = "miquctl " + args;
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return result;

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    return result;
}

InputDevicesInfo KeyboardBackend::get_devices_info() {
    InputDevicesInfo info;
    std::string json = run_miquctl("-j devices");
    if (json.empty()) return info;

    // Parse keyboards array
    size_t kb_pos = json.find("\"keyboards\"");
    size_t ptr_pos = json.find("\"pointers\"");
    size_t touch_pos = json.find("\"touch\"");

    if (kb_pos != std::string::npos) {
        std::string kb_section = json.substr(kb_pos, (ptr_pos != std::string::npos ? ptr_pos - kb_pos : std::string::npos));
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(kb_section.begin(), kb_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            KeyboardDeviceInfo kb;

            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) kb.name = m[1].str();

            std::regex layout_re("\"layout\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, layout_re) && m.size() > 1) kb.layout = m[1].str();

            std::regex rate_re("\"repeat_rate\"\\s*:\\s*([0-9]+)");
            if (std::regex_search(obj, m, rate_re) && m.size() > 1) {
                try { kb.repeat_rate = std::stoi(m[1].str()); } catch (...) {}
            }

            std::regex delay_re("\"repeat_delay\"\\s*:\\s*([0-9]+)");
            if (std::regex_search(obj, m, delay_re) && m.size() > 1) {
                try { kb.repeat_delay = std::stoi(m[1].str()); } catch (...) {}
            }

            if (!kb.name.empty()) info.keyboards.push_back(kb);
        }
    }

    if (ptr_pos != std::string::npos) {
        std::string ptr_section = json.substr(ptr_pos, (touch_pos != std::string::npos ? touch_pos - ptr_pos : std::string::npos));
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(ptr_section.begin(), ptr_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            PointerDeviceInfo ptr;
            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) ptr.name = m[1].str();
            if (!ptr.name.empty()) info.pointers.push_back(ptr);
        }
    }

    if (touch_pos != std::string::npos) {
        std::string touch_section = json.substr(touch_pos);
        std::regex obj_regex(R"(\{[^{}]+\})");
        auto begin = std::sregex_iterator(touch_section.begin(), touch_section.end(), obj_regex);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it) {
            std::string obj = it->str();
            TouchDeviceInfo tch;
            std::smatch m;
            std::regex name_re("\"name\"\\s*:\\s*\"([^\"]+)\"");
            if (std::regex_search(obj, m, name_re) && m.size() > 1) tch.name = m[1].str();
            if (!tch.name.empty()) info.touch_devices.push_back(tch);
        }
    }

    return info;
}

void KeyboardBackend::update_config_file(const std::string& key, const std::string& value) {
    const char* home = std::getenv("HOME");
    if (!home) return;

    fs::path conf_dir = fs::path(home) / ".config" / "miquland";
    fs::path conf_file = conf_dir / "miquland.conf";

    if (!fs::exists(conf_file)) {
        fs::create_directories(conf_dir);
        if (fs::exists("/usr/share/miquland/miquland.conf")) {
            std::error_code ec;
            fs::copy_file("/usr/share/miquland/miquland.conf", conf_file, ec);
        }
    }

    if (!fs::exists(conf_file)) return;

    std::ifstream in(conf_file);
    if (!in.is_open()) return;

    std::vector<std::string> lines;
    std::string line;
    bool replaced = false;

    std::regex key_regex("^\\s*" + key + "\\s*=.*");

    while (std::getline(in, line)) {
        if (!replaced && std::regex_match(line, key_regex)) {
            size_t first_non_space = line.find_first_not_of(" \t");
            std::string indent = (first_non_space != std::string::npos) ? line.substr(0, first_non_space) : "";
            lines.push_back(indent + key + " = " + value);
            replaced = true;
        } else {
            lines.push_back(line);
        }
    }
    in.close();

    if (!replaced) {
        lines.push_back(key + " = " + value);
    }

    std::ofstream out(conf_file, std::ios::trunc);
    if (out.is_open()) {
        for (const auto& l : lines) {
            out << l << "\n";
        }
    }
}

void KeyboardBackend::set_repeat_info(int rate, int delay) {
    update_config_file("repeat_rate", std::to_string(rate));
    update_config_file("repeat_delay", std::to_string(delay));
    run_miquctl("reload");
}

void KeyboardBackend::set_keyboard_layout(const std::string& layout) {
    update_config_file("layout", layout);
    run_miquctl("reload");
}

} // namespace miqu
