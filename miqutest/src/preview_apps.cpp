#include "miller_view.hpp"
#include "preview_common.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {
void MillerView::render_empty_preview(const std::shared_ptr<Config>& config) {
    auto placeholder = TextViewBuilder::create()
        ->text("No item selected")
        ->center(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    add_preview_card(placeholder);
}

void MillerView::render_category_children_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    m_preview_layout->set_padding(Padding(10, 10));
    m_preview_layout->set_divider_spacing(4);

    for (size_t i = 0; i < item->children.size(); ++i) {
        const auto& child = item->children[i];

        auto item_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->spacing(10)
            ->build();

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

        LayoutParams text_lp;
        text_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        text_lp.weight = 1.0f;
        text_lp.gravity = Gravity::CenterVertical;
        item_row->add_view(text_box, text_lp);

        auto row_card = HoverCardViewBuilder::create()
            ->cornerRadius(get_inner_radius(config))
            ->elevation(0)
            ->addView(item_row)
            ->build();

        row_card->set_on_click_listener([this, i]() {
            navigate_enter();
            m_current_index = static_cast<int>(i);
            rebuild_columns();
            notify_state_changed();
        });

        m_preview_layout->add_view(row_card);
    }
}


void MillerView::render_installed_app_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. App Banner Card with Category on a new line, Actions on a new line, and Description
    auto app_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
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
            ->cornerRadius(get_chip_radius(config))
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
    auto on_open = [this, item]() {
        item->app_info.is_running = true;
        if (item->app_info.pid <= 0) item->app_info.pid = 4920;
        item->last_action_message = "Launched " + item->app_info.app_name + " (running, PID " + std::to_string(item->app_info.pid) + ")";
        rebuild_preview_column();
        notify_state_changed();
    };

    auto open_btn = ButtonBuilder::create()
        ->text("Open")
        ->primary(true)
        ->padding(16, 5)
        ->cornerRadius(get_btn_radius(config))
        ->onClick(on_open)
        ->build();

    LayoutParams open_lp;
    open_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    open_lp.height = 30;
    open_lp.gravity = Gravity::CenterVertical;
    actions_box->add_view(open_btn, open_lp);

    // 2. Force Close Button
    bool is_running = item->app_info.is_running;

    auto on_force_close = [this, item]() {
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
    };

    auto force_close_btn = ButtonBuilder::create()
        ->text("Force Close")
        ->style(ButtonStyle::Outlined)
        ->padding(14, 5)
        ->cornerRadius(get_btn_radius(config))
        ->onClick(on_force_close)
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
    auto on_uninstall = [this, item]() {
        item->last_action_message = "Uninstall requested for " + item->app_info.app_name;
        rebuild_preview_column();
        notify_state_changed();
    };

    auto uninstall_btn = ButtonBuilder::create()
        ->text("Uninstall")
        ->style(ButtonStyle::Outlined)
        ->padding(14, 5)
        ->cornerRadius(get_btn_radius(config))
        ->onClick(on_uninstall)
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
        ->cornerRadius(get_card_radius(config))
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
        ->cornerRadius(get_inner_radius(config))
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
        ->cornerRadius(get_card_radius(config))
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

    auto on_clear_cache = [this, item]() {
        item->app_info.size_data = "0 KB";
        item->app_info.size_total = item->app_info.size_binary;
        item->last_action_message = "Cache cleared (storage freed)";
        rebuild_current_column();
        rebuild_preview_column();
        notify_state_changed();
    };

    auto clear_cache_btn = ButtonBuilder::create()
        ->text("Clear Cache")
        ->flat(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->padding(10, 4)
        ->cornerRadius(get_btn_radius(config))
        ->onClick(on_clear_cache)
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

void MillerView::render_default_app_role_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
    // 1. Role Header Banner Card
    auto role_card = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
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
        ->cornerRadius(get_btn_radius(config))
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
        ->cornerRadius(get_card_radius(config))
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

        auto on_select_candidate = [this, item, ci]() {
            if (ci >= item->default_app_role.candidates.size()) return;
            for (size_t k = 0; k < item->default_app_role.candidates.size(); ++k) {
                item->default_app_role.candidates[k].is_default = (k == ci);
            }
            item->subtitle = item->default_app_role.candidates[ci].name;
            item->last_action_message = "Default updated to " + item->subtitle;
            rebuild_current_column();
            deferred_rebuild_preview();
            notify_state_changed();
        };

        if (is_def) {
            auto check_view = TextViewBuilder::create()
                ->text("✓ Default")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.primary)
                ->build();
            LayoutParams chk_lp;
            chk_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            chk_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(check_view, chk_lp);
        } else {
            auto select_label = TextViewBuilder::create()
                ->text("Set Default")
                ->caption(true)
                ->textColor(config->colors.primary.with_alpha(0.85f))
                ->build();
            LayoutParams set_lp;
            set_lp.width = static_cast<int>(LayoutDimension::WrapContent);
            set_lp.gravity = Gravity::CenterVertical;
            row_content->add_view(select_label, set_lp);
        }

        Color row_bg = is_def ? config->colors.primary_container.with_alpha(0.35f) : Color::transparent();
        int stroke_w = is_def ? 1 : 0;
        Color stroke_col = is_def ? config->colors.primary.with_alpha(0.5f) : Color::transparent();
        auto cand_row_card = HoverCardViewBuilder::create()
            ->selected(is_def, row_bg, stroke_w, stroke_col)
            ->cornerRadius(get_inner_radius(config))
            ->elevation(0)
            ->addView(row_content)
            ->build();

        cand_row_card->set_on_click_listener(on_select_candidate);

        cand_card->add_view(cand_row_card);
    }

    add_preview_card(cand_card);

    // 3. MIME Telemetry Card
    if (!item->telemetry.empty()) {
        add_preview_card(create_telemetry_card(item->telemetry, config, "MIME handler specifications"));
    }

    return;
}

