#include "miller_view.hpp"
#include "preview_common.hpp"
#include "display_backend.hpp"
#include "monitor_backend.hpp"
#include "sound_backend.hpp"
#include "battery_backend.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {
std::shared_ptr<View> MillerView::create_sound_volume_card(
    const std::shared_ptr<SettingItem>& item,
    const std::shared_ptr<Config>& config,
    const SoundState& state,
    bool is_speaker
) {
    int current_pct = is_speaker ? state.output_volume : state.input_volume;
    bool is_muted = is_speaker ? state.output_muted : state.input_muted;
    std::string dev_name = is_speaker ? state.default_sink_name : state.default_source_name;

    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto get_icon_str = [is_speaker, is_muted, current_pct]() -> std::string {
        if (is_muted) return is_speaker ? "󰝟" : "󰍭";
        if (is_speaker) {
            if (current_pct >= 70) return "󰕾";
            if (current_pct >= 30) return "󰖀";
            return "󰕿";
        }
        return "󰍬";
    };

    auto icon = TextViewBuilder::create()
        ->text(get_icon_str())
        ->textSize(config->metrics.font_size + 4)
        ->textColor(is_muted ? config->colors.on_surface_variant : config->colors.primary)
        ->build();
    LayoutParams icon_lp;
    icon_lp.width = 30;
    icon_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(icon, icon_lp);

    auto lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    std::string title_str = is_speaker ? "Master Output Volume" : "Microphone Input Level (Gain)";
    auto title = TextViewBuilder::create()
        ->text(title_str)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    lbl_box->add_view(title);

    std::string hint_str = (is_speaker ? "PipeWire master sink · " : "PipeWire capture source · ") + dev_name;
    auto hint = TextViewBuilder::create()
        ->text(hint_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    lbl_box->add_view(hint);

    LayoutParams lbl_lp;
    lbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl_lp.weight = 1.0f;
    lbl_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(lbl_box, lbl_lp);

    std::string val_str = std::to_string(current_pct) + "%" + (is_muted ? " [MUTED]" : "");
    m_active_slider_label = TextViewBuilder::create()
        ->text(val_str)
        ->textSize(config->metrics.font_size)
        ->bold(true)
        ->textColor(is_muted ? config->colors.on_surface_variant : config->colors.primary)
        ->build();
    LayoutParams val_lp;
    val_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    val_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(m_active_slider_label, val_lp);
    card->add_view(header_row);

    // Slider Row
    auto slider_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon_dim = TextViewBuilder::create()
        ->text(is_speaker ? "󰕿" : "󰍬")
        ->textSize(config->metrics.font_size - 2)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->build();
    LayoutParams idim_lp;
    idim_lp.width = 20;
    idim_lp.gravity = Gravity::CenterVertical;
    slider_row->add_view(icon_dim, idim_lp);

    auto label_ref = m_active_slider_label;
    m_active_slider = SliderBuilder::create()
        ->value(std::clamp(current_pct / 100.0f, 0.0f, 1.0f))
        ->onValueChanged([this, item, is_speaker, label_ref, icon](float val, bool user) {
            (void)user;
            float clamped = std::clamp(val, 0.0f, 1.0f);
            item->slider_val = clamped;
            int pct = static_cast<int>(std::round(clamped * 100.0f));

            if (label_ref) {
                label_ref->set_text(std::to_string(pct) + "%" + (item->switch_val ? " [MUTED]" : ""));
                label_ref->request_redraw();
            }

            if (icon && !item->switch_val) {
                if (is_speaker) {
                    if (pct >= 70) icon->set_text("󰕾");
                    else if (pct >= 30) icon->set_text("󰖀");
                    else icon->set_text("󰕿");
                }
                icon->request_redraw();
            }

            if (is_speaker) {
                SoundBackend::set_output_volume(pct);
            } else {
                SoundBackend::set_input_volume(pct);
            }
            notify_state_changed();
        })
        ->build();

    LayoutParams sl_lp;
    sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sl_lp.height = 24;
    sl_lp.weight = 1.0f;
    sl_lp.gravity = Gravity::CenterVertical;
    slider_row->add_view(m_active_slider, sl_lp);

    auto icon_bright = TextViewBuilder::create()
        ->text(is_speaker ? "󰕾" : "󰍬")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams ibright_lp;
    ibright_lp.width = 20;
    ibright_lp.gravity = Gravity::CenterVertical;
    slider_row->add_view(icon_bright, ibright_lp);

    card->add_view(slider_row);
    return card;
}

std::shared_ptr<View> MillerView::create_sound_presets_card(
    const std::shared_ptr<SettingItem>& item,
    const std::shared_ptr<Config>& config,
    int current_pct,
    bool is_speaker
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("QUICK PRESETS")
        ->bold(true)
        ->caption(true)
        ->textColor(config->colors.outline)
        ->build();
    card->add_view(title);

    auto presets_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();

    struct SoundPreset {
        std::string label;
        int value;
    };
    std::vector<SoundPreset> presets;
    if (is_speaker) {
        presets = {{"25% Low", 25}, {"50% Balanced", 50}, {"75% Loud", 75}, {"100% Max", 100}};
    } else {
        presets = {{"25% Low", 25}, {"50% Balanced", 50}, {"75% High", 75}, {"100% Max", 100}};
    }

    for (const auto& p : presets) {
        bool is_sel = (std::abs(current_pct - p.value) <= 3);

        auto btn = ButtonBuilder::create()
            ->text(p.label)
            ->textSize(config->metrics.caption_size)
            ->primary(is_sel)
            ->style(is_sel ? ButtonStyle::Primary : ButtonStyle::Standard)
            ->cornerRadius(get_btn_radius(config))
            ->padding(8, 6)
            ->onClick([this, item, is_speaker, target = p.value]() {
                item->slider_val = target / 100.0f;
                if (is_speaker) {
                    SoundBackend::set_output_volume(target);
                } else {
                    SoundBackend::set_input_volume(target);
                }
                deferred_rebuild_preview(60);
                notify_state_changed();
            })
            ->build();

        LayoutParams btn_lp;
        btn_lp.width = 0;
        btn_lp.height = 32;
        btn_lp.weight = 1.0f;
        presets_row->add_view(btn, btn_lp);
    }

    card->add_view(presets_row);
    return card;
}

std::shared_ptr<View> MillerView::create_sound_mute_card(
    const std::shared_ptr<SettingItem>& item,
    const std::shared_ptr<Config>& config,
    bool is_muted,
    bool is_speaker
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->gravity(Gravity::CenterVertical)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto icon = TextViewBuilder::create()
        ->text(is_muted ? (is_speaker ? "󰝟" : "󰍭") : (is_speaker ? "󰕾" : "󰍬"))
        ->textSize(config->metrics.font_size + 4)
        ->textColor(is_muted ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams icon_lp;
    icon_lp.width = 30;
    icon_lp.gravity = Gravity::CenterVertical;
    card->add_view(icon, icon_lp);

    auto lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    std::string title_str = is_speaker ? "Mute Speaker Output" : "Mute Microphone Input";
    auto title = TextViewBuilder::create()
        ->text(title_str)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    lbl_box->add_view(title);

    std::string hint_str = is_speaker ? "Silence all master audio output streams" : "Silence active microphone audio capture";
    auto hint = TextViewBuilder::create()
        ->text(hint_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    lbl_box->add_view(hint);

    LayoutParams mlbl_lp;
    mlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    mlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    mlbl_lp.weight = 1.0f;
    mlbl_lp.gravity = Gravity::CenterVertical;
    card->add_view(lbl_box, mlbl_lp);

    auto sw = SwitchBuilder::create()
        ->checked(is_muted)
        ->onCheckedChanged([this, item, is_speaker](bool checked) {
            item->switch_val = checked;
            if (is_speaker) {
                SoundBackend::set_output_mute(checked);
            } else {
                SoundBackend::set_input_mute(checked);
            }
            deferred_rebuild_preview(80);
            notify_state_changed();
        })
        ->build();

    LayoutParams sw_lp;
    sw_lp.width = 44;
    sw_lp.height = 24;
    sw_lp.gravity = Gravity::CenterVertical;
    card->add_view(sw, sw_lp);

    return card;
}

std::shared_ptr<View> MillerView::create_sound_devices_card(
    const std::shared_ptr<Config>& config,
    const std::vector<SoundDevice>& devices,
    bool is_speaker
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    std::string header_str = is_speaker ? "OUTPUT DESTINATIONS (SINKS)" : "INPUT SOURCES (MICROPHONES)";
    auto header = TextViewBuilder::create()
        ->text(header_str)
        ->bold(true)
        ->caption(true)
        ->textColor(config->colors.outline)
        ->build();
    card->add_view(header);

    if (devices.empty()) {
        std::string empty_str = is_speaker ? "No output audio sinks discovered" : "No input audio sources discovered";
        auto empty_label = TextViewBuilder::create()
            ->text(empty_str)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
            ->build();
        card->add_view(empty_label);
        return card;
    }

    for (const auto& dev : devices) {
        auto dev_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->spacing(12)
            ->padding(6, 4)
            ->build();

        std::string dev_icon_str = is_speaker ? "󰕾" : "󰍬";
        if (dev.name.find("Headphone") != std::string::npos || dev.name.find("Headset") != std::string::npos) {
            dev_icon_str = is_speaker ? "󰋋" : "󰋎";
        } else if (dev.name.find("HDMI") != std::string::npos || dev.name.find("DisplayPort") != std::string::npos) {
            dev_icon_str = "󰡁";
        }

        auto d_icon = TextViewBuilder::create()
            ->text(dev_icon_str)
            ->textSize(config->metrics.font_size + 2)
            ->textColor(dev.is_default ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        LayoutParams d_icon_lp;
        d_icon_lp.width = 24;
        d_icon_lp.gravity = Gravity::CenterVertical;
        dev_row->add_view(d_icon, d_icon_lp);

        auto d_col = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto d_name = TextViewBuilder::create()
            ->text(dev.name)
            ->bold(dev.is_default)
            ->textSize(config->metrics.font_size - 1)
            ->textColor(dev.is_default ? config->colors.on_surface : config->colors.on_surface_variant)
            ->build();
        d_col->add_view(d_name);

        int dev_vol = static_cast<int>(std::round(dev.volume * 100.0f));
        std::string d_sub = "Node " + std::to_string(dev.id) + " · Level: " + std::to_string(dev_vol) + "%" + (dev.is_muted ? " · MUTED" : "");
        auto d_sub_view = TextViewBuilder::create()
            ->text(d_sub)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
            ->build();
        d_col->add_view(d_sub_view);

        LayoutParams d_col_lp;
        d_col_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        d_col_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        d_col_lp.weight = 1.0f;
        d_col_lp.gravity = Gravity::CenterVertical;
        dev_row->add_view(d_col, d_col_lp);

        if (dev.is_default) {
            auto badge = TextViewBuilder::create()
                ->text("✓ Default")
                ->bold(true)
                ->textSize(config->metrics.caption_size)
                ->textColor(config->colors.primary)
                ->build();
            dev_row->add_view(badge);
        } else {
            auto sel_btn = ButtonBuilder::create()
                ->text("Set Default")
                ->textSize(config->metrics.caption_size)
                ->flat(true)
                ->cornerRadius(get_btn_radius(config))
                ->padding(8, 4)
                ->onClick([this, is_speaker, id = dev.id]() {
                    if (is_speaker) {
                        SoundBackend::set_default_sink(id);
                    } else {
                        SoundBackend::set_default_source(id);
                    }
                    deferred_rebuild_preview(150);
                    notify_state_changed();
                })
                ->build();
            dev_row->add_view(sel_btn);
        }

        card->add_view(dev_row);
    }

    return card;
}

void MillerView::render_sound_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    bool is_speaker = (item->id == "sound_speaker");
    auto state = SoundBackend::get_state();
    SoundBackend::sync_item_state(item, state);

    int current_pct = is_speaker ? state.output_volume : state.input_volume;
    bool is_muted = is_speaker ? state.output_muted : state.input_muted;

    // 1. Seekbar Volume / Gain Card
    add_preview_card(create_sound_volume_card(item, config, state, is_speaker));

    // 2. Quick Presets Card
    add_preview_card(create_sound_presets_card(item, config, current_pct, is_speaker));

    // 3. Mute Toggle Switch Card
    add_preview_card(create_sound_mute_card(item, config, is_muted, is_speaker));

    // 4. Audio Endpoints / Destinations Card
    const auto& devices = is_speaker ? state.output_devices : state.input_devices;
    add_preview_card(create_sound_devices_card(config, devices, is_speaker));

    // 5. Technical Specifications / Telemetry Card
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, item->telemetry_title));
    }
}


