#include "workspace_button.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/window.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include <pango/pangocairo.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <glob.h>
#include <cmath>
#include <iostream>

namespace miqubar {

WorkspaceButtonView::WorkspaceButtonView() {
    set_flat(true);
    set_text("");
    set_icon("");

    const auto& cfg = BarConfig::get();
    int item_h = cfg.get_item_height();
    set_layout_params(miqu::LayoutParams(item_h, item_h));

    sync_workspaces();

    set_on_click_listener([this]() {
        if (m_flyout_window) {
            hide_flyout();
            return;
        }
        if (!trigger_overview()) {
            show_flyout();
        }
    });

    auto mgr = miqu::WorkspaceManager::get();
    if (mgr) {
        mgr->on_workspaces_changed([this]() {
            sync_workspaces();
        });
    }
}

WorkspaceButtonView::~WorkspaceButtonView() {
    hide_flyout();
}

void WorkspaceButtonView::sync_workspaces() {
    auto mgr = miqu::WorkspaceManager::get();
    if (mgr) {
        m_workspaces = mgr->get_workspaces();
        const auto* act = mgr->get_active_workspace();
        if (act) {
            m_active_id = act->id;
        }
    }
    if (m_workspaces.empty()) {
        miqu::WorkspaceInfo fallback;
        fallback.id = 1;
        fallback.name = "1";
        fallback.is_active = true;
        m_workspaces.push_back(fallback);
    }
    request_redraw();
}

bool WorkspaceButtonView::on_mouse_move(int lx, int ly, const miqu::Rect& bounds) {
    bool h = bounds.contains(miqu::Point(lx, ly));
    if (h != m_is_hovered) {
        m_is_hovered = h;
        request_redraw();
    }
    return miqu::Button::on_mouse_move(lx, ly, bounds);
}

void WorkspaceButtonView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!is_visible() || !cr || bounds.width <= 0 || bounds.height <= 0) return;

    // Draw base button state (handles hover/press translucent tile)
    miqu::Button::draw(cr, bounds);

    auto config = miqu::Config::get();
    const auto& cfg = BarConfig::get();

    int cx = bounds.x + bounds.width / 2;
    int cy = bounds.y + bounds.height / 2;
    int count = static_cast<int>(m_workspaces.size());
    bool show_dots = (count > 1);

    if (cfg.workspace_display == "dots") {
        // Minimal dot strip mode
        int total_dots_w = 0;
        for (const auto& ws : m_workspaces) {
            total_dots_w += (ws.id == m_active_id ? 10 : 4) + 3;
        }
        total_dots_w -= 3;
        int cur_x = cx - total_dots_w / 2;
        int dot_y = bounds.y + (bounds.height - 3) / 2;

        for (const auto& ws : m_workspaces) {
            if (ws.id == m_active_id) {
                miqu::CardView::draw_rounded_rect(cr, cur_x, dot_y, 10, 3, 1.5);
                cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, config->colors.primary.a);
                cairo_fill(cr);
                cur_x += 10 + 3;
            } else {
                miqu::CardView::draw_rounded_rect(cr, cur_x, dot_y, 4, 3, 1.5);
                cairo_set_source_rgba(cr, config->colors.on_surface_variant.r, config->colors.on_surface_variant.g, config->colors.on_surface_variant.b, 0.50f);
                cairo_fill(cr);
                cur_x += 4 + 3;
            }
        }
        return;
    }

    if (cfg.workspace_display == "badge") {
        // Clean numeric pill badge without awkward desktop monitor icon
        int badge_w = 26;
        int badge_h = 24;
        int bx = cx - badge_w / 2;
        int by = cy - badge_h / 2;
        int radius = config->metrics.corner_radius > 0 ? std::min(badge_h / 2, config->metrics.corner_radius) : 6;

        miqu::CardView::draw_rounded_rect(cr, bx, by, badge_w, badge_h, radius);
        cairo_set_source_rgba(cr, config->colors.surface_variant.r, config->colors.surface_variant.g, config->colors.surface_variant.b, 0.45f);
        cairo_fill(cr);

        miqu::CardView::draw_rounded_rect(cr, bx + 0.5, by + 0.5, badge_w - 1.0, badge_h - 1.0, radius);
        cairo_set_source_rgba(cr, config->colors.outline.r, config->colors.outline.g, config->colors.outline.b, 0.25f);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        std::string num_str = std::to_string(m_active_id);
        PangoLayout* layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, num_str.c_str(), -1);
        std::string font_desc = (!config->metrics.font_family.empty() ? config->metrics.font_family : "Sans") + " 11 Bold";
        PangoFontDescription* desc = pango_font_description_from_string(font_desc.c_str());
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);
        int tw = 0, th = 0;
        pango_layout_get_pixel_size(layout, &tw, &th);
        cairo_move_to(cr, cx - tw / 2.0, cy - th / 2.0);
        cairo_set_source_rgba(cr, config->colors.on_surface.r, config->colors.on_surface.g, config->colors.on_surface.b, config->colors.on_surface.a);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
        return;
    }

    // Default: Windows 11 Task View icon (two overlapping desktop cards)
    if (show_dots) {
        cy -= 2;
    }

    cairo_save(cr);
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    auto fg = config->colors.on_surface;

    // Rear card (top-left)
    int rw = 15;
    int rh = 11;
    double rx = cx - 9.5;
    double ry = cy - 7.5;
    miqu::CardView::draw_rounded_rect(cr, rx, ry, rw, rh, 2.0);
    cairo_set_source_rgba(cr, fg.r, fg.g, fg.b, 0.45f);
    cairo_stroke(cr);

    // Front card (bottom-right)
    int fw = 15;
    int fh = 11;
    double fx = cx - 5.5;
    double fy = cy - 3.5;

    // Occlude rear card behind front card
    miqu::CardView::draw_rounded_rect(cr, fx, fy, fw, fh, 2.0);
    auto bg = m_is_hovered ? config->colors.surface_variant : config->colors.surface;
    cairo_set_source_rgba(cr, bg.r, bg.g, bg.b, bg.a);
    cairo_fill(cr);

    // Front card stroke
    miqu::CardView::draw_rounded_rect(cr, fx, fy, fw, fh, 2.0);
    cairo_set_source_rgba(cr, fg.r, fg.g, fg.b, 0.95f);
    cairo_stroke(cr);

    // Front card titlebar line
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, fx + 3.0, fy + 3.5);
    cairo_line_to(cr, fx + fw - 3.0, fy + 3.5);
    cairo_set_source_rgba(cr, fg.r, fg.g, fg.b, 0.40f);
    cairo_stroke(cr);

    // Workspace dots underneath if multiple workspaces
    if (show_dots) {
        int total_dots_w = 0;
        for (const auto& ws : m_workspaces) {
            total_dots_w += (ws.id == m_active_id ? 8 : 3) + 3;
        }
        total_dots_w -= 3;
        int cur_x = cx - total_dots_w / 2;
        int dot_y = bounds.y + bounds.height - 7;

        for (const auto& ws : m_workspaces) {
            if (ws.id == m_active_id) {
                miqu::CardView::draw_rounded_rect(cr, cur_x, dot_y, 8, 2.5, 1.25);
                cairo_set_source_rgba(cr, config->colors.primary.r, config->colors.primary.g, config->colors.primary.b, config->colors.primary.a);
                cairo_fill(cr);
                cur_x += 8 + 3;
            } else {
                miqu::CardView::draw_rounded_rect(cr, cur_x, dot_y, 3, 2.5, 1.25);
                cairo_set_source_rgba(cr, config->colors.on_surface_variant.r, config->colors.on_surface_variant.g, config->colors.on_surface_variant.b, 0.45f);
                cairo_fill(cr);
                cur_x += 3 + 3;
            }
        }
    }

    cairo_restore(cr);
}