void MillerView::render_generic_fallback_preview(const std::shared_ptr<SettingItem>& item, const std::shared_ptr<Config>& config) {
// 1. Header Banner
auto banner = LinearLayoutBuilder::create()
    ->orientation(Orientation::Horizontal)
    ->spacing(12)
    ->padding(16, 14)
    ->backgroundColor(config->colors.surface)
    ->stroke(1, config->colors.outline.with_alpha(0.3f))
    ->cornerRadius(get_card_radius(config))
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

if (!item->subtitle.empty()) {
    auto sub_text = TextViewBuilder::create()
        ->text(item->subtitle)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    left_info->add_view(sub_text);
}

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
        ->cornerRadius(get_inner_radius(config))
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
    auto primary_row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 14)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
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
    primary_row->add_view(primary_sw, sw_lp);

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
        ->cornerRadius(get_card_radius(config))
        ->build();

    // Secondary Switch
    if (item->has_secondary_switch) {
        auto sec_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->padding(10, 8)
            ->cornerRadius(get_inner_radius(config))
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

        auto sec_sw = SwitchBuilder::create()
            ->checked(item->secondary_switch_val)
            ->onCheckedChanged([this, item](bool checked) {
                item->secondary_switch_val = checked;
                deferred_rebuild_preview();
                notify_state_changed();
            })
            ->build();

        LayoutParams sec_sw_lp;
        sec_sw_lp.width = 44;
        sec_sw_lp.height = 24;
        sec_sw_lp.gravity = Gravity::CenterVertical;
        sec_row->add_view(sec_sw, sec_sw_lp);

        sub_box->add_view(sec_row);
    }

    // Slider Control
    if (item->has_slider) {
        bool is_read_only = item->slider_read_only;

        auto slider_box = LinearLayoutBuilder::create()
            ->orientation(Orientation::Vertical)
            ->spacing(6)
            ->padding(10, 8)
            ->cornerRadius(get_inner_radius(config))
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
            ->text(std::to_string(val_pct) + " " + item->slider_unit + (is_read_only ? " (Read-only)" : ""))
            ->textColor(is_read_only ? config->colors.on_surface_variant : config->colors.primary)
            ->build();
        s_row->add_view(m_active_slider_label);
        slider_box->add_view(s_row);

        m_active_slider = SliderBuilder::create()
            ->value(item->slider_val)
            ->onValueChanged([this, item, is_read_only](float val, bool user) {
                if (is_read_only) return;
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
        auto action_row = LinearLayoutBuilder::create()
            ->orientation(Orientation::Horizontal)
            ->spacing(12)
            ->padding(10, 8)
            ->cornerRadius(get_inner_radius(config))
            ->build();

        auto action_btn = ButtonBuilder::create()
            ->text(item->action_label)
            ->primary(true)
            ->onClick([this, item]() {
                trigger_active_action();
            })
            ->build();

        LayoutParams btn_lp;
        btn_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        btn_lp.height = 34;
        action_row->add_view(action_btn, btn_lp);

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


} // namespace miqu
