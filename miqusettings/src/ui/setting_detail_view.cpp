#include "ui/setting_detail_view.hpp"
#include "modules/bluetooth/bluetooth_detail_view.hpp"
#include "modules/ethernet/ethernet_detail_view.hpp"
#include "modules/wifi/wifi_detail_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/list_item_view.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <algorithm>
#include <sstream>

namespace miqu {

SettingDetailView::SettingDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    show_empty_state();
}

void SettingDetailView::clear() {
    clear_views();
    m_current_item.reset();
    m_current_content.reset();
}

void SettingDetailView::update_item(const std::shared_ptr<SettingItem>& item, bool is_fullscreen) {
    m_current_item = item;
    clear_views();

    if (!item) {
        show_empty_state();
        return;
    }

    if (item->id == "wifi" || item->id.rfind("wifi_", 0) == 0) {
        show_wifi_detail(is_fullscreen);
        return;
    }

    if (item->id == "bluetooth" || item->id.rfind("bt_", 0) == 0) {
        show_bluetooth_detail(is_fullscreen);
        return;
    }

    if (item->id == "ethernet" || item->id.rfind("eth_", 0) == 0) {
        show_ethernet_detail(is_fullscreen);
        return;
    }

    if (item->type == SettingType::Category) {
        show_category_preview(item);
    } else {
        show_leaf_detail(item, is_fullscreen);
    }
}

void SettingDetailView::show_empty_state() {
    auto config = Config::get();
    auto empty_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    empty_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    empty_box->set_divider_spacing(12);

    auto icon = std::make_shared<ImageView>("preferences-system");
    icon->set_target_size(48);
    icon->set_dim(0.2f);
    LayoutParams icon_lp;
    icon_lp.width = 48;
    icon_lp.height = 48;
    icon_lp.gravity = Gravity::CenterHorizontal;
    empty_box->add_view(icon, icon_lp);

    auto lbl = TextViewBuilder::create()
        ->text("Select a setting to view options")
        ->center(true)
        ->textSize(13)
        ->textColor(config->colors.outline)
        ->build();
    LayoutParams lbl_lp;
    lbl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl_lp.gravity = Gravity::CenterHorizontal;
    empty_box->add_view(lbl, lbl_lp);

    m_current_content = empty_box;
    add_view(empty_box);
}

void SettingDetailView::show_category_preview(const std::shared_ptr<SettingItem>& item) {
    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_divider_spacing(12);

    // Section preview header
    auto header = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header->set_padding(8, 6);
    header->set_divider_spacing(12);

    auto icon = std::make_shared<ImageView>(item->icon_name.empty() ? "preferences-system" : item->icon_name);
    icon->set_target_size(28);
    LayoutParams icon_lp;
    icon_lp.width = 28;
    icon_lp.height = 28;
    icon_lp.gravity = Gravity::CenterVertical;
    header->add_view(icon, icon_lp);

    auto title_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams title_col_lp;
    title_col_lp.width = 0;
    title_col_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    title_col_lp.weight = 1.0f;
    title_col->set_layout_params(title_col_lp);

    auto title = TextViewBuilder::create()
        ->text(item->title)
        ->bold(true)
        ->textSize(16)
        ->textColor(config->colors.on_surface)
        ->build();
    title_col->add_view(title);

    if (!item->subtitle.empty()) {
        auto subtitle = TextViewBuilder::create()
            ->text(item->subtitle)
            ->textSize(12)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        title_col->add_view(subtitle);
    }
    header->add_view(title_col);
    root->add_view(header);

    auto list = std::make_shared<ListView>();
    list->set_item_height(52);
    list->set_spacing(6);
    list->set_item_corner_radius(8);
    list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    auto children = item->children;
    for (const auto& ch : children) {
        auto item_view = ListItemViewBuilder::create()
            ->title(ch->title)
            ->subtitle(ch->subtitle)
            ->trailingText(ch->get_status_text())
            ->iconSource(ch->icon_name.empty() ? "preferences-system" : ch->icon_name)
            ->iconSize(24)
            ->build();
        list->add_item(item_view);
    }
    list->set_selected_index(-1);

    list->set_on_item_click_listener([this, children, list_weak = std::weak_ptr<ListView>(list)](size_t idx, std::shared_ptr<View>) {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_item_click_time).count();
        if (m_last_item_clicked_index == static_cast<int>(idx) && diff < 400) {
            if (idx < children.size() && m_on_navigate_into) {
                m_on_navigate_into(children[idx]);
            }
        } else {
            m_last_item_click_time = now;
            m_last_item_clicked_index = static_cast<int>(idx);
            if (auto l = list_weak.lock()) {
                l->set_selected_index(static_cast<int>(idx));
            }
        }
    });

    root->add_view(list);
    m_current_content = root;
    add_view(root);
}