void WorkspaceButtonView::show_flyout() {
    if (m_flyout_window) {
        hide_flyout();
        return;
    }
    sync_workspaces();

    auto config = miqu::Config::get();
    auto card = std::make_shared<miqu::CardView>();
    card->set_style(miqu::CardStyle::Outlined);
    card->set_radius(config->metrics.corner_radius);
    const auto& cfg = BarConfig::get();
    // Since popup elevation is handled by miquland.conf layer rule and CardView is tightly packed to edge, we don't need any elevation of CardView.
    card->set_elevation(0);

    auto row = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    row->set_divider_spacing(6);
    row->set_padding(8, 8, 8, 8);
    row->set_gravity(miqu::Gravity::Center);
    row->set_layout_params(miqu::LayoutParams(
        static_cast<int>(miqu::LayoutDimension::MatchParent),
        static_cast<int>(miqu::LayoutDimension::MatchParent)
    ));

    for (const auto& ws : m_workspaces) {
        auto btn = std::make_shared<miqu::Button>(std::to_string(ws.id));
        btn->set_text_size(11);
        btn->set_bold(true);
        btn->set_layout_params(miqu::LayoutParams(34, 34));
        if (ws.id == m_active_id) {
            btn->set_selected(true);
        } else {
            btn->set_flat(true);
        }
        btn->set_on_click_listener([this, id = ws.id]() {
            auto mgr = miqu::WorkspaceManager::get();
            if (mgr) {
                mgr->activate_workspace(id);
            }
            m_active_id = id;
            hide_flyout();
            request_redraw();
        });
        row->add_view(btn);
    }
    card->add_view(row);

    int count = static_cast<int>(m_workspaces.size());
    int flyout_w = count * 40 + 16;
    int flyout_h = 50;

    bool is_top = (cfg.position == "top");
    uint32_t anchors = (is_top ? 1 : 2) | 4;
    miqu::Margin margin;
    miqu::Rect bounds = get_bounds();
    int win_w = get_window() ? get_window()->get_width() : 1920;
    int target_x = bounds.x + (bounds.width - flyout_w) / 2;
    if (target_x < 8) target_x = 8;
    if (target_x + flyout_w > win_w - 8) target_x = win_w - flyout_w - 8;
    margin.left = target_x + cfg.margin_left;
    if (is_top) {
        margin.top = cfg.height + 8 + cfg.margin_top;
    } else {
        margin.bottom = cfg.height + 8 + cfg.margin_bottom;
    }

    m_flyout_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-workspaces")
        ->anchors(anchors)
        ->preferredSize(flyout_w, flyout_h)
        ->contentSize(flyout_w, flyout_h)
        ->contentMargin(margin)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(card)
        ->onClose([this]() {
            m_flyout_window = nullptr;
            request_redraw();
        })
        ->build();

    if (m_flyout_window) m_flyout_window->show();
}

