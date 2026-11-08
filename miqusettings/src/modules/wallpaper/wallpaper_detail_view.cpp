#include "modules/wallpaper/wallpaper_detail_view.hpp"
#include "modules/monitors/monitor_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/core/string_utils.hpp>
#include <miqutoolkit/core/window.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/image_button.hpp>
#include <miqutoolkit/view/spinner.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <pango/pangocairo.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <numeric>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <miqutoolkit/core/fs_utils.hpp>

namespace fs = std::filesystem;

namespace miqu {

using PangoLayoutPtr = std::unique_ptr<PangoLayout, decltype(&g_object_unref)>;
using PangoFontDescPtr = std::unique_ptr<PangoFontDescription, decltype(&pango_font_description_free)>;

static std::string s_cached_wallpaper_path = "";
static FitMode s_cached_fit_mode = FitMode::Cover;

// =============================================================================
// WallpaperDisplayCanvas: Custom aspect-ratio-respecting monitor preview
// =============================================================================
class WallpaperDisplayCanvas : public View {
public:
    WallpaperDisplayCanvas() {
        m_image_view = std::make_shared<ImageView>();
        m_image_view->set_fit_mode(FitMode::Cover);
        m_image_view->set_quality_mode(ImageQuality::FullOriginal);
    }

    ~WallpaperDisplayCanvas() override = default;

    void set_aspect_ratio(double ratio) {
        m_aspect_ratio = (ratio > 0.1) ? ratio : (16.0 / 9.0);
        request_redraw();
    }

    double get_aspect_ratio() const { return m_aspect_ratio; }

    std::shared_ptr<ImageView> get_image_view() const {
        return m_image_view;
    }

    void set_on_click_listener(std::function<void()> cb) {
        m_on_click = std::move(cb);
    }

