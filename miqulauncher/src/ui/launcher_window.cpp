#include "launcher_window.hpp"
#include "grid_item_view.hpp"
#include "system/binary_manager.hpp"
#include <miqutoolkit/view/list_item_view.hpp>
#include <miqutoolkit/view/relative_layout.hpp>
#include <miqutoolkit/view/image_button.hpp>
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

    // 3. Initialize universal search aggregator
    m_search_aggregator = std::make_unique<SearchAggregator>(
        &m_app_provider,
        &m_window_provider,
        &m_workspace_provider,
        &m_run_provider
    );

    const auto& init_mode = m_config.modes[m_active_mode_index];

    // 4. GridView setup
    if (!m_config.allow_mode_switch && init_mode.type == ModeType::Search && m_config.search_view_mode != "inherit") {
        m_is_list_view = (m_config.search_view_mode == "list");
    } else {
        m_is_list_view = (m_config.view_mode == "list");
    }

    m_grid = GridViewBuilder::create()
        ->selectionBorder(m_config.selection_border)
        ->onItemClick([this](size_t index, std::shared_ptr<View> view) {
            handle_item_click(index, view);
        })
        ->build();
    apply_view_mode();

    // 4. Search View setup
    int search_v_pad = std::max(2, (m_config.search_height - 24) / 2);
    m_search = SearchViewBuilder::create()
        ->title(init_mode.display_label)
        ->hint(init_mode.hint)
        ->focused(true)
        ->focusBorder(m_config.search_focus_border)
        ->elevation(m_config.search_elevation)
        ->padding(16, search_v_pad)
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

    // Mode cycle button (Cycles through modes in combo mode)
    if (m_config.allow_mode_switch && m_config.modes.size() > 1) {
        m_mode_cycle_btn = ImageButtonBuilder::create()
            ->icon("view-refresh")
            ->iconSize(18)
            ->cornerRadius(config->metrics.corner_radius)
            ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
            ->hoverColor(config->colors.surface_variant.with_alpha(0.70f))
            ->circle(false)
            ->onClick([this]() {
                if (m_config.modes.size() > 1) {
                    switch_mode((m_active_mode_index + 1) % m_config.modes.size());
                }
            })
            ->build();
        m_mode_cycle_btn->set_padding(6);
    }

    // View toggle button (Grid vs. List)
    m_view_toggle_btn = ImageButtonBuilder::create()
        ->icon(m_is_list_view ? "view-grid" : "view-list")
        ->iconSize(18)
        ->cornerRadius(config->metrics.corner_radius)
        ->backgroundColor(config->colors.surface_variant.with_alpha(0.35f))
        ->hoverColor(config->colors.surface_variant.with_alpha(0.70f))
        ->circle(false)
        ->onClick([this]() {
            toggle_view_mode();
        })
        ->build();
    m_view_toggle_btn->set_padding(6);

    // 5. Populate initial mode BEFORE constructing the Wayland window
    switch_mode(m_active_mode_index);

    // 6. Layout assembly
    int card_inner_w = std::max(200, m_config.width - (m_config.edge_padding * 2));
    int search_percent = std::clamp(m_config.search_width_percent, 20, 100);
    int search_w = std::clamp((card_inner_w * search_percent) / 100, 280, card_inner_w - 56);

    LayoutParams search_lp;
    search_lp.width = search_w;
    search_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    search_lp.center_horizontal = true;
    search_lp.center_vertical = true;

    LayoutParams toggle_lp;
    toggle_lp.width = 36;
    toggle_lp.height = 36;
    toggle_lp.align_parent_end = true;
    toggle_lp.center_vertical = true;

    auto header_builder = RelativeLayoutBuilder::create()
        ->addView(m_search, search_lp)
        ->addView(m_view_toggle_btn, toggle_lp)
        ->margin(0, 0, 0, 12);

    if (m_mode_cycle_btn) {
        LayoutParams cycle_lp;
        cycle_lp.width = 36;
        cycle_lp.height = 36;
        cycle_lp.align_parent_start = true;
        cycle_lp.center_vertical = true;
        header_builder->addView(m_mode_cycle_btn, cycle_lp);
    }

    m_header = header_builder->build();

    auto contentLayout = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->addView(m_header, LayoutParams(static_cast<int>(LayoutDimension::MatchParent), static_cast<int>(LayoutDimension::WrapContent)))
        ->addView(m_grid, LayoutParams(1.0f))
        ->build();

    auto rootCard = CardViewBuilder::create()
        ->backgroundColor(config->colors.background)
        ->stroke(config->metrics.border_width, config->colors.outline)
        ->cornerRadius(config->metrics.corner_radius)
        ->padding(m_config.edge_padding)
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
            if (event.has_ctrl() && (event.keysym == XKB_KEY_v || event.keysym == XKB_KEY_V)) {
                toggle_view_mode();
                return;
            }
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
        case ModeType::Search: {
            if (m_search_aggregator) {
                m_current_items = m_search_aggregator->query(query);
            }
            break;
        }
    }

    if (!m_config.show_subtitles) {
        for (auto& item : m_current_items) {
            item.subtitle.clear();
        }
    }

    m_view_cache.assign(m_current_items.size(), nullptr);

    // Pre-warm visible items for Frame 0 instant draw
    size_t prewarm_count = std::min<size_t>(m_current_items.size(), 12);
    for (size_t i = 0; i < prewarm_count; ++i) {
        m_view_cache[i] = create_item_view(i);
    }

    m_grid->set_item_provider(m_current_items.size(), [this](size_t index) -> std::shared_ptr<View> {
        if (index >= m_current_items.size()) return nullptr;
        if (index >= m_view_cache.size()) {
            m_view_cache.resize(m_current_items.size(), nullptr);
        }
        if (!m_view_cache[index]) {
            m_view_cache[index] = create_item_view(index);
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
        case ModeType::Search:
            if (m_search_aggregator) {
                m_search_aggregator->activate(data);
            }
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

    if (mode.type == ModeType::Search) {
        if (auto item = m_grid->get_selected_item()) {
            handle_item_click(m_grid->get_selected_index(), item);
        } else if (m_search_aggregator && !m_current_items.empty()) {
            m_search_aggregator->activate(m_current_items[0]);
            m_engine->quit();
        }
        return;
    }

    if (auto item = m_grid->get_selected_item()) {
        handle_item_click(m_grid->get_selected_index(), item);
    }
}

void LauncherWindow::apply_view_mode() {
    if (!m_grid) return;

    if (m_is_list_view) {
        m_grid->set_num_columns(1);
        m_grid->set_stretch_mode(StretchMode::ColumnWidth);
        m_grid->set_cell_height(48);
        m_grid->set_horizontal_spacing(0);
        m_grid->set_vertical_spacing(4);
    } else {
        m_grid->set_auto_fit(m_config.cell_size);
        m_grid->set_stretch_mode(StretchMode::ColumnWidth);
        m_grid->set_cell_height(m_config.cell_size);
        m_grid->set_horizontal_spacing(m_config.spacing);
        m_grid->set_vertical_spacing(m_config.spacing);
    }

    if (m_view_toggle_btn) {
        m_view_toggle_btn->set_icon(m_is_list_view ? "view-grid" : "view-list");
    }

    m_view_cache.clear();
    m_view_cache.assign(m_current_items.size(), nullptr);

    size_t prewarm_count = std::min<size_t>(m_current_items.size(), 12);
    for (size_t i = 0; i < prewarm_count; ++i) {
        m_view_cache[i] = create_item_view(i);
    }

    if (m_window) {
        m_window->schedule_redraw();
    }
}

void LauncherWindow::toggle_view_mode() {
    m_is_list_view = !m_is_list_view;
    apply_view_mode();
}

std::shared_ptr<View> LauncherWindow::create_item_view(size_t index) const {
    if (index >= m_current_items.size()) return nullptr;
    const auto& item = m_current_items[index];

    if (m_is_list_view) {
        std::string icon_src = item.icon_path.empty() ? item.icon_name : item.icon_path;
        auto builder = ListItemViewBuilder::create()
            ->title(item.title)
            ->subtitle(item.subtitle)
            ->trailingText(item.badge)
            ->iconSource(icon_src)
            ->iconSize(28)
            ->isImage(item.is_image);
        if (!item.badge.empty()) {
            builder->highlightTrailing(true);
        }
        if (item.is_image) {
            builder->qualityMode(ImageQuality::ThumbnailFast);
        }
        if (item.is_active || item.subtitle.find("Active") != std::string::npos) {
            builder->highlightSubtitle(true);
        }
        return builder->build();
    } else {
        return std::make_shared<LauncherGridItemView>(item);
    }
}

} // namespace miqu
