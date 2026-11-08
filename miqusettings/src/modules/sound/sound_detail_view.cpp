#include "sound_detail_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <algorithm>
#include <cmath>

namespace miqu {

SoundDetailView::SoundDetailView() {
    m_state = SoundBackend::get_state();
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void SoundDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void SoundDetailView::setup_ui() {
    clear_views();

    m_scroll = std::make_shared<ScrollView>();
    m_scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_content_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_content_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_content_layout->set_divider_spacing(16);
    m_content_layout->set_padding(m_is_fullscreen ? 32 : 16);

    m_content_layout->add_view(create_hero_card());
    m_content_layout->add_view(create_output_volume_card());
    m_content_layout->add_view(create_output_devices_card());
    m_content_layout->add_view(create_input_volume_card());
    if (!m_state.input_devices.empty()) {
        m_content_layout->add_view(create_input_devices_card());
    }

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> SoundDetailView::create_hero_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(16);

    auto icon = std::make_shared<ImageView>("audio-speakers");
    icon->set_target_size(m_is_fullscreen ? 64 : 48);
    row->add_view(icon);

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    col->set_divider_spacing(4);

    auto title = TextViewBuilder::create()
        ->text("Sound & Audio")
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 16)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    std::string desc = "PipeWire output & input volume control • " + m_state.default_sink_name;
    auto subtitle = TextViewBuilder::create()
        ->text(desc)
        ->caption(true)
        ->muted(true)
        ->build();
    col->add_view(subtitle);

    std::string badge_text = "Master: " + std::to_string(m_state.output_volume) + "%";
    if (m_state.output_muted) badge_text += " [MUTED]";
    auto badge = TextViewBuilder::create()
        ->text(badge_text)
        ->bold(true)
        ->textSize(12)
        ->textColor(m_state.output_muted ? config->colors.on_surface_variant : config->colors.primary)
        ->build();
    col->add_view(badge);

    row->add_view(col);
    card->add_view(row);
    return card;
}

std::shared_ptr<View> SoundDetailView::create_output_volume_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(12);

    auto header = TextViewBuilder::create()
        ->text("MASTER OUTPUT VOLUME")
        ->bold(true)
        ->caption(true)
        ->muted(true)
        ->build();
    col->add_view(header);

    // Label row
    auto label_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    label_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    std::string vol_str = "Output Level: " + std::to_string(m_state.output_volume) + "%";
    if (m_state.output_muted) vol_str += " (MUTED)";
    m_out_vol_label = TextViewBuilder::create()
        ->text(vol_str)
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    m_out_vol_label->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    label_row->add_view(m_out_vol_label);

    auto mute_btn = ButtonBuilder::create()
        ->text(m_state.output_muted ? "Unmute" : "Mute")
        ->icon(m_state.output_muted ? "audio-volume-muted" : "audio-volume-high")
        ->build();
    mute_btn->set_flat(!m_state.output_muted);
    mute_btn->set_selected(m_state.output_muted);
    mute_btn->set_padding(8, 4);
    mute_btn->set_on_click_listener([this, mute_btn]() {
        SoundBackend::toggle_output_mute();
        m_state.output_muted = !m_state.output_muted;
        mute_btn->set_text(m_state.output_muted ? "Unmute" : "Mute");
        mute_btn->set_icon(m_state.output_muted ? "audio-volume-muted" : "audio-volume-high");
        mute_btn->set_flat(!m_state.output_muted);
        mute_btn->set_selected(m_state.output_muted);

        std::string txt = "Output Level: " + std::to_string(m_state.output_volume) + "%";
        if (m_state.output_muted) txt += " (MUTED)";
        if (m_out_vol_label) m_out_vol_label->set_text(txt);
        request_redraw();
    });
    label_row->add_view(mute_btn);
    col->add_view(label_row);

