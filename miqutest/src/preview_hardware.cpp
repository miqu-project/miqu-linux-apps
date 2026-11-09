#include "miller_view.hpp"
#include "preview_common.hpp"
#include "display_backend.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {
void MillerView::render_sound_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    bool is_speaker = (item->id == "sound_speaker");

    // 1. Seekbar Card (Volume for Speaker, Gain for Mic)
    auto seekbar_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto s_header_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->build();

    std::string seekbar_title = is_speaker ? "Seekbar (Volume)" : "Gain";
    auto s_title_view = TextViewBuilder::create()
        ->text(item->icon + "  " + seekbar_title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams st_lp;
    st_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    st_lp.weight = 1.0f;
    s_header_row->add_view(s_title_view, st_lp);

    int val_pct = static_cast<int>(item->slider_val * 100);
    m_active_slider_label = TextViewBuilder::create()
        ->text(std::to_string(val_pct) + "%")
        ->textColor(config->colors.primary)
        ->build();
    s_header_row->add_view(m_active_slider_label);
    seekbar_card->add_view(s_header_row);

    m_active_slider = SliderBuilder::create()
        ->value(item->slider_val)
        ->onValueChanged([this, item](float val, bool user) {
            item->slider_val = val;
            if (m_active_slider_label) {
                int pct = static_cast<int>(val * 100);
                m_active_slider_label->set_text(std::to_string(pct) + "%");
            }
            rebuild_current_column();
            notify_state_changed();
        })
        ->build();

    LayoutParams slider_lp;
    slider_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    slider_lp.height = 24;
    seekbar_card->add_view(m_active_slider, slider_lp);
    add_preview_card(seekbar_card);

    // 2. Mute Card (Toggle Switch)
    auto mute_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto mute_lbl_box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto mute_title = TextViewBuilder::create()
        ->text("Mute")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    mute_lbl_box->add_view(mute_title);

    std::string mute_hint_str = is_speaker ? "Mute speaker audio output" : "Mute microphone input";
    auto mute_hint = TextViewBuilder::create()
        ->text(mute_hint_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    mute_lbl_box->add_view(mute_hint);

    LayoutParams mlbl_lp;
    mlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    mlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    mlbl_lp.weight = 1.0f;
    mlbl_lp.gravity = Gravity::CenterVertical;
    mute_card->add_view(mute_lbl_box, mlbl_lp);

    auto primary_sw = SwitchBuilder::create()
        ->checked(item->switch_val)
        ->onCheckedChanged([this, item](bool checked) {
            item->switch_val = checked;
            rebuild_current_column();
            deferred_rebuild_preview();
            notify_state_changed();
        })
        ->build();

    LayoutParams sw_lp;
    sw_lp.width = 44;
    sw_lp.height = 24;
    sw_lp.gravity = Gravity::CenterVertical;
    mute_card->add_view(primary_sw, sw_lp);
    add_preview_card(mute_card);

    // 3. Audio Device Specifications
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "Audio device specifications"));
    }

    return;
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

    // 2. Quick Brightness Presets Card (like miqusettings)
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
            int k = static_cast<int>(std::round(6500.0f - warmth * 3800.0f));
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

