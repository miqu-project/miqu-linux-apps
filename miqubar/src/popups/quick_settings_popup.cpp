#include "quick_settings_popup.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>

using namespace miqu;

namespace miqubar {

QuickSettingsPopupView::QuickSettingsPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    update_audio_info();
    setup_ui();
}

void QuickSettingsPopupView::setup_ui() {
    auto config = Config::get();

    set_style(CardStyle::Outlined);
    set_radius(config->metrics.corner_radius);
    set_elevation(6);

    // Root layout
    auto root_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_layout->set_padding(16, 16, 16, 16);

    int base_font_size = config->metrics.font_size > 0 ? config->metrics.font_size : 11;
    int h3_font_size = config->metrics.h3_size > 0 ? config->metrics.h3_size : 13;

    // 1. Header Title
    auto title = TextViewBuilder::create()
        ->text("Quick Settings")
        ->bold(true)
        ->textSize(h3_font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    title->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    title->set_margin(0, 0, 0, 12);
    root_layout->add_view(title);

    // 2. 2x2 Toggle Tiles
    auto row1 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row1->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row1->set_margin(0, 0, 0, 8);

    m_wifi_btn = ButtonBuilder::create()->text("Wi-Fi")->textSize(base_font_size)->build();
    m_wifi_btn->set_layout_params(LayoutParams(0, 38, 1.0f));
    m_wifi_btn->set_margin(0, 0, 4, 0);
    m_wifi_btn->set_on_click_listener([this]() {
        m_wifi_on = !m_wifi_on;
        update_tile_styles();
        request_redraw();
    });

    m_bt_btn = ButtonBuilder::create()->text("Bluetooth")->textSize(base_font_size)->build();
    m_bt_btn->set_layout_params(LayoutParams(0, 38, 1.0f));
    m_bt_btn->set_margin(4, 0, 0, 0);
    m_bt_btn->set_on_click_listener([this]() {
        m_bluetooth_on = !m_bluetooth_on;
        update_tile_styles();
        request_redraw();
    });

    row1->add_view(m_wifi_btn);
    row1->add_view(m_bt_btn);
    root_layout->add_view(row1);

    auto row2 = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row2->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row2->set_margin(0, 0, 0, 14);

    m_night_btn = ButtonBuilder::create()->text("Night Light")->textSize(base_font_size)->build();
    m_night_btn->set_layout_params(LayoutParams(0, 38, 1.0f));
    m_night_btn->set_margin(0, 0, 4, 0);
    m_night_btn->set_on_click_listener([this]() {
        m_night_light_on = !m_night_light_on;
        update_tile_styles();
        request_redraw();
    });

    m_power_btn = ButtonBuilder::create()->text("Power Save")->textSize(base_font_size)->build();
    m_power_btn->set_layout_params(LayoutParams(0, 38, 1.0f));
    m_power_btn->set_margin(4, 0, 0, 0);
    m_power_btn->set_on_click_listener([this]() {
        m_power_save_on = !m_power_save_on;
        update_tile_styles();
        request_redraw();
    });

    row2->add_view(m_night_btn);
    row2->add_view(m_power_btn);
    root_layout->add_view(row2);

    update_tile_styles();

    // 3. Volume Section
    std::string vol_text = "Volume: " + std::to_string(m_volume) + "%";
    if (m_muted) vol_text += " [MUTED]";
    m_vol_label = TextViewBuilder::create()
        ->text(vol_text)
        ->caption(true)
        ->muted(true)
        ->build();
    m_vol_label->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_vol_label->set_margin(0, 0, 0, 6);
    root_layout->add_view(m_vol_label);

    m_vol_slider = SliderBuilder::create()
        ->progress(std::clamp(m_volume / 100.0f, 0.0f, 1.0f))
        ->trackHeight(6)
        ->thumbRadius(8)
        ->progressColor(config->colors.primary)
        ->thumbColor(config->colors.primary)
        ->build();
    m_vol_slider->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_vol_slider->set_margin(2, 2, 2, 2);
    m_vol_slider->set_on_value_changed_listener([this](float val, bool from_user) {
        int pct = static_cast<int>(std::round(val * 100.0f));
        pct = std::clamp(pct, 0, 100);
        m_volume = pct;
        if (m_vol_label) {
            std::string text = "Volume: " + std::to_string(m_volume) + "%";
            if (m_muted) text += " [MUTED]";
            m_vol_label->set_text(text);
        }
        if (from_user) {
            set_volume(m_volume);
        }
    });
    root_layout->add_view(m_vol_slider);

    add_view(root_layout);
}

void QuickSettingsPopupView::update_tile_styles() {
    auto apply = [](const std::shared_ptr<miqu::Button>& btn, bool active) {
        if (!btn) return;
        btn->set_selected(active);
        btn->set_outlined(!active);
    };
    apply(m_wifi_btn, m_wifi_on);
    apply(m_bt_btn, m_bluetooth_on);
    apply(m_night_btn, m_night_light_on);
    apply(m_power_btn, m_power_save_on);
}

void QuickSettingsPopupView::update_audio_info() {
    FILE* fp = popen("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string str(buf);
            size_t pos = str.find("Volume:");
            if (pos != std::string::npos) {
                float v = std::stof(str.substr(pos + 7));
                m_volume = static_cast<int>(std::round(v * 100.0f));
            }
            m_muted = (str.find("[MUTED]") != std::string::npos);
        }
        pclose(fp);
    }
}

void QuickSettingsPopupView::set_volume(int pct) {
    m_volume = std::clamp(pct, 0, 100);
    std::string cmd = "wpctl set-volume @DEFAULT_AUDIO_SINK@ " + std::to_string(m_volume / 100.0f) + " 2>/dev/null";
    std::system(cmd.c_str());
    request_redraw();
}

miqu::Size QuickSettingsPopupView::measure_size() const {
    return {300, 260};
}

} // namespace miqubar