    // Slider
    m_out_slider = SliderBuilder::create()
        ->progress(std::clamp(m_state.output_volume / 100.0f, 0.0f, 1.0f))
        ->trackHeight(8)
        ->thumbRadius(9)
        ->progressColor(config->colors.primary)
        ->thumbColor(config->colors.primary)
        ->build();
    m_out_slider->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_out_slider->set_margin(0, 4, 0, 8);
    m_out_slider->set_on_value_changed_listener([this](float val, bool from_user) {
        int pct = static_cast<int>(std::round(val * 100.0f));
        pct = std::clamp(pct, 0, 100);
        m_state.output_volume = pct;
        if (m_out_vol_label) {
            std::string txt = "Output Level: " + std::to_string(m_state.output_volume) + "%";
            if (m_state.output_muted) txt += " (MUTED)";
            m_out_vol_label->set_text(txt);
        }
        if (from_user) {
            SoundBackend::set_output_volume(pct);
        }
    });
    col->add_view(m_out_slider);

    // Quick Presets
    auto presets_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    presets_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    presets_row->set_divider_spacing(8);

    std::vector<int> presets = {25, 50, 75, 100};
    for (int p : presets) {
        auto btn = ButtonBuilder::create()
            ->text(std::to_string(p) + "%")
            ->build();
        btn->set_flat(true);
        btn->set_padding(8, 4);
        btn->set_on_click_listener([this, p]() {
            m_state.output_volume = p;
            SoundBackend::set_output_volume(p);
            if (m_out_slider) m_out_slider->set_progress(p / 100.0f);
            if (m_out_vol_label) {
                std::string txt = "Output Level: " + std::to_string(p) + "%";
                if (m_state.output_muted) txt += " (MUTED)";
                m_out_vol_label->set_text(txt);
            }
            request_redraw();
        });
        presets_row->add_view(btn);
    }
    col->add_view(presets_row);

    card->add_view(col);
    return card;
}

std::shared_ptr<View> SoundDetailView::create_output_devices_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(10);

    auto header = TextViewBuilder::create()
        ->text("OUTPUT DESTINATIONS (SINKS)")
        ->bold(true)
        ->caption(true)
        ->muted(true)
        ->build();
    col->add_view(header);

    if (m_state.output_devices.empty()) {
        auto empty_label = TextViewBuilder::create()
            ->text("No output audio sinks discovered")
            ->caption(true)
            ->muted(true)
            ->build();
        col->add_view(empty_label);
    } else {
        for (const auto& dev : m_state.output_devices) {
            auto dev_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            dev_row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));
            dev_row->set_gravity(Gravity::CenterVertical);
            dev_row->set_divider_spacing(12);

            std::string icon_name = "audio-speakers";
            if (dev.name.find("Headphone") != std::string::npos || dev.name.find("Headset") != std::string::npos) {
                icon_name = "audio-headphones";
            }
            auto d_icon = std::make_shared<ImageView>(icon_name);
            d_icon->set_target_size(24);
            dev_row->add_view(d_icon);

            auto d_name = TextViewBuilder::create()
                ->text(dev.name)
                ->bold(dev.is_default)
                ->textSize(13)
                ->textColor(config->colors.on_surface)
                ->build();
            d_name->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
            dev_row->add_view(d_name);

            if (dev.is_default) {
                auto badge = TextViewBuilder::create()
                    ->text("✓ Default")
                    ->bold(true)
                    ->textSize(11)
                    ->textColor(config->colors.primary)
                    ->build();
                dev_row->add_view(badge);
            } else {
                auto sel_btn = ButtonBuilder::create()
                    ->text("Select")
                    ->build();
                sel_btn->set_flat(true);
                sel_btn->set_padding(6, 2);
                sel_btn->set_on_click_listener([this, dev_id = dev.id]() {
                    SoundBackend::set_default_sink(dev_id);
                    m_state = SoundBackend::get_state();
                    setup_ui();
                });
                dev_row->add_view(sel_btn);
            }
            col->add_view(dev_row);
        }
    }

    card->add_view(col);
    return card;
}