void MillerView::render_battery_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item->battery_cfg) return;

    // 1. Battery Master Status Card (Percentage, State, Remaining Time)
    auto hero_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto get_battery_icon = [item]() -> std::string {
        if (item->battery_cfg->is_charging) return "󰂄";
        if (item->battery_cfg->power_saver) return "󰂃";
        int p = item->battery_cfg->percentage;
        if (p >= 90) return "󰁹";
        if (p >= 70) return "󰂀";
        if (p >= 50) return "󰁾";
        if (p >= 30) return "󰁼";
        if (p >= 15) return "󰁺";
        return "󰂎";
    };

    auto bat_icon = TextViewBuilder::create()
        ->text(get_battery_icon())
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

    auto get_title_str = [item]() -> std::string {
        std::string pct = std::to_string(item->battery_cfg->percentage) + "%";
        if (item->battery_cfg->is_charging) {
            return pct + " • Plugged in, Charging";
        }
        if (item->battery_cfg->power_saver) {
            return pct + " • Power Saver Active";
        }
        return pct + " • Discharging";
    };

    auto get_subtitle_str = [item]() -> std::string {
        if (item->battery_cfg->is_charging) {
            return "1 hour 12 minutes until fully charged (65W USB-PD)";
        }
        if (item->battery_cfg->power_saver) {
            return "Approximately 9 hours 30 minutes remaining (+2h 45m extended)";
        }
        if (item->battery_cfg->power_profile == "Performance") {
            return "Approximately 4 hours 15 minutes remaining (Turbo mode active)";
        }
        return "Approximately 6 hours 45 minutes remaining";
    };

    auto bat_title = TextViewBuilder::create()
        ->text(get_title_str())
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    lbl_box->add_view(bat_title);

    auto bat_hint = TextViewBuilder::create()
        ->text(get_subtitle_str())
        ->caption(true)
        ->textColor(item->battery_cfg->power_saver ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    lbl_box->add_view(bat_hint);

    LayoutParams lbl_lp;
    lbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl_lp.weight = 1.0f;
    lbl_lp.gravity = Gravity::CenterVertical;
    hero_card->add_view(lbl_box, lbl_lp);

    add_preview_card(hero_card);

    // 2. Battery Specifications Telemetry Card
    std::string active_draw = item->battery_cfg->power_saver ? "5.2 W (Power Saver Active)" :
                             (item->battery_cfg->power_profile == "Performance" ? "14.2 W (Turbo Mode)" : "7.8 W (Estimated)");
    std::vector<TelemetryEntry> bat_telemetry = {
        {"Battery Health", "98.4% of design capacity"},
        {"Cycle Count", "142 cycles"},
        {"Full Charge Capacity", "59.0 Wh (Design: 60.0 Wh)"},
        {"Power Source", item->battery_cfg->is_charging ? "AC Adapter (65W USB-PD)" : "Battery (Discharging)"},
        {"Discharge Rate", active_draw},
        {"Cell Temperature", "31.4 °C"},
        {"Technology", "Lithium-Polymer (LiPo)"}
    };
    add_preview_card(create_telemetry_card(bat_telemetry, config, "Battery specifications"));
    return;
}

void MillerView::render_power_mode_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    if (!item->battery_cfg) return;

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
            if (prof_desc) {
                prof_desc->set_text(get_profile_desc());
                prof_desc->request_redraw();
            }
            rebuild_current_column();
            notify_state_changed();

            // Safely defer full preview rebuild until after the dropdown popup has cleanly dismissed
            if (auto* eng = AppEngine::instance()) {
                eng->post_delayed(20, [this]() {
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

    // 3. Power Management Specifications Telemetry Card
    std::string active_draw = item->battery_cfg->power_saver ? "5.2 W (Power Saver Active)" :
                             (item->battery_cfg->power_profile == "Performance" ? "14.2 W (Turbo Mode)" : "7.8 W (Estimated)");
    std::string gov = item->battery_cfg->power_saver ? "powersave (amd-pstate-epp)" :
                     (item->battery_cfg->power_profile == "Performance" ? "performance (amd-pstate-epp)" : "powersave (amd-pstate-epp)");
    std::string epp = item->battery_cfg->power_saver ? "power" :
                     (item->battery_cfg->power_profile == "Performance" ? "performance" : "balance_power");
    std::string platform_prof = item->battery_cfg->power_saver ? "low-power (ACPI platform profile)" :
                               (item->battery_cfg->power_profile == "Performance" ? "performance (ACPI platform profile)" : "balanced (ACPI platform profile)");

    std::vector<TelemetryEntry> power_telemetry = {
        {"Current Governor", gov},
        {"Energy Performance Preference", epp},
        {"Platform Profile", platform_prof},
        {"Active Power Draw", active_draw},
        {"Package Temp", "42.5 °C"}
    };
    add_preview_card(create_telemetry_card(power_telemetry, config, "Power management specifications"));
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
    cfg_card->add_view(create_spinner_row("Resolution", item->monitor_cfg.available_resolutions, item->monitor_cfg.current_resolution, [item, update_mon_sub](const std::string& val) {
        item->monitor_cfg.current_resolution = val;
        update_mon_sub();
    }));

    // Refresh Rate Spinner
    cfg_card->add_view(create_spinner_row("Refresh Rate", item->monitor_cfg.available_refresh_rates, item->monitor_cfg.current_refresh_rate, [item, update_mon_sub](const std::string& val) {
        item->monitor_cfg.current_refresh_rate = val;
        update_mon_sub();
    }));

    // Display Scaling Spinner
    cfg_card->add_view(create_spinner_row("Display Scaling", item->monitor_cfg.available_scales, item->monitor_cfg.current_scale, [this, item](const std::string& val) {
        item->monitor_cfg.current_scale = val;
        rebuild_current_column();
    }));

    // Orientation Spinner
    cfg_card->add_view(create_spinner_row("Orientation", item->monitor_cfg.available_orientations, item->monitor_cfg.current_orientation, [this, item](const std::string& val) {
        item->monitor_cfg.current_orientation = val;
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
