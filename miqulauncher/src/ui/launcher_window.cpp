#include "launcher_window.hpp"
#include "grid_item_view.hpp"
#include "system/binary_manager.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <thread>

namespace miqu {

LauncherWindow::LauncherWindow(AppEngine* engine, LauncherConfig config)
    : m_engine(engine), m_config(std::move(config)), m_active_mode_index(m_config.active_mode_index) {}

bool LauncherWindow::init() {
    if (!m_engine || m_config.modes.empty()) return false;

    if (m_active_mode_index >= m_config.modes.size()) {
        m_active_mode_index = 0;
    }

    if (m_config.dmenu_mode) {
        m_dmenu_provider.load_from_stdin();
    }

    auto config = Config::get();

    // 1. Pre-load app registry synchronously so initial drun items are ready
    m_app_provider.ensure_loaded();

    // 2. Pre-warm system binary index asynchronously
    std::thread([]() {
        BinaryManager::get_binary_entries();
    }).detach();

    // 3. GridView setup
    m_grid = GridViewBuilder::create()
        ->autoFit(m_config.cell_size)
        ->cellHeight(m_config.cell_size)
        ->spacing(m_config.spacing, m_config.spacing)
        ->onItemClick([this](size_t index, std::shared_ptr<View> view) {
            handle_item_click(index, view);
        })
        ->build();

    const auto& init_mode = m_config.modes[m_active_mode_index];

    // 4. Search View setup
    m_search = SearchViewBuilder::create()
        ->title(init_mode.display_label)
        ->hint(init_mode.hint)
        ->focused(true)
        ->padding(18, 12)
        ->margin(0, 0, 0, 12)
        ->onQueryTextListener(
            [this](const std::string& query) {
                refresh_current_mode();
            },
            [this](const std::string& submitQuery) {
                handle_submit(submitQuery);
            }
        )
        ->build();

    if (!m_config.initial_query.empty()) {
        m_search->set_query(m_config.initial_query);
    }

    // 5. Populate initial mode BEFORE constructing the Wayland window
    switch_mode(m_active_mode_index);

    // 6. Layout assembly
    auto contentLayout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->addView(m_search, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent)))
        ->addView(m_grid, LayoutParams(1.0f))
        ->build();

    auto rootCard = CardViewBuilder::create()
        ->backgroundColor(config->colors.background)
        ->stroke(config->metrics.border_width, config->colors.outline)
        ->cornerRadius(config->metrics.corner_radius)
        ->padding(16)
        ->addView(contentLayout, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::MatchParent)))
        ->build();

    // 7. Live state subscriptions
    m_window_provider.on_windows_changed([this]() {
        if (m_config.modes[m_active_mode_index].type == ModeType::Window) {
            refresh_current_mode();
        }
    });

    m_workspace_provider.on_workspaces_changed([this]() {
        if (m_config.modes[m_active_mode_index].type == ModeType::Workspace) {
            refresh_current_mode();
        }
    });

    // 8. Layer overlay Window (Frame 0 is 100% pre-warmed & ready to draw)
    auto builder = WindowBuilder::create()
        ->role(WindowRole::LayerOverlay)
        ->appId("miqulauncher")
        ->keyboardInteractive(true)
        ->preferredSize(m_config.width, m_config.height)
        ->contentSize(m_config.width, m_config.height)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(rootCard)
        ->onClose([this]() {
            m_engine->quit();
        });

    if (!m_config.dim_backdrop) {
        builder->anchors(0)->dimBackdrop(false);
    } else {
        builder->dimBackdrop(true);
    }

    m_window = builder
        ->onKey([this](const KeyPressEvent& event) {
            if (!event.pressed) return;
            if (m_config.allow_mode_switch && m_config.modes.size() > 1) {
                if (event.has_shift() && event.keysym == XKB_KEY_Right) {
                    switch_mode((m_active_mode_index + 1) % m_config.modes.size());
                } else if (event.has_shift() && event.keysym == XKB_KEY_Left) {
                    switch_mode((m_active_mode_index + m_config.modes.size() - 1) % m_config.modes.size());
                } else if (event.has_alt()) {
                    if (event.keysym >= XKB_KEY_1 && event.keysym < XKB_KEY_1 + static_cast<uint32_t>(m_config.modes.size())) {
                        switch_mode(event.keysym - XKB_KEY_1);
                    }
                }
            }
        })
        ->build();

    return m_window != nullptr;
}