std::shared_ptr<View> SoundDetailView::create_input_volume_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(12);

    auto header = TextViewBuilder::create()
        ->text("MICROPHONE / INPUT")
        ->bold(true)
        ->caption(true)
        ->muted(true)
        ->build();
    col->add_view(header);

    auto label_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    label_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    std::string in_txt = "Input Gain: " + std::to_string(m_state.input_volume) + "%";
    if (m_state.input_muted) in_txt += " (MUTED)";
    m_in_vol_label = TextViewBuilder::create()
        ->text(in_txt)
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    m_in_vol_label->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    label_row->add_view(m_in_vol_label);

    auto mic_mute_btn = ButtonBuilder::create()
        ->text(m_state.input_muted ? "Unmute Mic" : "Mute Mic")
        ->icon(m_state.input_muted ? "audio-volume-muted" : "audio-input-microphone")
        ->build();
    mic_mute_btn->set_flat(!m_state.input_muted);
    mic_mute_btn->set_selected(m_state.input_muted);
    mic_mute_btn->set_padding(8, 4);
    mic_mute_btn->set_on_click_listener([this, mic_mute_btn]() {
        SoundBackend::toggle_input_mute();
        m_state.input_muted = !m_state.input_muted;
        mic_mute_btn->set_text(m_state.input_muted ? "Unmute Mic" : "Mute Mic");
        mic_mute_btn->set_icon(m_state.input_muted ? "audio-volume-muted" : "audio-input-microphone");
        mic_mute_btn->set_flat(!m_state.input_muted);
        mic_mute_btn->set_selected(m_state.input_muted);

        std::string txt = "Input Gain: " + std::to_string(m_state.input_volume) + "%";
        if (m_state.input_muted) txt += " (MUTED)";
        if (m_in_vol_label) m_in_vol_label->set_text(txt);
        request_redraw();
    });
    label_row->add_view(mic_mute_btn);
    col->add_view(label_row);

    m_in_slider = SliderBuilder::create()
        ->progress(std::clamp(m_state.input_volume / 100.0f, 0.0f, 1.0f))
        ->trackHeight(8)
        ->thumbRadius(9)
        ->progressColor(config->colors.primary)
        ->thumbColor(config->colors.primary)
        ->build();
    m_in_slider->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_in_slider->set_margin(0, 4, 0, 8);
    m_in_slider->set_on_value_changed_listener([this](float val, bool from_user) {
        int pct = static_cast<int>(std::round(val * 100.0f));
        pct = std::clamp(pct, 0, 100);
        m_state.input_volume = pct;
        if (m_in_vol_label) {
            std::string txt = "Input Gain: " + std::to_string(m_state.input_volume) + "%";
            if (m_state.input_muted) txt += " (MUTED)";
            m_in_vol_label->set_text(txt);
        }
        if (from_user) {
            SoundBackend::set_input_volume(pct);
        }
    });
    col->add_view(m_in_slider);

    card->add_view(col);
    return card;
}

std::shared_ptr<View> SoundDetailView::create_input_devices_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(10);

    auto header = TextViewBuilder::create()
        ->text("INPUT SOURCES (MICROPHONES)")
        ->bold(true)
        ->caption(true)
        ->muted(true)
        ->build();
    col->add_view(header);

    for (const auto& dev : m_state.input_devices) {
        auto dev_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        dev_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        dev_row->set_gravity(Gravity::CenterVertical);
        dev_row->set_divider_spacing(12);

        auto d_icon = std::make_shared<ImageView>("audio-input-microphone");
        d_icon->set_target_size(24);
        dev_row->add_view(d_icon);

        auto d_name = TextViewBuilder::create()
            ->text(dev.name)
            ->bold(dev.is_default)
            ->textSize(13)
            ->textColor(config->colors.on_surface)
            ->build();
        d_name->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
        dev_row->add_view(d_name);

        if (dev.is_default) {
            auto badge = TextViewBuilder::create()
                ->text("✓ Default")
                ->bold(true)
                ->textSize(11)
                ->textColor(config->colors.primary)
                ->build();
            dev_row->add_view(badge);
        } else {
            auto sel_btn = ButtonBuilder::create()
                ->text("Select")
                ->build();
            sel_btn->set_flat(true);
            sel_btn->set_padding(6, 2);
            sel_btn->set_on_click_listener([this, dev_id = dev.id]() {
                SoundBackend::set_default_source(dev_id);
                m_state = SoundBackend::get_state();
                setup_ui();
            });
            dev_row->add_view(sel_btn);
        }
        col->add_view(dev_row);
    }

    card->add_view(col);
    return card;
}

} // namespace miqu