    void draw(cairo_t* cr, const Rect& bounds) override {
        if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;
        m_bounds = bounds;

        auto config = Config::get();

        int pad_h = 24;
        int pad_v = 16;
        int avail_w = std::max(0, bounds.width - pad_h * 2);
        int avail_h = std::max(0, bounds.height - pad_v * 2);
        if (avail_w <= 0 || avail_h <= 0) return;

        // Calculate maximum rectangle fitting within avail bounds with exact aspect ratio
        int target_w = avail_w;
        int target_h = static_cast<int>(std::round(target_w / m_aspect_ratio));
        if (target_h > avail_h) {
            target_h = avail_h;
            target_w = static_cast<int>(std::round(target_h * m_aspect_ratio));
        }

        int target_x = bounds.x + (bounds.width - target_w) / 2;
        int target_y = bounds.y + (bounds.height - target_h) / 2;
        m_screen_rect = Rect(target_x, target_y, target_w, target_h);

        int corner_r = 12;

        // Subtle ambient shadow
        CardView::draw_elevation_shadow(cr, m_screen_rect, corner_r, 4.0);

        // Clip and draw screen surface
        cairo_save(cr);
        CardView::draw_rounded_rect(cr, m_screen_rect.x, m_screen_rect.y, m_screen_rect.width, m_screen_rect.height, corner_r);
        cairo_clip(cr);

        // Screen canvas background (surface_variant or dark theme surface)
        cairo_set_source_rgba(cr, config->colors.surface_variant.r,
                                  config->colors.surface_variant.g,
                                  config->colors.surface_variant.b, 1.0f);
        cairo_paint(cr);

        if (m_image_view && !m_image_view->get_image_resource().empty()) {
            // Draw image filling the display aspect ratio area according to FitMode
            m_image_view->set_bounds(m_screen_rect);
            m_image_view->draw(cr, m_screen_rect);
        } else {
            // Empty state: clean subtle dashed outline & placeholder text
            cairo_save(cr);
            double dashes[] = {6.0, 6.0};
            cairo_set_dash(cr, dashes, 2, 0);
            cairo_set_source_rgba(cr, config->colors.outline_variant.r,
                                      config->colors.outline_variant.g,
                                      config->colors.outline_variant.b, 0.45f);
            cairo_set_line_width(cr, 1.5);
            CardView::draw_rounded_rect(cr, m_screen_rect.x + 8, m_screen_rect.y + 8,
                                        m_screen_rect.width - 16, m_screen_rect.height - 16, corner_r - 2);
            cairo_stroke(cr);
            cairo_restore(cr);

            // Placeholder text inside empty image view
            PangoLayoutPtr layout(pango_cairo_create_layout(cr), g_object_unref);
            std::string font_family = config->metrics.font_family.empty() ? "Sans" : config->metrics.font_family;
            std::string font_desc = font_family + " 13";
            PangoFontDescPtr desc(pango_font_description_from_string(font_desc.c_str()), pango_font_description_free);
            pango_layout_set_font_description(layout.get(), desc.get());

            std::string empty_text = "No Wallpaper Loaded\nClick Browse to choose an image";
            pango_layout_set_text(layout.get(), empty_text.c_str(), -1);
            pango_layout_set_alignment(layout.get(), PANGO_ALIGN_CENTER);

            int text_w = 0, text_h = 0;
            pango_layout_get_pixel_size(layout.get(), &text_w, &text_h);

            int tx = m_screen_rect.x + (m_screen_rect.width - text_w) / 2;
            int ty = m_screen_rect.y + (m_screen_rect.height - text_h) / 2;

            cairo_move_to(cr, tx, ty);
            cairo_set_source_rgba(cr, config->colors.outline.r,
                                      config->colors.outline.g,
                                      config->colors.outline.b, 0.7f);
            pango_cairo_show_layout(cr, layout.get());
        }

        cairo_restore(cr);

        // Screen bezel stroke outline
        cairo_save(cr);
        CardView::draw_rounded_rect(cr, m_screen_rect.x, m_screen_rect.y, m_screen_rect.width, m_screen_rect.height, corner_r);
        Color stroke_col = m_hovered ? config->colors.primary : config->colors.outline_variant;
        cairo_set_source_rgba(cr, stroke_col.r, stroke_col.g, stroke_col.b, m_hovered ? 0.9f : 0.6f);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override {
        bool inside = m_screen_rect.contains(lx, ly);
        if (inside != m_hovered) {
            m_hovered = inside;
            request_redraw();
        }
        return inside;
    }

    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override {
        if (button == MouseButton::Left) {
            if (m_screen_rect.contains(lx, ly)) {
                if (!pressed && m_on_click) {
                    m_on_click();
                }
                return true;
            }
        }
        return false;
    }

private:
    std::shared_ptr<ImageView> m_image_view;
    double m_aspect_ratio = 16.0 / 9.0;
    Rect m_screen_rect;
    bool m_hovered = false;
    std::function<void()> m_on_click;
};

// =============================================================================
// WallpaperDetailView Implementation
// =============================================================================
WallpaperDetailView::WallpaperDetailView() {
    m_pending_wallpaper_path = s_cached_wallpaper_path;
    m_pending_fit_mode = s_cached_fit_mode;
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    query_display_aspect();
    setup_ui();
    init_from_miqubg();
}

WallpaperDetailView::~WallpaperDetailView() {
    *m_alive = false;
}

std::string WallpaperDetailView::get_cached_wallpaper_path() {
    return s_cached_wallpaper_path;
}

FitMode WallpaperDetailView::get_cached_fit_mode() {
    return s_cached_fit_mode;
}

void WallpaperDetailView::init_from_miqubg() {
    auto alive = m_alive;
    std::thread([this, alive]() {
        const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
        std::string runtime_dir = xdg_runtime ? xdg_runtime : ("/run/user/" + std::to_string(getuid()));
        const char* wayland_display = getenv("WAYLAND_DISPLAY");
        std::string display_name = wayland_display ? wayland_display : "wayland-0";
        std::string sock_path = runtime_dir + "/miqubg-" + display_name + ".sock";

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return;

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            close(fd);
            return;
        }

        std::string req = "GET\n";
        write(fd, req.c_str(), req.length());

        char buf[1024];
        std::memset(buf, 0, sizeof(buf));
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (n <= 0) return;
        std::string resp(buf, n);
        while (!resp.empty() && (resp.back() == '\n' || resp.back() == '\r')) {
            resp.pop_back();
        }

        if (resp.rfind("OK ", 0) != 0) return;

        std::istringstream iss(resp.substr(3));
        std::string mode_str, color_hex, image_path;
        if (iss >> mode_str >> color_hex) {
            std::getline(iss, image_path);
            size_t first = image_path.find_first_not_of(" \t");
            if (first != std::string::npos) {
                image_path = image_path.substr(first);
            } else {
                image_path = "";
            }

            FitMode fit = miqubg_string_to_fit_mode(mode_str);

            if (AppEngine::instance()) {
                AppEngine::instance()->post([this, alive, image_path, fit]() {
                    if (!*alive) return;
                    s_cached_wallpaper_path = image_path;
                    s_cached_fit_mode = fit;
                    m_pending_wallpaper_path = image_path;
                    m_pending_fit_mode = fit;

                    if (m_canvas) {
                        m_canvas->get_image_view()->set_fit_mode(fit);
                        if (!image_path.empty()) {
                            m_canvas->get_image_view()->set_image_resource(image_path);
                        }
                        m_canvas->request_redraw();
                    }
                    if (m_fit_spinner) {
                        int idx = 0;
                        switch (fit) {
                            case FitMode::Cover:   idx = 0; break;
                            case FitMode::Contain: idx = 1; break;
                            case FitMode::Fill:    idx = 2; break;
                            case FitMode::Center:  idx = 3; break;
                            case FitMode::Tile:    idx = 4; break;
                        }
                        m_fit_spinner->set_selected_index(idx);
                    }
                    update_status_text();
                    update_apply_button_state();
                    if (get_window()) {
                        get_window()->schedule_redraw();
                    }
                });
            }
        }
    }).detach();
}

std::string WallpaperDetailView::fit_mode_to_miqubg_string(FitMode mode) {
    switch (mode) {
        case FitMode::Cover:   return "cover";
        case FitMode::Contain: return "contain";
        case FitMode::Fill:    return "stretch";
        case FitMode::Center:  return "center";
        case FitMode::Tile:    return "tile";
    }
    return "cover";
}

FitMode WallpaperDetailView::miqubg_string_to_fit_mode(const std::string& str) {
    std::string lower = StringUtils::to_lower(str);
    if (lower == "contain" || lower == "fit") return FitMode::Contain;
    if (lower == "stretch" || lower == "fill") return FitMode::Fill;
    if (lower == "center") return FitMode::Center;
    if (lower == "tile") return FitMode::Tile;
    return FitMode::Cover;
}

void WallpaperDetailView::apply_to_miqubg(const std::string& path, FitMode mode) {
    if (path.empty()) return;

    std::string mode_str = fit_mode_to_miqubg_string(mode);
    std::thread([path, mode_str]() {
        const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
        std::string runtime_dir = xdg_runtime ? xdg_runtime : ("/run/user/" + std::to_string(getuid()));
        const char* wayland_display = getenv("WAYLAND_DISPLAY");
        std::string display_name = wayland_display ? wayland_display : "wayland-0";
        std::string sock_path = runtime_dir + "/miqubg-" + display_name + ".sock";

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return;

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            std::string cmd = "SET * " + mode_str + " #000000 " + path + "\n";
            write(fd, cmd.c_str(), cmd.length());
        }
        close(fd);
    }).detach();
}