void MillerView::render_brightness_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Sync state with hardware via DisplayBackend
    DisplayBackend::sync_item_state(item);
    auto b_info = DisplayBackend::get_brightness_info();

    int current_pct = static_cast<int>(std::round(item->brightness_val * 100.0f));

    // Dynamic icon based on brightness level
    std::string bright_icon_str = (current_pct >= 75) ? "󰃠" : ((current_pct >= 40) ? "󰃟" : "󰃞");

    // 1. Brightness Adjustment Card
    auto bright_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon = TextViewBuilder::create()
        ->text(bright_icon_str)
        ->textSize(config->metrics.font_size + 4)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams icon_lp;
    icon_lp.width = 30;
    icon_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(icon, icon_lp);

    auto lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Display Brightness")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    lbl_box->add_view(title);

    std::string hint_str = b_info.available
        ? ("Backlight level: " + std::to_string(current_pct) + "% · " + b_info.device_name)
        : "Adjust panel backlight luminance";

    auto hint = TextViewBuilder::create()
        ->text(hint_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    lbl_box->add_view(hint);

    LayoutParams lbl_lp;
    lbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl_lp.weight = 1.0f;
    lbl_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(lbl_box, lbl_lp);

    auto val_label = TextViewBuilder::create()
        ->text(std::to_string(current_pct) + "%")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->bold(true)
        ->build();
    LayoutParams val_lp;
    val_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    val_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(val_label, val_lp);

    bright_card->add_view(header_row);

    // Slider Row with Dim and Bright icon indicators
    auto slider_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon_dim = TextViewBuilder::create()
        ->text("󰃞")
        ->textSize(config->metrics.font_size - 2)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
        ->build();
    LayoutParams idim_lp;
    idim_lp.width = 20;
    idim_lp.gravity = Gravity::CenterVertical;
    slider_row->add_view(icon_dim, idim_lp);

    auto slider = SliderBuilder::create()
        ->value(item->brightness_val)
        ->onValueChanged([this, item, val_label, icon, hint, b_info](float val, bool user) {
            (void)user;
            float clamped = std::clamp(val, 0.01f, 1.0f);
            item->brightness_val = clamped;
            int pct = static_cast<int>(std::round(clamped * 100.0f));
            val_label->set_text(std::to_string(pct) + "%");
            val_label->request_redraw();

            // Dynamically update icon and hint string
            if (pct >= 75) icon->set_text("󰃠");
            else if (pct >= 40) icon->set_text("󰃟");
            else icon->set_text("󰃞");
            icon->request_redraw();

            if (b_info.available) {
                hint->set_text("Backlight level: " + std::to_string(pct) + "% · " + b_info.device_name);
                hint->request_redraw();
            }

            // Real-time hardware adjustment via DisplayBackend
            DisplayBackend::set_brightness_percent(pct);
            notify_state_changed();
        })
        ->build();

    LayoutParams sl_lp;
    sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sl_lp.height = 24;
    sl_lp.weight = 1.0f;
    sl_lp.gravity = Gravity::CenterVertical;
    slider_row->add_view(slider, sl_lp);

    auto icon_bright = TextViewBuilder::create()
        ->text("󰃠")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams ibright_lp;
    ibright_lp.width = 20;
    ibright_lp.gravity = Gravity::CenterVertical;
    slider_row->add_view(icon_bright, ibright_lp);

    bright_card->add_view(slider_row);
    add_preview_card(bright_card);

    // 2. Quick Brightness Presets Card
    auto presets_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(14, 12)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto presets_title = TextViewBuilder::create()
        ->text("QUICK PRESETS")
        ->bold(true)
        ->caption(true)
        ->textColor(config->colors.outline)
        ->build();
    presets_card->add_view(presets_title);

    auto presets_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();

    struct Preset {
        std::string label;
        float value;
    };
    std::vector<Preset> presets = {
        {"25% Dim", 0.25f},
        {"50% Balanced", 0.50f},
        {"75% Bright", 0.75f},
        {"100% Max", 1.00f}
    };

    for (const auto& p : presets) {
        int cur_p = static_cast<int>(std::round(item->brightness_val * 100.0f));
        int target_p = static_cast<int>(std::round(p.value * 100.0f));
        bool is_sel = (std::abs(cur_p - target_p) <= 5);

        auto btn = ButtonBuilder::create()
            ->text(p.label)
            ->textSize(config->metrics.caption_size)
            ->primary(is_sel)
            ->style(is_sel ? ButtonStyle::Primary : ButtonStyle::Standard)
            ->cornerRadius(get_btn_radius(config))
            ->padding(8, 6)
            ->onClick([this, item, p, target_p]() {
                item->brightness_val = p.value;
                DisplayBackend::set_brightness_percent(target_p);
                DisplayBackend::sync_item_state(item);
                deferred_rebuild_preview();
                notify_state_changed();
            })
            ->build();

        LayoutParams btn_lp;
        btn_lp.width = 0;
        btn_lp.height = 32;
        btn_lp.weight = 1.0f;
        presets_row->add_view(btn, btn_lp);
    }
    presets_card->add_view(presets_row);
    add_preview_card(presets_card);

    // 3. Backlight & Panel Specifications (Telemetry from hardware backend)
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, item->telemetry_title.empty() ? "Backlight & panel specifications" : item->telemetry_title));
    }
    return;
}