void LauncherWindow::switch_mode(size_t mode_index) {
    if (mode_index >= m_config.modes.size()) return;
    m_active_mode_index = mode_index;
    const auto& mode = m_config.modes[mode_index];

    if (mode.type == ModeType::Script) {
        m_script_provider = std::make_unique<ScriptProvider>(mode.script_path);
    } else {
        m_script_provider.reset();
    }

    m_search->set_title(mode.display_label);
    m_search->set_hint(mode.hint);
    m_search->set_focused(true);

    refresh_current_mode();
}

void LauncherWindow::refresh_current_mode() {
    std::string query = m_search ? m_search->get_query() : "";
    const auto& mode = m_config.modes[m_active_mode_index];

    int desired_selected_idx = 0;

    switch (mode.type) {
        case ModeType::App: {
            m_current_items = m_app_provider.get_items(query);
            break;
        }
        case ModeType::Window: {
            m_current_items = m_window_provider.get_items(query);
            break;
        }
        case ModeType::Workspace: {
            int active_idx = -1;
            m_current_items = m_workspace_provider.get_items(query, &active_idx);
            if (active_idx >= 0 && query.empty()) {
                desired_selected_idx = active_idx;
            }
            break;
        }
        case ModeType::Run: {
            m_current_items = m_run_provider.get_items(query);
            break;
        }
        case ModeType::Script: {
            if (m_script_provider) {
                m_current_items = m_script_provider->query(query);
            } else {
                m_current_items.clear();
            }
            break;
        }
        case ModeType::Dmenu: {
            m_current_items = m_dmenu_provider.query(query);
            break;
        }
    }

    if (!m_config.show_subtitles) {
        for (auto& item : m_current_items) {
            item.subtitle.clear();
        }
    }

    m_view_cache.assign(m_current_items.size(), nullptr);

    // Pre-warm visible items for Frame 0 instant draw (exact visible viewport)
    int stride = std::max(1, m_config.cell_size + m_config.spacing);
    int cols = std::max(1, m_config.width / stride);
    int grid_h = std::max(1, m_config.height - 80);
    int rows = std::max(1, grid_h / stride);
    size_t visible_capacity = static_cast<size_t>(cols * rows);
    size_t prewarm_count = std::min<size_t>(m_current_items.size(), std::max<size_t>(10, visible_capacity));
    for (size_t i = 0; i < prewarm_count; ++i) {
        m_view_cache[i] = std::make_shared<LauncherGridItemView>(m_current_items[i]);
    }

    m_grid->set_item_provider(m_current_items.size(), [this](size_t index) -> std::shared_ptr<View> {
        if (index >= m_current_items.size()) return nullptr;
        if (index >= m_view_cache.size()) {
            m_view_cache.resize(m_current_items.size(), nullptr);
        }
        if (!m_view_cache[index]) {
            m_view_cache[index] = std::make_shared<LauncherGridItemView>(m_current_items[index]);
        }
        return m_view_cache[index];
    });

    if (m_current_items.empty()) {
        m_grid->set_selected_index(-1);
    } else {
        m_grid->set_selected_index(desired_selected_idx);
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void LauncherWindow::handle_item_click(size_t index, std::shared_ptr<View> view) {
    LauncherItem data;
    if (index < m_current_items.size()) {
        data = m_current_items[index];
    } else {
        auto item_view = std::dynamic_pointer_cast<LauncherGridItemView>(view);
        if (!item_view) return;
        data = item_view->get_data();
    }

    const auto& mode = m_config.modes[m_active_mode_index];

    switch (mode.type) {
        case ModeType::App:
            m_app_provider.launch(data);
            break;
        case ModeType::Window:
            m_window_provider.activate(data);
            break;
        case ModeType::Workspace:
            m_workspace_provider.activate(data);
            break;
        case ModeType::Run:
            m_run_provider.launch(data);
            break;
        case ModeType::Script:
            if (m_script_provider) {
                m_script_provider->activate(data);
            }
            break;
        case ModeType::Dmenu:
            m_dmenu_provider.activate(data);
            break;
    }

    m_engine->quit();
}

void LauncherWindow::handle_submit(const std::string& query) {
    const auto& mode = m_config.modes[m_active_mode_index];

    if (mode.type == ModeType::Run && !query.empty()) {
        m_run_provider.launch_raw(query);
        m_engine->quit();
        return;
    }

    if (mode.type == ModeType::Dmenu) {
        if (auto item = m_grid->get_selected_item()) {
            handle_item_click(m_grid->get_selected_index(), item);
        } else if (!query.empty()) {
            m_dmenu_provider.submit(query);
            m_engine->quit();
        }
        return;
    }

    if (auto item = m_grid->get_selected_item()) {
        handle_item_click(m_grid->get_selected_index(), item);
    }
}

} // namespace miqu
