#pragma once

#include <miqutoolkit/miqutoolkit.hpp>
#include "model/launcher_mode.hpp"
#include "providers/app_provider.hpp"
#include "providers/window_provider.hpp"
#include "providers/workspace_provider.hpp"
#include "providers/run_provider.hpp"
#include "providers/script_provider.hpp"
#include "providers/dmenu_provider.hpp"
#include <memory>
#include <vector>

namespace miqu {

class LauncherGridItemView;

class LauncherWindow {
public:
    LauncherWindow(AppEngine* engine, LauncherConfig config);
    ~LauncherWindow() = default;

    bool init();

private:
    void switch_mode(size_t mode_index);
    void refresh_current_mode();
    void handle_item_click(size_t index, std::shared_ptr<View> view);
    void handle_submit(const std::string& query);

    AppEngine* m_engine = nullptr;
    LauncherConfig m_config;
    size_t m_active_mode_index = 0;

    std::shared_ptr<Window> m_window;
    std::shared_ptr<GridView> m_grid;
    std::shared_ptr<SearchView> m_search;

    std::vector<LauncherItem> m_current_items;
    mutable std::vector<std::shared_ptr<LauncherGridItemView>> m_view_cache;

    AppProvider m_app_provider;
    WindowProvider m_window_provider;
    WorkspaceProvider m_workspace_provider;
    RunProvider m_run_provider;
    std::unique_ptr<ScriptProvider> m_script_provider;
    DmenuProvider m_dmenu_provider;
};

} // namespace miqu
