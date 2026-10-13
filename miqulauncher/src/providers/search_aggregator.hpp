#pragma once

#include "model/launcher_item.hpp"
#include "app_provider.hpp"
#include "window_provider.hpp"
#include "workspace_provider.hpp"
#include "run_provider.hpp"
#include "file_provider.hpp"
#include "calc_provider.hpp"
#include "web_provider.hpp"
#include <vector>
#include <string>

namespace miqu {

class SearchAggregator {
public:
    SearchAggregator(
        AppProvider* app_provider,
        WindowProvider* window_provider,
        WorkspaceProvider* workspace_provider,
        RunProvider* run_provider
    );

    std::vector<LauncherItem> query(const std::string& search_text);
    void activate(const LauncherItem& item);

private:
    AppProvider* m_app_provider = nullptr;
    WindowProvider* m_window_provider = nullptr;
    WorkspaceProvider* m_workspace_provider = nullptr;
    RunProvider* m_run_provider = nullptr;

    FileProvider m_file_provider;
    CalcProvider m_calc_provider;
    WebProvider m_web_provider;
};

} // namespace miqu
