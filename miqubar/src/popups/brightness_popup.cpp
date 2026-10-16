#include "brightness_popup.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace miqu;

namespace miqubar {

BrightnessControlPopupView::BrightnessControlPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    update_brightness_info();
    setup_ui();
}

void BrightnessControlPopupView::update_brightness_info() {
    // 1. Try reading directly from sysfs backlight
    const fs::path sysfs_backlight("/sys/class/backlight");
    if (fs::exists(sysfs_backlight) && fs::is_directory(sysfs_backlight)) {
        for (const auto& entry : fs::directory_iterator(sysfs_backlight)) {
            if (entry.is_directory() || entry.is_symlink()) {
                fs::path cur_p = entry.path() / "brightness";
                fs::path max_p = entry.path() / "max_brightness";
                if (fs::exists(cur_p) && fs::exists(max_p)) {
                    std::ifstream cur_f(cur_p);
                    std::ifstream max_f(max_p);
                    long cur_val = 0, max_val = 0;
                    if (cur_f >> cur_val && max_f >> max_val && max_val > 0) {
                        m_backlight_path = entry.path().string();
                        m_brightness = std::clamp(static_cast<int>(std::round((cur_val * 100.0) / max_val)), 0, 100);
                        return;
                    }
                }
            }
        }
    }

    // 2. Fallback to brightnessctl -m
    FILE* fp = popen("brightnessctl -m 2>/dev/null", "r");
    if (fp) {
        char buf[256];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string line(buf);
            // format: device,class,curr,pct%,max
            size_t p = line.find('%');
            if (p != std::string::npos) {
                size_t start = line.rfind(',', p);
                if (start != std::string::npos) {
                    try {
                        m_brightness = std::clamp(std::stoi(line.substr(start + 1, p - start - 1)), 0, 100);
                    } catch (...) {}
                }
            }
        }
        pclose(fp);
    }
}

void BrightnessControlPopupView::set_brightness(int pct) {
    m_brightness = std::clamp(pct, 5, 100);
    std::string cmd = "brightnessctl set " + std::to_string(m_brightness) + "% -q 2>/dev/null";
    std::system(cmd.c_str());

    if (m_label) {
        m_label->set_text("Brightness: " + std::to_string(m_brightness) + "%");
    }
    if (m_icon_btn) {
        std::string icon = "notification-display-brightness-full";
        if (m_brightness < 30) icon = "notification-display-brightness-low";
        else if (m_brightness < 70) icon = "notification-display-brightness-medium";
        else if (m_brightness < 95) icon = "notification-display-brightness-high";
        m_icon_btn->set_icon(icon);
    }
    request_redraw();
}

void BrightnessControlPopupView::setup_ui() {
    auto config = Config::get();

    set_style(CardStyle::Outlined);
    set_radius(config->metrics.corner_radius > 0 ? config->metrics.corner_radius + 4 : 12);
    set_elevation(0);

    // Root layout
    auto root_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_layout->set_padding(20, 20, 20, 20);

    int h2_font_size = config->metrics.h2_size > 0 ? config->metrics.h2_size : 15;

    // 1. Header Row (Title "Display Brightness" + Icon Button)
    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header_row->set_gravity(Gravity::CenterVertical);
    header_row->set_margin(0, 0, 0, 14);

    auto title = TextViewBuilder::create()
        ->text("Display Brightness")
        ->bold(true)
        ->textSize(h2_font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    title->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    header_row->add_view(title);

    std::string icon_name = "notification-display-brightness-full";
    if (m_brightness < 30) icon_name = "notification-display-brightness-low";
    else if (m_brightness < 70) icon_name = "notification-display-brightness-medium";
    else if (m_brightness < 95) icon_name = "notification-display-brightness-high";

    m_icon_btn = ButtonBuilder::create()
        ->text("")
        ->icon(icon_name)
        ->iconSize(22)
        ->build();
    m_icon_btn->set_flat(true);
    m_icon_btn->set_padding(4);
    m_icon_btn->set_layout_params(LayoutParams(34, 34));
    m_icon_btn->set_on_click_listener([this]() {
        // Preset: Toggle between 100% and 50%
        set_brightness(m_brightness > 75 ? 50 : 100);
        if (m_slider) {
            m_slider->set_progress(m_brightness / 100.0f);
        }
    });
    header_row->add_view(m_icon_btn);

    root_layout->add_view(header_row);

    // 2. Brightness Label & Slider
    m_label = TextViewBuilder::create()
        ->text("Brightness: " + std::to_string(m_brightness) + "%")
        ->caption(true)
        ->muted(true)
        ->build();
    m_label->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_label->set_margin(0, 0, 0, 10);
    root_layout->add_view(m_label);

    m_slider = SliderBuilder::create()
        ->progress(std::clamp(m_brightness / 100.0f, 0.05f, 1.0f))
        ->trackHeight(8)
        ->thumbRadius(9)
        ->progressColor(config->colors.primary)
        ->thumbColor(config->colors.primary)
        ->build();
    m_slider->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_slider->set_margin(2, 4, 2, 4);
    m_slider->set_on_value_changed_listener([this](float val, bool from_user) {
        int pct = static_cast<int>(std::round(val * 100.0f));
        pct = std::clamp(pct, 5, 100);
        if (from_user) {
            set_brightness(pct);
        } else {
            m_brightness = pct;
            if (m_label) {
                m_label->set_text("Brightness: " + std::to_string(m_brightness) + "%");
            }
        }
    });
    root_layout->add_view(m_slider);

    add_view(root_layout);
}

Size BrightnessControlPopupView::measure_size() const {
    return {340, 150};
}

} // namespace miqubar
