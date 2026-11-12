#include "test_app.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <iostream>

namespace miqu {

TestApp::TestApp(std::shared_ptr<AppEngine> engine)
    : m_engine(std::move(engine)) {}

bool TestApp::init() {
    if (!m_engine) return false;

    setup_ui();

    m_window = WindowBuilder::create()
        ->title("Control Panel")
        ->appId("miqucontrolpanel")
        ->role(WindowRole::Toplevel)
        ->preferredSize(980, 640)
        ->closeOnEscape(false)
        ->keyboardInteractive(true)
        ->contentView(m_root_layout)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->onKey([this](const KeyPressEvent& ev) {
            handle_key_press(ev);
        })
        ->build();

    if (!m_window) {
        std::cerr << "[miqucontrolpanel] Failed to build main window.\n";
        return false;
    }

    m_engine->register_window(m_window);
    sync_theme();
    update_header();

    return true;
}

void TestApp::setup_ui() {
    auto config = Config::get();

    m_root_layout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(0)
        ->backgroundColor(Color::transparent())
        ->build();

    // 1. Top Toolbar (Centered Title "Settings", no subtitle, power button on right)
    m_toolbar = ToolbarBuilder::create()
        ->title("Settings")
        ->titleAlignment(TitleAlignment::Center)
        ->showDivider(false)
        ->onClose([this]() {
            if (auto* eng = AppEngine::instance()) {
                eng->quit(0);
            }
        })
        ->closeTooltip("Quit Settings")
        ->build();

    LayoutParams toolbar_lp;
    toolbar_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    toolbar_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    m_root_layout->add_view(m_toolbar, toolbar_lp);

    // 2. Separate Ribbon between Toolbar and Main Content (Back button + Path)
    m_ribbon = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(16, 0)
        ->spacing(12)
        ->backgroundColor(Color::transparent())
        ->build();

    m_back_label = TextViewBuilder::create()
        ->text("‹ Back")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();

    auto back_content = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->padding(10, 3)
        ->spacing(4)
        ->addView(m_back_label)
        ->build();

    m_back_btn = CardViewBuilder::create()
        ->style(CardStyle::Filled)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->elevation(0)
        ->addView(back_content)
        ->build();

    m_back_btn->set_on_click_listener([this]() {
        if (m_miller_view && m_miller_view->can_navigate_back()) {
            if (m_miller_view->is_searching()) {
                if (m_search_view) {
                    m_search_view->set_query("");
                    m_search_view->set_focused(false);
                }
                if (m_window) m_window->clear_focus();
            }
            m_miller_view->navigate_back();
        }
    });

    LayoutParams back_lp;
    back_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    back_lp.height = 26;
    back_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_back_btn, back_lp);

    m_path_label = TextViewBuilder::create()
        ->text("Settings")
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();

    LayoutParams path_lp;
    path_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    path_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    path_lp.weight = 1.0f;
    path_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_path_label, path_lp);

    // Search bar integrated into the ribbon header
    m_search_view = SearchViewBuilder::create()
        ->hint("Search")
        ->focused(false)
        ->focusBorder(false)
        ->padding(10, 2)
        ->elevation(0)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->stroke(0, Color::transparent())
        ->onQueryTextListener(
            [this](const std::string& query) {
                if (m_miller_view) {
                    m_miller_view->set_search_query(query);
                }
            },
            [this](const std::string&) {
                if (m_miller_view) {
                    m_miller_view->navigate_enter();
                }
            }
        )
        ->build();
    m_search_view->set_focused(false);

    LayoutParams search_lp;
    search_lp.width = 240;
    search_lp.height = 28;
    search_lp.gravity = Gravity::CenterVertical;
    m_ribbon->add_view(m_search_view, search_lp);

    LayoutParams ribbon_lp;
    ribbon_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    ribbon_lp.height = 36;
    m_root_layout->add_view(m_ribbon, ribbon_lp);

    // 3. Middle Body: MillerView
    m_miller_view = std::make_shared<MillerView>();
    m_miller_view->init();
    m_miller_view->set_on_state_changed([this]() {
        update_header();
    });

    LayoutParams miller_lp;
    miller_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    miller_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    miller_lp.weight = 1.0f;
    m_root_layout->add_view(m_miller_view, miller_lp);
}

