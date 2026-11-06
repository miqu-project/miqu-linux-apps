#pragma once

#include <string>
#include <vector>

namespace miqu {

struct WmOptions {
    std::string layout = "dwindle";
    int gaps_in = 8;
    int gaps_out = 12;
    int border_width = 1;
    int border_radius = 18;
    float opacity_active = 1.0f;
    float opacity_inactive = 1.0f;
    bool animations = true;
    bool blur = true;
    bool smart_gaps = false;
};

class WmBackend {
public:
    static WmOptions get_options();
    static void set_layout(const std::string& layout);
    static void toggle_split();
    static void set_smart_gaps(bool enabled);
    static void set_gaps(int inner, int outer);
    static void set_borders(int width, int radius);
    static void set_opacities(float active, float inactive);
    static void set_animations(bool enabled);
    static void set_blur(bool enabled);
    static void reload_compositor();
    static std::string run_miquctl(const std::string& args);

private:
    static void update_config_file(const std::string& key, const std::string& value);
};

} // namespace miqu
