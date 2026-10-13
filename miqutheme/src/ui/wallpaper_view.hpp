#pragma once

#include "backend/theme_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <functional>

namespace miqutheme {

class WallpaperView : public miqu::LinearLayout {
public:
    WallpaperView(std::shared_ptr<ThemeBackend> backend, std::function<void()> on_refresh_requested);
    ~WallpaperView() override = default;

    void refresh_ui();

private:
    std::shared_ptr<ThemeBackend> backend_;
    std::function<void()> on_refresh_requested_;

    ThemeMode viewing_mode_; // Tracks whether currently viewing Dark or Light wallpapers in dropdown
    std::string current_fit_;

    std::shared_ptr<miqu::Spinner> mode_spinner_;
    std::shared_ptr<miqu::Spinner> fit_spinner_;
    std::shared_ptr<miqu::GridView> grid_view_;

    std::vector<WallpaperInfo> current_wallpapers_;

    void build_ui();
    void populate_grid();
};

} // namespace miqutheme
