#include "miller_view.hpp"
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
        m_preview_layout->add_view(placeholder);
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

            m_preview_layout->add_view(row_card);
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
        m_preview_layout->add_view(seekbar_card);

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
        m_preview_layout->add_view(mute_card);

        // 3. Audio Device Specifications
        if (!item->telemetry.empty()) {
            m_preview_layout->add_view(create_telemetry_card(item->telemetry, config, "Audio device specifications"));
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
        m_preview_layout->add_view(wifi_card);

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

            m_preview_layout->add_view(net_card);
        }

        // 3. Interface Specifications
        if (!item->telemetry.empty()) {
            m_preview_layout->add_view(create_telemetry_card(item->telemetry, config, "Wireless interface specifications"));
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
        m_preview_layout->add_view(bt_card);

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
                    item->last_action_message = "Scan completed • 4 devices nearby";
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
                                  (dev.status == "Paired" ? config->colors.on_surface_variant : config->colors.on_surface_variant.with_alpha(0.7f));
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

                // Clicking device: connects it (or switches connection to it)
                dev_row_card->set_on_click_listener([this, item, di]() {
                    for (size_t k = 0; k < item->bt_devices.size(); ++k) {
                        if (k == di) {
                            item->bt_devices[k].status = "Connected";
                        } else if (item->bt_devices[k].status == "Connected") {
                            item->bt_devices[k].status = "Paired";
                        }
                    }
                    rebuild_preview_column();
                    notify_state_changed();
                });

                dev_card->add_view(dev_row_card);
            }

            m_preview_layout->add_view(dev_card);
        }

        // 3. Controller Specifications
        if (!item->telemetry.empty()) {
            m_preview_layout->add_view(create_telemetry_card(item->telemetry, config, "Bluetooth controller specifications"));
        }

        return;
    }

    // Dedicated Clean Layout for Ethernet: Live Status Card, Active Profile, IP/Routing parameters, Telemetry
    if (item->id == "net_ethernet") {
        // 1. Live Status Card (Physical cable status is authoritative; no pseudo-radio switch)
        bool is_conn = (item->eth_profile.status == "Connected");

        auto eth_card = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(16, 14)
            ->spacing(12)
            ->backgroundColor(config->colors.surface)
            ->stroke(1, config->colors.outline.with_alpha(0.3f))
            ->cornerRadius(12)
            ->build();

        // Ethernet Icon
        auto eth_icon = TextViewBuilder::create()
            ->text("󰈀")
            ->textSize(config->metrics.font_size)
            ->textColor(is_conn ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        LayoutParams eicon_lp;
        eicon_lp.width = 24;
        eicon_lp.gravity = Gravity::CenterVertical;
        eth_card->add_view(eth_icon, eicon_lp);

        auto eth_lbl_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(2)
            ->build();

        auto eth_title = TextViewBuilder::create()
            ->text("Ethernet")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface)
            ->build();
        eth_lbl_box->add_view(eth_title);

        std::string eth_hint_str = is_conn ? "enp4s0 • 2.5 Gbps Full Duplex" : "Cable unplugged";
        auto eth_hint = TextViewBuilder::create()
            ->text(eth_hint_str)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        eth_lbl_box->add_view(eth_hint);

        LayoutParams elbl_lp;
        elbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        elbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        elbl_lp.weight = 1.0f;
        elbl_lp.gravity = Gravity::CenterVertical;
        eth_card->add_view(eth_lbl_box, elbl_lp);

        // Status Badge / Pill
        std::string badge_text = is_conn ? "Connected" : "Unplugged";
        Color badge_bg = is_conn ? config->colors.primary_container : config->colors.surface_variant;
        Color badge_fg = is_conn ? config->colors.primary : config->colors.on_surface_variant;

        auto status_pill = TextViewBuilder::create()
            ->text(badge_text)
            ->caption(true)
            ->textColor(badge_fg)
            ->padding(8, 4)
            ->build();

        auto pill_card = CardViewBuilder::create()
            ->style(CardStyle::Filled)
            ->backgroundColor(badge_bg)
            ->cornerRadius(6)
            ->elevation(0)
            ->addView(status_pill)
            ->build();

        LayoutParams pill_lp;
        pill_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        pill_lp.gravity = Gravity::CenterVertical;
        eth_card->add_view(pill_card, pill_lp);

        m_preview_layout->add_view(eth_card);

        // 2. Active Wired Connection Profile & IP Parameters (Visible when connected)
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

            m_preview_layout->add_view(profile_card);
        }

        // 3. Hardware & Link Telemetry
        if (!item->telemetry.empty()) {
            m_preview_layout->add_view(create_telemetry_card(item->telemetry, config, "Hardware & link specifications"));
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
                item->subtitle = checked ? "All wireless communications disabled" : "Disable all wireless communications";
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
        m_preview_layout->add_view(airplane_card);

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

        m_preview_layout->add_view(info_card);
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
                            std::string sub = m->monitor_cfg.connector + " • 4K UHD";
                            if (m->monitor_cfg.is_primary) sub += " • Primary";
                            m->subtitle = sub;
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

        m_preview_layout->add_view(mon_card);

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
            item->subtitle = sub;
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

            LayoutParams sp_lp(160, 36);
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

        m_preview_layout->add_view(cfg_card);

        // 3. Hardware & Panel Telemetry
        if (!item->telemetry.empty()) {
            m_preview_layout->add_view(create_telemetry_card(item->telemetry, config, "Display panel specifications"));
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

        m_preview_layout->add_view(app_card);

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

        LayoutParams sp_lp(140, 36);
        net_card->add_view(net_spinner, sp_lp);

        m_preview_layout->add_view(net_card);

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
                item->subtitle = "v" + item->app_info.version + " • " + item->app_info.size_total;
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

        m_preview_layout->add_view(details_card);

        // 3. Telemetry Card
        if (!item->telemetry.empty()) {
            m_preview_layout->add_view(create_telemetry_card(item->telemetry, config, "Package & runtime specifications"));
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

        m_preview_layout->add_view(role_card);

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

        m_preview_layout->add_view(cand_card);

        // 3. MIME Telemetry Card
        if (!item->telemetry.empty()) {
            m_preview_layout->add_view(create_telemetry_card(item->telemetry, config, "MIME handler specifications"));
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
    m_preview_layout->add_view(banner);

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

        m_preview_layout->add_view(primary_row);
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

        m_preview_layout->add_view(sub_box);
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
        m_preview_layout->add_view(create_telemetry_card(item->telemetry, config, telem_title));
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
    if (m_preview_focused && item && (item->id == "sound_speaker" || item->id == "sound_mic" || item->id == "net_wifi" || item->id == "net_bluetooth" || item->id == "net_airplane")) {
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
    int max_c = (item && (item->id == "net_wifi" || item->id == "net_bluetooth" || item->id == "net_airplane")) ? 1 : ((item && (item->id == "sound_speaker" || item->id == "sound_mic")) ? 2 : 4);
    m_preview_control_index = (m_preview_control_index + 1) % max_c;
    rebuild_preview_column();
    notify_state_changed();
    return true;
}

bool MillerView::focus_prev_control() {
    auto item = get_current_selected_item();
    int max_c = (item && (item->id == "net_wifi" || item->id == "net_bluetooth" || item->id == "net_airplane")) ? 1 : ((item && (item->id == "sound_speaker" || item->id == "sound_mic")) ? 2 : 4);
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