bool WallpaperDetailView::has_unapplied_changes() const {
    if (m_pending_wallpaper_path.empty() && s_cached_wallpaper_path.empty()) return false;
    return (m_pending_wallpaper_path != s_cached_wallpaper_path) ||
           (m_pending_fit_mode != s_cached_fit_mode);
}

void WallpaperDetailView::update_status_text() {
    if (!m_status_label) return;
    std::string desc = "Display: " + m_resolution_str;
    if (!m_pending_wallpaper_path.empty()) {
        fs::path p(m_pending_wallpaper_path);
        desc += " • " + p.filename().string();
    }
    if (has_unapplied_changes()) {
        desc += " (Pending apply)";
    }
    m_status_label->set_text(desc);
    m_status_label->request_redraw();
}

void WallpaperDetailView::update_apply_button_state() {
    if (!m_apply_button) return;
    if (has_unapplied_changes()) {
        m_apply_button->set_text("Apply");
        m_apply_button->set_style(ButtonStyle::Primary);
    } else {
        m_apply_button->set_text("Applied");
        m_apply_button->set_style(ButtonStyle::Standard);
    }
    m_apply_button->request_redraw();
    if (get_window()) {
        get_window()->schedule_redraw();
    }
}

void WallpaperDetailView::apply_changes() {
    if (m_pending_wallpaper_path.empty()) return;
    apply_to_miqubg(m_pending_wallpaper_path, m_pending_fit_mode);
    s_cached_wallpaper_path = m_pending_wallpaper_path;
    s_cached_fit_mode = m_pending_fit_mode;
    update_apply_button_state();
    update_status_text();
}