void TestApp::sync_theme() {
    auto config = Config::get();
    m_root_layout->set_background_color(Color::transparent());

    if (m_ribbon) {
        m_ribbon->set_background_color(Color::transparent());
    }
    if (m_path_label) {
        m_path_label->set_text_color(config->colors.on_surface_variant);
        m_path_label->set_text_size(config->metrics.font_size);
    }
    if (m_back_label) {
        m_back_label->set_text_color(config->colors.primary);
        m_back_label->set_text_size(config->metrics.font_size);
    }
    if (m_back_btn) {
        m_back_btn->set_card_background_color(config->colors.surface_variant.with_alpha(0.35f));
    }
    if (m_search_view) {
        m_search_view->set_background_color(config->colors.surface_variant.with_alpha(0.35f));
        m_search_view->set_stroke(0, Color::transparent());
    }

    if (m_miller_view) {
        m_miller_view->sync_theme();
    }

    update_header();
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void TestApp::update_header() {
    if (!m_miller_view) return;

    if (m_path_label) {
        m_path_label->set_text(m_miller_view->get_breadcrumb_path());
    }

    if (m_back_btn) {
        bool can_back = m_miller_view->can_navigate_back();
        m_back_btn->set_visibility(can_back ? Visibility::Visible : Visibility::Gone);
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void TestApp::handle_key_press(const KeyPressEvent& event) {
    if (!event.pressed || !m_miller_view) return;

    // Ctrl+F from anywhere instantly focuses the search bar
    if (event.has_ctrl() && (event.keysym == XKB_KEY_f || event.keysym == XKB_KEY_F)) {
        if (m_search_view) {
            m_search_view->set_focused(true);
            if (m_window) m_window->schedule_redraw();
            return;
        }
    }

    // Tab / Shift+Tab cycles focus among focusable views across the window
    if (event.keysym == XKB_KEY_Tab || event.keysym == XKB_KEY_ISO_Left_Tab) {
        if (m_window) {
            if (event.has_shift() || event.keysym == XKB_KEY_ISO_Left_Tab) {
                m_window->focus_prev();
            } else {
                m_window->focus_next();
            }
            m_window->schedule_redraw();
            return;
        }
    }

    if (m_window && m_window->get_focused_view() && m_window->get_focused_view()->has_focus()) {
        bool is_search_focused = (m_search_view && (m_search_view->is_focused() || m_search_view->has_focus() ||
                                  (m_search_view->get_edit_text() && m_search_view->get_edit_text()->has_focus())));

        if (event.keysym == XKB_KEY_Escape) {
            m_window->clear_focus();
            if (m_search_view) {
                m_search_view->set_focused(false);
                m_search_view->set_query("");
            }
            if (m_miller_view && m_miller_view->is_searching()) {
                m_miller_view->set_search_query("");
            }
            if (m_miller_view && m_miller_view->is_preview_focused()) {
                m_miller_view->set_preview_focused(false);
            }
            if (m_miller_view && m_miller_view->is_preview_expanded()) {
                m_miller_view->set_preview_expanded(false);
            }
            if (m_window) m_window->schedule_redraw();
            return;
        }

        if (is_search_focused) {
            if (event.keysym == XKB_KEY_Up) {
                m_miller_view->navigate_up();
                return;
            }
            if (event.keysym == XKB_KEY_Down) {
                m_miller_view->navigate_down();
                return;
            }
            if (event.keysym == XKB_KEY_Return || event.keysym == XKB_KEY_KP_Enter) {
                m_miller_view->navigate_enter();
                return;
            }
        } else {
            bool is_edit_text = (dynamic_cast<EditText*>(m_window->get_focused_view()) != nullptr);
            if (!is_edit_text) {
                if (!event.has_shift() && (event.keysym == XKB_KEY_Left || event.keysym == XKB_KEY_h)) {
                    m_miller_view->navigate_back();
                    return;
                }
                if (event.keysym == XKB_KEY_j) {
                    if (m_window->focus_direction(FocusDirection::Down)) {
                        m_window->schedule_redraw();
                        return;
                    }
                } else if (event.keysym == XKB_KEY_k) {
                    if (m_window->focus_direction(FocusDirection::Up)) {
                        m_window->schedule_redraw();
                        return;
                    }
                }
            }
        }
        return;
    }

    switch (event.keysym) {
        case XKB_KEY_k:
        case XKB_KEY_Up:
            m_miller_view->navigate_up();
            break;

        case XKB_KEY_j:
        case XKB_KEY_Down:
            m_miller_view->navigate_down();
            break;

        case XKB_KEY_l:
        case XKB_KEY_Right:
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            m_miller_view->navigate_enter();
            break;

        case XKB_KEY_h:
        case XKB_KEY_Left:
            if (!event.has_shift()) {
                m_miller_view->navigate_back();
            }
            break;

        case XKB_KEY_Escape:
            if (!m_miller_view->navigate_back()) {
                if (m_engine) m_engine->quit();
            }
            break;

        default:
            break;
    }
}

} // namespace miqu