void SettingDetailView::show_leaf_detail(const std::shared_ptr<SettingItem>& item, bool is_fullscreen) {
    auto config = Config::get();
    auto scroll = std::make_shared<ScrollView>();
    scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    root->set_divider_spacing(20);
    root->set_padding(is_fullscreen ? 32 : 16);

    // 1. Header Card (Hero)
    auto hero_card = std::make_shared<CardView>();
    hero_card->set_card_background_color(config->colors.surface);
    hero_card->set_radius(config->metrics.corner_radius);
    hero_card->set_padding(20);
    LayoutParams hero_lp;
    hero_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    hero_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    hero_card->set_layout_params(hero_lp);

    auto hero_content = std::make_shared<LinearLayout>(Orientation::Horizontal);
    hero_content->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    hero_content->set_divider_spacing(18);

    auto icon = std::make_shared<ImageView>(item->icon_name.empty() ? "preferences-system" : item->icon_name);
    icon->set_target_size(is_fullscreen ? 64 : 48);
    LayoutParams icon_lp;
    icon_lp.width = is_fullscreen ? 64 : 48;
    icon_lp.height = is_fullscreen ? 64 : 48;
    icon_lp.gravity = Gravity::CenterVertical;
    hero_content->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text(item->title)
        ->bold(true)
        ->textSize(is_fullscreen ? 24 : 20)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    if (!item->subtitle.empty()) {
        auto subtitle = TextViewBuilder::create()
            ->text(item->subtitle)
            ->textSize(13)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        text_col->add_view(subtitle);
    }
    hero_content->add_view(text_col);

    if (!is_fullscreen) {
        auto btn_expand = ButtonBuilder::create()
            ->text("⛶")
            ->textSize(14)
            ->textColor(config->colors.primary)
            ->backgroundColor(config->colors.surface_variant)
            ->cornerRadius(6)
            ->onClick([this]() {
                if (m_on_request_fullscreen) m_on_request_fullscreen();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 36;
        btn_lp.height = 36;
        btn_lp.gravity = Gravity::CenterVertical;
        btn_expand->set_layout_params(btn_lp);
        hero_content->add_view(btn_expand);
    }

    hero_card->add_view(hero_content);
    root->add_view(hero_card);

    // 2. Control Card based on SettingType
    auto control_card = std::make_shared<CardView>();
    control_card->set_card_background_color(config->colors.surface);
    control_card->set_radius(config->metrics.corner_radius);
    control_card->set_padding(20);
    LayoutParams ctrl_lp;
    ctrl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    ctrl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    control_card->set_layout_params(ctrl_lp);

    if (item->type == SettingType::Toggle) {
        auto toggle_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        toggle_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        toggle_row->set_divider_spacing(16);

        auto lbl_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        LayoutParams lbl_lp;
        lbl_lp.width = 0;
        lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        lbl_lp.weight = 1.0f;
        lbl_col->set_layout_params(lbl_lp);

        auto opt_title = TextViewBuilder::create()
            ->text("Enable " + item->title)
            ->bold(true)
            ->textSize(15)
            ->textColor(config->colors.on_surface)
            ->build();
        lbl_col->add_view(opt_title);

        auto status_lbl = TextViewBuilder::create()
            ->text(item->bool_val ? "Status: Active" : "Status: Inactive")
            ->textSize(13)
            ->textColor(item->bool_val ? config->colors.primary : config->colors.on_surface_variant)
            ->build();
        lbl_col->add_view(status_lbl);
        toggle_row->add_view(lbl_col);

        auto sw = SwitchBuilder::create()
            ->checked(item->bool_val)
            ->onCheckedChanged([this, item, status_lbl, config](bool checked) {
                item->bool_val = checked;
                status_lbl->set_text(checked ? "Status: Active" : "Status: Inactive");
                status_lbl->set_text_color(checked ? config->colors.primary : config->colors.on_surface_variant);
                status_lbl->request_redraw();
                if (m_on_setting_changed) {
                    m_on_setting_changed(item);
                }
            })
            ->build();
        LayoutParams sw_lp;
        sw_lp.width = static_cast<int>(LayoutDimension::WrapContent);
        sw_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        sw_lp.gravity = Gravity::CenterVertical;
        sw->set_layout_params(sw_lp);
        toggle_row->add_view(sw);

        control_card->add_view(toggle_row);
        root->add_view(control_card);

    } else if (item->type == SettingType::SliderControl) {
        auto slider_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        slider_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        slider_col->set_divider_spacing(16);

        auto val_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        val_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        auto slider_title = TextViewBuilder::create()
            ->text(item->title)
            ->bold(true)
            ->textSize(15)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams st_lp;
        st_lp.width = 0;
        st_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        st_lp.weight = 1.0f;
        slider_title->set_layout_params(st_lp);
        val_row->add_view(slider_title);

        int pct = static_cast<int>(item->float_val * 100.0f + 0.5f);
        auto val_text = TextViewBuilder::create()
            ->text(std::to_string(pct) + "%")
            ->bold(true)
            ->textSize(15)
            ->textColor(config->colors.primary)
            ->build();
        val_row->add_view(val_text);
        slider_col->add_view(val_row);

        auto slider = SliderBuilder::create()
            ->value(item->float_val)
            ->onValueChanged([this, item, val_text](float val, bool) {
                item->float_val = val;
                int p = static_cast<int>(val * 100.0f + 0.5f);
                val_text->set_text(std::to_string(p) + "%");
                val_text->request_redraw();
                if (m_on_setting_changed) {
                    m_on_setting_changed(item);
                }
            })
            ->build();
        LayoutParams sl_lp;
        sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
        sl_lp.height = 30;
        slider->set_layout_params(sl_lp);
        slider_col->add_view(slider);

        control_card->add_view(slider_col);
        root->add_view(control_card);

    } else if (item->type == SettingType::Selection) {
        auto sel_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        sel_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        sel_col->set_divider_spacing(8);

        auto sel_lbl = TextViewBuilder::create()
            ->text("Choose an option:")
            ->bold(true)
            ->textSize(14)
            ->textColor(config->colors.outline)
            ->build();
        sel_col->add_view(sel_lbl);

        for (size_t i = 0; i < item->options.size(); ++i) {
            bool selected = (static_cast<int>(i) == item->selected_option_idx);
            auto opt_btn = ButtonBuilder::create()
                ->text((selected ? "●  " : "○  ") + item->options[i])
                ->textSize(14)
                ->textColor(selected ? config->colors.primary : config->colors.on_surface)
                ->backgroundColor(selected ? config->colors.surface_variant : config->colors.surface)
                ->cornerRadius(8)
                ->onClick([this, item, i, is_fullscreen]() {
                    item->selected_option_idx = static_cast<int>(i);
                    item->subtitle = item->options[i];
                    if (m_on_setting_changed) {
                        m_on_setting_changed(item);
                    }
                    update_item(item, is_fullscreen);
                })
                ->build();
            LayoutParams opt_lp;
            opt_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            opt_lp.height = 46;
            opt_btn->set_layout_params(opt_lp);
            sel_col->add_view(opt_btn);
        }

        control_card->add_view(sel_col);
        root->add_view(control_card);

    } else if (item->type == SettingType::Info) {
        auto info_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        info_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        info_col->set_divider_spacing(12);

        auto info_title = TextViewBuilder::create()
            ->text("Property Details")
            ->bold(true)
            ->textSize(14)
            ->textColor(config->colors.outline)
            ->build();
        info_col->add_view(info_title);

        auto val_lbl = TextViewBuilder::create()
            ->text(item->subtitle.empty() ? item->title : item->subtitle)
            ->textSize(15)
            ->textColor(config->colors.on_surface)
            ->build();
        info_col->add_view(val_lbl);

        auto badge = TextViewBuilder::create()
            ->text("✓ Verified System Configuration")
            ->textSize(12)
            ->textColor(config->colors.primary)
            ->build();
        info_col->add_view(badge);

        control_card->add_view(info_col);
        root->add_view(control_card);
    }

    scroll->set_content_view(root);
    m_current_content = scroll;
    add_view(scroll);
}

void SettingDetailView::show_bluetooth_detail(bool is_fullscreen) {
    auto bt_view = std::make_shared<BluetoothDetailView>();
    bt_view->set_is_fullscreen(is_fullscreen);
    bt_view->set_on_request_fullscreen([this]() {
        if (m_on_request_fullscreen) m_on_request_fullscreen();
    });
    m_current_content = bt_view;
    add_view(bt_view);
}

void SettingDetailView::show_ethernet_detail(bool is_fullscreen) {
    auto eth_view = std::make_shared<EthernetDetailView>();
    eth_view->set_is_fullscreen(is_fullscreen);
    eth_view->set_on_request_fullscreen([this]() {
        if (m_on_request_fullscreen) m_on_request_fullscreen();
    });
    m_current_content = eth_view;
    add_view(eth_view);
}

void SettingDetailView::show_wifi_detail(bool is_fullscreen) {
    auto wifi_view = std::make_shared<WifiDetailView>();
    wifi_view->set_is_fullscreen(is_fullscreen);
    wifi_view->set_on_request_fullscreen([this]() {
        if (m_on_request_fullscreen) m_on_request_fullscreen();
    });
    m_current_content = wifi_view;
    add_view(wifi_view);
}

} // namespace miqu