void MillerView::render_night_light_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (DisplayBackend::is_available()) {
        DisplayBackend::sync_nightlight_state(item);
    }

    // 1. Hero switch card (Enable or Disable)
    bool is_nl_on = item->nightlight_cfg.enabled;
    auto hero_sw_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto nl_icon = TextViewBuilder::create()
        ->text("󰃟")
        ->textSize(config->metrics.font_size + 4)
        ->textColor(is_nl_on ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams nlicon_lp;
    nlicon_lp.width = 30;
    nlicon_lp.gravity = Gravity::CenterVertical;
    hero_sw_card->add_view(nl_icon, nlicon_lp);

    auto nl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto nl_title = TextViewBuilder::create()
        ->text("Night Light")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    nl_box->add_view(nl_title);

    auto nl_hint = TextViewBuilder::create()
        ->text(is_nl_on ? "Screen tinted amber to reduce blue light" : "Disabled • Normal display color temperature")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    nl_box->add_view(nl_hint);

    LayoutParams nlbox_lp;
    nlbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    nlbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    nlbox_lp.weight = 1.0f;
    nlbox_lp.gravity = Gravity::CenterVertical;
    hero_sw_card->add_view(nl_box, nlbox_lp);

    auto nl_switch = SwitchBuilder::create()
        ->checked(is_nl_on)
        ->onCheckedChanged([this, item](bool checked) {
            item->nightlight_cfg.enabled = checked;
            int k = DisplayBackend::kelvin_from_warmth(item->nightlight_cfg.warmth);
            DisplayBackend::set_night_light_enabled(checked, k);
            DisplayBackend::sync_nightlight_state(item);
            deferred_rebuild_preview();
            notify_state_changed();
        })
        ->build();

    LayoutParams nlsw_lp;
    nlsw_lp.width = 44;
    nlsw_lp.height = 24;
    nlsw_lp.gravity = Gravity::CenterVertical;
    hero_sw_card->add_view(nl_switch, nlsw_lp);

    add_preview_card(hero_sw_card);

    // 2. If enabled, adjust warmth card
    if (is_nl_on) {
        auto warmth_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 14)
            ->spacing(10)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(get_card_radius(config))
            ->build();

        auto w_header = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto w_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto w_title = TextViewBuilder::create()
            ->text("Color Warmth")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        w_box->add_view(w_title);

        auto w_hint = TextViewBuilder::create()
            ->text("Warmer colors appear amber and soft on the eyes")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        w_box->add_view(w_hint);

        LayoutParams wbox_lp;
        wbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        wbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        wbox_lp.weight = 1.0f;
        wbox_lp.gravity = Gravity::CenterVertical;
        w_header->add_view(w_box, wbox_lp);

        auto format_kelvin = [](float warmth) -> std::string {
            int k = DisplayBackend::kelvin_from_warmth(warmth);
            return std::to_string(k) + " K";
        };

        auto w_label = TextViewBuilder::create()
            ->text(format_kelvin(item->nightlight_cfg.warmth))
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams wlbl_lp;
        wlbl_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        wlbl_lp.gravity = Gravity::CenterVertical;
        w_header->add_view(w_label, wlbl_lp);

        warmth_card->add_view(w_header);

        auto w_slider = SliderBuilder::create()
            ->value(item->nightlight_cfg.warmth)
            ->onValueChanged([this, item, w_label, format_kelvin](float val, bool user) {
                (void)user;
                item->nightlight_cfg.warmth = val;
                w_label->set_text(format_kelvin(val));
                w_label->request_redraw();
                if (item->nightlight_cfg.enabled) {
                    DisplayBackend::set_gamma_temperature(DisplayBackend::kelvin_from_warmth(val));
                }
                DisplayBackend::sync_nightlight_state(item);
                notify_state_changed();
            })
            ->build();

        LayoutParams wsl_lp;
        wsl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        wsl_lp.height = 24;
        warmth_card->add_view(w_slider, wsl_lp);

        add_preview_card(warmth_card);
    }

    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, item->telemetry_title.empty() ? "Gamma & color specifications" : item->telemetry_title));
    }
    return;
}