void WorkspaceButtonView::hide_flyout() {
    if (m_flyout_window) {
        m_flyout_window->close();
        m_flyout_window = nullptr;
    }
}

#pragma pack(push, 1)
struct IpcHeader {
    char magic[4];
    uint32_t type;
    uint32_t payload_len;
};
#pragma pack(pop)

bool WorkspaceButtonView::trigger_overview() {
    const char* runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char* wayland_display = getenv("WAYLAND_DISPLAY");
    std::string sock_path;

    if (runtime_dir && wayland_display) {
        std::string cand = std::string(runtime_dir) + "/miquland-" + wayland_display + ".sock";
        if (access(cand.c_str(), F_OK) == 0) {
            sock_path = cand;
        }
    }

    if (sock_path.empty() && runtime_dir) {
        glob_t g;
        if (glob((std::string(runtime_dir) + "/miquland-*.sock").c_str(), GLOB_NOSORT, nullptr, &g) == 0) {
            if (g.gl_pathc > 0 && g.gl_pathv[0]) sock_path = g.gl_pathv[0];
            globfree(&g);
        }
    }

    if (sock_path.empty()) {
        return false;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    struct timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 150000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return false;
    }

    std::string msg = "dispatch overview_toggle\n";
    ssize_t ret = write(fd, msg.data(), msg.size());
    if (ret != static_cast<ssize_t>(msg.size())) {
        close(fd);
        return false;
    }

    IpcHeader hdr{};
    size_t total = 0;
    while (total < sizeof(hdr)) {
        ssize_t n = read(fd, reinterpret_cast<char*>(&hdr) + total, sizeof(hdr) - total);
        if (n <= 0) {
            close(fd);
            return false;
        }
        total += n;
    }

    if (memcmp(hdr.magic, "MIQU", 4) != 0 || hdr.payload_len == 0 || hdr.payload_len > 65536) {
        close(fd);
        return false;
    }

    std::string payload;
    payload.resize(hdr.payload_len);
    total = 0;
    while (total < hdr.payload_len) {
        ssize_t n = read(fd, payload.data() + total, hdr.payload_len - total);
        if (n <= 0) {
            close(fd);
            return false;
        }
        total += n;
    }
    close(fd);

    return (payload.find("\"status\":\"ok\"") != std::string::npos);
}

} // namespace miqubar
