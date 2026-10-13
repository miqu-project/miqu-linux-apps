#include "wallpaper_view.hpp"
#include <algorithm>

namespace miqutheme {

using namespace miqu;

WallpaperView::WallpaperView(std::shared_ptr<ThemeBackend> backend, std::function<void()> on_refresh_requested)
    : LinearLayout(Orientation::Vertical),
      backend_(std::move(backend)),
      on_refresh_requested_(std::move(on_refresh_requested)) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    set_padding(18, 12, 18, 12);

    viewing_mode_ = backend_->get_current_mode();
    current_fit_ = backend_->get_wallpaper_fit();

    build_ui();
}

void WallpaperView::refresh_ui() {
    // Sync viewing mode with current system mode if it changed externally
    viewing_mode_ = backend_->get_current_mode();
    current_fit_ = backend_->get_wallpaper_fit();
    if (mode_spinner_) {
        mode_spinner_->set_selected_index(viewing_mode_ == ThemeMode::Dark ? 0 : 1);
    }
    populate_grid();
    request_redraw();
}

void WallpaperView::build_ui() {
    clear_views();

    // =========================================================================
    // 1. TOP CONTROL BAR (Dropdown Mode Filter & Fit Selector)
    // =========================================================================
    auto top_bar = std::make_shared<LinearLayout>(Orientation::Horizontal);
    top_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    top_bar->set_margin(0, 0, 0, 14);

    // Mode Dropdown Spinner
    std::vector<std::string> mode_items = {
        "Mode: Dark Wallpapers",
        "Mode: Light Wallpapers"
    };

    mode_spinner_ = SpinnerBuilder::create()
        ->items(mode_items)
        ->selectedIndex(viewing_mode_ == ThemeMode::Dark ? 0 : 1)
        ->onItemSelected([this](int idx, const std::string&) {
            viewing_mode_ = (idx == 0) ? ThemeMode::Dark : ThemeMode::Light;
            populate_grid();
        })
        ->build();
    mode_spinner_->set_layout_params(LayoutParams(
        210,
        34
    ));
    mode_spinner_->set_margin(0, 0, 10, 0);
    top_bar->add_view(mode_spinner_);

    // Spacer
    auto spacer = std::make_shared<LinearLayout>(Orientation::Horizontal);
    spacer->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    top_bar->add_view(spacer);

    // Fit Mode Dropdown Spinner
    std::vector<std::string> fit_labels = {
        "Fit: Fill",
        "Fit: Contain",
        "Fit: Stretch",
        "Fit: Center",
        "Fit: Tile"
    };
    std::vector<std::string> fit_values = {
        "fill", "fit", "stretch", "center", "tile"
    };

    int current_fit_idx = 0;
    for (size_t i = 0; i < fit_values.size(); ++i) {
        if (fit_values[i] == current_fit_) {
            current_fit_idx = static_cast<int>(i);
            break;
        }
    }

    fit_spinner_ = SpinnerBuilder::create()
        ->items(fit_labels)
        ->selectedIndex(current_fit_idx)
        ->onItemSelected([this, fit_values](int idx, const std::string&) {
            if (idx >= 0 && idx < static_cast<int>(fit_values.size())) {
                current_fit_ = fit_values[idx];
                std::string active_wp = backend_->get_active_wallpaper(viewing_mode_);
                if (!active_wp.empty()) {
                    backend_->apply_wallpaper(active_wp, current_fit_);
                }
            }
        })
        ->build();
    fit_spinner_->set_layout_params(LayoutParams(
        150,
        34
    ));
    top_bar->add_view(fit_spinner_);

    add_view(top_bar);

    // =========================================================================
    // 2. WALLPAPER GALLERY GRID (Using ImageQuality::ThumbnailFast)
    // =========================================================================
    grid_view_ = std::make_shared<GridView>();
    grid_view_->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        0,
        1.0f
    ));
    grid_view_->set_auto_fit(190);
    grid_view_->set_cell_height(150);
    grid_view_->set_horizontal_spacing(12);
    grid_view_->set_vertical_spacing(12);
    grid_view_->set_selection_border(true);

    grid_view_->set_on_item_click_listener([this](size_t idx, std::shared_ptr<View>) {
        if (idx < current_wallpapers_.size()) {
            const auto& wp = current_wallpapers_[idx];
            backend_->apply_wallpaper(wp.full_path, current_fit_);
            if (on_refresh_requested_) on_refresh_requested_();
        }
    });

    add_view(grid_view_);
    populate_grid();
}

void WallpaperView::populate_grid() {
    if (!grid_view_) return;

    grid_view_->clear_items();
    current_wallpapers_ = backend_->get_wallpapers(viewing_mode_);

    std::string active_wp = backend_->get_active_wallpaper(viewing_mode_);
    int selected_idx = -1;

    for (size_t i = 0; i < current_wallpapers_.size(); ++i) {
        const auto& wp = current_wallpapers_[i];
        bool is_active = (wp.full_path == active_wp);
        if (is_active) selected_idx = static_cast<int>(i);

        // Format clean label without extension
        std::string title = wp.filename;
        auto dot = title.find_last_of('.');
        if (dot != std::string::npos) title = title.substr(0, dot);
        if (title.length() > 20) title = title.substr(0, 18) + "...";

        auto item = GridItemViewBuilder::create()
            ->title(title)
            ->subtitle(is_active ? "Active ✓" : "")
            ->iconSource(wp.full_path)
            ->isImage(true)
            ->qualityMode(ImageQuality::ThumbnailFast)
            ->build();

        item->set_corner_radius(10);
        if (is_active) {
            item->set_title_bold(true);
            item->set_highlight_title(true);
        }

        grid_view_->add_item(item);
    }

    if (selected_idx >= 0) {
        grid_view_->set_selected_index(selected_idx);
    }

    request_redraw();
}

} // namespace miqutheme