static std::shared_ptr<View> create_battery_master_card(const std::shared_ptr<SettingItem>& item,
                                                        const std::shared_ptr<Config>& config) {
    auto hero_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    std::string icon_glyph = BatteryBackend::get_battery_icon(*item->battery_cfg);
    auto bat_icon = TextViewBuilder::create()
        ->text(icon_glyph)
        ->textSize(config->metrics.font_size + 4)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams bicon_lp;
    bicon_lp.width = 30;
    bicon_lp.gravity = Gravity::CenterVertical;
    hero_card->add_view(bat_icon, bicon_lp);

    auto lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    std::string title_str;
    if (!item->battery_cfg->has_battery) {
        title_str = "AC Power • Running on Mains";
    } else {
        std::string pct = std::to_string(item->battery_cfg->percentage) + "%";
        if (item->battery_cfg->is_charging) {
            title_str = pct + " • Plugged in, Charging";
        } else if (item->battery_cfg->status == "Full") {
            title_str = pct + " • Fully Charged";
        } else if (item->battery_cfg->power_saver) {
            title_str = pct + " • Power Saver Active";
        } else {
            title_str = pct + " • Discharging";
        }
    }

    std::string subtitle_str = item->battery_cfg->time_remaining_str;
    if (subtitle_str.empty()) {
        subtitle_str = item->battery_cfg->is_charging ? "Calculating time until full..." : "Calculating remaining runtime...";
    }

    auto bat_title = TextViewBuilder::create()
        ->text(title_str)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    lbl_box->add_view(bat_title);

    auto bat_hint = TextViewBuilder::create()
        ->text(subtitle_str)
        ->caption(true)
        ->textColor(item->battery_cfg->power_saver ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    lbl_box->add_view(bat_hint);

    LayoutParams lbl_lp;
    lbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    lbl_lp.weight = 1.0f;
    lbl_lp.gravity = Gravity::CenterVertical;
    hero_card->add_view(lbl_box, lbl_lp);

    if (item->battery_cfg->has_battery) {
        auto pct_pill = TextViewBuilder::create()
            ->text(std::to_string(item->battery_cfg->percentage) + "%")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->padding(8, 4)
            ->build();

        auto pill_card = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(config->colors.primary_container.with_alpha(0.5f))
            ->cornerRadius(get_btn_radius(config))
            ->elevation(0)
            ->addView(pct_pill)
            ->build();

        LayoutParams pill_lp;
        pill_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.gravity = Gravity::CenterVertical;
        hero_card->add_view(pill_card, pill_lp);
    }

    return hero_card;
}

void MillerView::render_battery_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item->battery_cfg) return;

    // 0. Live refresh from hardware sysfs
    BatteryBackend::query_battery_info(*item->battery_cfg);
    if (item->battery_cfg->has_battery) {
        item->subtitle = std::to_string(item->battery_cfg->percentage) + "% • " + item->battery_cfg->status;
    } else {
        item->subtitle = "AC Power • Running on Mains";
    }
    item->icon = BatteryBackend::get_battery_icon(*item->battery_cfg);

    // 1. Battery Master Status Card
    add_preview_card(create_battery_master_card(item, config));

    // 2. Battery Specifications Telemetry Card
    BatteryBackend::populate_battery_telemetry(*item->battery_cfg, item->telemetry);
    add_preview_card(create_telemetry_card(item->telemetry, config, "Battery specifications"));
    return;
}

