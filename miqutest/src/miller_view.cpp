#include "miller_view.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {

namespace {
std::shared_ptr<View> create_telemetry_card(const std::vector<TelemetryEntry>& telemetry,
                                            const std::shared_ptr<Config>& config,
                                            const std::string& card_title = "Technical specifications") {
    auto telem_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(8)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(12)
        ->build();

    auto telem_title = TextViewBuilder::create()
        ->text(card_title.empty() ? "Technical specifications" : card_title)
        ->caption(true)
        ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
        ->build();
    telem_card->add_view(telem_title);

    for (const auto& entry : telemetry) {
        auto row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(2, 2)
            ->build();

        auto k = TextViewBuilder::create()
            ->text(entry.key)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        LayoutParams k_lp;
        k_lp.width = 140;
        row->add_view(k, k_lp);

        auto v = TextViewBuilder::create()
            ->text(entry.value)
            ->caption(true)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams v_lp;
        v_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        v_lp.weight = 1.0f;
        row->add_view(v, v_lp);

        telem_card->add_view(row);
    }
    LayoutParams card_lp;
    card_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    card_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    telem_card->set_layout_params(card_lp);
    return telem_card;
}
} // namespace

MillerView::MillerView() : LinearLayout(Orientation::Horizontal) {}

MillerView::~MillerView() {
    cancel_pending_sibling_click();
}

void MillerView::cancel_pending_sibling_click() {
    if (m_single_click_timer_id != 0) {
        if (auto* eng = AppEngine::instance()) {
            eng->cancel_delayed(m_single_click_timer_id);
        }
        m_single_click_timer_id = 0;
    }
}

void MillerView::execute_sibling_swap(const std::shared_ptr<SettingItem>& item, size_t index) {
    cancel_pending_sibling_click();
    m_last_sibling_clicked_index = -1;
    m_last_sibling_click_time_ms = 0;

    if (item->is_category) {
        if (!m_path.empty()) {
            m_path.back() = item;
        } else {
            m_path.push_back(item);
        }
        if (!m_history_indices.empty()) {
            m_history_indices.back() = static_cast<int>(index);
        }
        m_current_index = 0;
        m_preview_focused = false;
        rebuild_columns();
        notify_state_changed();
    } else {
        execute_shift_up(index);
    }
}

void MillerView::execute_shift_up(size_t index) {
    cancel_pending_sibling_click();
    m_last_sibling_clicked_index = -1;
    m_last_sibling_click_time_ms = 0;

    if (!m_path.empty()) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_history_indices.pop_back();
        }
    }
    m_current_index = static_cast<int>(index);
    m_preview_focused = false;
    rebuild_columns();
    notify_state_changed();
}

void MillerView::init() {
    m_root_tree = SettingsModel::build_default_tree();

    set_orientation(Orientation::Horizontal);
    set_divider_spacing(0);
    set_background_color(Color::transparent());

    // 1. Column 1: Parent (Backgroundless LinearLayout)
    m_parent_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(12, 12)
        ->backgroundColor(Color::transparent())
        ->build();

    LayoutParams parent_lp;
    parent_lp.width = 320;
    parent_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    add_view(m_parent_layout, parent_lp);

    // 2. Column 2: Current (Backgroundless LinearLayout)
    m_current_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(12, 12)
        ->backgroundColor(Color::transparent())
        ->build();

    LayoutParams current_lp;
    current_lp.width = 320;
    current_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    add_view(m_current_layout, current_lp);

    // 3. Column 3: Preview (Backgroundless ScrollView / LinearLayout)
    m_preview_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(10)
        ->padding(16, 16)
        ->backgroundColor(Color::transparent())
        ->build();

    m_preview_scroll = ScrollViewBuilder::create()
        ->contentView(m_preview_layout)
        ->scrollbar(true)
        ->build();

    LayoutParams preview_lp;
    preview_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    preview_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    preview_lp.weight = 1.0f;
    add_view(m_preview_scroll, preview_lp);

    sync_theme();
    rebuild_columns();
}

void MillerView::sync_theme() {
    request_redraw();
}

bool MillerView::on_key(const KeyPressEvent& event) {
    (void)event;
    // In MillerView, keyboard navigation is managed strictly and authoritatively
    // through MillerView's navigation methods (navigate_up, navigate_down, etc.).
    // We consume key events here to prevent un-focused child view hierarchies
    // (such as preview column spinners and controls) from receiving accidental
    // fallthrough key dispatches from Window::dispatch_key_event.
    return true;
}

const std::vector<std::shared_ptr<SettingItem>>& MillerView::get_current_list() const {
    if (m_path.empty()) {
        return m_root_tree;
    }
    return m_path.back()->children;
}

std::shared_ptr<SettingItem> MillerView::get_current_selected_item() const {
    const auto& list = get_current_list();
    if (list.empty()) return nullptr;
    int idx = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);
    return list[idx];
}

void MillerView::notify_state_changed() {
    if (m_on_state_changed) {
        m_on_state_changed();
    }
    request_redraw();
}

void MillerView::rebuild_columns() {
    rebuild_parent_column();
    rebuild_current_column();
    rebuild_preview_column();
    request_redraw();
}

void MillerView::rebuild_parent_column() {
    m_parent_layout->clear_views();
    auto config = Config::get();

    if (m_path.empty()) {
        return;
    }

    m_parent_layout->set_gravity(Gravity::None);

    // Depth > 0: Show parent's siblings directly without top navigation header
    const std::vector<std::shared_ptr<SettingItem>>* parent_siblings = nullptr;
    if (m_path.size() == 1) {
        parent_siblings = &m_root_tree;
    } else {
        parent_siblings = &(m_path[m_path.size() - 2]->children);
    }

    auto active_parent = m_path.back();
    for (size_t i = 0; i < parent_siblings->size(); ++i) {
        const auto& item = (*parent_siblings)[i];
        bool is_active = (item == active_parent);

        auto row_label = TextViewBuilder::create()
            ->text(item->icon + " " + item->title)
            ->textSize(config->metrics.font_size)
            ->textColor(is_active ? config->colors.primary : config->colors.on_surface_variant)
            ->padding(10, 8)
            ->build();

        Color bg = is_active ? config->colors.primary_container.with_alpha(0.35f) : Color::transparent();
        auto row_card = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(bg)
            ->cornerRadius(6)
            ->elevation(0)
            ->addView(row_label)
            ->build();

        // Hybrid Interaction: Single-click shifts up one level (Option A)
        // Double-click directly swaps active branch at current depth (Option B)
        row_card->set_on_click_listener([this, item, i]() {
            auto now = std::chrono::steady_clock::now();
            uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            if (m_last_sibling_clicked_index == static_cast<int>(i) &&
                (now_ms - m_last_sibling_click_time_ms) <= 280) {
                // Double Click: cancel pending single-click and swap sibling branch
                cancel_pending_sibling_click();
                execute_sibling_swap(item, i);
            } else {
                // First Click: schedule single-click shift up after debounce
                cancel_pending_sibling_click();
                m_last_sibling_clicked_index = static_cast<int>(i);
                m_last_sibling_click_time_ms = now_ms;

                if (auto* eng = AppEngine::instance()) {
                    m_single_click_timer_id = eng->post_delayed(220, [this, i]() {
                        m_single_click_timer_id = 0;
                        m_last_sibling_clicked_index = -1;
                        m_last_sibling_click_time_ms = 0;
                        execute_shift_up(i);
                    });
                } else {
                    execute_shift_up(i);
                }
            }
        });

        m_parent_layout->add_view(row_card);
    }
}

