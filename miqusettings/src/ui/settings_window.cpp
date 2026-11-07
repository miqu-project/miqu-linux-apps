#include "ui/settings_window.hpp"
#include "config/settings_config.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/list_item_view.hpp>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>

namespace miqu {

std::shared_ptr<SettingsWindow> SettingsWindow::create(std::shared_ptr<AppEngine> engine) {
    auto win = std::shared_ptr<SettingsWindow>(new SettingsWindow(std::move(engine)));
    if (!win->init()) {
        return nullptr;
    }
    return win;
}

SettingsWindow::SettingsWindow(std::shared_ptr<AppEngine> engine)
    : m_engine(std::move(engine)) {}

bool SettingsWindow::init() {
    m_root_node = SettingItem::create_default_tree();
    build_ui();
    sync_theme();
    select_page(0);
    return m_window != nullptr;
}

void SettingsWindow::show() {
    if (m_window) {
        m_window->show();
    }
}

void SettingsWindow::build_ui() {
    auto config = Config::get();
    auto& scfg = SettingsConfig::get();

    // 1. Toolbar
    m_toolbar = ToolbarBuilder::create()
        ->title("Settings")
        ->titleAlignment(TitleAlignment::Start)
        ->onBack([this]() {
            if (m_detail_view && m_detail_view->is_subpage_open()) {
                m_detail_view->navigate_back_to_main_page();
            }
        })
        ->onClose([this]() {
            if (m_window) m_window->request_close();
            else m_engine->quit();
        })
        ->build();
    m_toolbar->set_back_visible(false);

    // 2. Sidebar List
    m_sidebar_list = std::make_shared<ListView>();
    m_sidebar_list->set_item_height(50);
    m_sidebar_list->set_spacing(3);
    m_sidebar_list->set_item_corner_radius(std::max(4, config->metrics.corner_radius - 2));
    m_sidebar_list->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    for (const auto& item : m_root_node->children) {
        auto item_view = ListItemViewBuilder::create()
            ->title(item->title)
            ->iconSource(item->icon_name.empty() ? "preferences-system" : item->icon_name)
            ->iconSize(22)
            ->build();
        m_sidebar_list->add_item(item_view);
    }

    m_sidebar_list->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        select_page(static_cast<int>(idx));
    });

    // 3. Sidebar Card
    m_sidebar_card = std::make_shared<CardView>();
    m_sidebar_card->set_card_background_color(config->colors.surface_variant);
    m_sidebar_card->set_radius(config->metrics.corner_radius);
    m_sidebar_card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    m_sidebar_card->set_elevation(0);
    m_sidebar_card->set_padding(8);

    LayoutParams sidebar_lp;
    sidebar_lp.width = 240;
    sidebar_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    m_sidebar_card->set_layout_params(sidebar_lp);
    m_sidebar_card->add_view(m_sidebar_list);

    // 4. Detail View (Active Page / Fragment)
    m_detail_view = std::make_shared<SettingDetailView>();
    LayoutParams detail_lp;
    detail_lp.width = 0;
    detail_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    detail_lp.weight = 1.0f;
    m_detail_view->set_layout_params(detail_lp);

    m_detail_view->set_on_subpage_changed([this](bool in_subpage) {
        if (m_toolbar) {
            m_toolbar->set_back_visible(in_subpage);
        }
        if (m_window) {
            m_window->schedule_redraw();
        }
    });

    // 5. Body: Sidebar on Left, Detail View on Right
    m_body = std::make_shared<LinearLayout>(Orientation::Horizontal);
    LayoutParams body_lp;
    body_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    body_lp.height = 0;
    body_lp.weight = 1.0f;
    m_body->set_layout_params(body_lp);
    m_body->set_padding(8, 0, 8, 8);
    m_body->set_divider_spacing(8);

    m_body->add_view(m_sidebar_card);
    m_body->add_view(m_detail_view);

    // 6. Root Container
    m_root_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_root_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_root_container->set_background_color(config->colors.background);
    m_root_container->add_view(m_toolbar);
    m_root_container->add_view(m_body);

    // 7. Window
    m_window = WindowBuilder::create()
        ->title("Settings - miqusettings")
        ->appId("miqusettings")
        ->role(WindowRole::Toplevel)
        ->preferredSize(scfg.window_width, scfg.window_height)
        ->closeOnEscape(false)
        ->contentView(m_root_container)
        ->onKey([this](const KeyPressEvent& ev) {
            if (!ev.pressed) return;
            if (ev.keysym == XKB_KEY_Escape) {
                if (m_detail_view && m_detail_view->is_subpage_open()) {
                    m_detail_view->navigate_back_to_main_page();
                } else {
                    if (m_window) m_window->request_close();
                }
            }
        })
        ->build();
}

void SettingsWindow::select_page(int index) {
    const int count = static_cast<int>(m_root_node->children.size());
    if (count == 0) return;
    m_selected_index = std::clamp(index, 0, count - 1);

    if (m_sidebar_list) {
        m_sidebar_list->set_selected_index(m_selected_index);
    }

    auto& item = m_root_node->children[m_selected_index];
    if (m_detail_view) {
        m_detail_view->show_page(item->id);
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void SettingsWindow::sync_theme() {
    auto config = Config::get();
    if (!config) return;

    if (m_sidebar_card) {
        m_sidebar_card->set_card_background_color(config->colors.surface_variant);
        m_sidebar_card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
        m_sidebar_card->set_radius(config->metrics.corner_radius);
    }
    if (m_sidebar_list) {
        m_sidebar_list->set_item_corner_radius(std::max(4, config->metrics.corner_radius - 2));
    }
    if (m_root_container) {
        m_root_container->set_background_color(config->colors.background);
    }

    if (m_window) {
        m_window->refresh_theme();
        m_window->schedule_redraw();
    }
}

void SettingsWindow::navigate_to_id(const std::string& target_id) {
    if (!m_root_node || target_id.empty()) return;

    for (int i = 0; i < static_cast<int>(m_root_node->children.size()); ++i) {
        if (m_root_node->children[i]->id == target_id) {
            select_page(i);
            return;
        }
    }

    if (target_id.rfind("app_", 0) == 0) {
        for (int i = 0; i < static_cast<int>(m_root_node->children.size()); ++i) {
            if (m_root_node->children[i]->id == "apps_list") {
                select_page(i);
                m_detail_view->show_page(target_id);
                return;
            }
        }
    }

    if (target_id.rfind("defrole_", 0) == 0) {
        for (int i = 0; i < static_cast<int>(m_root_node->children.size()); ++i) {
            if (m_root_node->children[i]->id == "default_apps") {
                select_page(i);
                m_detail_view->show_page(target_id);
                return;
            }
        }
    }

    if (target_id == "appearance") {
        navigate_to_id("wallpaper");
    } else if (target_id == "network") {
        navigate_to_id("wifi");
    } else if (target_id == "display") {
        navigate_to_id("monitors");
    } else if (target_id == "security") {
        navigate_to_id("firewall");
    }
}

} // namespace miqu