void MillerView::render_power_mode_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item->battery_cfg) return;

    // Refresh active profile and battery state
    BatteryBackend::query_battery_info(*item->battery_cfg);
    item->battery_cfg->power_profile = BatteryBackend::get_power_profile();
    item->battery_cfg->power_saver = (item->battery_cfg->power_profile == "Power Saver");
    item->subtitle = item->battery_cfg->power_profile;

    // 1. Operating Profile Card with Spinner
    auto prof_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto prof_title = TextViewBuilder::create()
        ->text("Operating profile")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    prof_card->add_view(prof_title);

    auto prof_lbl = TextViewBuilder::create()
        ->text("Mode / Profile")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    prof_card->add_view(prof_lbl);

    int prof_sel_idx = 1; // Default "Balanced"
    for (size_t i = 0; i < item->battery_cfg->available_profiles.size(); ++i) {
        if (item->battery_cfg->available_profiles[i] == item->battery_cfg->power_profile) {
            prof_sel_idx = static_cast<int>(i);
            break;
        }
    }

    auto get_profile_desc = [item]() -> std::string {
        if (item->battery_cfg->power_profile == "Power Saver") {
            return "Limits CPU frequency to base clock, pauses background indexing, and forces 60 Hz display refresh.";
        } else if (item->battery_cfg->power_profile == "Performance") {
            return "Unlocks full CPU turbo frequencies and max GPU performance; increases power consumption.";
        }
        return "Standard dynamic CPU scaling balancing responsive performance with optimal battery endurance.";
    };

    auto prof_desc = TextViewBuilder::create()
        ->text(get_profile_desc())
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();

    auto prof_spinner = SpinnerBuilder::create()
        ->items(item->battery_cfg->available_profiles)
        ->selectedIndex(prof_sel_idx)
        ->fontSize(config->metrics.font_size)
        ->padding(12, 6)
        ->cornerRadius(get_inner_radius(config))
        ->onItemSelected([this, item, prof_desc, get_profile_desc](int idx, const std::string& val) {
            (void)idx;
            item->battery_cfg->power_profile = val;
            item->battery_cfg->power_saver = (val == "Power Saver");
            item->subtitle = val;
            BatteryBackend::set_power_profile_async(val);

            if (prof_desc) {
                prof_desc->set_text(get_profile_desc());
                prof_desc->request_redraw();
            }
            rebuild_current_column();
            notify_state_changed();

            // Safely defer full preview rebuild until after the dropdown popup has cleanly dismissed
            if (auto* eng = AppEngine::instance()) {
                eng->post_delayed(50, [this]() {
                    rebuild_preview_column();
                    notify_state_changed();
                });
            }
        })
        ->build();

    LayoutParams psp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    prof_card->add_view(prof_spinner, psp_lp);
    prof_card->add_view(prof_desc);

    add_preview_card(prof_card);

    // 2. Power Management Specifications Telemetry Card
    BatteryBackend::populate_power_telemetry(*item->battery_cfg, item->telemetry);
    add_preview_card(create_telemetry_card(item->telemetry, config, "Power management specifications"));
    return;
}