void WallpaperDetailView::query_display_aspect() {
    auto monitors = MonitorBackend::get_monitors();
    bool found = false;
    for (const auto& mon : monitors) {
        if (mon.is_primary && mon.current_width > 0 && mon.current_height > 0) {
            m_disp_width = mon.current_width;
            m_disp_height = mon.current_height;
            found = true;
            break;
        }
    }
    if (!found && !monitors.empty()) {
        for (const auto& mon : monitors) {
            if (mon.current_width > 0 && mon.current_height > 0) {
                m_disp_width = mon.current_width;
                m_disp_height = mon.current_height;
                found = true;
                break;
            }
        }
    }

    if (m_disp_width <= 0 || m_disp_height <= 0) {
        m_disp_width = 1920;
        m_disp_height = 1080;
    }

    m_aspect_ratio = static_cast<double>(m_disp_width) / static_cast<double>(m_disp_height);

    int g = std::gcd(m_disp_width, m_disp_height);
    int rw = (g > 0) ? (m_disp_width / g) : 16;
    int rh = (g > 0) ? (m_disp_height / g) : 9;
    if (rw == 8 && rh == 5) {
        rw = 16;
        rh = 10;
    }
    m_resolution_str = std::to_string(m_disp_width) + " × " + std::to_string(m_disp_height) +
                       " (" + std::to_string(rw) + ":" + std::to_string(rh) + ")";
}