void MillerView::rebuild_current_column() {
    m_current_layout->clear_views();
    auto config = Config::get();

    const auto& list = get_current_list();
    if (list.empty()) return;

    m_current_index = std::clamp(m_current_index, 0, static_cast<int>(list.size()) - 1);

    // List items
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& item = list[i];
        bool is_selected = (static_cast<int>(i) == m_current_index);

        auto item_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(10)
            ->build();

        // Left Icon
        auto icon_view = TextViewBuilder::create()
            ->text(item->icon.empty() ? "⚙" : item->icon)
            ->textSize(config->metrics.font_size)
            ->textColor(is_selected ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        LayoutParams icon_lp;
        icon_lp.width = 24;
        icon_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        icon_lp.gravity = Gravity::CenterVertical;
        item_row->add_view(icon_view, icon_lp);

        // Text container (Title and optional contextual Subtitle)
        auto text_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(1)
            ->build();

        auto title_view = TextViewBuilder::create()
            ->text(item->title)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->ellipsize(true)
            ->build();
        text_box->add_view(title_view);

        if (!item->subtitle.empty()) {
            auto sub_view = TextViewBuilder::create()
                ->text(item->subtitle)
                ->caption(true)
                ->textColor(is_selected ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
                ->ellipsize(true)
                ->build();
            text_box->add_view(sub_view);
        }

        LayoutParams text_lp;
        text_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        text_lp.weight = 1.0f;
        text_lp.gravity = Gravity::CenterVertical;
        item_row->add_view(text_box, text_lp);

        // Trailing indicator: Only show '›' for expandable categories.
        // No right-end indicators (no ON/OFF, no slider values) in navigation columns
        // to eliminate costly backend queries before landing on proper preview.
        if (item->is_category) {
            auto trail_view = TextViewBuilder::create()
                ->text("›")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
                ->build();
            LayoutParams trail_lp;
            trail_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            trail_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            trail_lp.gravity = Gravity::CenterVertical;
            item_row->add_view(trail_view, trail_lp);
        }

        // Wrap in CardView for selection styling
        Color bg = is_selected ? config->colors.primary_container.with_alpha(0.65f) : Color::transparent();
        int stroke_w = is_selected ? 1 : 0;
        Color stroke_col = is_selected ? config->colors.primary.with_alpha(0.7f) : Color::transparent();

        auto row_card = CardViewBuilder::create()
            ->style(is_selected ? CardStyle::Outlined : CardStyle::Filled)
            ->backgroundColor(bg)
            ->stroke(stroke_w, stroke_col)
            ->cornerRadius(8)
            ->elevation(0)
            ->addView(item_row)
            ->build();

        // Click handler
        row_card->set_on_click_listener([this, i, item]() {
            if (static_cast<int>(i) != m_current_index) {
                m_current_index = static_cast<int>(i);
                m_preview_focused = false;
                rebuild_columns();
                notify_state_changed();
            } else if (item->is_category) {
                navigate_enter();
            } else {
                m_preview_focused = !m_preview_focused;
                rebuild_preview_column();
                notify_state_changed();
            }
        });

        m_current_layout->add_view(row_card);
    }
}

void MillerView::add_preview_card(std::shared_ptr<View> card) {
    if (!card) return;
    LayoutParams lp = card->get_layout_params();
    lp.width = static_cast<int>(LayoutDimension::MatchParent);
    if (lp.height <= 0 && lp.height != static_cast<int>(LayoutDimension::MatchParent)) {
        lp.height = static_cast<int>(LayoutDimension::WrapContent);
    }
    m_preview_layout->add_view(std::move(card), lp);
}

void MillerView::rebuild_preview_column() {
    m_preview_layout->clear_views();
    m_active_primary_switch = nullptr;
    m_active_secondary_switch = nullptr;
    m_active_slider = nullptr;
    m_active_slider_label = nullptr;
    m_active_action_btn = nullptr;
    m_active_action_feedback = nullptr;

    auto config = Config::get();
    auto item = get_current_selected_item();

    if (!item) {
        auto placeholder = TextViewBuilder::create()
            ->text("No item selected")
            ->center(true)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        add_preview_card(placeholder);
        return;
    }

    if (item->is_category) {
        // --- DIRECT CONTENTS LIST (GENUINE MILLER CHILD COLUMN) ---
        m_preview_layout->set_padding(Padding(12, 12));
        m_preview_layout->set_divider_spacing(4);

        // Child item list matching column style
        for (size_t i = 0; i < item->children.size(); ++i) {
            const auto& child = item->children[i];

            auto item_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(10)
                ->build();

            // Icon
            auto icon_view = TextViewBuilder::create()
                ->text(child->icon.empty() ? "⚙" : child->icon)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams icon_lp;
            icon_lp.width = 24;
            icon_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            icon_lp.gravity = Gravity::CenterVertical;
            item_row->add_view(icon_view, icon_lp);

            // Text container (Title and optional contextual Subtitle)
            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(1)
                ->build();

            auto title_view = TextViewBuilder::create()
                ->text(child->title)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->ellipsize(true)
                ->build();
            text_box->add_view(title_view);

            if (!child->subtitle.empty()) {
                auto sub_view = TextViewBuilder::create()
                    ->text(child->subtitle)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                    ->ellipsize(true)
                    ->build();
                text_box->add_view(sub_view);
            }

            LayoutParams text_lp;
            text_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            text_lp.weight = 1.0f;
            text_lp.gravity = Gravity::CenterVertical;
            item_row->add_view(text_box, text_lp);

            // Trailing indicator: Only show '›' for expandable categories.
            // No right-end indicators (no ON/OFF, no slider values) in navigation columns.
            if (child->is_category) {
                auto trail_view = TextViewBuilder::create()
                    ->text("›")
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
                    ->build();
                LayoutParams trail_lp;
                trail_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                trail_lp.height = static_cast<int>(LayoutDimension::WrapContent);
                trail_lp.gravity = Gravity::CenterVertical;
                item_row->add_view(trail_view, trail_lp);
            }

            auto row_card = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(Color::transparent())
                ->cornerRadius(8)
                ->elevation(0)
                ->addView(item_row)
                ->build();

            // Clicking child directly enters category and selects child
            row_card->set_on_click_listener([this, item, i]() {
                m_path.push_back(item);
                m_history_indices.push_back(m_current_index);
                m_current_index = static_cast<int>(i);
                m_preview_focused = false;
                rebuild_columns();
                notify_state_changed();
            });

            add_preview_card(row_card);
        }
        return;
    }

    // --- DEAD END PREVIEW: INTERACTIVE SETTINGS DETAIL PANEL ---
    m_preview_layout->set_padding(Padding(14, 14));
    m_preview_layout->set_divider_spacing(10);

    // Dedicated Clean Layout for Speaker / Mic: ONLY Seekbar, Mute, and Device Specification
    if (item->id == "sound_speaker" || item->id == "sound_mic") {
        bool is_speaker = (item->id == "sound_speaker");

        // 1. Seekbar Card (Volume for Speaker, Gain for Mic)
        bool slider_focused = m_preview_focused && (m_preview_control_index == 0);
        auto seekbar_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 14)
            ->spacing(10)
            ->backgroundColor(config->colors.surface)
            ->stroke(slider_focused ? 2 : 1, slider_focused ? config->colors.primary : config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
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
        bool mute_focused = m_preview_focused && (m_preview_control_index == 1);
        auto mute_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(mute_focused ? 2 : 1, mute_focused ? config->colors.primary : config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
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

        m_active_primary_switch = SwitchBuilder::create()
            ->checked(item->switch_val)
            ->onCheckedChanged([this, item](bool checked) {
                item->switch_val = checked;
                rebuild_current_column();
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();

        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        mute_card->add_view(m_active_primary_switch, sw_lp);
        add_preview_card(mute_card);

        // 3. Audio Device Specifications
        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, "Audio device specifications"));
        }

        return;
    }

    // Dedicated Clean Layout for Wi-Fi: Switch (like Speaker mute), Networks list, Scan button, Telemetry
    if (item->id == "net_wifi") {
        // 1. Wi-Fi Power Toggle Card (Just like mute button of speaker)
        bool switch_focused = m_preview_focused && (m_preview_control_index == 0);
        auto wifi_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(switch_focused ? 2 : 1, switch_focused ? config->colors.primary : config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto wifi_icon = TextViewBuilder::create()
            ->text("󰤨")
            ->textSize(config->metrics.font_size + 4)
            ->textColor(item->switch_val ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        LayoutParams wicon_lp;
        wicon_lp.width = 30;
        wicon_lp.gravity = Gravity::CenterVertical;
        wifi_card->add_view(wifi_icon, wicon_lp);

        auto wifi_lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto wifi_title = TextViewBuilder::create()
            ->text("Wi-Fi")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        wifi_lbl_box->add_view(wifi_title);

        std::string wifi_hint_str = item->switch_val ? "Connected to Miqu-Fiber-5G" : "Wi-Fi is turned off";
        auto wifi_hint = TextViewBuilder::create()
            ->text(wifi_hint_str)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        wifi_lbl_box->add_view(wifi_hint);

        LayoutParams wlbl_lp;
        wlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        wlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        wlbl_lp.weight = 1.0f;
        wlbl_lp.gravity = Gravity::CenterVertical;
        wifi_card->add_view(wifi_lbl_box, wlbl_lp);

        m_active_primary_switch = SwitchBuilder::create()
            ->checked(item->switch_val)
            ->onCheckedChanged([this, item](bool checked) {
                item->switch_val = checked;
                rebuild_current_column();
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();

        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        wifi_card->add_view(m_active_primary_switch, sw_lp);
        add_preview_card(wifi_card);

        // 2. Networks Section (Visible when Wi-Fi is enabled)
        if (item->switch_val) {
            auto net_card = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(8)
                ->padding(16, 14)
                ->backgroundColor(config->colors.surface)
                ->stroke(1, config->colors.outline.with_alpha(0.3f))
                ->cornerRadius(12)
                ->build();

            // Header row with "Networks" title and "Scan" button
            auto net_header_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto net_title = TextViewBuilder::create()
                ->text("Networks")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams nt_lp;
            nt_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            nt_lp.weight = 1.0f;
            nt_lp.gravity = Gravity::CenterVertical;
            net_header_row->add_view(net_title, nt_lp);

            auto scan_btn = ButtonBuilder::create()
                ->text("Scan")
                ->flat(true)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->padding(10, 4)
                ->cornerRadius(6)
                ->onClick([this, item]() {
                    item->last_action_message = "Scan completed • 5 networks found";
                    rebuild_preview_column();
                    notify_state_changed();
                })
                ->build();
            LayoutParams scan_lp;
            scan_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            scan_lp.height = 28;
            scan_lp.gravity = Gravity::CenterVertical;
            net_header_row->add_view(scan_btn, scan_lp);
            net_card->add_view(net_header_row);

            if (!item->last_action_message.empty()) {
                auto scan_status = TextViewBuilder::create()
                    ->text(item->last_action_message)
                    ->caption(true)
                    ->textColor(config->colors.primary)
                    ->build();
                net_card->add_view(scan_status);
            }

            // Networks List
            for (size_t ni = 0; ni < item->wifi_networks.size(); ++ni) {
                auto& net = item->wifi_networks[ni];
                bool is_conn = (net.status == "Connected");

                auto row_content = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Horizontal)
                    ->padding(10, 8)
                    ->spacing(12)
                    ->build();

                // Signal icon
                auto sig_view = TextViewBuilder::create()
                    ->text(net.icon)
                    ->textSize(config->metrics.font_size)
                    ->textColor(is_conn ? config->colors.primary : config->colors.on_surface_variant)
                    ->build();
                LayoutParams sig_lp;
                sig_lp.width = 24;
                sig_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(sig_view, sig_lp);

                // SSID and Status subtitle
                auto text_box = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Vertical)
                    ->spacing(2)
                    ->build();

                auto ssid_text = TextViewBuilder::create()
                    ->text(net.ssid)
                    ->textSize(config->metrics.font_size)
                    ->textColor(is_conn ? config->colors.primary : config->colors.on_surface)
                    ->ellipsize(true)
                    ->build();
                text_box->add_view(ssid_text);

                Color sub_color = is_conn ? config->colors.primary :
                                  (net.status == "Saved" ? config->colors.on_surface_variant : config->colors.on_surface_variant.with_alpha(0.7f));
                auto status_text = TextViewBuilder::create()
                    ->text(net.status)
                    ->caption(true)
                    ->textColor(sub_color)
                    ->build();
                text_box->add_view(status_text);

                LayoutParams tb_lp;
                tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                tb_lp.weight = 1.0f;
                tb_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(text_box, tb_lp);

                // Trailing indicator: checkmark for connected, lock icon for secured
                if (is_conn) {
                    auto check_view = TextViewBuilder::create()
                        ->text("✓")
                        ->textSize(config->metrics.font_size)
                        ->textColor(config->colors.primary)
                        ->build();
                    LayoutParams chk_lp;
                    chk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                    chk_lp.gravity = Gravity::CenterVertical;
                    row_content->add_view(check_view, chk_lp);
                } else if (net.secured) {
                    auto lock_view = TextViewBuilder::create()
                        ->text("󰌾")
                        ->textSize(config->metrics.font_size)
                        ->textColor(config->colors.on_surface_variant.with_alpha(0.6f))
                        ->build();
                    LayoutParams lk_lp;
                    lk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                    lk_lp.gravity = Gravity::CenterVertical;
                    row_content->add_view(lock_view, lk_lp);
                }

                Color row_bg = is_conn ? config->colors.primary_container.with_alpha(0.35f) : Color::transparent();
                auto net_row_card = CardViewBuilder::create()
                    ->style(CardStyle::Filled)
                    ->backgroundColor(row_bg)
                    ->cornerRadius(8)
                    ->elevation(0)
                    ->addView(row_content)
                    ->build();

                // Clicking network: switches connection to this network
                net_row_card->set_on_click_listener([this, item, ni]() {
                    for (size_t k = 0; k < item->wifi_networks.size(); ++k) {
                        if (k == ni) {
                            item->wifi_networks[k].status = "Connected";
                        } else if (item->wifi_networks[k].status == "Connected") {
                            item->wifi_networks[k].status = "Saved";
                        }
                    }
                    rebuild_preview_column();
                    notify_state_changed();
                });

                net_card->add_view(net_row_card);
            }

            add_preview_card(net_card);
        }

        // 3. Interface Specifications
        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, "Wireless interface specifications"));
        }

        return;
    }

    // Dedicated Clean Layout for Bluetooth: Switch (like Wi-Fi), Devices list, Scan button, Telemetry
    if (item->id == "net_bluetooth") {
        // 1. Bluetooth Power Toggle Card
        bool switch_focused = m_preview_focused && (m_preview_control_index == 0);
        auto bt_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(switch_focused ? 2 : 1, switch_focused ? config->colors.primary : config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto bt_icon = TextViewBuilder::create()
            ->text("󰂯")
            ->textSize(config->metrics.font_size + 4)
            ->textColor(item->switch_val ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        LayoutParams bicon_lp;
        bicon_lp.width = 30;
        bicon_lp.gravity = Gravity::CenterVertical;
        bt_card->add_view(bt_icon, bicon_lp);

        auto bt_lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto bt_title = TextViewBuilder::create()
            ->text("Bluetooth")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        bt_lbl_box->add_view(bt_title);

        std::string connected_device_name;
        for (const auto& dev : item->bt_devices) {
            if (dev.status == "Connected") {
                connected_device_name = dev.name;
                break;
            }
        }

        std::string bt_hint_str;
        if (!item->switch_val) {
            bt_hint_str = "Bluetooth is turned off";
        } else if (!connected_device_name.empty()) {
            bt_hint_str = "Connected to " + connected_device_name;
        } else {
            bt_hint_str = "Discoverable as Miqu-Desktop";
        }

        auto bt_hint = TextViewBuilder::create()
            ->text(bt_hint_str)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        bt_lbl_box->add_view(bt_hint);

        LayoutParams blbl_lp;
        blbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        blbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        blbl_lp.weight = 1.0f;
        blbl_lp.gravity = Gravity::CenterVertical;
        bt_card->add_view(bt_lbl_box, blbl_lp);

        m_active_primary_switch = SwitchBuilder::create()
            ->checked(item->switch_val)
            ->onCheckedChanged([this, item](bool checked) {
                item->switch_val = checked;
                rebuild_current_column();
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();

        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        bt_card->add_view(m_active_primary_switch, sw_lp);
        add_preview_card(bt_card);

        // 2. Devices Section (Visible when Bluetooth is enabled)
        if (item->switch_val) {
            auto dev_card = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(8)
                ->padding(16, 14)
                ->backgroundColor(config->colors.surface)
                ->stroke(1, config->colors.outline.with_alpha(0.3f))
                ->cornerRadius(12)
                ->build();

            // Header row with "Devices" title and "Scan" button
            auto dev_header_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto dev_title = TextViewBuilder::create()
                ->text("Devices")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams dt_lp;
            dt_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            dt_lp.weight = 1.0f;
            dt_lp.gravity = Gravity::CenterVertical;
            dev_header_row->add_view(dev_title, dt_lp);

            auto scan_btn = ButtonBuilder::create()
                ->text("Scan")
                ->flat(true)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->padding(10, 4)
                ->cornerRadius(6)
                ->onClick([this, item]() {
                    bool found_new = false;
                    bool already_has_bose = false;
                    for (const auto& d : item->bt_devices) {
                        if (d.name == "Bose QuietComfort 45") {
                            already_has_bose = true;
                            break;
                        }
                    }
                    if (!already_has_bose) {
                        item->bt_devices.push_back({"Bose QuietComfort 45", "Tap to pair", "󰋋", ""});
                        item->last_action_message = "Scan completed • Found Bose QuietComfort 45";
                        found_new = true;
                    } else {
                        bool already_has_mx = false;
                        for (const auto& d : item->bt_devices) {
                            if (d.name == "MX Anywhere 3S") {
                                already_has_mx = true;
                                break;
                            }
                        }
                        if (!already_has_mx) {
                            item->bt_devices.push_back({"MX Anywhere 3S", "Tap to pair", "󰍽", ""});
                            item->last_action_message = "Scan completed • Found MX Anywhere 3S";
                            found_new = true;
                        }
                    }
                    if (!found_new) {
                        item->last_action_message = "Scan completed • All nearby devices discovered";
                    }
                    rebuild_preview_column();
                    notify_state_changed();
                })
                ->build();
            LayoutParams scan_lp;
            scan_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            scan_lp.height = 28;
            scan_lp.gravity = Gravity::CenterVertical;
            dev_header_row->add_view(scan_btn, scan_lp);
            dev_card->add_view(dev_header_row);

            if (!item->last_action_message.empty()) {
                auto scan_status = TextViewBuilder::create()
                    ->text(item->last_action_message)
                    ->caption(true)
                    ->textColor(config->colors.primary)
                    ->build();
                dev_card->add_view(scan_status);
            }

            // Devices List
            for (size_t di = 0; di < item->bt_devices.size(); ++di) {
                auto& dev = item->bt_devices[di];
                bool is_conn = (dev.status == "Connected");

                auto row_content = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Horizontal)
                    ->padding(10, 8)
                    ->spacing(12)
                    ->build();

                // Device icon
                auto icon_view = TextViewBuilder::create()
                    ->text(dev.icon.empty() ? "󰂯" : dev.icon)
                    ->textSize(config->metrics.font_size)
                    ->textColor(is_conn ? config->colors.primary : config->colors.on_surface_variant)
                    ->build();
                LayoutParams icon_lp;
                icon_lp.width = 24;
                icon_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(icon_view, icon_lp);

                // Name and Status subtitle
                auto text_box = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Vertical)
                    ->spacing(2)
                    ->build();

                auto name_text = TextViewBuilder::create()
                    ->text(dev.name)
                    ->textSize(config->metrics.font_size)
                    ->textColor(is_conn ? config->colors.primary : config->colors.on_surface)
                    ->ellipsize(true)
                    ->build();
                text_box->add_view(name_text);

                std::string status_sub = dev.status;
                if (is_conn && !dev.battery.empty()) {
                    status_sub = "Connected • " + dev.battery;
                }

                Color sub_color = is_conn ? config->colors.primary :
                                  (dev.status == "Tap to pair" ? config->colors.primary :
                                  (dev.status == "Paired" ? config->colors.on_surface_variant : config->colors.on_surface_variant.with_alpha(0.7f)));
                auto status_text = TextViewBuilder::create()
                    ->text(status_sub)
                    ->caption(true)
                    ->textColor(sub_color)
                    ->build();
                text_box->add_view(status_text);

                LayoutParams tb_lp;
                tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                tb_lp.weight = 1.0f;
                tb_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(text_box, tb_lp);

                // Trailing indicator: checkmark for connected
                if (is_conn) {
                    auto check_view = TextViewBuilder::create()
                        ->text("✓")
                        ->textSize(config->metrics.font_size)
                        ->textColor(config->colors.primary)
                        ->build();
                    LayoutParams chk_lp;
                    chk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                    chk_lp.gravity = Gravity::CenterVertical;
                    row_content->add_view(check_view, chk_lp);
                }

                Color row_bg = is_conn ? config->colors.primary_container.with_alpha(0.35f) : Color::transparent();
                auto dev_row_card = CardViewBuilder::create()
                    ->style(CardStyle::Filled)
                    ->backgroundColor(row_bg)
                    ->cornerRadius(8)
                    ->elevation(0)
                    ->addView(row_content)
                    ->build();

                // Clicking device: connects it (or pairs & connects if Tap to pair)
                dev_row_card->set_on_click_listener([this, item, di]() {
                    for (size_t k = 0; k < item->bt_devices.size(); ++k) {
                        if (k == di) {
                            item->bt_devices[k].status = "Connected";
                            if (item->bt_devices[k].battery.empty()) {
                                item->bt_devices[k].battery = "85%";
                            }
                        } else if (item->bt_devices[k].status == "Connected") {
                            item->bt_devices[k].status = "Paired";
                        }
                    }
                    rebuild_preview_column();
                    notify_state_changed();
                });

                dev_card->add_view(dev_row_card);
            }

            add_preview_card(dev_card);
        }

        // 3. Controller Specifications
        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, "Bluetooth controller specifications"));
        }

        return;
    }

    // Dedicated Clean Layout for Ethernet: Active Profile, IP/Routing parameters, Telemetry
    if (item->id == "net_ethernet") {
        bool is_conn = (item->eth_profile.status == "Connected");

        // 1. Active Wired Connection Profile & IP Parameters (Visible when connected)
        if (is_conn) {
            auto profile_card = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(10)
                ->padding(16, 14)
                ->backgroundColor(config->colors.surface)
                ->stroke(1, config->colors.outline.with_alpha(0.3f))
                ->cornerRadius(12)
                ->build();

            // Header row with "Wired Connection" title and "Renew DHCP" button
            auto header_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto header_title = TextViewBuilder::create()
                ->text("Wired Connection")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams ht_lp;
            ht_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            ht_lp.weight = 1.0f;
            ht_lp.gravity = Gravity::CenterVertical;
            header_row->add_view(header_title, ht_lp);

            auto renew_btn = ButtonBuilder::create()
                ->text("Renew DHCP")
                ->flat(true)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->padding(10, 4)
                ->cornerRadius(6)
                ->onClick([this, item]() {
                    item->last_action_message = "DHCP lease renewed • IPv4 192.168.1.105";
                    rebuild_preview_column();
                    notify_state_changed();
                })
                ->build();
            LayoutParams renew_lp;
            renew_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            renew_lp.height = 28;
            renew_lp.gravity = Gravity::CenterVertical;
            header_row->add_view(renew_btn, renew_lp);
            profile_card->add_view(header_row);

            if (!item->last_action_message.empty()) {
                auto renew_status = TextViewBuilder::create()
                    ->text(item->last_action_message)
                    ->caption(true)
                    ->textColor(config->colors.primary)
                    ->build();
                profile_card->add_view(renew_status);
            }

            // Active Connection Highlight Card
            auto conn_row_content = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(12)
                ->build();

            auto conn_icon = TextViewBuilder::create()
                ->text("󰈀")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->build();
            LayoutParams cicon_lp;
            cicon_lp.width = 24;
            cicon_lp.gravity = Gravity::CenterVertical;
            conn_row_content->add_view(conn_icon, cicon_lp);

            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto profile_name = TextViewBuilder::create()
                ->text(item->eth_profile.name)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->build();
            text_box->add_view(profile_name);

            auto profile_sub = TextViewBuilder::create()
                ->text(item->eth_profile.mode + " • " + item->eth_profile.status)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            text_box->add_view(profile_sub);

            LayoutParams tb_lp;
            tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            tb_lp.weight = 1.0f;
            tb_lp.gravity = Gravity::CenterVertical;
            conn_row_content->add_view(text_box, tb_lp);

            auto check_view = TextViewBuilder::create()
                ->text("✓")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->build();
            LayoutParams chk_lp;
            chk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            chk_lp.gravity = Gravity::CenterVertical;
            conn_row_content->add_view(check_view, chk_lp);

            auto conn_row_card = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(config->colors.primary_container.with_alpha(0.35f))
                ->cornerRadius(8)
                ->elevation(0)
                ->addView(conn_row_content)
                ->build();
            profile_card->add_view(conn_row_card);

            // IP Parameters Details
            std::vector<std::pair<std::string, std::string>> ip_params = {
                {"IP Address", item->eth_profile.ip_address},
                {"Subnet Mask", item->eth_profile.subnet},
                {"Default Gateway", item->eth_profile.gateway},
                {"DNS Servers", item->eth_profile.dns}
            };

            for (const auto& [k, v] : ip_params) {
                auto row = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Horizontal)
                    ->padding(2, 2)
                    ->build();

                auto k_view = TextViewBuilder::create()
                    ->text(k)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant)
                    ->build();
                LayoutParams k_lp;
                k_lp.width = 140;
                row->add_view(k_view, k_lp);

                auto v_view = TextViewBuilder::create()
                    ->text(v)
                    ->caption(true)
                    ->textColor(config->colors.on_surface)
                    ->build();
                LayoutParams v_lp;
                v_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                v_lp.weight = 1.0f;
                row->add_view(v_view, v_lp);

                profile_card->add_view(row);
            }

            add_preview_card(profile_card);
        }

        // 3. Hardware & Link Telemetry
        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, "Hardware & link specifications"));
        }

        return;
    }

    // Dedicated Clean Layout for Airplane Mode: Master Switch + Bottom Info of affected radios
    if (item->id == "net_airplane") {
        // 1. Airplane Mode Primary Switch Card
        bool switch_focused = m_preview_focused && (m_preview_control_index == 0);
        auto airplane_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(switch_focused ? 2 : 1, switch_focused ? config->colors.primary : config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto plane_icon = TextViewBuilder::create()
            ->text("󰀝")
            ->textSize(config->metrics.font_size + 4)
            ->textColor(item->switch_val ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        LayoutParams picon_lp;
        picon_lp.width = 30;
        picon_lp.gravity = Gravity::CenterVertical;
        airplane_card->add_view(plane_icon, picon_lp);

        auto plane_lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto plane_title = TextViewBuilder::create()
            ->text("Airplane Mode")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        plane_lbl_box->add_view(plane_title);

        std::string plane_sub = item->switch_val ? "All wireless communications disabled" : "Wireless communications enabled";
        auto plane_hint = TextViewBuilder::create()
            ->text(plane_sub)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        plane_lbl_box->add_view(plane_hint);

        LayoutParams plbl_lp;
        plbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        plbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        plbl_lp.weight = 1.0f;
        plbl_lp.gravity = Gravity::CenterVertical;
        airplane_card->add_view(plane_lbl_box, plbl_lp);

        m_active_primary_switch = SwitchBuilder::create()
            ->checked(item->switch_val)
            ->onCheckedChanged([this, item](bool checked) {
                item->switch_val = checked;
                rebuild_current_column();
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();

        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        airplane_card->add_view(m_active_primary_switch, sw_lp);
        add_preview_card(airplane_card);

        // 2. Bottom Info Card: What things are going to be disabled
        auto info_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto info_title = TextViewBuilder::create()
            ->text("Affected Wireless Radios")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        info_card->add_view(info_title);

        struct RadioImpact {
            std::string name;
            std::string icon;
            std::string desc;
        };

        std::vector<RadioImpact> radios = {
            {"Wi-Fi Radio", "󰤨", "Disables 802.11 Wi-Fi networks, local connections, and scanning"},
            {"Bluetooth", "󰂯", "Disconnects wireless audio, peripherals, keyboards, and discovery"},
            {"Cellular & Mobile", "󰄡", "Suspends WWAN mobile data modems and SIM network connectivity"},
            {"GPS & Location", "󰍎", "Powers down satellite positioning and location-sensing hardware"}
        };

        for (const auto& radio : radios) {
            auto row_content = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(12)
                ->build();

            // Radio icon
            auto r_icon = TextViewBuilder::create()
                ->text(radio.icon)
                ->textSize(config->metrics.font_size)
                ->textColor(item->switch_val ? config->colors.on_surface_variant.with_alpha(0.6f) : config->colors.primary)
                ->build();
            LayoutParams ricon_lp;
            ricon_lp.width = 24;
            ricon_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(r_icon, ricon_lp);

            auto text_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto r_name = TextViewBuilder::create()
                ->text(radio.name)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            text_box->add_view(r_name);

            auto r_desc = TextViewBuilder::create()
                ->text(radio.desc)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            text_box->add_view(r_desc);

            LayoutParams tb_lp;
            tb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            tb_lp.weight = 1.0f;
            tb_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(text_box, tb_lp);

            // Status Badge
            std::string status_str = item->switch_val ? "Disabled" : "Active";
            Color badge_bg = item->switch_val ? config->colors.surface_variant : config->colors.primary_container.with_alpha(0.4f);
            Color badge_fg = item->switch_val ? config->colors.on_surface_variant : config->colors.primary;

            auto status_view = TextViewBuilder::create()
                ->text(status_str)
                ->caption(true)
                ->textColor(badge_fg)
                ->padding(8, 4)
                ->build();

            auto pill = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(badge_bg)
                ->cornerRadius(6)
                ->elevation(0)
                ->addView(status_view)
                ->build();

            LayoutParams pill_lp;
            pill_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            pill_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            pill_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(pill, pill_lp);

            Color row_bg = item->switch_val ? config->colors.surface_variant.with_alpha(0.2f) : Color::transparent();
            auto row_card = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(row_bg)
                ->cornerRadius(8)
                ->elevation(0)
                ->addView(row_content)
                ->build();

            info_card->add_view(row_card);
        }

        add_preview_card(info_card);
        return;
    }

    // Dedicated Interactive UI for Private DNS (DNS-over-TLS)
    if (item->is_private_dns) {
        // 1. Configuration Card (Single Spinner on new line, Hostname EditText on new line)
        auto cfg_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto cfg_title = TextViewBuilder::create()
            ->text("DNS-over-TLS configuration")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        cfg_card->add_view(cfg_title);

        // Label for Spinner
        auto sp_lbl = TextViewBuilder::create()
            ->text("Mode / Provider")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        cfg_card->add_view(sp_lbl);

        // Forward declare details views to update in-place without tearing down spinner
        const auto& presets = item->private_dns_cfg.presets;
        const PrivateDnsPreset* active_preset = &presets[0];
        for (const auto& p : presets) {
            if (p.name == item->private_dns_cfg.selected_option) {
                active_preset = &p;
                break;
            }
        }

        bool is_custom = (item->private_dns_cfg.selected_option == "Custom Hostname");
        std::string initial_ep_host = is_custom ? item->private_dns_cfg.custom_hostname : active_preset->hostname;

        auto host_val = TextViewBuilder::create()
            ->text(initial_ep_host.empty() ? "(Not set)" : initial_ep_host)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();

        auto desc_val = TextViewBuilder::create()
            ->text(active_preset->description)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();

        auto ip_val = TextViewBuilder::create()
            ->text(active_preset->endpoints)
            ->caption(true)
            ->textColor(config->colors.on_surface)
            ->build();

        std::string initial_edit_text = is_custom ? item->private_dns_cfg.custom_hostname : active_preset->hostname;
        auto custom_edit = EditTextBuilder::create()
            ->text(initial_edit_text)
            ->hint(is_custom ? "Enter custom DoT hostname (e.g. dns.nextdns.io)" : "")
            ->editable(is_custom)
            ->padding(14, 8)
            ->build();

        auto sync_dns_ui = [item, host_val, desc_val, ip_val]() {
            bool custom = (item->private_dns_cfg.selected_option == "Custom Hostname");
            for (const auto& p : item->private_dns_cfg.presets) {
                if (p.name == item->private_dns_cfg.selected_option) {
                    std::string h = custom ? item->private_dns_cfg.custom_hostname : p.hostname;
                    host_val->set_text(h.empty() ? "(Not set)" : h);
                    desc_val->set_text(p.description);
                    ip_val->set_text(p.endpoints);
                    host_val->request_redraw();
                    desc_val->request_redraw();
                    ip_val->request_redraw();
                    break;
                }
            }
        };

        // Find selected index for Spinner
        int sel_idx = 0;
        for (size_t i = 0; i < item->private_dns_cfg.options.size(); ++i) {
            if (item->private_dns_cfg.options[i] == item->private_dns_cfg.selected_option) {
                sel_idx = static_cast<int>(i);
                break;
            }
        }

        // Single Spinner on a NEW line
        auto dns_spinner = SpinnerBuilder::create()
            ->items(item->private_dns_cfg.options)
            ->selectedIndex(sel_idx)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(8)
            ->onItemSelected([item, custom_edit, sync_dns_ui](int idx, const std::string& val) {
                (void)idx;
                item->private_dns_cfg.selected_option = val;
                bool custom = (val == "Custom Hostname");
                custom_edit->set_editable(custom);
                if (custom) {
                    custom_edit->set_text(item->private_dns_cfg.custom_hostname);
                    custom_edit->set_hint("Enter custom DoT hostname (e.g. dns.nextdns.io)");
                } else {
                    for (const auto& p : item->private_dns_cfg.presets) {
                        if (p.name == val) {
                            custom_edit->set_text(p.hostname);
                            custom_edit->set_hint("");
                            break;
                        }
                    }
                }
                sync_dns_ui();
            })
            ->build();

        LayoutParams sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        cfg_card->add_view(dns_spinner, sp_lp);

        // Hostname Label
        auto host_lbl = TextViewBuilder::create()
            ->text("Provider Hostname")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        cfg_card->add_view(host_lbl);

        // Conditional EditText on a NEW line
        custom_edit->set_on_text_changed_listener([item, sync_dns_ui](std::shared_ptr<EditText>, const std::string& new_text) {
            item->private_dns_cfg.custom_hostname = new_text;
            sync_dns_ui();
        });

        LayoutParams edit_lp;
        edit_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        edit_lp.height = 38;
        cfg_card->add_view(custom_edit, edit_lp);

        add_preview_card(cfg_card);

        // 3. Active Endpoint & Verification Card
        auto ep_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto ep_header = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto ep_title = TextViewBuilder::create()
            ->text("Active endpoint & features")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        LayoutParams ept_lp;
        ept_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        ept_lp.weight = 1.0f;
        ept_lp.gravity = Gravity::CenterVertical;
        ep_header->add_view(ep_title, ept_lp);

        auto test_btn = ButtonBuilder::create()
            ->text("Test Resolution")
            ->flat(true)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->padding(10, 4)
            ->cornerRadius(6)
            ->onClick([this, item]() {
                item->last_action_message = "DNS handshake verified • Latency: 3.8 ms";
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();
        LayoutParams tb_lp;
        tb_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        tb_lp.height = 28;
        tb_lp.gravity = Gravity::CenterVertical;
        ep_header->add_view(test_btn, tb_lp);
        ep_card->add_view(ep_header);

        if (!item->last_action_message.empty()) {
            auto test_feedback = TextViewBuilder::create()
                ->text(item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            ep_card->add_view(test_feedback);
        }

        // Hostname row
        auto host_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();
        auto host_caption = TextViewBuilder::create()
            ->text("TLS Hostname")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        host_box->add_view(host_caption);
        host_box->add_view(host_val);
        ep_card->add_view(host_box);

        // Description box
        auto desc_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();
        auto desc_caption = TextViewBuilder::create()
            ->text("Protection Scope")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        desc_box->add_view(desc_caption);
        desc_box->add_view(desc_val);
        ep_card->add_view(desc_box);

        // Endpoints box
        auto ip_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();
        auto ip_caption = TextViewBuilder::create()
            ->text("Upstream Anycast Resolvers")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        ip_box->add_view(ip_caption);
        ip_box->add_view(ip_val);
        ep_card->add_view(ip_box);

        add_preview_card(ep_card);

        // 4. DNS & Encryption Specifications Telemetry
        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, "DNS & encryption specifications"));
        }

        return;
    }

    // Dedicated Interactive UI for Firewall
    if (item->is_firewall) {
        // 1. Top Card: Simple Bluetooth-like On/Off Switch with Subtitle "Powered by UFW"
        bool switch_focused = m_preview_focused && (m_preview_control_index == 0);
        auto fw_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(switch_focused ? 2 : 1, switch_focused ? config->colors.primary : config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto fw_icon = TextViewBuilder::create()
            ->text("󰒃")
            ->textSize(config->metrics.font_size + 4)
            ->textColor(item->firewall_cfg.enabled ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        LayoutParams fwicon_lp;
        fwicon_lp.width = 30;
        fwicon_lp.gravity = Gravity::CenterVertical;
        fw_card->add_view(fw_icon, fwicon_lp);

        auto fw_lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto fw_title = TextViewBuilder::create()
            ->text("Firewall")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        fw_lbl_box->add_view(fw_title);

        auto fw_hint = TextViewBuilder::create()
            ->text("Powered by UFW")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        fw_lbl_box->add_view(fw_hint);

        LayoutParams fwlbl_lp;
        fwlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        fwlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        fwlbl_lp.weight = 1.0f;
        fwlbl_lp.gravity = Gravity::CenterVertical;
        fw_card->add_view(fw_lbl_box, fwlbl_lp);

        m_active_primary_switch = SwitchBuilder::create()
            ->checked(item->firewall_cfg.enabled)
            ->onCheckedChanged([this, item](bool checked) {
                item->firewall_cfg.enabled = checked;
                rebuild_current_column();
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();

        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        fw_card->add_view(m_active_primary_switch, sw_lp);

        add_preview_card(fw_card);

        if (item->firewall_cfg.enabled) {
            // 2. Policy Card: Incoming Spinner, Outgoing Spinner
            auto policy_card = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(10)
                ->padding(16, 14)
                ->backgroundColor(config->colors.surface)
                ->stroke(1, config->colors.outline.with_alpha(0.3f))
                ->cornerRadius(12)
                ->build();

            auto policy_title = TextViewBuilder::create()
                ->text("Default Policies")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            policy_card->add_view(policy_title);

            // Incoming traffic
            auto in_lbl = TextViewBuilder::create()
                ->text("Incoming traffic")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            policy_card->add_view(in_lbl);

            std::vector<std::string> in_policies = {"Drop (Deny)", "Reject", "Allow"};
            int in_sel = 0;
            for (size_t i = 0; i < in_policies.size(); ++i) {
                if (in_policies[i] == item->firewall_cfg.incoming_policy) {
                    in_sel = static_cast<int>(i);
                    break;
                }
            }

            auto in_spinner = SpinnerBuilder::create()
                ->items(in_policies)
                ->selectedIndex(in_sel)
                ->fontSize(config->metrics.font_size)
                ->padding(12, 6)
                ->cornerRadius(8)
                ->onItemSelected([item, in_policies](int idx, const std::string& val) {
                    (void)idx;
                    item->firewall_cfg.incoming_policy = val;
                })
                ->build();
            LayoutParams in_sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
            policy_card->add_view(in_spinner, in_sp_lp);

            // Outgoing traffic
            auto out_lbl = TextViewBuilder::create()
                ->text("Outgoing traffic")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            policy_card->add_view(out_lbl);

            std::vector<std::string> out_policies = {"Allow", "Drop (Deny)", "Reject"};
            int out_sel = 0;
            for (size_t i = 0; i < out_policies.size(); ++i) {
                if (out_policies[i] == item->firewall_cfg.outgoing_policy) {
                    out_sel = static_cast<int>(i);
                    break;
                }
            }

            auto out_spinner = SpinnerBuilder::create()
                ->items(out_policies)
                ->selectedIndex(out_sel)
                ->fontSize(config->metrics.font_size)
                ->padding(12, 6)
                ->cornerRadius(8)
                ->onItemSelected([item, out_policies](int idx, const std::string& val) {
                    (void)idx;
                    item->firewall_cfg.outgoing_policy = val;
                })
                ->build();
            LayoutParams out_sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
            policy_card->add_view(out_spinner, out_sp_lp);

            add_preview_card(policy_card);

            // 3. Preset Card: Home, Work, Cellular, Public
            auto preset_card = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(10)
                ->padding(16, 14)
                ->backgroundColor(config->colors.surface)
                ->stroke(1, config->colors.outline.with_alpha(0.3f))
                ->cornerRadius(12)
                ->build();

            auto preset_title = TextViewBuilder::create()
                ->text("Security Preset")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            preset_card->add_view(preset_title);

            auto preset_lbl = TextViewBuilder::create()
                ->text("Profile / Location")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            preset_card->add_view(preset_lbl);

            auto get_preset_desc = [](const std::string& p) -> std::string {
                if (p == "Work") return "Restricts inbound traffic to corporate VPN and office intranet; enables managed remote access.";
                if (p == "Cellular") return "Minimizes background traffic and blocks non-essential inbound sockets to preserve metered data.";
                if (p == "Public") return "Strict stealth isolation. Silently drops all unsolicited probes and disables local discovery.";
                return "Permits local network device discovery, KDE Connect, and home media streaming while blocking external WAN probes.";
            };

            auto preset_desc = TextViewBuilder::create()
                ->text(get_preset_desc(item->firewall_cfg.selected_preset))
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();

            std::vector<std::string> presets = {"Home", "Work", "Cellular", "Public"};
            int preset_sel = 0;
            for (size_t i = 0; i < presets.size(); ++i) {
                if (presets[i] == item->firewall_cfg.selected_preset) {
                    preset_sel = static_cast<int>(i);
                    break;
                }
            }

            auto preset_spinner = SpinnerBuilder::create()
                ->items(presets)
                ->selectedIndex(preset_sel)
                ->fontSize(config->metrics.font_size)
                ->padding(12, 6)
                ->cornerRadius(8)
                ->onItemSelected([item, preset_desc, get_preset_desc](int idx, const std::string& val) {
                    (void)idx;
                    item->firewall_cfg.selected_preset = val;
                    preset_desc->set_text(get_preset_desc(val));
                    preset_desc->request_redraw();
                })
                ->build();
            LayoutParams preset_sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
            preset_card->add_view(preset_spinner, preset_sp_lp);
            preset_card->add_view(preset_desc);

            add_preview_card(preset_card);

            // 4. Custom Rules Card: switches for common services + Add Rule popup button on header
            auto rules_card = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(12)
                ->padding(16, 14)
                ->backgroundColor(config->colors.surface)
                ->stroke(1, config->colors.outline.with_alpha(0.3f))
                ->cornerRadius(12)
                ->build();

            // Card Header Row: Title on Left, Add Rule Button on Right
            auto rules_hdr = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto rules_title = TextViewBuilder::create()
                ->text("Custom Rules")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams rtitle_lp;
            rtitle_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            rtitle_lp.weight = 1.0f;
            rtitle_lp.gravity = Gravity::CenterVertical;
            rules_hdr->add_view(rules_title, rtitle_lp);

            auto add_btn = ButtonBuilder::create()
                ->text("+ Add Rule")
                ->style(ButtonStyle::Outlined)
                ->padding(12, 4)
                ->cornerRadius(6)
                ->build();

            // When Add Rule button is clicked, show Popup
            add_btn->set_on_click_listener([this, item, config, add_btn]() {
                if (m_active_popup) {
                    m_active_popup->dismiss();
                    m_active_popup = nullptr;
                }

                auto popup_col = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Vertical)
                    ->padding(16, 14)
                    ->spacing(10)
                    ->backgroundColor(config->colors.surface)
                    ->stroke(1, config->colors.outline.with_alpha(0.4f))
                    ->cornerRadius(12)
                    ->build();

                auto pop_title = TextViewBuilder::create()
                    ->text("Add Firewall Rule")
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.on_surface)
                    ->build();
                popup_col->add_view(pop_title);

                auto name_lbl = TextViewBuilder::create()
                    ->text("Rule Name / Service")
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.8f))
                    ->build();
                popup_col->add_view(name_lbl);

                auto name_edit = EditTextBuilder::create()
                    ->text("")
                    ->hint("e.g. Plex, Syncthing, Deluge")
                    ->editable(true)
                    ->padding(12, 6)
                    ->build();
                LayoutParams n_lp;
                n_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                n_lp.height = 36;
                popup_col->add_view(name_edit, n_lp);

                auto port_lbl = TextViewBuilder::create()
                    ->text("Port / Protocol")
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.8f))
                    ->build();
                popup_col->add_view(port_lbl);

                auto port_edit = EditTextBuilder::create()
                    ->text("")
                    ->hint("e.g. 8080/tcp, 22000/udp")
                    ->editable(true)
                    ->padding(12, 6)
                    ->build();
                LayoutParams p_lp;
                p_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                p_lp.height = 36;
                popup_col->add_view(port_edit, p_lp);

                // Action buttons
                auto btn_box = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Horizontal)
                    ->spacing(8)
                    ->gravity(Gravity::End)
                    ->build();

                auto cancel_btn = ButtonBuilder::create()
                    ->text("Cancel")
                    ->style(ButtonStyle::Outlined)
                    ->padding(12, 4)
                    ->cornerRadius(6)
                    ->onClick([this]() {
                        if (m_active_popup) {
                            m_active_popup->dismiss();
                            m_active_popup = nullptr;
                        }
                    })
                    ->build();
                btn_box->add_view(cancel_btn);

                auto submit_btn = ButtonBuilder::create()
                    ->text("Add")
                    ->primary(true)
                    ->padding(14, 4)
                    ->cornerRadius(6)
                    ->onClick([this, item, name_edit, port_edit]() {
                        std::string r_name = name_edit->get_text();
                        std::string r_port = port_edit->get_text();
                        if (r_name.empty()) r_name = "Custom Port";
                        if (r_port.empty()) r_port = "8080/tcp";
                        item->firewall_cfg.custom_rules.push_back({r_name, r_port, true});
                        if (m_active_popup) {
                            m_active_popup->dismiss();
                            m_active_popup = nullptr;
                        }
                        rebuild_preview_column();
                        notify_state_changed();
                    })
                    ->build();
                btn_box->add_view(submit_btn);

                popup_col->add_view(btn_box);

                m_active_popup = PopupWindowBuilder::create()
                    ->content(popup_col)
                    ->width(280)
                    ->elevation(6)
                    ->cornerRadius(12)
                    ->dismissOnOutsideClick(true)
                    ->dismissOnEscape(true)
                    ->build();

                m_active_popup->show_as_dropdown(add_btn, PopupGravity::BottomEnd);
            });

            LayoutParams add_btn_lp;
            add_btn_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            add_btn_lp.height = 28;
            add_btn_lp.gravity = Gravity::CenterVertical;
            rules_hdr->add_view(add_btn, add_btn_lp);

            rules_card->add_view(rules_hdr);

            // Vertically stacked switches helper
            auto add_rule_row = [&](const std::string& name, const std::string& desc, bool is_checked, std::function<void(bool)> on_toggle) {
                auto row = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Horizontal)
                    ->spacing(12)
                    ->gravity(Gravity::CenterVertical)
                    ->build();

                auto box = LinearLayoutBuilder::create()
                    ->orientation(Orientation::Vertical)
                    ->spacing(2)
                    ->build();

                auto lbl = TextViewBuilder::create()
                    ->text(name)
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.on_surface)
                    ->build();
                box->add_view(lbl);

                auto sub = TextViewBuilder::create()
                    ->text(desc)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                    ->build();
                box->add_view(sub);

                LayoutParams box_lp;
                box_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                box_lp.weight = 1.0f;
                box_lp.gravity = Gravity::CenterVertical;
                row->add_view(box, box_lp);

                auto sw = SwitchBuilder::create()
                    ->checked(is_checked)
                    ->onCheckedChanged(std::move(on_toggle))
                    ->build();
                LayoutParams sw_lp;
                sw_lp.width = 44;
                sw_lp.height = 24;
                sw_lp.gravity = Gravity::CenterVertical;
                row->add_view(sw, sw_lp);

                rules_card->add_view(row);
            };

            // Predefined common rules
            add_rule_row("KDE Connect", "Allow local device pairing & clipboard sync (UDP/TCP 1714–1764)",
                         item->firewall_cfg.allow_kdeconnect, [item](bool c) { item->firewall_cfg.allow_kdeconnect = c; });

            add_rule_row("SSH Server", "Allow secure shell remote access (TCP 22)",
                         item->firewall_cfg.allow_ssh, [item](bool c) { item->firewall_cfg.allow_ssh = c; });

            add_rule_row("Samba File Sharing", "Allow local network folder sharing (TCP 445 / UDP 137–138)",
                         item->firewall_cfg.allow_samba, [item](bool c) { item->firewall_cfg.allow_samba = c; });

            add_rule_row("Network Printing (CUPS)", "Allow local IPP printer discovery (UDP/TCP 631)",
                         item->firewall_cfg.allow_cups, [item](bool c) { item->firewall_cfg.allow_cups = c; });

            // User-added custom rules
            for (size_t i = 0; i < item->firewall_cfg.custom_rules.size(); ++i) {
                auto& crule = item->firewall_cfg.custom_rules[i];
                add_rule_row(crule.name, "Custom port rule (" + crule.port_proto + ")",
                             crule.enabled, [&crule](bool c) { crule.enabled = c; });
            }

            add_preview_card(rules_card);

            // 5. Monitor Card: Can be disabled with switch
            auto monitor_card = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(10)
                ->padding(16, 14)
                ->backgroundColor(config->colors.surface)
                ->stroke(1, config->colors.outline.with_alpha(0.3f))
                ->cornerRadius(12)
                ->build();

            auto mon_hdr = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->spacing(12)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto mon_icon = TextViewBuilder::create()
                ->text("󰒍")
                ->textSize(config->metrics.font_size + 2)
                ->textColor(item->firewall_cfg.monitor_enabled ? config->colors.primary : config->colors.on_surface_variant)
                ->build();
            LayoutParams mico_lp;
            mico_lp.width = 24;
            mico_lp.gravity = Gravity::CenterVertical;
            mon_hdr->add_view(mon_icon, mico_lp);

            auto mon_lbl_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto mon_title = TextViewBuilder::create()
                ->text("Monitor")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            mon_lbl_box->add_view(mon_title);

            auto mon_sub = TextViewBuilder::create()
                ->text(item->firewall_cfg.monitor_enabled ? "Active • Real-time socket & probe auditing" : "Disabled • Logging and packet inspection inactive")
                ->caption(true)
                ->textColor(item->firewall_cfg.monitor_enabled ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            mon_lbl_box->add_view(mon_sub);

            LayoutParams mlbl_lp;
            mlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            mlbl_lp.weight = 1.0f;
            mlbl_lp.gravity = Gravity::CenterVertical;
            mon_hdr->add_view(mon_lbl_box, mlbl_lp);

            auto mon_sw = SwitchBuilder::create()
                ->checked(item->firewall_cfg.monitor_enabled)
                ->onCheckedChanged([this, item](bool checked) {
                    item->firewall_cfg.monitor_enabled = checked;
                    rebuild_preview_column();
                    notify_state_changed();
                })
                ->build();
            LayoutParams mon_sw_lp;
            mon_sw_lp.width = 44;
            mon_sw_lp.height = 24;
            mon_sw_lp.gravity = Gravity::CenterVertical;
            mon_hdr->add_view(mon_sw, mon_sw_lp);

            monitor_card->add_view(mon_hdr);

            if (item->firewall_cfg.monitor_enabled) {
                std::vector<TelemetryEntry> mon_telemetry = {
                    {"Backend Engine", "UFW (iptables / netfilter)"},
                    {"Blocked Probes Today", std::to_string(item->firewall_cfg.blocked_today) + " unauthorized requests"},
                    {"Active Connections", std::to_string(item->firewall_cfg.active_connections) + " running sockets"},
                    {"Monitored Interfaces", item->firewall_cfg.monitored_interfaces},
                    {"Last Blocked Event", item->firewall_cfg.last_blocked_probe}
                };

                for (const auto& entry : mon_telemetry) {
                    auto mrow = LinearLayoutBuilder::create()
                        ->orientation(Orientation::Horizontal)
                        ->padding(2, 2)
                        ->build();

                    auto mk = TextViewBuilder::create()
                        ->text(entry.key)
                        ->caption(true)
                        ->textColor(config->colors.on_surface_variant)
                        ->build();
                    LayoutParams mk_lp;
                    mk_lp.width = 150;
                    mrow->add_view(mk, mk_lp);

                    auto mv = TextViewBuilder::create()
                        ->text(entry.value)
                        ->caption(true)
                        ->textColor(config->colors.on_surface)
                        ->build();
                    LayoutParams mv_lp;
                    mv_lp.width = static_cast<int>(LayoutDimension::MatchParent);
                    mv_lp.weight = 1.0f;
                    mrow->add_view(mv, mv_lp);

                    monitor_card->add_view(mrow);
                }
            }

            add_preview_card(monitor_card);
        }

        return;
    }

    // Dedicated Interactive UI for Per App Network Access
    if (item->is_per_app_network) {
        auto card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto icon = TextViewBuilder::create()
            ->text("󰋽")
            ->textSize(config->metrics.font_size + 2)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams icon_lp;
        icon_lp.width = 24;
        icon_lp.gravity = Gravity::CenterVertical;
        card->add_view(icon, icon_lp);

        auto hint = TextViewBuilder::create()
            ->text("Please go to Applications->Installed App->App Info->Network Access.")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface_variant)
            ->build();

        LayoutParams hint_lp;
        hint_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        hint_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        hint_lp.weight = 1.0f;
        hint_lp.gravity = Gravity::CenterVertical;
        card->add_view(hint, hint_lp);

        add_preview_card(card);
        return;
    }

    // Dedicated Minimal UI for Display Brightness
    if (item->is_display_brightness) {
        // 1. Simple Brightness Adjusting Card (No hero card)
        auto bright_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 14)
            ->spacing(10)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto header_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto icon = TextViewBuilder::create()
            ->text("󰃠")
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

        auto hint = TextViewBuilder::create()
            ->text("Adjust panel backlight luminance")
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

        int current_pct = static_cast<int>(std::round(item->brightness_val * 100.0f));
        auto val_label = TextViewBuilder::create()
            ->text(std::to_string(current_pct) + "%")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams val_lp;
        val_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        val_lp.gravity = Gravity::CenterVertical;
        header_row->add_view(val_label, val_lp);

        bright_card->add_view(header_row);

        auto slider = SliderBuilder::create()
            ->value(item->brightness_val)
            ->onValueChanged([this, item, val_label](float val, bool user) {
                (void)user;
                float clamped = std::clamp(val, 0.05f, 1.0f);
                item->brightness_val = clamped;
                int pct = static_cast<int>(std::round(clamped * 100.0f));
                val_label->set_text(std::to_string(pct) + "%");
                val_label->request_redraw();
                notify_state_changed();
            })
            ->build();

        LayoutParams sl_lp;
        sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        sl_lp.height = 24;
        bright_card->add_view(slider, sl_lp);

        add_preview_card(bright_card);

        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, item->telemetry_title.empty() ? "Backlight & panel specifications" : item->telemetry_title));
        }
        return;
    }

    // Dedicated Interactive UI for Tiling Layout & Window Management
    if (item->is_tiling_config) {
        bool show_layout = (item->id == "tile_mode");
        bool show_windows = (item->id == "tile_windows");
        bool show_split = (item->id == "tile_split");
        bool show_focus = (item->id == "tile_focus");

        auto main_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 16)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto add_divider = [&main_card, config]() {
            auto divider = LinearLayoutBuilder::create()
                ->backgroundColor(config->colors.outline.with_alpha(0.12f))
                ->build();
            LayoutParams dlp;
            dlp.width = static_cast<int>(LayoutDimension::MatchParent);
            dlp.height = 1;
            main_card->add_view(divider, dlp);
        };

        auto add_toggle_row = [&](const std::string& title, const std::string& hint, bool checked, std::function<void(bool)> on_toggle) {
            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->spacing(12)
                ->build();

            auto box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto title_view = TextViewBuilder::create()
                ->text(title)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            box->add_view(title_view);

            if (!hint.empty()) {
                auto hint_view = TextViewBuilder::create()
                    ->text(hint)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                    ->build();
                box->add_view(hint_view);
            }

            LayoutParams box_lp;
            box_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            box_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            box_lp.weight = 1.0f;
            box_lp.gravity = Gravity::CenterVertical;
            row->add_view(box, box_lp);

            auto sw = SwitchBuilder::create()
                ->checked(checked)
                ->onCheckedChanged([this, on_toggle](bool val) {
                    if (on_toggle) on_toggle(val);
                    notify_state_changed();
                })
                ->build();
            row->add_view(sw);
            main_card->add_view(row);
        };

        auto add_slider_row = [&](const std::string& title, const std::string& hint, float initial_val, const std::string& initial_label, std::function<std::string(float)> on_change) {
            auto header = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto title_view = TextViewBuilder::create()
                ->text(title)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            box->add_view(title_view);

            if (!hint.empty()) {
                auto hint_view = TextViewBuilder::create()
                    ->text(hint)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                    ->build();
                box->add_view(hint_view);
            }

            LayoutParams box_lp;
            box_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            box_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            box_lp.weight = 1.0f;
            box_lp.gravity = Gravity::CenterVertical;
            header->add_view(box, box_lp);

            auto val_label = TextViewBuilder::create()
                ->text(initial_label)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->build();
            LayoutParams val_lp;
            val_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            val_lp.gravity = Gravity::CenterVertical;
            header->add_view(val_label, val_lp);
            main_card->add_view(header);

            auto slider = SliderBuilder::create()
                ->value(initial_val)
                ->onValueChanged([this, val_label, on_change](float val, bool user) {
                    (void)user;
                    if (on_change) {
                        std::string text = on_change(val);
                        val_label->set_text(text);
                        val_label->request_redraw();
                    }
                    notify_state_changed();
                })
                ->build();
            LayoutParams sl_lp;
            sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            sl_lp.height = 24;
            main_card->add_view(slider, sl_lp);
        };

        auto add_spinner_block = [&](const std::string& title, const std::string& hint, const std::vector<std::string>& items, int cur_idx, std::function<void(int, const std::string&)> on_select) {
            auto block = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(6)
                ->build();

            auto box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto title_view = TextViewBuilder::create()
                ->text(title)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            box->add_view(title_view);

            if (!hint.empty()) {
                auto hint_view = TextViewBuilder::create()
                    ->text(hint)
                    ->caption(true)
                    ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                    ->build();
                box->add_view(hint_view);
            }
            block->add_view(box);

            auto spinner = SpinnerBuilder::create()
                ->items(items)
                ->selectedIndex(cur_idx)
                ->fontSize(config->metrics.font_size)
                ->padding(12, 6)
                ->cornerRadius(8)
                ->onItemSelected([this, on_select](int idx, const std::string& val) {
                    if (on_select) on_select(idx, val);
                    notify_state_changed();
                })
                ->build();
            LayoutParams sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
            block->add_view(spinner, sp_lp);
            main_card->add_view(block);
        };

        // -------------------------------------------------------------
        // 1. Layout Mode
        // -------------------------------------------------------------
        if (show_layout) {
            auto s1_title = TextViewBuilder::create()
                ->text("Layout Mode")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            main_card->add_view(s1_title);

            int cur_mode_idx = 0;
            for (size_t i = 0; i < item->tiling_cfg->available_layout_modes.size(); ++i) {
                if (item->tiling_cfg->available_layout_modes[i] == item->tiling_cfg->layout_mode) {
                    cur_mode_idx = static_cast<int>(i);
                    break;
                }
            }

            add_spinner_block(
                "Tiling algorithm",
                "Select the active window arrangement engine for this workspace",
                item->tiling_cfg->available_layout_modes,
                cur_mode_idx,
                [item](int idx, const std::string& val) {
                    (void)idx;
                    item->tiling_cfg->layout_mode = val;
                }
            );

            // Algorithmic description block
            auto desc_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->padding(12, 10)
                ->spacing(4)
                ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
                ->stroke(1, config->colors.outline.with_alpha(0.25f))
                ->cornerRadius(8)
                ->build();

            auto desc_title = TextViewBuilder::create()
                ->text("󰕰  Algorithm Behavior")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            desc_box->add_view(desc_title);

            std::string desc_text = "Dynamic Binary Space Partitioning (BSP). Windows automatically split recursively into halves based on window aspect ratio and cursor placement. Snappy and adaptive for dynamic multitasking.";
            if (item->tiling_cfg->layout_mode.find("Master") != std::string::npos) {
                desc_text = "Master-and-stack layout. The primary master window on the left takes dominant screen area; remaining windows stack vertically along the right column.";
            } else if (item->tiling_cfg->layout_mode.find("Bottom") != std::string::npos) {
                desc_text = "Bottom-stack layout. The master window occupies the top half of the screen; auxiliary windows stack horizontally side-by-side across the bottom.";
            } else if (item->tiling_cfg->layout_mode.find("Centered") != std::string::npos) {
                desc_text = "Centered-master layout. A prominent central workspace is flanked symmetrically by auxiliary vertical stacks on both left and right.";
            } else if (item->tiling_cfg->layout_mode.find("Spiral") != std::string::npos) {
                desc_text = "Fibonacci golden spiral layout. Window splits alternate clockwise between horizontal and vertical for each newly opened window.";
            } else if (item->tiling_cfg->layout_mode.find("Columns") != std::string::npos) {
                desc_text = "Equal column partition. Divides available screen space into equal-width vertical columns side-by-side across the monitor.";
            } else if (item->tiling_cfg->layout_mode.find("Grid") != std::string::npos) {
                desc_text = "Grid matrix layout. Automatically arranges all open windows into a balanced N × M grid with dynamic row balancing.";
            } else if (item->tiling_cfg->layout_mode.find("Monocle") != std::string::npos) {
                desc_text = "Full-screen monocle layout. The focused window fills the entire usable display area; non-focused windows remain stacked underneath.";
            }

            auto desc_body = TextViewBuilder::create()
                ->text(desc_text)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            desc_box->add_view(desc_body);

            LayoutParams dbox_lp;
            dbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            dbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            main_card->add_view(desc_box, dbox_lp);
        }

        // -------------------------------------------------------------
        // 2. Windows
        // -------------------------------------------------------------
        if (show_windows) {
            auto s2_title = TextViewBuilder::create()
                ->text("Windows & Geometry")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            main_card->add_view(s2_title);

            // Inner Window Gap (0 to 32 px)
            add_slider_row(
                "Inner window gap",
                "Spacing between adjacent tiled windows (in pixels)",
                static_cast<float>(item->tiling_cfg->inner_gap) / 32.0f,
                std::to_string(item->tiling_cfg->inner_gap) + " px",
                [item](float val) -> std::string {
                    int px = static_cast<int>(std::round(val * 32.0f));
                    item->tiling_cfg->inner_gap = px;
                    return std::to_string(px) + " px";
                }
            );

            // Outer Screen Edge Gap (0 to 48 px)
            add_slider_row(
                "Outer screen edge gap",
                "Margin between window borders and monitor screen edges",
                static_cast<float>(item->tiling_cfg->outer_gap) / 48.0f,
                std::to_string(item->tiling_cfg->outer_gap) + " px",
                [item](float val) -> std::string {
                    int px = static_cast<int>(std::round(val * 48.0f));
                    item->tiling_cfg->outer_gap = px;
                    return std::to_string(px) + " px";
                }
            );

            // Smart Gaps
            add_toggle_row(
                "Smart gaps",
                "Automatically collapse gaps and borders when only one window is open",
                item->tiling_cfg->smart_gaps,
                [item](bool val) {
                    item->tiling_cfg->smart_gaps = val;
                }
            );

            // Window Border Width (0 to 8 px)
            add_slider_row(
                "Window border width",
                "Border stroke thickness in pixels (0 disables window borders)",
                static_cast<float>(item->tiling_cfg->border_width) / 8.0f,
                item->tiling_cfg->border_width == 0 ? "Disabled (0 px)" : (std::to_string(item->tiling_cfg->border_width) + " px"),
                [item](float val) -> std::string {
                    int px = static_cast<int>(std::round(val * 8.0f));
                    item->tiling_cfg->border_width = px;
                    return px == 0 ? "Disabled (0 px)" : (std::to_string(px) + " px");
                }
            );

            // Window Corner Radius (0 to 32 px)
            add_slider_row(
                "Window corner radius",
                "Rounding curvature applied to window frames and drop shadows",
                static_cast<float>(item->tiling_cfg->corner_radius) / 32.0f,
                std::to_string(item->tiling_cfg->corner_radius) + " px",
                [item](float val) -> std::string {
                    int px = static_cast<int>(std::round(val * 32.0f));
                    item->tiling_cfg->corner_radius = px;
                    return std::to_string(px) + " px";
                }
            );

            // Active Border Color Preset
            int cur_act_idx = 0;
            for (size_t i = 0; i < item->tiling_cfg->active_border_presets.size(); ++i) {
                if (item->tiling_cfg->active_border_presets[i] == item->tiling_cfg->active_border_preset) {
                    cur_act_idx = static_cast<int>(i);
                    break;
                }
            }
            add_spinner_block(
                "Active window border color",
                "Border color or multi-stop linear gradient for focused window",
                item->tiling_cfg->active_border_presets,
                cur_act_idx,
                [item](int idx, const std::string& val) {
                    (void)idx;
                    item->tiling_cfg->active_border_preset = val;
                    if (val.find("Yellow-Red") != std::string::npos) item->tiling_cfg->active_border_value = "#ffff00 #ff0000 45deg";
                    else if (val.find("Cyan-Blue") != std::string::npos) item->tiling_cfg->active_border_value = "#00ffcc #0066ff 45deg";
                    else if (val.find("Purple-Blue") != std::string::npos) item->tiling_cfg->active_border_value = "#ff007f #7928ca #0066ff 135deg";
                    else if (val.find("Neon") != std::string::npos) item->tiling_cfg->active_border_value = "#00ff88 #00ccff 90deg";
                    else if (val.find("Monochrome") != std::string::npos) item->tiling_cfg->active_border_value = "#ffffff";
                    else item->tiling_cfg->active_border_value = "#0066ff";
                }
            );

            // Inactive Border Color Preset
            int cur_inact_idx = 0;
            for (size_t i = 0; i < item->tiling_cfg->inactive_border_presets.size(); ++i) {
                if (item->tiling_cfg->inactive_border_presets[i] == item->tiling_cfg->inactive_border_preset) {
                    cur_inact_idx = static_cast<int>(i);
                    break;
                }
            }
            add_spinner_block(
                "Inactive window border color",
                "Border styling for unfocused background windows",
                item->tiling_cfg->inactive_border_presets,
                cur_inact_idx,
                [item](int idx, const std::string& val) {
                    (void)idx;
                    item->tiling_cfg->inactive_border_preset = val;
                    if (val.find("Dark Slate") != std::string::npos) item->tiling_cfg->inactive_border_value = "#334455 #222d38 45deg";
                    else if (val.find("Graphite") != std::string::npos) item->tiling_cfg->inactive_border_value = "#2d3748";
                    else if (val.find("Muted Amber") != std::string::npos) item->tiling_cfg->inactive_border_value = "#aa9900 #aa3333 45deg";
                    else if (val.find("Subtle") != std::string::npos) item->tiling_cfg->inactive_border_value = "#ffffff33";
                    else item->tiling_cfg->inactive_border_value = "#00000000";
                }
            );

            // Border Specular Shading
            add_toggle_row(
                "Border specular shading",
                "Subtle 3D lighting and specular bevel depth across the border stroke",
                item->tiling_cfg->border_shading,
                [item](bool val) {
                    item->tiling_cfg->border_shading = val;
                }
            );

            if (item->tiling_cfg->border_shading) {
                add_slider_row(
                    "Shading intensity",
                    "Specular bevel strength across the window frame",
                    item->tiling_cfg->border_shading_strength,
                    std::to_string(static_cast<int>(item->tiling_cfg->border_shading_strength * 100)) + "%",
                    [item](float val) -> std::string {
                        item->tiling_cfg->border_shading_strength = val;
                        return std::to_string(static_cast<int>(val * 100)) + "%";
                    }
                );
            }

            // Direct Border Drag Resizing
            add_toggle_row(
                "Direct border drag resizing",
                "Resize windows by dragging directly on window borders without modifier key",
                item->tiling_cfg->resize_on_border,
                [item](bool val) {
                    item->tiling_cfg->resize_on_border = val;
                }
            );

            // Auto-Float Fixed-Size Windows
            add_toggle_row(
                "Auto-float fixed-size dialogs",
                "Automatically floats splash screens and dialogs declaring fixed min/max size constraints",
                item->tiling_cfg->auto_float_fixed_size,
                [item](bool val) {
                    item->tiling_cfg->auto_float_fixed_size = val;
                }
            );

            // Live Interactive Mini Window Geometry Preview
            int p_outer = std::max(4, item->tiling_cfg->outer_gap / 2);
            int p_inner = std::max(4, item->tiling_cfg->inner_gap / 2);
            int p_radius = std::clamp(item->tiling_cfg->corner_radius / 2, 0, 16);
            int p_border = std::max(1, item->tiling_cfg->border_width);

            auto monitor_canvas = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(p_outer, p_outer)
                ->spacing(p_inner)
                ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
                ->stroke(1, config->colors.outline.with_alpha(0.4f))
                ->cornerRadius(10)
                ->build();

            auto win1 = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->padding(8, 8)
                ->gravity(Gravity::Center)
                ->backgroundColor(config->colors.surface)
                ->stroke(p_border, config->colors.primary)
                ->cornerRadius(p_radius)
                ->build();
            auto win1_title = TextViewBuilder::create()
                ->text("Active Window (Super+1)")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            win1->add_view(win1_title);
            LayoutParams w1_lp;
            w1_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            w1_lp.height = 64;
            w1_lp.weight = 1.0f;
            monitor_canvas->add_view(win1, w1_lp);

            auto win2 = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->padding(8, 8)
                ->gravity(Gravity::Center)
                ->backgroundColor(config->colors.surface)
                ->stroke(std::max(1, p_border), config->colors.outline.with_alpha(0.5f))
                ->cornerRadius(p_radius)
                ->build();
            auto win2_title = TextViewBuilder::create()
                ->text("Inactive Window (Super+2)")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            win2->add_view(win2_title);
            LayoutParams w2_lp;
            w2_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            w2_lp.height = 64;
            w2_lp.weight = 1.0f;
            monitor_canvas->add_view(win2, w2_lp);

            auto preview_label = TextViewBuilder::create()
                ->text("󰍹  Live Workspace Layout Preview (Gaps & Borders)")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            main_card->add_view(preview_label);

            LayoutParams mc_lp;
            mc_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            mc_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            main_card->add_view(monitor_canvas, mc_lp);
        }

        // -------------------------------------------------------------
        // 3. Dwindle (BSP) Tuning
        // -------------------------------------------------------------
        if (show_split) {
            auto s3_title = TextViewBuilder::create()
                ->text("Dwindle (BSP) Tuning")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            main_card->add_view(s3_title);

            bool is_dwindle_active = (item->tiling_cfg->layout_mode.find("Dwindle") != std::string::npos);
            auto banner = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->padding(12, 10)
                ->spacing(4)
                ->backgroundColor(is_dwindle_active ? config->colors.primary_container.with_alpha(0.35f) : config->colors.surface_variant.with_alpha(0.35f))
                ->stroke(1, is_dwindle_active ? config->colors.primary.with_alpha(0.4f) : config->colors.outline.with_alpha(0.25f))
                ->cornerRadius(8)
                ->build();

            auto banner_title = TextViewBuilder::create()
                ->text(is_dwindle_active ? "󰋼  Active Layout Engine" : "󰅚  Inactive for Current Layout")
                ->caption(true)
                ->textColor(is_dwindle_active ? config->colors.primary : config->colors.on_surface_variant)
                ->build();
            banner->add_view(banner_title);

            std::string banner_msg = is_dwindle_active 
                ? "These binary space partition options directly govern window division rules in the active Dwindle layout."
                : "Active layout is '" + item->tiling_cfg->layout_mode + "'. These options configure the 'dwindle' block in miquland.conf and apply when switching to Dwindle.";
            auto banner_text = TextViewBuilder::create()
                ->text(banner_msg)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            banner->add_view(banner_text);

            LayoutParams b_lp;
            b_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            b_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            main_card->add_view(banner, b_lp);

            // Default Split Ratio (20% to 80%)
            float initial_ratio_norm = (item->tiling_cfg->default_split_ratio - 0.20f) / 0.60f;
            int r_pct = static_cast<int>(std::round(item->tiling_cfg->default_split_ratio * 100.0f));
            add_slider_row(
                "Default split ratio",
                "Initial screen division ratio when a new window partitions a workspace",
                initial_ratio_norm,
                std::to_string(r_pct) + "% / " + std::to_string(100 - r_pct) + "%",
                [item](float val) -> std::string {
                    float ratio = 0.20f + val * 0.60f;
                    item->tiling_cfg->default_split_ratio = ratio;
                    int pct = static_cast<int>(std::round(ratio * 100.0f));
                    return std::to_string(pct) + "% / " + std::to_string(100 - pct) + "%";
                }
            );

            // Smart Cursor-Aware Splitting
            add_toggle_row(
                "Smart cursor-aware splitting",
                "Cursor position inside target window dictates horizontal or vertical split side",
                item->tiling_cfg->smart_split,
                [item](bool val) {
                    item->tiling_cfg->smart_split = val;
                }
            );

            // Split Focused Window
            add_toggle_row(
                "Split focused window",
                "New windows partition the currently active window instead of spawning at the spiral tail",
                item->tiling_cfg->split_focus,
                [item](bool val) {
                    item->tiling_cfg->split_focus = val;
                }
            );

            // Preserve Split Orientation
            add_toggle_row(
                "Preserve split orientation",
                "Retains horizontal or vertical split axes when windows are moved or closed",
                item->tiling_cfg->preserve_split,
                [item](bool val) {
                    item->tiling_cfg->preserve_split = val;
                }
            );

            // Widescreen Split Threshold Multiplier (0.8x to 2.5x)
            float initial_mult_norm = (item->tiling_cfg->split_width_multiplier - 0.8f) / 1.7f;
            auto format_mult = [](float m) -> std::string {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%.1f×", m);
                return std::string(buf);
            };
            add_slider_row(
                "Widescreen split width multiplier",
                "Aspect ratio threshold to favor side-by-side columns (width >= height × mult)",
                initial_mult_norm,
                format_mult(item->tiling_cfg->split_width_multiplier),
                [item, format_mult](float val) -> std::string {
                    float mult = 0.8f + val * 1.7f;
                    item->tiling_cfg->split_width_multiplier = mult;
                    return format_mult(mult);
                }
            );
        }

        // -------------------------------------------------------------
        // 4. Focus Behaviour
        // -------------------------------------------------------------
        if (show_focus) {
            auto s4_title = TextViewBuilder::create()
                ->text("Focus Behaviour")
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            main_card->add_view(s4_title);

            // Focus Follows Mouse
            add_toggle_row(
                "Focus follows mouse (Sloppy focus)",
                "Moving mouse cursor over a window automatically shifts keyboard focus to it",
                item->tiling_cfg->focus_follows_mouse,
                [item](bool val) {
                    item->tiling_cfg->focus_follows_mouse = val;
                }
            );

            // Click to Focus & Raise
            add_toggle_row(
                "Click to focus and raise",
                "Clicking anywhere on an unfocused window brings it to the top of its layer and activates it",
                item->tiling_cfg->click_to_focus,
                [item](bool val) {
                    item->tiling_cfg->click_to_focus = val;
                }
            );

            // Warp Cursor to Focused Window
            add_toggle_row(
                "Warp cursor on focus change",
                "Automatically moves mouse pointer to the center of the focused window on keyboard navigation",
                item->tiling_cfg->warp_cursor_on_focus,
                [item](bool val) {
                    item->tiling_cfg->warp_cursor_on_focus = val;
                }
            );

            // Focus on Workspace Switch
            add_toggle_row(
                "Focus on workspace switch",
                "Automatically restores focus to the last active window when switching workspaces",
                item->tiling_cfg->focus_on_workspace_switch,
                [item](bool val) {
                    item->tiling_cfg->focus_on_workspace_switch = val;
                }
            );
        }

        add_preview_card(main_card);
        return;
    }

    // Dedicated Single-Card Interactive UI for Screen Timeout & Lock
    if (item->is_timeout_and_lock) {
        auto main_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 16)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto add_divider = [&main_card, config]() {
            auto divider = LinearLayoutBuilder::create()
                ->backgroundColor(config->colors.outline.with_alpha(0.12f))
                ->build();
            LayoutParams dlp;
            dlp.width = static_cast<int>(LayoutDimension::MatchParent);
            dlp.height = 1;
            main_card->add_view(divider, dlp);
        };

        // -------------------------------------------------------------
        // Section 1: Inactivity Timers
        // -------------------------------------------------------------
        auto s1_title = TextViewBuilder::create()
            ->text("Inactivity Timers")
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        main_card->add_view(s1_title);

        // Timer 1: Lock Screen Inactivity
        std::vector<int> lock_steps = {1, 2, 5, 10, 15, 30, 0};
        auto format_lock_mins = [](int mins) -> std::string {
            if (mins == 0) return "Never";
            if (mins == 1) return "1 minute";
            return std::to_string(mins) + " minutes";
        };

        auto l_header = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto l_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto l_title = TextViewBuilder::create()
            ->text("Lock screen after inactivity")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        l_box->add_view(l_title);

        auto l_hint = TextViewBuilder::create()
            ->text("Protects your session and hides open windows")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        l_box->add_view(l_hint);

        LayoutParams lbox_lp;
        lbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        lbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        lbox_lp.weight = 1.0f;
        lbox_lp.gravity = Gravity::CenterVertical;
        l_header->add_view(l_box, lbox_lp);

        auto l_val_label = TextViewBuilder::create()
            ->text(format_lock_mins(item->timeout_lock_cfg.lock_timeout_minutes))
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams lval_lp;
        lval_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        lval_lp.gravity = Gravity::CenterVertical;
        l_header->add_view(l_val_label, lval_lp);
        main_card->add_view(l_header);

        int cur_l_idx = 0;
        for (size_t i = 0; i < lock_steps.size(); ++i) {
            if (lock_steps[i] == item->timeout_lock_cfg.lock_timeout_minutes) {
                cur_l_idx = static_cast<int>(i);
                break;
            }
        }
        float initial_l_val = static_cast<float>(cur_l_idx) / static_cast<float>(lock_steps.size() - 1);

        auto l_slider = SliderBuilder::create()
            ->value(initial_l_val)
            ->onValueChanged([this, item, l_val_label, lock_steps, format_lock_mins](float val, bool user) {
                (void)user;
                int idx = static_cast<int>(std::round(val * static_cast<float>(lock_steps.size() - 1)));
                idx = std::clamp(idx, 0, static_cast<int>(lock_steps.size() - 1));
                int mins = lock_steps[idx];
                item->timeout_lock_cfg.lock_timeout_minutes = mins;
                l_val_label->set_text(format_lock_mins(mins));
                l_val_label->request_redraw();
                notify_state_changed();
            })
            ->build();
        LayoutParams lsl_lp;
        lsl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        lsl_lp.height = 24;
        main_card->add_view(l_slider, lsl_lp);

        // Timer 2: Turn Off Display Inactivity
        std::vector<int> disp_steps = {1, 2, 5, 10, 15, 30, 60, 0};
        auto format_disp_mins = [](int mins) -> std::string {
            if (mins == 0) return "Never";
            if (mins == 1) return "1 minute";
            return std::to_string(mins) + " minutes";
        };

        auto d_header = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto d_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto d_title = TextViewBuilder::create()
            ->text("Turn off display after inactivity")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        d_box->add_view(d_title);

        auto d_hint = TextViewBuilder::create()
            ->text("Powers off screen backlight to save power and battery")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        d_box->add_view(d_hint);

        LayoutParams dbox_lp;
        dbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        dbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        dbox_lp.weight = 1.0f;
        dbox_lp.gravity = Gravity::CenterVertical;
        d_header->add_view(d_box, dbox_lp);

        auto d_val_label = TextViewBuilder::create()
            ->text(format_disp_mins(item->timeout_lock_cfg.display_off_minutes))
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams dval_lp;
        dval_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        dval_lp.gravity = Gravity::CenterVertical;
        d_header->add_view(d_val_label, dval_lp);
        main_card->add_view(d_header);

        int cur_d_idx = 2; // 5 min default
        for (size_t i = 0; i < disp_steps.size(); ++i) {
            if (disp_steps[i] == item->timeout_lock_cfg.display_off_minutes) {
                cur_d_idx = static_cast<int>(i);
                break;
            }
        }
        float initial_d_val = static_cast<float>(cur_d_idx) / static_cast<float>(disp_steps.size() - 1);

        auto d_slider = SliderBuilder::create()
            ->value(initial_d_val)
            ->onValueChanged([this, item, d_val_label, disp_steps, format_disp_mins](float val, bool user) {
                (void)user;
                int idx = static_cast<int>(std::round(val * static_cast<float>(disp_steps.size() - 1)));
                idx = std::clamp(idx, 0, static_cast<int>(disp_steps.size() - 1));
                int mins = disp_steps[idx];
                item->timeout_lock_cfg.display_off_minutes = mins;
                d_val_label->set_text(format_disp_mins(mins));
                d_val_label->request_redraw();
                notify_state_changed();
            })
            ->build();
        LayoutParams dsl_lp;
        dsl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        dsl_lp.height = 24;
        main_card->add_view(d_slider, dsl_lp);

        // Timer 3: Sleep Inactivity
        std::vector<int> sleep_steps = {5, 10, 15, 30, 60, 120, 0};
        auto format_sleep_mins = [](int mins) -> std::string {
            if (mins == 0) return "Never";
            if (mins == 60) return "1 hour";
            if (mins > 60) return std::to_string(mins / 60) + " hours";
            return std::to_string(mins) + " minutes";
        };

        auto sleep_header = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto sleep_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto sleep_title = TextViewBuilder::create()
            ->text("Put computer to sleep after inactivity")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        sleep_box->add_view(sleep_title);

        auto sleep_hint = TextViewBuilder::create()
            ->text("Pauses apps and halts CPU while holding memory in RAM")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        sleep_box->add_view(sleep_hint);

        LayoutParams sleepbox_lp;
        sleepbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        sleepbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        sleepbox_lp.weight = 1.0f;
        sleepbox_lp.gravity = Gravity::CenterVertical;
        sleep_header->add_view(sleep_box, sleepbox_lp);

        auto sleep_val_label = TextViewBuilder::create()
            ->text(format_sleep_mins(item->timeout_lock_cfg.sleep_minutes))
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams sleepval_lp;
        sleepval_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        sleepval_lp.gravity = Gravity::CenterVertical;
        sleep_header->add_view(sleep_val_label, sleepval_lp);
        main_card->add_view(sleep_header);

        int cur_sleep_idx = 2; // 15 min default
        for (size_t i = 0; i < sleep_steps.size(); ++i) {
            if (sleep_steps[i] == item->timeout_lock_cfg.sleep_minutes) {
                cur_sleep_idx = static_cast<int>(i);
                break;
            }
        }
        float initial_sleep_val = static_cast<float>(cur_sleep_idx) / static_cast<float>(sleep_steps.size() - 1);

        auto sleep_slider = SliderBuilder::create()
            ->value(initial_sleep_val)
            ->onValueChanged([this, item, sleep_val_label, sleep_steps, format_sleep_mins](float val, bool user) {
                (void)user;
                int idx = static_cast<int>(std::round(val * static_cast<float>(sleep_steps.size() - 1)));
                idx = std::clamp(idx, 0, static_cast<int>(sleep_steps.size() - 1));
                int mins = sleep_steps[idx];
                item->timeout_lock_cfg.sleep_minutes = mins;
                sleep_val_label->set_text(format_sleep_mins(mins));
                sleep_val_label->request_redraw();
                notify_state_changed();
            })
            ->build();
        LayoutParams sleepsl_lp;
        sleepsl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        sleepsl_lp.height = 24;
        main_card->add_view(sleep_slider, sleepsl_lp);

        // Timer 4: Auto-Hibernate Delay
        std::vector<int> hib_steps = {30, 60, 120, 240, 0};
        auto format_hib_mins = [](int mins) -> std::string {
            if (mins == 0) return "Never (Stay in sleep)";
            if (mins == 60) return "1 hour";
            if (mins > 60) return std::to_string(mins / 60) + " hours";
            return std::to_string(mins) + " minutes";
        };

        auto hib_header = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto hib_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto hib_title = TextViewBuilder::create()
            ->text("Auto-hibernate after prolonged sleep")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        hib_box->add_view(hib_title);

        auto hib_hint = TextViewBuilder::create()
            ->text("Writes memory to swap disk to eliminate battery drain while asleep")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        hib_box->add_view(hib_hint);

        LayoutParams hibbox_lp;
        hibbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        hibbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        hibbox_lp.weight = 1.0f;
        hibbox_lp.gravity = Gravity::CenterVertical;
        hib_header->add_view(hib_box, hibbox_lp);

        auto hib_val_label = TextViewBuilder::create()
            ->text(format_hib_mins(item->timeout_lock_cfg.hibernate_delay_minutes))
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams hibval_lp;
        hibval_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        hibval_lp.gravity = Gravity::CenterVertical;
        hib_header->add_view(hib_val_label, hibval_lp);
        main_card->add_view(hib_header);

        int cur_hib_idx = 2; // 120 min default
        for (size_t i = 0; i < hib_steps.size(); ++i) {
            if (hib_steps[i] == item->timeout_lock_cfg.hibernate_delay_minutes) {
                cur_hib_idx = static_cast<int>(i);
                break;
            }
        }
        float initial_hib_val = static_cast<float>(cur_hib_idx) / static_cast<float>(hib_steps.size() - 1);

        auto hib_slider = SliderBuilder::create()
            ->value(initial_hib_val)
            ->onValueChanged([this, item, hib_val_label, hib_steps, format_hib_mins](float val, bool user) {
                (void)user;
                int idx = static_cast<int>(std::round(val * static_cast<float>(hib_steps.size() - 1)));
                idx = std::clamp(idx, 0, static_cast<int>(hib_steps.size() - 1));
                int mins = hib_steps[idx];
                item->timeout_lock_cfg.hibernate_delay_minutes = mins;
                hib_val_label->set_text(format_hib_mins(mins));
                hib_val_label->request_redraw();
                notify_state_changed();
            })
            ->build();
        LayoutParams hibsl_lp;
        hibsl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        hibsl_lp.height = 24;
        main_card->add_view(hib_slider, hibsl_lp);

        add_divider();

        // -------------------------------------------------------------
        // Section 2: Power Triggers & Sleep Modes (systemd-logind)
        // -------------------------------------------------------------
        auto s2_title = TextViewBuilder::create()
            ->text("Power Triggers & Sleep Modes")
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        main_card->add_view(s2_title);

        // 1. Sleep Mode
        auto sm_block = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(6)
            ->build();

        auto sm_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto sm_title = TextViewBuilder::create()
            ->text("System sleep mode")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        sm_box->add_view(sm_title);

        auto sm_hint = TextViewBuilder::create()
            ->text("Low-power hardware state when sleep is invoked")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        sm_box->add_view(sm_hint);
        sm_block->add_view(sm_box);

        std::vector<std::string> sleep_modes = {
            "Hybrid Sleep (Recommended)",
            "Sleep (RAM)",
            "Hibernate (Disk)"
        };
        int cur_sm_idx = 0;
        for (size_t i = 0; i < sleep_modes.size(); ++i) {
            if (sleep_modes[i] == item->timeout_lock_cfg.sleep_mode) {
                cur_sm_idx = static_cast<int>(i);
                break;
            }
        }
        auto sm_spinner = SpinnerBuilder::create()
            ->items(sleep_modes)
            ->selectedIndex(cur_sm_idx)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(8)
            ->onItemSelected([this, item](int idx, const std::string& val) {
                (void)idx;
                item->timeout_lock_cfg.sleep_mode = val;
                notify_state_changed();
            })
            ->build();
        LayoutParams smsp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        sm_block->add_view(sm_spinner, smsp_lp);
        main_card->add_view(sm_block);

        // 2. Lid Switch Action
        auto lid_action_block = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(6)
            ->build();

        auto la_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto la_title = TextViewBuilder::create()
            ->text("When laptop lid is closed")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        la_box->add_view(la_title);

        auto la_hint = TextViewBuilder::create()
            ->text("Hardware action triggered upon shutting laptop lid (HandleLidSwitch)")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        la_box->add_view(la_hint);
        lid_action_block->add_view(la_box);

        std::vector<std::string> lid_actions = {
            "Sleep",
            "Lock session only",
            "Hibernate",
            "Turn off display only",
            "Ignore (Do nothing)"
        };
        int cur_la_idx = 0;
        for (size_t i = 0; i < lid_actions.size(); ++i) {
            if (lid_actions[i] == item->timeout_lock_cfg.lid_action) {
                cur_la_idx = static_cast<int>(i);
                break;
            }
        }
        auto la_spinner = SpinnerBuilder::create()
            ->items(lid_actions)
            ->selectedIndex(cur_la_idx)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(8)
            ->onItemSelected([this, item](int idx, const std::string& val) {
                (void)idx;
                item->timeout_lock_cfg.lid_action = val;
                notify_state_changed();
            })
            ->build();
        LayoutParams lasp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        lid_action_block->add_view(la_spinner, lasp_lp);
        main_card->add_view(lid_action_block);

        // 3. Power Button Action
        auto pwr_action_block = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(6)
            ->build();

        auto pa_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto pa_title = TextViewBuilder::create()
            ->text("When power button is pressed")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        pa_box->add_view(pa_title);

        auto pa_hint = TextViewBuilder::create()
            ->text("Action triggered on physical power button press (HandlePowerKey)")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        pa_box->add_view(pa_hint);
        pwr_action_block->add_view(pa_box);

        std::vector<std::string> pwr_actions = {
            "Show power menu (miqupower)",
            "Sleep",
            "Lock session",
            "Hibernate",
            "Power off immediately"
        };
        int cur_pa_idx = 0;
        for (size_t i = 0; i < pwr_actions.size(); ++i) {
            if (pwr_actions[i] == item->timeout_lock_cfg.power_button_action) {
                cur_pa_idx = static_cast<int>(i);
                break;
            }
        }
        auto pa_spinner = SpinnerBuilder::create()
            ->items(pwr_actions)
            ->selectedIndex(cur_pa_idx)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(8)
            ->onItemSelected([this, item](int idx, const std::string& val) {
                (void)idx;
                item->timeout_lock_cfg.power_button_action = val;
                notify_state_changed();
            })
            ->build();
        LayoutParams pasp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        pwr_action_block->add_view(pa_spinner, pasp_lp);
        main_card->add_view(pwr_action_block);

        // 4. Critical Battery Action
        auto crit_action_block = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(6)
            ->build();

        auto ca_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto ca_title = TextViewBuilder::create()
            ->text("Critical battery level action")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        ca_box->add_view(ca_title);

        auto ca_hint = TextViewBuilder::create()
            ->text("Emergency power action when battery reaches 5% (UPower)")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        ca_box->add_view(ca_hint);
        crit_action_block->add_view(ca_box);

        std::vector<std::string> crit_actions = {
            "Hibernate",
            "Sleep",
            "Power off immediately"
        };
        int cur_ca_idx = 0;
        for (size_t i = 0; i < crit_actions.size(); ++i) {
            if (crit_actions[i] == item->timeout_lock_cfg.critical_battery_action) {
                cur_ca_idx = static_cast<int>(i);
                break;
            }
        }
        auto ca_spinner = SpinnerBuilder::create()
            ->items(crit_actions)
            ->selectedIndex(cur_ca_idx)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(8)
            ->onItemSelected([this, item](int idx, const std::string& val) {
                (void)idx;
                item->timeout_lock_cfg.critical_battery_action = val;
                notify_state_changed();
            })
            ->build();
        LayoutParams casp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        crit_action_block->add_view(ca_spinner, casp_lp);
        main_card->add_view(crit_action_block);

        add_divider();

        // -------------------------------------------------------------
        // Section 3: Display & Power Behavior
        // -------------------------------------------------------------
        auto s3_title = TextViewBuilder::create()
            ->text("Display & Power Behavior")
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        main_card->add_view(s3_title);

        auto dim_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto dim_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto dim_title = TextViewBuilder::create()
            ->text("Dim screen before turning off")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        dim_box->add_view(dim_title);

        auto dim_hint = TextViewBuilder::create()
            ->text("Gradually reduces brightness as a gentle warning before sleep")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        dim_box->add_view(dim_hint);

        LayoutParams dimbox_lp;
        dimbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        dimbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        dimbox_lp.weight = 1.0f;
        dimbox_lp.gravity = Gravity::CenterVertical;
        dim_row->add_view(dim_box, dimbox_lp);

        auto dim_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.dim_screen)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.dim_screen = checked;
                notify_state_changed();
            })
            ->build();
        LayoutParams dimsw_lp;
        dimsw_lp.width = 44;
        dimsw_lp.height = 24;
        dimsw_lp.gravity = Gravity::CenterVertical;
        dim_row->add_view(dim_switch, dimsw_lp);
        main_card->add_view(dim_row);

        auto prev_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto prev_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto prev_title = TextViewBuilder::create()
            ->text("Prevent sleep during media playback")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        prev_box->add_view(prev_title);

        auto prev_hint = TextViewBuilder::create()
            ->text("Keeps screen awake while watching video or listening to audio")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        prev_box->add_view(prev_hint);

        LayoutParams prevbox_lp;
        prevbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        prevbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        prevbox_lp.weight = 1.0f;
        prevbox_lp.gravity = Gravity::CenterVertical;
        prev_row->add_view(prev_box, prevbox_lp);

        auto prev_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.prevent_sleep_media)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.prevent_sleep_media = checked;
                notify_state_changed();
            })
            ->build();
        LayoutParams prevsw_lp;
        prevsw_lp.width = 44;
        prevsw_lp.height = 24;
        prevsw_lp.gravity = Gravity::CenterVertical;
        prev_row->add_view(prev_switch, prevsw_lp);
        main_card->add_view(prev_row);

        auto bat_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto bat_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto bat_title = TextViewBuilder::create()
            ->text("Shorten timeouts on battery power")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        bat_box->add_view(bat_title);

        auto bat_hint = TextViewBuilder::create()
            ->text("Automatically reduces inactivity delays by 50% when running on battery")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        bat_box->add_view(bat_hint);

        LayoutParams batbox_lp;
        batbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        batbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        batbox_lp.weight = 1.0f;
        batbox_lp.gravity = Gravity::CenterVertical;
        bat_row->add_view(bat_box, batbox_lp);

        auto bat_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.energy_saver_battery)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.energy_saver_battery = checked;
                notify_state_changed();
            })
            ->build();
        LayoutParams batsw_lp;
        batsw_lp.width = 44;
        batsw_lp.height = 24;
        batsw_lp.gravity = Gravity::CenterVertical;
        bat_row->add_view(bat_switch, batsw_lp);
        main_card->add_view(bat_row);

        auto clam_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto clam_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto clam_title = TextViewBuilder::create()
            ->text("Stay awake when closed if external monitor is connected")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        clam_box->add_view(clam_title);

        auto clam_hint = TextViewBuilder::create()
            ->text("Clamshell docking • Continues display output to external monitor")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        clam_box->add_view(clam_hint);

        LayoutParams clambox_lp;
        clambox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        clambox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        clambox_lp.weight = 1.0f;
        clambox_lp.gravity = Gravity::CenterVertical;
        clam_row->add_view(clam_box, clambox_lp);

        auto clam_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.clamshell_mode)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.clamshell_mode = checked;
                notify_state_changed();
            })
            ->build();
        LayoutParams clamsw_lp;
        clamsw_lp.width = 44;
        clamsw_lp.height = 24;
        clamsw_lp.gravity = Gravity::CenterVertical;
        clam_row->add_view(clam_switch, clamsw_lp);
        main_card->add_view(clam_row);

        add_divider();

        // -------------------------------------------------------------
        // Section 4: Security & Lockscreen (miqulock)
        // -------------------------------------------------------------
        auto s4_title = TextViewBuilder::create()
            ->text("Security & Lockscreen (miqulock)")
            ->caption(true)
            ->textColor(config->colors.primary)
            ->build();
        main_card->add_view(s4_title);

        // 1. Lock before sleep
        auto lock_sleep_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto ls_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto ls_title = TextViewBuilder::create()
            ->text("Lock session before system enters sleep")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        ls_box->add_view(ls_title);

        auto ls_hint = TextViewBuilder::create()
            ->text("miquidle before_sleep_cmd • Delays suspend until screen is fully locked")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        ls_box->add_view(ls_hint);

        LayoutParams lsbox_lp;
        lsbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        lsbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        lsbox_lp.weight = 1.0f;
        lsbox_lp.gravity = Gravity::CenterVertical;
        lock_sleep_row->add_view(ls_box, lsbox_lp);

        auto ls_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.lock_before_sleep)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.lock_before_sleep = checked;
                notify_state_changed();
            })
            ->build();
        LayoutParams lssw_lp;
        lssw_lp.width = 44;
        lssw_lp.height = 24;
        lssw_lp.gravity = Gravity::CenterVertical;
        lock_sleep_row->add_view(ls_switch, lssw_lp);
        main_card->add_view(lock_sleep_row);

        // 2. Lock on display off
        auto dispoff_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto dispoff_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto dispoff_title = TextViewBuilder::create()
            ->text("Lock session when display turns off")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        dispoff_box->add_view(dispoff_title);

        auto dispoff_hint = TextViewBuilder::create()
            ->text("Secures session as soon as screen powers down or sleeps")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        dispoff_box->add_view(dispoff_hint);

        LayoutParams dispoffbox_lp;
        dispoffbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        dispoffbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        dispoffbox_lp.weight = 1.0f;
        dispoffbox_lp.gravity = Gravity::CenterVertical;
        dispoff_row->add_view(dispoff_box, dispoffbox_lp);

        auto dispoff_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.lock_on_display_off)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.lock_on_display_off = checked;
                notify_state_changed();
            })
            ->build();
        LayoutParams dispoffsw_lp;
        dispoffsw_lp.width = 44;
        dispoffsw_lp.height = 24;
        dispoffsw_lp.gravity = Gravity::CenterVertical;
        dispoff_row->add_view(dispoff_switch, dispoffsw_lp);
        main_card->add_view(dispoff_row);

        // 3. Require password immediately
        auto pwd_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto pwd_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto pwd_title = TextViewBuilder::create()
            ->text("Require password immediately on wake")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        pwd_box->add_view(pwd_title);

        auto pwd_hint = TextViewBuilder::create()
            ->text("Prompts for password instantly when screen is engaged")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        pwd_box->add_view(pwd_hint);

        LayoutParams pwdbox_lp;
        pwdbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        pwdbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        pwdbox_lp.weight = 1.0f;
        pwdbox_lp.gravity = Gravity::CenterVertical;
        pwd_row->add_view(pwd_box, pwdbox_lp);

        auto pwd_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.require_password_immediately)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.require_password_immediately = checked;
                if (checked) {
                    item->timeout_lock_cfg.grace_period_seconds = 0;
                } else if (item->timeout_lock_cfg.grace_period_seconds == 0) {
                    item->timeout_lock_cfg.grace_period_seconds = 15;
                }
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();
        LayoutParams pwdsw_lp;
        pwdsw_lp.width = 44;
        pwdsw_lp.height = 24;
        pwdsw_lp.gravity = Gravity::CenterVertical;
        pwd_row->add_view(pwd_switch, pwdsw_lp);
        main_card->add_view(pwd_row);

        auto format_grace = [](int secs) -> std::string {
            if (secs == 0) return "0 seconds (Instant)";
            if (secs < 60) return std::to_string(secs) + " seconds";
            return std::to_string(secs / 60) + (secs == 60 ? " minute" : " minutes");
        };

        if (!item->timeout_lock_cfg.require_password_immediately) {
            auto g_header = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto g_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto g_title = TextViewBuilder::create()
                ->text("Grace period window")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            g_box->add_view(g_title);

            auto g_hint = TextViewBuilder::create()
                ->text("Wake without entering password if activity occurs within window")
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            g_box->add_view(g_hint);

            LayoutParams grow_lp;
            grow_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            grow_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            grow_lp.weight = 1.0f;
            grow_lp.gravity = Gravity::CenterVertical;
            g_header->add_view(g_box, grow_lp);

            std::vector<int> grace_steps = {5, 15, 30, 60, 120, 300};

            auto g_val_label = TextViewBuilder::create()
                ->text(format_grace(item->timeout_lock_cfg.grace_period_seconds))
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->build();
            LayoutParams gval_lp;
            gval_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            gval_lp.gravity = Gravity::CenterVertical;
            g_header->add_view(g_val_label, gval_lp);
            main_card->add_view(g_header);

            int cur_g_idx = 1;
            for (size_t i = 0; i < grace_steps.size(); ++i) {
                if (grace_steps[i] == item->timeout_lock_cfg.grace_period_seconds) {
                    cur_g_idx = static_cast<int>(i);
                    break;
                }
            }
            float initial_g_val = static_cast<float>(cur_g_idx) / static_cast<float>(grace_steps.size() - 1);

            auto g_slider = SliderBuilder::create()
                ->value(initial_g_val)
                ->onValueChanged([this, item, g_val_label, grace_steps, format_grace](float val, bool user) {
                    (void)user;
                    int idx = static_cast<int>(std::round(val * static_cast<float>(grace_steps.size() - 1)));
                    idx = std::clamp(idx, 0, static_cast<int>(grace_steps.size() - 1));
                    int secs = grace_steps[idx];
                    item->timeout_lock_cfg.grace_period_seconds = secs;
                    g_val_label->set_text(format_grace(secs));
                    g_val_label->request_redraw();
                    notify_state_changed();
                })
                ->build();
            LayoutParams gsl_lp;
            gsl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            gsl_lp.height = 24;
            main_card->add_view(g_slider, gsl_lp);
        }

        // 4. Show power buttons on lockscreen
        auto act_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto act_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto act_title = TextViewBuilder::create()
            ->text("Show power buttons on lockscreen")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        act_box->add_view(act_title);

        auto act_hint = TextViewBuilder::create()
            ->text("miqulock show_power_actions • Sleep, Hibernate, and Power Off controls")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        act_box->add_view(act_hint);

        LayoutParams actbox_lp;
        actbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        actbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        actbox_lp.weight = 1.0f;
        actbox_lp.gravity = Gravity::CenterVertical;
        act_row->add_view(act_box, actbox_lp);

        auto act_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.lockscreen_show_power_actions)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.lockscreen_show_power_actions = checked;
                notify_state_changed();
            })
            ->build();
        LayoutParams actsw_lp;
        actsw_lp.width = 44;
        actsw_lp.height = 24;
        actsw_lp.gravity = Gravity::CenterVertical;
        act_row->add_view(act_switch, actsw_lp);
        main_card->add_view(act_row);

        // 5. 24-hour clock on lockscreen
        auto clk_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto clk_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto clk_title = TextViewBuilder::create()
            ->text("Use 24-hour clock on lockscreen")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        clk_box->add_view(clk_title);

        auto clk_hint = TextViewBuilder::create()
            ->text("miqulock time_format • Displays %H:%M instead of 12-hour AM/PM")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        clk_box->add_view(clk_hint);

        LayoutParams clkbox_lp;
        clkbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        clkbox_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        clkbox_lp.weight = 1.0f;
        clkbox_lp.gravity = Gravity::CenterVertical;
        clk_row->add_view(clk_box, clkbox_lp);

        auto clk_switch = SwitchBuilder::create()
            ->checked(item->timeout_lock_cfg.lockscreen_24h_clock)
            ->onCheckedChanged([this, item](bool checked) {
                item->timeout_lock_cfg.lockscreen_24h_clock = checked;
                notify_state_changed();
            })
            ->build();
        LayoutParams clksw_lp;
        clksw_lp.width = 44;
        clksw_lp.height = 24;
        clksw_lp.gravity = Gravity::CenterVertical;
        clk_row->add_view(clk_switch, clksw_lp);
        main_card->add_view(clk_row);

        add_preview_card(main_card);

        // -------------------------------------------------------------
        // Definitions & Terminology Explained Card
        // -------------------------------------------------------------
        auto defs_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 16)
            ->spacing(10)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto defs_title_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->spacing(8)
            ->build();

        auto defs_icon = TextViewBuilder::create()
            ->text("󰋼")
            ->textSize(config->metrics.font_size + 2)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams deficon_lp;
        deficon_lp.width = 24;
        deficon_lp.gravity = Gravity::CenterVertical;
        defs_title_row->add_view(defs_icon, deficon_lp);

        auto defs_heading = TextViewBuilder::create()
            ->text("Power & Lock Terminology Explained")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        defs_title_row->add_view(defs_heading);
        defs_card->add_view(defs_title_row);

        auto add_def_entry = [&defs_card, config](const std::string& term, const std::string& meaning) {
            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto t_label = TextViewBuilder::create()
                ->text(term)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->build();
            row->add_view(t_label);

            auto m_label = TextViewBuilder::create()
                ->text(meaning)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.85f))
                ->build();
            row->add_view(m_label);

            LayoutParams r_lp;
            r_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            r_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            defs_card->add_view(row, r_lp);
        };

        add_def_entry("Display Off (DPMS)", "Powers down the screen backlight only. The CPU, Wi-Fi, and background applications continue running normally at full speed.");
        add_def_entry("Sleep (Suspend to RAM)", "Pauses the CPU and freezes open applications in RAM with minimal power draw (~0.3W). Wakes back to your desktop in 1 to 2 seconds.");
        add_def_entry("Hibernate (Suspend to Disk)", "Writes a complete memory snapshot to swap disk and cuts 100% of power (0W). Preserves your open work indefinitely even on zero battery.");
        add_def_entry("Hybrid Sleep", "Enters Sleep immediately for fast 1-second wake, then automatically Hibernates to swap disk after prolonged inactivity to protect the battery.");
        add_def_entry("Lock Screen (miqulock)", "Obscures your workspace behind PAM authentication. Requires your password to unlock while apps continue running uninterrupted in the background.");
        add_def_entry("Grace Period", "A brief safety window right after the display turns off where pressing any key immediately resumes your desktop without asking for your password.");
        add_def_entry("Clamshell Docking", "Allows you to close your laptop lid without triggering sleep or lock when an external monitor and keyboard/mouse are connected.");

        add_preview_card(defs_card);

        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, item->telemetry_title.empty() ? "Inactivity & security specifications" : item->telemetry_title));
        }
        return;
    }

    // Dedicated Minimal UI for Night Light
    if (item->is_display_nightlight) {
        // 1. Hero switch card (Enable or Disable)
        bool is_nl_on = item->nightlight_cfg.enabled;
        auto hero_sw_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
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
                rebuild_preview_column();
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
                ->cornerRadius(12)
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


    // Dedicated Interactive UI for Battery Info
    if (item->is_battery_info) {
        if (!item->battery_cfg) return;

        // 1. Battery Master Status Card (Percentage, State, Remaining Time)
        auto hero_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
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

    // Dedicated Interactive UI for Power & Performance Mode
    if (item->is_power_mode) {
        if (!item->battery_cfg) return;

        // 1. Operating Profile Card with Spinner
        auto prof_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
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

        auto prof_spinner = SpinnerBuilder::create()
            ->items(item->battery_cfg->available_profiles)
            ->selectedIndex(prof_sel_idx)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(8)
            ->onItemSelected([this, item](int idx, const std::string& val) {
                (void)idx;
                item->battery_cfg->power_profile = val;
                item->battery_cfg->power_saver = (val == "Power Saver");
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();

        LayoutParams psp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        prof_card->add_view(prof_spinner, psp_lp);

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

    // Dedicated Interactive UI for Monitors & Displays
    if (item->is_monitor) {
        // 1. Monitor Banner & Primary Display Status Card
        auto mon_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
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
                ->cornerRadius(6)
                ->elevation(0)
                ->addView(prim_badge)
                ->build();

            LayoutParams pill_lp;
            pill_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            pill_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            pill_lp.gravity = Gravity::CenterVertical;
            mon_card->add_view(pill_card, pill_lp);
        } else {
            auto make_prim_btn = ButtonBuilder::create()
                ->text("Make Primary")
                ->primary(true)
                ->padding(10, 4)
                ->cornerRadius(6)
                ->onClick([this, item]() {
                    const auto& list = get_current_list();
                    for (auto& m : list) {
                        if (m->is_monitor) {
                            m->monitor_cfg.is_primary = (m == item);
                        }
                    }
                    rebuild_current_column();
                    rebuild_preview_column();
                    notify_state_changed();
                })
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
            ->cornerRadius(12)
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
                ->padding(0, 3)
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
                ->cornerRadius(8)
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

    // Dedicated Clean Layout for Installed Application
    if (item->is_installed_app) {
        // 1. App Banner Card with Category on a new line, Actions on a new line, and Description
        auto app_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        // Row 1: App Icon and Labels (Title, Category on new line, Version)
        auto top_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(12)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto app_icon = TextViewBuilder::create()
            ->text(item->icon.empty() ? "󰏖" : item->icon)
            ->textSize(config->metrics.font_size + 6)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams aicon_lp;
        aicon_lp.width = 32;
        aicon_lp.gravity = Gravity::CenterVertical;
        top_row->add_view(app_icon, aicon_lp);

        auto app_lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(4)
            ->build();

        // Line 1: App Title
        auto app_title = TextViewBuilder::create()
            ->text(item->app_info.app_name)
            ->textSize(config->metrics.font_size + 1)
            ->textColor(config->colors.on_surface)
            ->build();
        app_lbl_box->add_view(app_title);

        // Line 2: Category Badge on its own new line
        if (!item->app_info.category.empty()) {
            std::string cat_label = item->app_info.category;
            size_t paren = cat_label.find(" (");
            if (paren != std::string::npos) {
                cat_label = cat_label.substr(0, paren);
            }

            auto cat_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->gravity(Gravity::CenterVertical)
                ->build();

            auto badge = LinearLayoutBuilder::create()
                ->padding(6, 2)
                ->cornerRadius(4)
                ->backgroundColor(config->colors.primary.with_alpha(0.12f))
                ->stroke(1, config->colors.primary.with_alpha(0.3f))
                ->build();

            auto badge_text = TextViewBuilder::create()
                ->text(cat_label)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            badge->add_view(badge_text);

            cat_row->add_view(badge);
            app_lbl_box->add_view(cat_row);
        }

        // Line 3: Version, Publisher, and Process State
        std::string app_sub = "v" + item->app_info.version + " • " + item->app_info.publisher;
        if (item->app_info.is_running) {
            app_sub += " • Running (PID " + std::to_string(item->app_info.pid > 0 ? item->app_info.pid : 4821) + ")";
        } else {
            app_sub += " • Stopped";
        }
        auto app_hint = TextViewBuilder::create()
            ->text(app_sub)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        app_lbl_box->add_view(app_hint);

        LayoutParams albl_lp;
        albl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        albl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        albl_lp.weight = 1.0f;
        albl_lp.gravity = Gravity::CenterVertical;
        top_row->add_view(app_lbl_box, albl_lp);

        app_card->add_view(top_row);

        // Row 2: Actions (Open, Force Close, Uninstall) on their own new line
        auto actions_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(10)
            ->gravity(Gravity::CenterVertical)
            ->build();

        // 1. Open Button
        auto open_btn = ButtonBuilder::create()
            ->text("Open")
            ->primary(true)
            ->padding(16, 5)
            ->cornerRadius(6)
            ->onClick([this, item]() {
                item->app_info.is_running = true;
                if (item->app_info.pid <= 0) item->app_info.pid = 4920;
                item->last_action_message = "Launched " + item->app_info.app_name + " (running, PID " + std::to_string(item->app_info.pid) + ")";
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();
        LayoutParams open_lp;
        open_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        open_lp.height = 30;
        open_lp.gravity = Gravity::CenterVertical;
        actions_box->add_view(open_btn, open_lp);

        // 2. Force Close Button
        bool is_running = item->app_info.is_running;
        auto force_close_btn = ButtonBuilder::create()
            ->text("Force Close")
            ->style(ButtonStyle::Outlined)
            ->padding(14, 5)
            ->cornerRadius(6)
            ->onClick([this, item]() {
                if (item->app_info.is_running) {
                    int old_pid = item->app_info.pid;
                    item->app_info.is_running = false;
                    item->app_info.pid = 0;
                    item->last_action_message = "Force closed " + item->app_info.app_name + " (terminated PID " + std::to_string(old_pid > 0 ? old_pid : 4821) + ")";
                } else {
                    item->last_action_message = item->app_info.app_name + " is not running";
                }
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();
        if (!is_running) {
            force_close_btn->set_text_color(config->colors.on_surface_variant.with_alpha(0.4f));
        }
        LayoutParams fc_lp;
        fc_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        fc_lp.height = 30;
        fc_lp.gravity = Gravity::CenterVertical;
        actions_box->add_view(force_close_btn, fc_lp);

        // 3. Uninstall Button
        auto uninstall_btn = ButtonBuilder::create()
            ->text("Uninstall")
            ->style(ButtonStyle::Outlined)
            ->padding(14, 5)
            ->cornerRadius(6)
            ->onClick([this, item]() {
                item->last_action_message = "Uninstall requested for " + item->app_info.app_name;
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();
        LayoutParams un_lp;
        un_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        un_lp.height = 30;
        un_lp.gravity = Gravity::CenterVertical;
        actions_box->add_view(uninstall_btn, un_lp);

        if (!item->last_action_message.empty()) {
            auto feedback = TextViewBuilder::create()
                ->text(item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            LayoutParams fb_lp;
            fb_lp.gravity = Gravity::CenterVertical;
            actions_box->add_view(feedback, fb_lp);
        }

        app_card->add_view(actions_box);

        // Row 3: Description summary on new line
        if (!item->app_info.description.empty()) {
            auto desc_view = TextViewBuilder::create()
                ->text(item->app_info.description)
                ->wrap(true)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant.with_alpha(0.85f))
                ->build();
            LayoutParams desc_lp;
            desc_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            app_card->add_view(desc_view, desc_lp);
        }

        add_preview_card(app_card);

        // 2. Network Access Card (Simple & Concise)
        auto net_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(10)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        // Top Row: Icon + Title & Status Subtitle
        auto net_header = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(12)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto net_icon = TextViewBuilder::create()
            ->text("󰒍")
            ->textSize(config->metrics.font_size + 3)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams nico_lp;
        nico_lp.width = 24;
        nico_lp.gravity = Gravity::CenterVertical;
        net_header->add_view(net_icon, nico_lp);

        auto net_title_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto net_title = TextViewBuilder::create()
            ->text("Network Access")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        net_title_box->add_view(net_title);

        std::string initial_status = "Allowed (Full)";
        if (item->app_info.network_policy == "Local Only") {
            initial_status = "Local subnet only";
        } else if (item->app_info.network_policy == "Block") {
            initial_status = "Blocked";
        }

        auto net_sub = TextViewBuilder::create()
            ->text(initial_status)
            ->caption(true)
            ->textColor(item->app_info.network_policy == "Block" ? config->colors.on_surface_variant : config->colors.primary)
            ->build();
        net_title_box->add_view(net_sub);

        LayoutParams ntb_lp;
        ntb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        ntb_lp.weight = 1.0f;
        ntb_lp.gravity = Gravity::CenterVertical;
        net_header->add_view(net_title_box, ntb_lp);

        net_card->add_view(net_header);

        // Spinner Selector on its own new line
        std::vector<std::string> spinner_items = {"Allow", "Local Only", "Block"};
        int sel_idx = 0;
        if (item->app_info.network_policy == "Local Only") sel_idx = 1;
        else if (item->app_info.network_policy == "Block") sel_idx = 2;

        auto net_spinner = SpinnerBuilder::create()
            ->items(spinner_items)
            ->selectedIndex(sel_idx)
            ->fontSize(config->metrics.font_size)
            ->padding(12, 6)
            ->cornerRadius(8)
            ->onItemSelected([item, spinner_items, net_sub, config](int idx, const std::string& val) {
                (void)val;
                if (idx >= 0 && idx < static_cast<int>(spinner_items.size())) {
                    item->app_info.network_policy = spinner_items[idx];
                    item->last_action_message = "Network access set to " + item->app_info.network_policy;
                    if (item->app_info.network_policy == "Allow") {
                        net_sub->set_text("Allowed (Full)");
                        net_sub->set_text_color(config->colors.primary);
                    } else if (item->app_info.network_policy == "Local Only") {
                        net_sub->set_text("Local subnet only");
                        net_sub->set_text_color(config->colors.primary);
                    } else {
                        net_sub->set_text("Blocked");
                        net_sub->set_text_color(config->colors.on_surface_variant);
                    }
                    net_sub->request_redraw();
                }
            })
            ->build();

        LayoutParams sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
        net_card->add_view(net_spinner, sp_lp);

        add_preview_card(net_card);

        // 3. Application & Storage Details Card
        auto details_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(8)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        // Header row with "Application & Storage Details" and "Clear Cache" button
        auto header_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->gravity(Gravity::CenterVertical)
            ->build();

        auto details_title = TextViewBuilder::create()
            ->text("Application & Storage Details")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        LayoutParams dt_lp;
        dt_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        dt_lp.weight = 1.0f;
        dt_lp.gravity = Gravity::CenterVertical;
        header_row->add_view(details_title, dt_lp);

        auto clear_cache_btn = ButtonBuilder::create()
            ->text("Clear Cache")
            ->flat(true)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->padding(10, 4)
            ->cornerRadius(6)
            ->onClick([this, item]() {
                item->app_info.size_data = "0 KB";
                item->app_info.size_total = item->app_info.size_binary;
                item->last_action_message = "Cache cleared (storage freed)";
                rebuild_current_column();
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();
        LayoutParams cc_lp;
        cc_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        cc_lp.height = 28;
        cc_lp.gravity = Gravity::CenterVertical;
        header_row->add_view(clear_cache_btn, cc_lp);
        details_card->add_view(header_row);

        if (!item->last_action_message.empty()) {
            auto feedback = TextViewBuilder::create()
                ->text(item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            details_card->add_view(feedback);
        }

        std::vector<std::pair<std::string, std::string>> detail_entries = {
            {"Category", item->app_info.category},
            {"Execution State", item->app_info.is_running ? ("Running (PID " + std::to_string(item->app_info.pid > 0 ? item->app_info.pid : 4821) + ")") : "Stopped"},
            {"Network Access", item->app_info.network_policy},
            {"Description", item->app_info.description},
            {"Total Size", item->app_info.size_total},
            {"Binary Size", item->app_info.size_binary},
            {"Data & Cache", item->app_info.size_data},
            {"Binary Path", item->app_info.binary_path},
            {"Desktop File", item->app_info.desktop_file},
            {"Package Source", item->app_info.package_type},
            {"Permissions", item->app_info.permissions}
        };

        for (const auto& [k, v] : detail_entries) {
            auto row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(2, 2)
                ->build();

            auto k_view = TextViewBuilder::create()
                ->text(k)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams k_lp;
            k_lp.width = 140;
            row->add_view(k_view, k_lp);

            auto v_view = TextViewBuilder::create()
                ->text(v)
                ->caption(true)
                ->wrap(true)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams v_lp;
            v_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            v_lp.weight = 1.0f;
            row->add_view(v_view, v_lp);

            details_card->add_view(row);
        }

        add_preview_card(details_card);

        // 3. Telemetry Card
        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, "Package & runtime specifications"));
        }

        return;
    }

    // Dedicated Clean Layout for Default Application Role
    if (item->is_default_app_role) {
        // 1. Role Header Banner Card
        auto role_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto role_icon = TextViewBuilder::create()
            ->text(item->icon.empty() ? "󰆍" : item->icon)
            ->textSize(config->metrics.font_size + 4)
            ->textColor(config->colors.primary)
            ->build();
        LayoutParams ricon_lp;
        ricon_lp.width = 30;
        ricon_lp.gravity = Gravity::CenterVertical;
        role_card->add_view(role_icon, ricon_lp);

        auto role_lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto role_title = TextViewBuilder::create()
            ->text(item->default_app_role.role_name)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        role_lbl_box->add_view(role_title);

        std::string role_hint_str = "Default application for " + item->default_app_role.mime_types;
        auto role_hint = TextViewBuilder::create()
            ->text(role_hint_str)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->ellipsize(true)
            ->build();
        role_lbl_box->add_view(role_hint);

        LayoutParams rlbl_lp;
        rlbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        rlbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        rlbl_lp.weight = 1.0f;
        rlbl_lp.gravity = Gravity::CenterVertical;
        role_card->add_view(role_lbl_box, rlbl_lp);

        // Find current default name
        std::string def_name = item->subtitle;
        for (const auto& c : item->default_app_role.candidates) {
            if (c.is_default) {
                def_name = c.name;
                break;
            }
        }

        auto def_pill = TextViewBuilder::create()
            ->text(def_name + " ✓")
            ->caption(true)
            ->textColor(config->colors.primary)
            ->padding(8, 4)
            ->build();

        auto pill_card = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(config->colors.primary_container.with_alpha(0.5f))
            ->cornerRadius(6)
            ->elevation(0)
            ->addView(def_pill)
            ->build();

        LayoutParams pill_lp;
        pill_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.gravity = Gravity::CenterVertical;
        role_card->add_view(pill_card, pill_lp);

        add_preview_card(role_card);

        // 2. Candidates Selector Card
        auto cand_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(8)
            ->padding(16, 14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto cand_title = TextViewBuilder::create()
            ->text("Available Applications")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        cand_card->add_view(cand_title);

        if (!item->last_action_message.empty()) {
            auto feedback = TextViewBuilder::create()
                ->text(item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.primary)
                ->build();
            cand_card->add_view(feedback);
        }

        for (size_t ci = 0; ci < item->default_app_role.candidates.size(); ++ci) {
            auto& cand = item->default_app_role.candidates[ci];
            bool is_def = cand.is_default;

            auto row_content = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(10, 8)
                ->spacing(12)
                ->build();

            auto c_icon = TextViewBuilder::create()
                ->text(cand.icon.empty() ? "󰀻" : cand.icon)
                ->textSize(config->metrics.font_size)
                ->textColor(is_def ? config->colors.primary : config->colors.on_surface_variant)
                ->build();
            LayoutParams cicon_lp;
            cicon_lp.width = 24;
            cicon_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(c_icon, cicon_lp);

            auto c_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(2)
                ->build();

            auto c_name = TextViewBuilder::create()
                ->text(cand.name)
                ->textSize(config->metrics.font_size)
                ->textColor(is_def ? config->colors.primary : config->colors.on_surface)
                ->build();
            c_box->add_view(c_name);

            std::string c_sub = is_def ? "Current default application" : cand.description;
            auto c_desc = TextViewBuilder::create()
                ->text(c_sub)
                ->caption(true)
                ->textColor(is_def ? config->colors.primary : config->colors.on_surface_variant.with_alpha(0.7f))
                ->build();
            c_box->add_view(c_desc);

            LayoutParams cbox_lp;
            cbox_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            cbox_lp.weight = 1.0f;
            cbox_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(c_box, cbox_lp);

            if (is_def) {
                auto check_view = TextViewBuilder::create()
                    ->text("✓")
                    ->textSize(config->metrics.font_size)
                    ->textColor(config->colors.primary)
                    ->build();
                LayoutParams chk_lp;
                chk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                chk_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(check_view, chk_lp);
            } else {
                auto set_btn = ButtonBuilder::create()
                    ->text("Set Default")
                    ->flat(true)
                    ->textColor(config->colors.primary)
                    ->padding(8, 2)
                    ->build();
                LayoutParams set_lp;
                set_lp.width = static_cast<int>(LayoutDimension::WrapContent);
                set_lp.gravity = Gravity::CenterVertical;
                row_content->add_view(set_btn, set_lp);
            }

            Color row_bg = is_def ? config->colors.primary_container.with_alpha(0.35f) : Color::transparent();
            auto cand_row_card = CardViewBuilder::create()
                ->style(CardStyle::Filled)
                ->backgroundColor(row_bg)
                ->cornerRadius(8)
                ->elevation(0)
                ->addView(row_content)
                ->build();

            cand_row_card->set_on_click_listener([this, item, ci]() {
                for (size_t k = 0; k < item->default_app_role.candidates.size(); ++k) {
                    item->default_app_role.candidates[k].is_default = (k == ci);
                }
                item->subtitle = item->default_app_role.candidates[ci].name;
                item->last_action_message = "Default updated to " + item->subtitle;
                rebuild_current_column();
                rebuild_preview_column();
                notify_state_changed();
            });

            cand_card->add_view(cand_row_card);
        }

        add_preview_card(cand_card);

        // 3. MIME Telemetry Card
        if (!item->telemetry.empty()) {
            add_preview_card(create_telemetry_card(item->telemetry, config, "MIME handler specifications"));
        }

        return;
    }

    // 1. Header Banner
    auto banner = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(12)
        ->padding(16, 14)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(12)
        ->build();

    auto left_info = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->spacing(8)
        ->build();

    auto title_text = TextViewBuilder::create()
        ->text(item->icon + "  " + item->title)
        ->h2()
        ->textColor(config->colors.on_surface)
        ->build();
    title_row->add_view(title_text);
    left_info->add_view(title_row);

    auto sub_text = TextViewBuilder::create()
        ->text(item->subtitle)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    left_info->add_view(sub_text);

    LayoutParams left_lp;
    left_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    left_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    left_lp.weight = 1.0f;
    banner->add_view(left_info, left_lp);

    // Status pill
    if (item->has_switch) {
        std::string badge_text = item->switch_val ? "Active" : "Disabled";
        Color badge_bg = item->switch_val ? config->colors.primary_container : config->colors.surface_variant;
        Color badge_fg = item->switch_val ? config->colors.primary : config->colors.on_surface_variant;

        auto status_pill = TextViewBuilder::create()
            ->text(badge_text)
            ->caption(true)
            ->textColor(badge_fg)
            ->padding(10, 6)
            ->build();

        auto pill_card = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(badge_bg)
            ->cornerRadius(14)
            ->elevation(0)
            ->addView(status_pill)
            ->build();

        LayoutParams pill_lp;
        pill_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.gravity = Gravity::CenterVertical;
        banner->add_view(pill_card, pill_lp);
    }
    add_preview_card(banner);

    // 2. Primary Toggle Card
    if (item->has_switch) {
        bool focused = m_preview_focused && (m_preview_control_index == 0);

        auto primary_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(focused ? 2 : 1, focused ? config->colors.primary : config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        auto lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto sw_title = TextViewBuilder::create()
            ->text(item->switch_label)
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        lbl_box->add_view(sw_title);

        auto sw_hint = TextViewBuilder::create()
            ->text("Primary toggle state [Space key to switch]")
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        lbl_box->add_view(sw_hint);

        LayoutParams lbl_lp;
        lbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        lbl_lp.weight = 1.0f;
        lbl_lp.gravity = Gravity::CenterVertical;
        primary_row->add_view(lbl_box, lbl_lp);

        m_active_primary_switch = SwitchBuilder::create()
            ->checked(item->switch_val)
            ->onCheckedChanged([this, item](bool checked) {
                item->switch_val = checked;
                rebuild_current_column();
                rebuild_preview_column();
                notify_state_changed();
            })
            ->build();

        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        primary_row->add_view(m_active_primary_switch, sw_lp);

        add_preview_card(primary_row);
    }

    // 3. Secondary Controls Card (Secondary switch, Slider, Action Button)
    if (item->has_secondary_switch || item->has_slider || item->has_action) {
        auto sub_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->padding(16, 14)
            ->spacing(14)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        // Secondary Switch
        if (item->has_secondary_switch) {
            bool focused = m_preview_focused && (m_preview_control_index == 1);

            auto sec_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->padding(6, 6)
                ->backgroundColor(focused ? config->colors.primary_container.with_alpha(0.2f) : Color::transparent())
                ->cornerRadius(6)
                ->build();

            auto sec_lbl = TextViewBuilder::create()
                ->text(item->secondary_switch_label)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams sec_lbl_lp;
            sec_lbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            sec_lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            sec_lbl_lp.weight = 1.0f;
            sec_lbl_lp.gravity = Gravity::CenterVertical;
            sec_row->add_view(sec_lbl, sec_lbl_lp);

            m_active_secondary_switch = SwitchBuilder::create()
                ->checked(item->secondary_switch_val)
                ->onCheckedChanged([this, item](bool checked) {
                    item->secondary_switch_val = checked;
                    notify_state_changed();
                })
                ->build();

            LayoutParams sec_sw_lp;
            sec_sw_lp.width = 44;
            sec_sw_lp.height = 24;
            sec_sw_lp.gravity = Gravity::CenterVertical;
            sec_row->add_view(m_active_secondary_switch, sec_sw_lp);

            sub_box->add_view(sec_row);
        }

        // Slider Control
        if (item->has_slider) {
            bool focused = m_preview_focused && (m_preview_control_index == 2);

            auto slider_box = LinearLayoutBuilder::create()
                ->orientation(Orientation::Vertical)
                ->spacing(6)
                ->padding(6, 6)
                ->backgroundColor(focused ? config->colors.primary_container.with_alpha(0.2f) : Color::transparent())
                ->cornerRadius(6)
                ->build();

            auto s_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->build();

            auto s_title = TextViewBuilder::create()
                ->text(item->slider_label)
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams s_title_lp;
            s_title_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            s_title_lp.weight = 1.0f;
            s_row->add_view(s_title, s_title_lp);

            int val_pct = static_cast<int>(item->slider_val * 100);
            m_active_slider_label = TextViewBuilder::create()
                ->text(std::to_string(val_pct) + " " + item->slider_unit)
                ->textColor(config->colors.primary)
                ->build();
            s_row->add_view(m_active_slider_label);
            slider_box->add_view(s_row);

            m_active_slider = SliderBuilder::create()
                ->value(item->slider_val)
                ->onValueChanged([this, item](float val, bool user) {
                    item->slider_val = val;
                    if (m_active_slider_label) {
                        int pct = static_cast<int>(val * 100);
                        m_active_slider_label->set_text(std::to_string(pct) + " " + item->slider_unit);
                    }
                    rebuild_current_column();
                    notify_state_changed();
                })
                ->build();

            LayoutParams slider_lp;
            slider_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            slider_lp.height = 24;
            slider_box->add_view(m_active_slider, slider_lp);

            sub_box->add_view(slider_box);
        }

        // Action Button
        if (item->has_action) {
            bool focused = m_preview_focused && (m_preview_control_index == 3);

            auto action_row = LinearLayoutBuilder::create()
                ->orientation(Orientation::Horizontal)
                ->spacing(12)
                ->padding(6, 6)
                ->backgroundColor(focused ? config->colors.primary_container.with_alpha(0.2f) : Color::transparent())
                ->cornerRadius(6)
                ->build();

            m_active_action_btn = ButtonBuilder::create()
                ->text(item->action_label)
                ->primary(true)
                ->onClick([this, item]() {
                    trigger_active_action();
                })
                ->build();

            LayoutParams btn_lp;
            btn_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            btn_lp.height = 34;
            action_row->add_view(m_active_action_btn, btn_lp);

            m_active_action_feedback = TextViewBuilder::create()
                ->text(item->last_action_message.empty() ? "Ready." : item->last_action_message)
                ->caption(true)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams fb_lp;
            fb_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            fb_lp.weight = 1.0f;
            fb_lp.gravity = Gravity::CenterVertical;
            action_row->add_view(m_active_action_feedback, fb_lp);

            sub_box->add_view(action_row);
        }

        add_preview_card(sub_box);
    }

    // 4. Telemetry / Diagnostics Card
    if (!item->telemetry.empty()) {
        std::string telem_title = "Technical specifications";
        if (!item->telemetry_title.empty()) {
            telem_title = item->telemetry_title;
        } else if (item->id.rfind("theme_", 0) == 0) {
            telem_title = "Theme specifications";
        } else if (item->id.rfind("fx_", 0) == 0) {
            telem_title = "Compositor specifications";
        } else if (item->id.rfind("nl_", 0) == 0 || item->id.rfind("disp_", 0) == 0) {
            telem_title = "Display specifications";
        } else if (item->id.rfind("audio_", 0) == 0) {
            telem_title = "Audio specifications";
        } else if (item->id.rfind("fw_", 0) == 0) {
            telem_title = "Firewall specifications";
        } else if (item->id.rfind("lock_", 0) == 0) {
            telem_title = "Security specifications";
        } else if (item->id.rfind("power_", 0) == 0) {
            telem_title = "Power specifications";
        } else if (item->id.rfind("diag_", 0) == 0) {
            telem_title = "System specifications";
        }
        add_preview_card(create_telemetry_card(item->telemetry, config, telem_title));
    }
}

bool MillerView::navigate_up() {
    cancel_pending_sibling_click();
    if (m_preview_focused) {
        return focus_prev_control();
    }
    const auto& list = get_current_list();
    if (list.empty()) return false;

    if (m_current_index > 0) {
        m_current_index--;
        rebuild_columns();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::navigate_down() {
    cancel_pending_sibling_click();
    if (m_preview_focused) {
        return focus_next_control();
    }
    const auto& list = get_current_list();
    if (list.empty()) return false;

    if (m_current_index < static_cast<int>(list.size()) - 1) {
        m_current_index++;
        rebuild_columns();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::navigate_enter() {
    cancel_pending_sibling_click();
    auto item = get_current_selected_item();
    if (!item) return false;

    if (item->is_category) {
        m_path.push_back(item);
        m_history_indices.push_back(m_current_index);
        m_current_index = 0;
        m_preview_focused = false;
        rebuild_columns();
        notify_state_changed();
        return true;
    } else {
        // Entering dead-end: focus into preview controls
        m_preview_focused = true;
        m_preview_control_index = 0;
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }
}

bool MillerView::navigate_back() {
    cancel_pending_sibling_click();
    if (m_preview_focused) {
        m_preview_focused = false;
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }

    if (!m_path.empty()) {
        m_path.pop_back();
        if (!m_history_indices.empty()) {
            m_current_index = m_history_indices.back();
            m_history_indices.pop_back();
        } else {
            m_current_index = 0;
        }
        m_preview_focused = false;
        rebuild_columns();
        notify_state_changed();
        return true;
    }

    return false;
}

bool MillerView::toggle_active() {
    auto item = get_current_selected_item();
    if (m_preview_focused && item && (item->id == "sound_speaker" || item->id == "sound_mic" || item->id == "net_wifi" || item->id == "net_bluetooth" || item->id == "net_airplane" || item->is_firewall)) {
        if (m_active_primary_switch) {
            m_active_primary_switch->toggle();
            return true;
        }
    }

    if (m_preview_focused) {
        if (m_preview_control_index == 0 && m_active_primary_switch) {
            m_active_primary_switch->toggle();
            return true;
        } else if (m_preview_control_index == 1 && m_active_secondary_switch) {
            m_active_secondary_switch->toggle();
            return true;
        } else if (m_preview_control_index == 3) {
            return trigger_active_action();
        }
    }

    if (item && !item->is_category && item->has_switch) {
        item->switch_val = !item->switch_val;
        rebuild_columns();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::adjust_active_slider(float delta) {
    auto item = get_current_selected_item();
    if (item && !item->is_category && item->has_slider) {
        item->slider_val = std::clamp(item->slider_val + delta, 0.0f, 1.0f);
        if (m_active_slider) {
            m_active_slider->set_value(item->slider_val);
        }
        rebuild_current_column();
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }
    return false;
}

bool MillerView::focus_next_control() {
    auto item = get_current_selected_item();
    int max_c = (item && (item->id == "net_wifi" || item->id == "net_bluetooth" || item->id == "net_airplane" || item->is_firewall)) ? 1 : ((item && (item->id == "sound_speaker" || item->id == "sound_mic")) ? 2 : 4);
    m_preview_control_index = (m_preview_control_index + 1) % max_c;
    rebuild_preview_column();
    notify_state_changed();
    return true;
}

bool MillerView::focus_prev_control() {
    auto item = get_current_selected_item();
    int max_c = (item && (item->id == "net_wifi" || item->id == "net_bluetooth" || item->id == "net_airplane" || item->is_firewall)) ? 1 : ((item && (item->id == "sound_speaker" || item->id == "sound_mic")) ? 2 : 4);
    m_preview_control_index = (m_preview_control_index + max_c - 1) % max_c;
    rebuild_preview_column();
    notify_state_changed();
    return true;
}

bool MillerView::trigger_active_action() {
    auto item = get_current_selected_item();
    if (item && item->has_action) {
        item->last_action_message = "Done at " + std::to_string(std::time(nullptr));
        if (m_active_action_feedback) {
            m_active_action_feedback->set_text(item->last_action_message);
        }
        rebuild_preview_column();
        notify_state_changed();
        return true;
    }
    return false;
}

std::string MillerView::get_breadcrumb_path() const {
    std::string path = "Settings";
    for (const auto& item : m_path) {
        path += " › " + item->title;
    }
    auto current = get_current_selected_item();
    if (current) {
        path += " › " + current->title;
    }
    return path;
}

std::string MillerView::get_status_text() const {
    const auto& list = get_current_list();
    std::string mode = m_preview_focused ? "[PREVIEW CONTROLS FOCUSED]" : "[LIST NAVIGATION]";
    std::string index_info = "Item " + std::to_string(m_current_index + 1) + "/" + std::to_string(list.size());
    std::string depth_info = "Level " + std::to_string(m_path.size() + 1);
    return mode + "  •  " + depth_info + "  •  " + index_info;
}

} // namespace miqu
