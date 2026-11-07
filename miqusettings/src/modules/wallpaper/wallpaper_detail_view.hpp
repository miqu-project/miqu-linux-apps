#pragma once

#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/image_button.hpp>
#include <miqutoolkit/view/spinner.hpp>
#include <miqutoolkit/view/button.hpp>
#include <string>
#include <memory>
#include <atomic>

namespace miqu {

class WallpaperDisplayCanvas;

class WallpaperDetailView : public FrameLayout {
public:
    WallpaperDetailView();
    ~WallpaperDetailView() override;

    void open_file_picker();
    void set_wallpaper_image(const std::string& path);
    void set_fit_mode(FitMode mode);
    void apply_changes();
    bool has_unapplied_changes() const;

    static std::string get_cached_wallpaper_path();
    static FitMode get_cached_fit_mode();
    static void apply_to_miqubg(const std::string& path, FitMode mode);
    void init_from_miqubg();
    static std::string fit_mode_to_miqubg_string(FitMode mode);
    static FitMode miqubg_string_to_fit_mode(const std::string& str);

private:
    void setup_ui();
    void query_display_aspect();
    std::shared_ptr<View> create_control_header();
    void update_apply_button_state();
    void update_status_text();

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
    std::atomic<bool> m_picker_running{false};

    int m_disp_width = 1920;
    int m_disp_height = 1080;
    double m_aspect_ratio = 16.0 / 9.0;
    std::string m_resolution_str = "1920 × 1080 (16:9)";

    std::string m_pending_wallpaper_path;
    FitMode m_pending_fit_mode = FitMode::Cover;

    std::shared_ptr<LinearLayout> m_root_layout;
    std::shared_ptr<WallpaperDisplayCanvas> m_canvas;
    std::shared_ptr<Spinner> m_fit_spinner;
    std::shared_ptr<ImageButton> m_browse_button;
    std::shared_ptr<Button> m_apply_button;
    std::shared_ptr<TextView> m_status_label;
};

} // namespace miqu