void MillerView::render_monitors_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Monitor Banner & Primary Display Status Card
    auto mon_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto mon_icon = TextViewBuilder::create()
        ->text("󰍹")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams micon_lp;
    micon_lp.width = 24;
    micon_lp.gravity = Gravity::CenterVertical;
    mon_card->add_view(mon_icon, micon_lp);

    auto mon_lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto mon_title = TextViewBuilder::create()
        ->text(item->monitor_cfg.model_name)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    mon_lbl_box->add_view(mon_title);

    std::string mon_sub = item->monitor_cfg.connector + " • " + item->monitor_cfg.current_resolution + " @ " + item->monitor_cfg.current_refresh_rate;
    auto mon_hint = TextViewBuilder::create()
        ->text(mon_sub)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    mon_lbl_box->add_view(mon_hint);

    LayoutParams mlbl_lp;
    mlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    mlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    mlbl_lp.weight = 1.0f;
    mlbl_lp.gravity = Gravity::CenterVertical;
    mon_card->add_view(mon_lbl_box, mlbl_lp);

    if (item->monitor_cfg.is_primary) {
        auto prim_badge = TextViewBuilder::create()
            ->text("Primary Display ✓")
            ->caption(true)
            ->textColor(config->colors.primary)
            ->padding(8, 4)
            ->build();

        auto pill_card = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(config->colors.primary_container.with_alpha(0.5f))
            ->cornerRadius(get_btn_radius(config))
            ->elevation(0)
            ->addView(prim_badge)
            ->build();

        LayoutParams pill_lp;
        pill_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.gravity = Gravity::CenterVertical;
        mon_card->add_view(pill_card, pill_lp);
    } else {
        auto on_make_primary = [this, item]() {
            MonitorBackend::set_primary(item->monitor_cfg.connector);
            const auto& list = get_current_list();
            for (auto& m : list) {
                if (m->is_monitor) {
                    m->monitor_cfg.is_primary = (m == item);
                }
            }
            rebuild_current_column();
            rebuild_preview_column();
            notify_state_changed();
        };

        auto make_prim_btn = ButtonBuilder::create()
            ->text("Make Primary")
            ->primary(true)
            ->padding(10, 4)
            ->cornerRadius(get_btn_radius(config))
            ->onClick(on_make_primary)
            ->build();

        LayoutParams prim_lp;
        prim_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        prim_lp.height = 28;
        prim_lp.gravity = Gravity::CenterVertical;
        mon_card->add_view(make_prim_btn, prim_lp);
    }

    add_preview_card(mon_card);

    // 2. Interactive Display Settings Card (Resolution, Refresh Rate, Scale, Orientation as Spinners)
    auto cfg_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto cfg_title = TextViewBuilder::create()
        ->text("Display configuration")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    cfg_card->add_view(cfg_title);

    auto update_mon_sub = [this, item, mon_hint]() {
        std::string sub = item->monitor_cfg.connector + " • " + item->monitor_cfg.current_resolution + " @ " + item->monitor_cfg.current_refresh_rate;
        if (item->monitor_cfg.is_primary) sub += " • Primary";
        if (mon_hint) {
            mon_hint->set_text(sub);
            mon_hint->request_redraw();
        }
        rebuild_current_column();
    };

    auto create_spinner_row = [&](const std::string& row_title,
                                  const std::vector<std::string>& options,
                                  const std::string& current_val,
                                  std::function<void(const std::string&)> on_select) {
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->padding(6, 4)
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto lbl = TextViewBuilder::create()
            ->text(row_title)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams lbl_lp;
        lbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        lbl_lp.weight = 1.0f;
        lbl_lp.gravity = Gravity::CenterVertical;
        row->add_view(lbl, lbl_lp);

        int sel_idx = 0;
        for (size_t i = 0; i < options.size(); ++i) {
            if (options[i] == current_val) {
                sel_idx = static_cast<int>(i);
                break;
            }
        }

        auto spinner = SpinnerBuilder::create()
            ->items(options)
            ->selectedIndex(sel_idx)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(get_inner_radius(config))
            ->onItemSelected([options, on_select](int idx, const std::string& val) {
                (void)val;
                if (idx >= 0 && idx < static_cast<int>(options.size())) {
                    on_select(options[idx]);
                }
            })
            ->build();

        LayoutParams sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        sp_lp.gravity = Gravity::CenterVertical;
        row->add_view(spinner, sp_lp);

        return row;
    };

    // Resolution Spinner
    cfg_card->add_view(create_spinner_row("Resolution", item->monitor_cfg.available_resolutions, item->monitor_cfg.current_resolution, [this, item, update_mon_sub](const std::string& val) {
        item->monitor_cfg.current_resolution = val;
        auto [w, h] = MonitorBackend::parse_resolution(val);
        auto new_rates = MonitorBackend::get_available_refresh_rates(item->monitor_cfg.connector, w, h);
        if (!new_rates.empty()) {
            item->monitor_cfg.available_refresh_rates = new_rates;
            bool found = false;
            for (const auto& r : new_rates) {
                if (r == item->monitor_cfg.current_refresh_rate) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                item->monitor_cfg.current_refresh_rate = new_rates.front();
            }
        }
        float hz = MonitorBackend::parse_refresh_rate(item->monitor_cfg.current_refresh_rate);
        MonitorBackend::set_mode(item->monitor_cfg.connector, w, h, hz);
        update_mon_sub();
        rebuild_preview_column();
    }));

    // Refresh Rate Spinner
    cfg_card->add_view(create_spinner_row("Refresh Rate", item->monitor_cfg.available_refresh_rates, item->monitor_cfg.current_refresh_rate, [item, update_mon_sub](const std::string& val) {
        item->monitor_cfg.current_refresh_rate = val;
        auto [w, h] = MonitorBackend::parse_resolution(item->monitor_cfg.current_resolution);
        float hz = MonitorBackend::parse_refresh_rate(val);
        MonitorBackend::set_mode(item->monitor_cfg.connector, w, h, hz);
        update_mon_sub();
    }));

    // Display Scaling Spinner
    cfg_card->add_view(create_spinner_row("Display Scaling", item->monitor_cfg.available_scales, item->monitor_cfg.current_scale, [this, item](const std::string& val) {
        item->monitor_cfg.current_scale = val;
        float sc = MonitorBackend::parse_scale(val);
        MonitorBackend::set_scale(item->monitor_cfg.connector, sc);
        rebuild_current_column();
    }));

    // Orientation Spinner
    cfg_card->add_view(create_spinner_row("Orientation", item->monitor_cfg.available_orientations, item->monitor_cfg.current_orientation, [this, item](const std::string& val) {
        item->monitor_cfg.current_orientation = val;
        std::string tr = MonitorBackend::ui_to_transform(val);
        MonitorBackend::set_transform(item->monitor_cfg.connector, tr);
        rebuild_current_column();
    }));

    add_preview_card(cfg_card);

    // 3. Hardware & Panel Telemetry
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "Display panel specifications"));
    }

    return;
}


} // namespace miqu