std::shared_ptr<View> WallpaperDetailView::create_control_header() {
    auto config = Config::get();

    auto header_card = std::make_shared<CardView>();
    header_card->set_card_background_color(config->colors.surface);
    header_card->set_radius(config->metrics.corner_radius);
    header_card->set_padding(14, 12);
    LayoutParams hdr_lp;
    hdr_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    hdr_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    header_card->set_layout_params(hdr_lp);

    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header_row->set_divider_spacing(12);

    // Icon on left
    auto icon = std::make_shared<ImageView>("preferences-desktop-wallpaper");
    icon->set_target_size(36);
    LayoutParams icon_lp(36, 36);
    icon_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(icon, icon_lp);

    // Title & subtitle / resolution
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_col_lp(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    text_col_lp.gravity = Gravity::CenterVertical;
    text_col->set_layout_params(text_col_lp);
    text_col->set_divider_spacing(2);

    auto title = TextViewBuilder::create()
        ->text("Wallpaper")
        ->bold(true)
        ->textSize(16)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string desc = "Display: " + m_resolution_str;
    if (!m_pending_wallpaper_path.empty()) {
        fs::path p(m_pending_wallpaper_path);
        desc += " • " + p.filename().string();
    }
    if (has_unapplied_changes()) {
        desc += " (Pending apply)";
    }
    m_status_label = TextViewBuilder::create()
        ->text(desc)
        ->textSize(12)
        ->textColor(config->colors.outline)
        ->build();
    text_col->add_view(m_status_label);

    header_row->add_view(text_col);

    // Fit Mode Spinner & Label
    auto fit_lbl = TextViewBuilder::create()
        ->text("Fit:")
        ->textSize(13)
        ->textColor(config->colors.outline)
        ->build();
    LayoutParams fit_lbl_lp;
    fit_lbl_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    fit_lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    fit_lbl_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(fit_lbl, fit_lbl_lp);

    int fit_idx = 0;
    switch (m_pending_fit_mode) {
        case FitMode::Cover:   fit_idx = 0; break;
        case FitMode::Contain: fit_idx = 1; break;
        case FitMode::Fill:    fit_idx = 2; break;
        case FitMode::Center:  fit_idx = 3; break;
        case FitMode::Tile:    fit_idx = 4; break;
    }

    m_fit_spinner = SpinnerBuilder::create()
        ->items({"Cover", "Contain", "Fill", "Center", "Tile"})
        ->selectedIndex(fit_idx)
        ->textSize(12)
        ->padding(12, 6)
        ->cornerRadius(8)
        ->onItemSelected([this](int idx, const std::string& item) {
            (void)idx;
            if (item == "Cover") set_fit_mode(FitMode::Cover);
            else if (item == "Contain") set_fit_mode(FitMode::Contain);
            else if (item == "Fill") set_fit_mode(FitMode::Fill);
            else if (item == "Center") set_fit_mode(FitMode::Center);
            else if (item == "Tile") set_fit_mode(FitMode::Tile);
        })
        ->build();
    LayoutParams sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    sp_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(m_fit_spinner, sp_lp);

    // Browse Icon Button
    m_browse_button = std::make_shared<ImageButton>("document-open");
    m_browse_button->set_image_resource("document-open");
    m_browse_button->set_icon_size(20);
    m_browse_button->set_circle(false);
    m_browse_button->set_corner_radius(8);
    m_browse_button->set_padding(8);
    m_browse_button->set_background_color(config->colors.surface_variant);
    m_browse_button->set_tooltip("Browse Wallpaper Image (XDG File Picker)");
    m_browse_button->set_on_click_listener([this]() {
        open_file_picker();
    });
    LayoutParams btn_lp(38, 38);
    btn_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(m_browse_button, btn_lp);

    // Apply Button
    m_apply_button = ButtonBuilder::create()
        ->text(has_unapplied_changes() ? "Apply" : "Applied")
        ->style(has_unapplied_changes() ? ButtonStyle::Primary : ButtonStyle::Standard)
        ->textSize(12)
        ->bold(true)
        ->cornerRadius(8)
        ->padding(14, 6)
        ->onClick([this]() {
            apply_changes();
        })
        ->build();
    LayoutParams apply_lp(84, 36);
    apply_lp.gravity = Gravity::CenterVertical;
    header_row->add_view(m_apply_button, apply_lp);

    header_card->add_view(header_row);
    return header_card;
}

void WallpaperDetailView::setup_ui() {
    clear_views();

    m_root_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_root_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    m_root_layout->set_divider_spacing(16);
    m_root_layout->set_padding(20);

    // 1. Control Header
    m_root_layout->add_view(create_control_header());

    // 2. Display Canvas (aspect-ratio respecting)
    m_canvas = std::make_shared<WallpaperDisplayCanvas>();
    m_canvas->set_aspect_ratio(m_aspect_ratio);
    m_canvas->get_image_view()->set_fit_mode(m_pending_fit_mode);
    if (!m_pending_wallpaper_path.empty()) {
        m_canvas->get_image_view()->set_image_resource(m_pending_wallpaper_path);
    }
    m_canvas->set_on_click_listener([this]() {
        open_file_picker();
    });

    LayoutParams canvas_lp;
    canvas_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    canvas_lp.height = 0;
    canvas_lp.weight = 1.0f;
    m_canvas->set_layout_params(canvas_lp);
    m_root_layout->add_view(m_canvas);

    add_view(m_root_layout);
}

void WallpaperDetailView::open_file_picker() {
    if (m_picker_running) return;
    m_picker_running = true;

    auto alive = m_alive;
    std::thread([this, alive]() {
        // Execute XDG file picker (using portal or zenity)
        std::string cmd = "GTK_USE_PORTAL=1 zenity --file-selection --title=\"Select Wallpaper\" "
                          "--file-filter=\"Images | *.png *.jpg *.jpeg *.webp *.bmp *.svg *.avif *.jxl\" "
                          "2>/dev/null";

        std::string selected_path;
        FILE* fp = popen(cmd.c_str(), "r");
        if (fp) {
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), fp)) {
                selected_path += buffer;
            }
            pclose(fp);
        }

        selected_path = StringUtils::trim(selected_path, " \t\r\n\"");

        if (AppEngine::instance()) {
            AppEngine::instance()->post([this, alive, selected_path]() {
                if (!*alive) return;
                m_picker_running = false;
                if (!selected_path.empty() && fs::exists(selected_path)) {
                    set_wallpaper_image(selected_path);
                }
            });
        }
    }).detach();
}

void WallpaperDetailView::set_wallpaper_image(const std::string& path) {
    m_pending_wallpaper_path = path;
    if (m_canvas) {
        m_canvas->get_image_view()->set_image_resource(path);
        m_canvas->request_redraw();
    }
    update_status_text();
    update_apply_button_state();
    if (get_window()) {
        get_window()->schedule_redraw();
    }
}

void WallpaperDetailView::set_fit_mode(FitMode mode) {
    m_pending_fit_mode = mode;
    if (m_canvas) {
        m_canvas->get_image_view()->set_fit_mode(mode);
        m_canvas->request_redraw();
    }
    update_status_text();
    update_apply_button_state();
    if (get_window()) {
        get_window()->schedule_redraw();
    }
}

} // namespace miqu
