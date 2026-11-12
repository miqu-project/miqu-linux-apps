#include "miller_view.hpp"
#include "preview_common.hpp"
#include "input_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace miqu {

namespace {

constexpr int MATCH_PARENT = static_cast<int>(LayoutDimension::MatchParent);
constexpr int WRAP_CONTENT = static_cast<int>(LayoutDimension::WrapContent);

std::shared_ptr<View> create_input_hero_card(
    const std::string& icon_str,
    const std::string& title_str,
    const std::string& subtitle_str,
    const std::string& badge_str,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(12)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto icon = TextViewBuilder::create()
        ->text(icon_str)
        ->textSize(static_cast<int>(config->metrics.font_size * 2.2f))
        ->textColor(config->colors.primary)
        ->build();
    row->add_view(icon, LayoutParams(42, WRAP_CONTENT, Gravity::CenterVertical));

    auto col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(3)
        ->build();

    auto title = TextViewBuilder::create()
        ->text(title_str)
        ->textSize(static_cast<int>(config->metrics.font_size * 1.25f))
        ->bold(true)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text(subtitle_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    col->add_view(subtitle);

    row->add_view(col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
    card->add_view(row);

    if (!badge_str.empty()) {
        auto badge = TextViewBuilder::create()
            ->text(badge_str)
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        card->add_view(badge);
    }

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_keyboards_list_card(
    const std::vector<KeyboardDevice>& keyboards,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Connected Keyboards (" + std::to_string(keyboards.size()) + ")")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    for (const auto& kb : keyboards) {
        auto row_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(10)
            ->gravity(Gravity::CenterVertical)
            ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto kb_icon = TextViewBuilder::create()
            ->text(kb.is_primary ? "󰌌" : "󰌏")
            ->textSize(config->metrics.font_size + 2)
            ->textColor(kb.is_primary ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        row_card->add_view(kb_icon, LayoutParams(24, WRAP_CONTENT, Gravity::CenterVertical));

        auto text_col = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto name_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(8)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto name = TextViewBuilder::create()
            ->text(kb.name)
            ->textSize(config->metrics.font_size)
            ->bold(kb.is_primary)
            ->textColor(config->colors.on_surface)
            ->build();
        name_row->add_view(name);

        if (kb.is_primary) {
            auto badge = TextViewBuilder::create()
                ->text("Primary")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            name_row->add_view(badge);
        }
        text_col->add_view(name_row);

        auto details = TextViewBuilder::create()
            ->text("Layout: " + kb.layout + "  •  Repeat: " + std::to_string(kb.repeat_rate) + "/s (" + std::to_string(kb.repeat_delay) + "ms)")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        text_col->add_view(details);

        row_card->add_view(text_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
        card->add_view(row_card);
    }

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_layout_selector_card(
    const std::string& active_layout,
    std::function<void(const std::string&)> on_select,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Quick Layout Selection")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Switch default XKB keymap layout across compositor & applications:")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(subtitle);

    auto btn_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::CenterVertical)
        ->build();

    struct LayoutOption { std::string code; std::string label; };
    std::vector<LayoutOption> layouts = {
        {"us", "US (ANSI)"},
        {"gb", "UK (ISO)"},
        {"de", "DE (German)"},
        {"fr", "FR (French)"},
        {"es", "ES (Spanish)"},
        {"ru", "RU (Russian)"},
        {"jp", "JP (Japanese)"}
    };

    for (const auto& l : layouts) {
        bool is_cur = (l.code == active_layout);
        auto btn = ButtonBuilder::create()
            ->text(l.code)
            ->style(is_cur ? ButtonStyle::Primary : ButtonStyle::Standard)
            ->onClick([on_select, code = l.code]() {
                if (on_select) on_select(code);
            })
            ->build();
        btn_row->add_view(btn, LayoutParams(WRAP_CONTENT, 30));
    }

    card->add_view(btn_row);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_typetest_sandbox_card(
    const std::string& title_str,
    const std::string& hint_str,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(8)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text(title_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text(hint_str)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    auto search_box = SearchViewBuilder::create()
        ->hint("Click here and type to test keystrokes...")
        ->focused(false)
        ->focusBorder(false)
        ->padding(10, 8)
        ->elevation(0)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.4f))
        ->build();
    search_box->set_focused(false);
    if (auto et = search_box->get_edit_text()) {
        et->set_focused(false);
    }
    card->add_view(search_box, LayoutParams(MATCH_PARENT, 36));

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_repeat_delay_card(
    int delay_ms,
    std::function<void(int)> on_delay_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Repeat Delay (Sensitivity)")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    header->add_view(title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    auto val_label = TextViewBuilder::create()
        ->text(std::to_string(delay_ms) + " ms")
        ->bold(true)
        ->textColor(config->colors.primary)
        ->build();
    header->add_view(val_label, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    card->add_view(header);

    auto desc = TextViewBuilder::create()
        ->text("Time before character repetition begins while holding a key down:")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    // Slider range 150ms to 1000ms
    float norm = (delay_ms - 150.0f) / (1000.0f - 150.0f);
    auto slider = SliderBuilder::create()
        ->value(std::clamp(norm, 0.0f, 1.0f))
        ->onValueChanged([val_label, on_delay_changed](float val, bool user) {
            (void)user;
            int ms = static_cast<int>(std::round(150.0f + val * 850.0f));
            val_label->set_text(std::to_string(ms) + " ms");
            val_label->request_redraw();
            if (on_delay_changed) on_delay_changed(ms);
        })
        ->build();
    card->add_view(slider, LayoutParams(MATCH_PARENT, 26));

    // Preset buttons
    auto presets = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();

    struct Preset { std::string label; int ms; };
    std::vector<Preset> list = {
        {"Short (250ms)", 250},
        {"Default (600ms)", 600},
        {"Long (800ms)", 800}
    };

    for (const auto& p : list) {
        auto btn = ButtonBuilder::create()
            ->text(p.label)
            ->style(ButtonStyle::Standard)
            ->onClick([on_delay_changed, ms = p.ms]() {
                if (on_delay_changed) on_delay_changed(ms);
            })
            ->build();
        presets->add_view(btn, LayoutParams(WRAP_CONTENT, 28));
    }
    card->add_view(presets);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_repeat_rate_card(
    int rate_cps,
    std::function<void(int)> on_rate_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Repeat Rate (Speed)")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    header->add_view(title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    auto val_label = TextViewBuilder::create()
        ->text(std::to_string(rate_cps) + " char/s")
        ->bold(true)
        ->textColor(config->colors.primary)
        ->build();
    header->add_view(val_label, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    card->add_view(header);

    auto desc = TextViewBuilder::create()
        ->text("Characters repeated per second while continuously holding a key down:")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    card->add_view(desc);

    // Slider range 10 to 50 char/s
    float norm = (rate_cps - 10.0f) / (50.0f - 10.0f);
    auto slider = SliderBuilder::create()
        ->value(std::clamp(norm, 0.0f, 1.0f))
        ->onValueChanged([val_label, on_rate_changed](float val, bool user) {
            (void)user;
            int r = static_cast<int>(std::round(10.0f + val * 40.0f));
            val_label->set_text(std::to_string(r) + " char/s");
            val_label->request_redraw();
            if (on_rate_changed) on_rate_changed(r);
        })
        ->build();
    card->add_view(slider, LayoutParams(MATCH_PARENT, 26));

    // Preset buttons
    auto presets = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();

    struct Preset { std::string label; int r; };
    std::vector<Preset> list = {
        {"Slow (15/s)", 15},
        {"Balanced (25/s)", 25},
        {"Fast (40/s)", 40}
    };

    for (const auto& p : list) {
        auto btn = ButtonBuilder::create()
            ->text(p.label)
            ->style(ButtonStyle::Standard)
            ->onClick([on_rate_changed, r = p.r]() {
                if (on_rate_changed) on_rate_changed(r);
            })
            ->build();
        presets->add_view(btn, LayoutParams(WRAP_CONTENT, 28));
    }
    card->add_view(presets);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_touchpad_ergonomics_card(
    bool tap_to_click,
    bool natural_scroll,
    bool disable_while_typing,
    std::function<void(bool, bool, bool)> on_update,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Touchpad Ergonomics")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto add_toggle_row = [&](const std::string& name, const std::string& desc_text, bool cur_val, auto cb) {
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(10)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto col = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto lbl = TextViewBuilder::create()
            ->text(name)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        col->add_view(lbl);

        auto d = TextViewBuilder::create()
            ->text(desc_text)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        col->add_view(d);

        row->add_view(col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

        auto sw = SwitchBuilder::create()
            ->checked(cur_val)
            ->onCheckedChanged([cb](bool chk) {
                cb(chk);
            })
            ->build();
        row->add_view(sw, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        card->add_view(row);
    };

    add_toggle_row(
        "Natural Scrolling",
        "Content moves smoothly in the direction of finger motion",
        natural_scroll,
        [tap_to_click, disable_while_typing, on_update](bool chk) {
            if (on_update) on_update(tap_to_click, chk, disable_while_typing);
        }
    );

    add_toggle_row(
        "Tap-to-Click",
        "Tap surface with 1 finger for left-click, 2 for right-click",
        tap_to_click,
        [natural_scroll, disable_while_typing, on_update](bool chk) {
            if (on_update) on_update(chk, natural_scroll, disable_while_typing);
        }
    );

    add_toggle_row(
        "Disable While Typing",
        "Ignore accidental touchpad palm touches during keyboard typing",
        disable_while_typing,
        [tap_to_click, natural_scroll, on_update](bool chk) {
            if (on_update) on_update(tap_to_click, natural_scroll, chk);
        }
    );

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_pointer_speed_card(
    float speed_val,
    const std::string& accel_profile,
    std::function<void(float)> on_speed_changed,
    std::function<void(const std::string&)> on_profile_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Pointer Speed & Acceleration")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    header->add_view(title, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    int pct = static_cast<int>(std::round(speed_val * 100.0f));
    auto val_label = TextViewBuilder::create()
        ->text(std::to_string(pct) + "%")
        ->bold(true)
        ->textColor(config->colors.primary)
        ->build();
    header->add_view(val_label, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    card->add_view(header);

    auto slider = SliderBuilder::create()
        ->value(std::clamp(speed_val, 0.0f, 1.0f))
        ->onValueChanged([val_label, on_speed_changed](float val, bool user) {
            (void)user;
            int p = static_cast<int>(std::round(val * 100.0f));
            val_label->set_text(std::to_string(p) + "%");
            val_label->request_redraw();
            if (on_speed_changed) on_speed_changed(val);
        })
        ->build();
    card->add_view(slider, LayoutParams(MATCH_PARENT, 26));

    // Acceleration Profile
    auto prof_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto prof_lbl = TextViewBuilder::create()
        ->text("Acceleration Profile:")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    prof_row->add_view(prof_lbl);

    bool is_adap = (accel_profile == "adaptive");
    auto btn_adap = ButtonBuilder::create()
        ->text("Adaptive (Dynamic)")
        ->style(is_adap ? ButtonStyle::Primary : ButtonStyle::Standard)
        ->onClick([on_profile_changed]() {
            if (on_profile_changed) on_profile_changed("adaptive");
        })
        ->build();
    prof_row->add_view(btn_adap, LayoutParams(WRAP_CONTENT, 28));

    auto btn_flat = ButtonBuilder::create()
        ->text("Flat (1:1 Raw)")
        ->style(!is_adap ? ButtonStyle::Primary : ButtonStyle::Standard)
        ->onClick([on_profile_changed]() {
            if (on_profile_changed) on_profile_changed("flat");
        })
        ->build();
    prof_row->add_view(btn_flat, LayoutParams(WRAP_CONTENT, 28));

    card->add_view(prof_row);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_pointers_list_card(
    const std::vector<PointerDevice>& pointers,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Detected Pointing Devices (" + std::to_string(pointers.size()) + ")")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    for (const auto& ptr : pointers) {
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(10)
            ->gravity(Gravity::CenterVertical)
            ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
            ->cornerRadius(get_inner_radius(config))
            ->build();

        std::string icon_str = ptr.is_touchpad ? "󰟸" : (ptr.is_trackpoint ? "󰍽" : "󰍽");
        auto icon = TextViewBuilder::create()
            ->text(icon_str)
            ->textSize(config->metrics.font_size + 2)
            ->textColor(config->colors.primary)
            ->build();
        row->add_view(icon, LayoutParams(24, WRAP_CONTENT, Gravity::CenterVertical));

        auto text_col = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto name = TextViewBuilder::create()
            ->text(ptr.name)
            ->textSize(config->metrics.font_size)
            ->bold(true)
            ->textColor(config->colors.on_surface)
            ->build();
        text_col->add_view(name);

        auto type_lbl = TextViewBuilder::create()
            ->text(ptr.display_type)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        text_col->add_view(type_lbl);

        row->add_view(text_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
        card->add_view(row);
    }

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_touch_devices_card(
    const std::vector<TouchDevice>& touch_devices,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(10)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Detected Touchscreen Digitizers (" + std::to_string(touch_devices.size()) + ")")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    if (touch_devices.empty()) {
        auto empty = TextViewBuilder::create()
            ->text("No physical touchscreen digitizers detected on this seat")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        card->add_view(empty);
    } else {
        for (const auto& tch : touch_devices) {
            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(10)
                ->gravity(Gravity::CenterVertical)
                ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
                ->cornerRadius(get_inner_radius(config))
                ->build();

            auto icon = TextViewBuilder::create()
                ->text("󰟸")
                ->textSize(config->metrics.font_size + 2)
                ->textColor(config->colors.primary)
                ->build();
            row->add_view(icon, LayoutParams(24, WRAP_CONTENT, Gravity::CenterVertical));

            auto col = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto name = TextViewBuilder::create()
                ->text(tch.name)
                ->textSize(config->metrics.font_size)
                ->bold(true)
                ->textColor(config->colors.on_surface)
                ->build();
            col->add_view(name);

            auto sub = TextViewBuilder::create()
                ->text("Capacitive Multi-Touch  •  Associated Display: " + tch.mapped_output)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            col->add_view(sub);

            row->add_view(col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
            card->add_view(row);
        }
    }

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

std::shared_ptr<View> create_gestures_card(
    bool swipe_enabled,
    float multiplier,
    std::function<void(bool, float)> on_gesture_changed,
    const std::shared_ptr<Config>& config
) {
    auto card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();

    auto title = TextViewBuilder::create()
        ->text("Desktop Navigation Gestures")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    card->add_view(title);

    auto add_gesture_row = [&](const std::string& gesture, const std::string& action) {
        auto r = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(4, 2)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto g_lbl = TextViewBuilder::create()
            ->text(gesture)
            ->textSize(config->metrics.font_size - 1)
            ->bold(true)
            ->textColor(config->colors.primary)
            ->build();
        r->add_view(g_lbl, LayoutParams(190, WRAP_CONTENT));

        auto a_lbl = TextViewBuilder::create()
            ->text(action)
            ->caption(true)
            ->textColor(config->colors.on_surface)
            ->build();
        r->add_view(a_lbl, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));
        card->add_view(r);
    };

    add_gesture_row("3-Finger Horizontal Swipe", "Switch active workspace with smooth kinematics physics");
    add_gesture_row("4-Finger Vertical Swipe Up", "Live Application Overview & Live Window Expose");
    add_gesture_row("4-Finger Vertical Swipe Down", "Lock Desktop Session (miqulock)");

    auto sw_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(10)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto sw_col = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto sw_title = TextViewBuilder::create()
        ->text("Interactive Workspace Swipe")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    sw_col->add_view(sw_title);

    auto sw_desc = TextViewBuilder::create()
        ->text("Follow finger position continuously during workspace transitions")
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    sw_col->add_view(sw_desc);

    sw_row->add_view(sw_col, LayoutParams(MATCH_PARENT, WRAP_CONTENT, 1.0f));

    auto sw = SwitchBuilder::create()
        ->checked(swipe_enabled)
        ->onCheckedChanged([multiplier, on_gesture_changed](bool chk) {
            if (on_gesture_changed) on_gesture_changed(chk, multiplier);
        })
        ->build();
    sw_row->add_view(sw, LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
    card->add_view(sw_row);

    LayoutParams card_lp;
    card_lp.width = MATCH_PARENT;
    card_lp.height = WRAP_CONTENT;
    card->set_layout_params(card_lp);
    return card;
}

} // namespace

void MillerView::render_keyboards_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    size_t count = state.keyboards.size();
    std::string sub = std::to_string(count) + " hardware keyboard" + (count == 1 ? "" : "s") + " detected · wlroots libinput Introspection ✓";
    std::string badge = "Active Keymap: English (US) [" + state.active_layout + "]";

    add_preview_card(create_input_hero_card("󰌌", "Keyboards & Keymap", sub, badge, config));
    add_preview_card(create_keyboards_list_card(state.keyboards, config));

    add_preview_card(create_layout_selector_card(
        state.active_layout,
        [this](const std::string& l) {
            InputBackend::set_keyboard_layout(l);
            rebuild_preview_column();
        },
        config
    ));

    add_preview_card(create_typetest_sandbox_card(
        "Typing Test Sandbox",
        "Click the test box below and type to test layout mapping and keystrokes live:",
        config
    ));

    std::vector<TelemetryEntry> telem = {
        {"Keyboards Detected", std::to_string(count)},
        {"Driver Layer", "libinput (Kernel evdev)"},
        {"Wayland Protocol", "wl_keyboard / text-input-v3"},
        {"Keymap Engine", "libxkbcommon v1.7.0"},
        {"Active XKB Keymap", state.active_layout + " (ANSI pc105+inet)"},
        {"Modifier Mapping", "Caps, Ctrl, Super, Alt, AltGr, NumLock"}
    };
    add_preview_card(create_telemetry_card(telem, config, "Hardware keyboard specifications"));
}

void MillerView::render_repeat_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    std::string sub = "Repeat Delay: " + std::to_string(state.repeat_delay) + " ms  •  Repeat Rate: " + std::to_string(state.repeat_rate) + " char/s";
    add_preview_card(create_input_hero_card("󰁯", "Typing & Repeat Speed", sub, "libinput Key Repeat Solver ✓", config));

    add_preview_card(create_repeat_delay_card(
        state.repeat_delay,
        [this, state](int d) {
            InputBackend::set_repeat_info(state.repeat_rate, d);
        },
        config
    ));

    add_preview_card(create_repeat_rate_card(
        state.repeat_rate,
        [this, state](int r) {
            InputBackend::set_repeat_info(r, state.repeat_delay);
        },
        config
    ));

    add_preview_card(create_typetest_sandbox_card(
        "Key Repeat Sandbox",
        "Hold down any key inside the test field below to test repeat delay and speed:",
        config
    ));

    std::vector<TelemetryEntry> telem = {
        {"Repeat Rate", std::to_string(state.repeat_rate) + " characters/second"},
        {"Repeat Delay", std::to_string(state.repeat_delay) + " milliseconds"},
        {"Kinematics Engine", "wl_keyboard repeat_info"},
        {"Precision Timer", "Linux timerfd / event-loop polling"},
        {"Auto-Repeat Filter", "POSIX compliant / XKB rate model"}
    };
    add_preview_card(create_telemetry_card(telem, config, "Typing & repeat specifications"));
}

void MillerView::render_pointers_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    size_t count = state.pointers.size();
    std::string sub = std::to_string(count) + " pointing device" + (count == 1 ? "" : "s") + " detected · libinput acceleration active";
    add_preview_card(create_input_hero_card("󰍽", "Pointers & Touchpad", sub, "Precision Touchpad & Pointer Introspection ✓", config));

    add_preview_card(create_touchpad_ergonomics_card(
        state.tap_to_click,
        state.natural_scroll,
        state.disable_while_typing,
        [this](bool tap, bool nat, bool dwt) {
            InputBackend::set_touchpad_options(tap, nat, dwt);
            rebuild_preview_column();
        },
        config
    ));

    add_preview_card(create_pointer_speed_card(
        state.pointer_speed,
        state.accel_profile,
        [](float spd) {
            InputBackend::set_pointer_speed(spd);
        },
        [this](const std::string& prof) {
            InputBackend::set_accel_profile(prof);
            rebuild_preview_column();
        },
        config
    ));

    add_preview_card(create_pointers_list_card(state.pointers, config));

    std::vector<TelemetryEntry> telem = {
        {"Pointing Devices", std::to_string(count)},
        {"Driver Subsystem", "libinput (Kernel evdev)"},
        {"Touchpad Tap-to-Click", state.tap_to_click ? "Active (1/2/3 fingers)" : "Disabled"},
        {"Natural Scrolling", state.natural_scroll ? "Natural (Content follows finger)" : "Traditional"},
        {"Palm Rejection", state.disable_while_typing ? "Enabled (Disable while typing)" : "Disabled"},
        {"Acceleration Profile", state.accel_profile == "adaptive" ? "Adaptive (Dynamic curve)" : "Flat (1:1)"},
        {"Polling Frequency", "1000 Hz (High Precision)"}
    };
    add_preview_card(create_telemetry_card(telem, config, "Pointing device specifications"));
}

void MillerView::render_touch_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    (void)item;
    auto state = InputBackend::get_state();

    size_t count = state.touch_devices.size();
    std::string sub = std::to_string(count) + " touch digitizer" + (count == 1 ? "" : "s") + " active · Wayland touch protocol";
    add_preview_card(create_input_hero_card("󰟸", "Touchscreen & Gestures", sub, "Hardware Multi-Touch Introspection ✓", config));

    add_preview_card(create_touch_devices_card(state.touch_devices, config));

    add_preview_card(create_gestures_card(
        state.workspace_swipe,
        state.gesture_multiplier,
        [this](bool en, float mul) {
            InputBackend::set_workspace_swipe(en, mul);
            rebuild_preview_column();
        },
        config
    ));

    std::vector<TelemetryEntry> telem = {
        {"Digitizers Detected", std::to_string(count)},
        {"Touch Protocol", "wl_touch / libinput direct"},
        {"Multi-Touch Points", "10 simultaneous touch points"},
        {"Output Association", !state.touch_devices.empty() ? state.touch_devices[0].mapped_output : "None"},
        {"Gesture Engine", "miquland Internal Kinematics Solver"},
        {"Edge Swipe Resistance", "0.15 threshold"}
    };
    add_preview_card(create_telemetry_card(telem, config, "Digitizer & gesture specifications"));
}

} // namespace miqu
