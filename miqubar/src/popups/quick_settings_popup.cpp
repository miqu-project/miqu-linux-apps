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

SoundControlPopupView::SoundControlPopupView(std::function<void()> on_close)
    : m_on_close(std::move(on_close)) {
    update_audio_info();
    setup_ui();
}

void SoundControlPopupView::setup_ui() {
    auto config = Config::get();

    set_style(CardStyle::Outlined);
    set_radius(config->metrics.corner_radius > 0 ? config->metrics.corner_radius + 4 : 12);
    // Since popup elevation is handled by miquland.conf layer rule and CardView is tightly packed to edge, we don't need any elevation of CardView.
    set_elevation(0);

    // Root layout
    auto root_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    root_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root_layout->set_padding(20, 20, 20, 20);

    int h2_font_size = config->metrics.h2_size > 0 ? config->metrics.h2_size : 15;

    // 1. Header Row (Title "Sound Output" + Dedicated Mute Button)
    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header_row->set_gravity(Gravity::CenterVertical);
    header_row->set_margin(0, 0, 0, 14);

    auto title = TextViewBuilder::create()
        ->text("Sound Output")
        ->bold(true)
        ->textSize(h2_font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    title->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    header_row->add_view(title);

    // Dedicated Mute button with dynamic icon only
    m_mute_btn = ButtonBuilder::create()
        ->text("")
        ->icon(m_muted ? "audio-volume-muted" : "audio-volume-high")
        ->iconSize(22)
        ->build();
    m_mute_btn->set_flat(!m_muted);
    m_mute_btn->set_selected(m_muted);
    m_mute_btn->set_padding(4);
    m_mute_btn->set_layout_params(LayoutParams(34, 34));
    m_mute_btn->set_on_click_listener([this]() {
        toggle_mute();
    });
    header_row->add_view(m_mute_btn);

    root_layout->add_view(header_row);

    // 2. Volume Label & Slider
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
    m_vol_label->set_margin(0, 0, 0, 10);
    root_layout->add_view(m_vol_label);

    m_vol_slider = SliderBuilder::create()
        ->progress(std::clamp(m_volume / 100.0f, 0.0f, 1.0f))
        ->trackHeight(8)
        ->thumbRadius(9)
        ->progressColor(config->colors.primary)
        ->thumbColor(config->colors.primary)
        ->build();
    m_vol_slider->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_vol_slider->set_margin(2, 4, 2, 4);
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

void SoundControlPopupView::update_audio_info() {
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

void SoundControlPopupView::set_volume(int pct) {
    m_volume = std::clamp(pct, 0, 100);
    std::string cmd = "wpctl set-volume -l 1.0 @DEFAULT_AUDIO_SINK@ " + std::to_string(m_volume / 100.0f) + " 2>/dev/null";
    std::system(cmd.c_str());
    request_redraw();
}

void SoundControlPopupView::toggle_mute() {
    std::system("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle 2>/dev/null || pactl set-sink-mute @DEFAULT_SINK@ toggle 2>/dev/null");
    update_audio_info();
    if (m_vol_label) {
        std::string text = "Volume: " + std::to_string(m_volume) + "%";
        if (m_muted) text += " [MUTED]";
        m_vol_label->set_text(text);
    }
    if (m_mute_btn) {
        m_mute_btn->set_text("");
        m_mute_btn->set_icon(m_muted ? "audio-volume-muted" : "audio-volume-high");
        m_mute_btn->set_flat(!m_muted);
        m_mute_btn->set_selected(m_muted);
    }
    request_redraw();
}

miqu::Size SoundControlPopupView::measure_size() const {
    return {340, 150};
}

} // namespace miqubar
