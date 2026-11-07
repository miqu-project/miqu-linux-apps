#include "modules/firewall/firewall_detail_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <miqutoolkit/view/edit_text.hpp>
#include <pango/pangocairo.h>
#include <cmath>
#include <algorithm>

namespace miqu {

static const Color COLOR_ACTIVE_GREEN(0.188f, 0.820f, 0.345f, 1.0f); // #30d158
static const Color COLOR_INACTIVE_GRAY(0.545f, 0.545f, 0.600f, 1.0f); // #8b8b99
static const Color COLOR_DENY_AMBER(0.950f, 0.650f, 0.200f, 1.0f);   // Warning amber

// =============================================================================
// Sparkline Area Chart View (Live Telemetry Rendering)
// =============================================================================
class SparklineGraphView : public View {
public:
    explicit SparklineGraphView(std::vector<float> data)
        : m_data(std::move(data)) {
        set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            110
        ));
    }

    void set_data(std::vector<float> data) {
        m_data = std::move(data);
        request_redraw();
    }

    void draw(cairo_t* cr, const Rect& bounds) override {
        if (!is_visible() || bounds.width <= 0 || bounds.height <= 0) return;

        auto config = Config::get();
        cairo_save(cr);

        // 1. Draw plot container background
        cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
        cairo_set_source_rgba(cr, config->colors.surface_variant.r, config->colors.surface_variant.g, config->colors.surface_variant.b, 0.35);
        cairo_fill(cr);

        int pad_l = 36;
        int pad_r = 14;
        int pad_t = 12;
        int pad_b = 18;

        int plot_x = bounds.x + pad_l;
        int plot_y = bounds.y + pad_t;
        int plot_w = std::max(10, bounds.width - pad_l - pad_r);
        int plot_h = std::max(10, bounds.height - pad_t - pad_b);

        // Find max value in dataset (at least 10 for clean scale)
        float max_val = 10.0f;
        for (float v : m_data) {
            if (v > max_val) max_val = v;
        }

        // 2. Draw Y axis labels & horizontal grid lines
        struct YLevel { int val; float y_norm; };
        std::vector<YLevel> levels = {
            {static_cast<int>(max_val), 0.0f},
            {static_cast<int>(max_val / 2.0f), 0.5f},
            {0, 1.0f}
        };

        for (const auto& lvl : levels) {
            float cur_y = plot_y + lvl.y_norm * plot_h;

            // Grid line
            cairo_set_source_rgba(cr, config->colors.outline_variant.r, config->colors.outline_variant.g, config->colors.outline_variant.b, 0.25);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, plot_x, cur_y);
            cairo_line_to(cr, plot_x + plot_w, cur_y);
            cairo_stroke(cr);

            // Label
            PangoLayout* layout = pango_cairo_create_layout(cr);
            std::string text = std::to_string(lvl.val);
            pango_layout_set_text(layout, text.c_str(), -1);

            PangoFontDescription* font_desc = pango_font_description_from_string("Sans 8");
            pango_layout_set_font_description(layout, font_desc);
            pango_font_description_free(font_desc);

            cairo_set_source_rgba(cr, config->colors.on_surface_variant.r, config->colors.on_surface_variant.g, config->colors.on_surface_variant.b, 0.8);
            cairo_move_to(cr, bounds.x + 6, cur_y - 7);
            pango_cairo_show_layout(cr, layout);
            g_object_unref(layout);
        }

        // 3. Draw curve and fill area
        if (m_data.size() >= 2) {
            size_t n = m_data.size();
            std::vector<Point> pts;
            pts.reserve(n);

            for (size_t i = 0; i < n; ++i) {
                float norm_x = static_cast<float>(i) / static_cast<float>(n - 1);
                float val = std::clamp(m_data[i], 0.0f, max_val);
                float norm_y = 1.0f - (val / max_val);

                int px = plot_x + static_cast<int>(norm_x * plot_w + 0.5f);
                int py = plot_y + static_cast<int>(norm_y * plot_h + 0.5f);
                pts.push_back(Point(px, py));
            }

            // Closed area path for gradient fill
            cairo_move_to(cr, pts[0].x, pts[0].y);
            for (size_t i = 0; i < pts.size() - 1; ++i) {
                Point p0 = (i > 0) ? pts[i - 1] : pts[i];
                Point p1 = pts[i];
                Point p2 = pts[i + 1];
                Point p3 = (i + 2 < pts.size()) ? pts[i + 2] : p2;

                double cp1x = p1.x + (p2.x - p0.x) / 6.0;
                double cp1y = p1.y + (p2.y - p0.y) / 6.0;
                double cp2x = p2.x - (p3.x - p1.x) / 6.0;
                double cp2y = p2.y - (p3.y - p1.y) / 6.0;

                cairo_curve_to(cr, cp1x, cp1y, cp2x, cp2y, p2.x, p2.y);
            }
            cairo_line_to(cr, pts.back().x, plot_y + plot_h);
            cairo_line_to(cr, pts.front().x, plot_y + plot_h);
            cairo_close_path(cr);

            // Translucent gradient fill under curve
            cairo_pattern_t* pat = cairo_pattern_create_linear(0, plot_y, 0, plot_y + plot_h);
            cairo_pattern_add_color_stop_rgba(pat, 0.0, COLOR_ACTIVE_GREEN.r, COLOR_ACTIVE_GREEN.g, COLOR_ACTIVE_GREEN.b, 0.35);
            cairo_pattern_add_color_stop_rgba(pat, 1.0, COLOR_ACTIVE_GREEN.r, COLOR_ACTIVE_GREEN.g, COLOR_ACTIVE_GREEN.b, 0.02);
            cairo_set_source(cr, pat);
            cairo_fill(cr);
            cairo_pattern_destroy(pat);

            // Stroke the curve
            cairo_move_to(cr, pts[0].x, pts[0].y);
            for (size_t i = 0; i < pts.size() - 1; ++i) {
                Point p0 = (i > 0) ? pts[i - 1] : pts[i];
                Point p1 = pts[i];
                Point p2 = pts[i + 1];
                Point p3 = (i + 2 < pts.size()) ? pts[i + 2] : p2;

                double cp1x = p1.x + (p2.x - p0.x) / 6.0;
                double cp1y = p1.y + (p2.y - p0.y) / 6.0;
                double cp2x = p2.x - (p3.x - p1.x) / 6.0;
                double cp2y = p2.y - (p3.y - p1.y) / 6.0;

                cairo_curve_to(cr, cp1x, cp1y, cp2x, cp2y, p2.x, p2.y);
            }
            cairo_set_source_rgba(cr, COLOR_ACTIVE_GREEN.r, COLOR_ACTIVE_GREEN.g, COLOR_ACTIVE_GREEN.b, 0.95);
            cairo_set_line_width(cr, 2.0);
            cairo_stroke(cr);
        }

        cairo_restore(cr);
    }

private:
    std::vector<float> m_data;
};

// =============================================================================
// FirewallDetailView Implementation
// =============================================================================

FirewallDetailView::FirewallDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void FirewallDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void FirewallDetailView::setup_ui() {
    clear_views();
    auto config = Config::get();

    m_scroll = std::make_shared<ScrollView>();
    m_scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_content_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_content_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_content_layout->set_divider_spacing(16);
    m_content_layout->set_padding(m_is_fullscreen ? 28 : 14);

    // Status banner if Polkit is authenticating or if an action failed
    if (!m_status_message.empty()) {
        m_content_layout->add_view(create_status_banner());
    }

    if (!m_is_fullscreen) {
        // Compact 3rd column preview
        m_content_layout->add_view(create_hero_preview_card());
        m_content_layout->add_view(create_master_switch_card());
        m_content_layout->add_view(create_traffic_policy_card());
        m_content_layout->add_view(create_profile_presets_section());

        auto btn_open_full = ButtonBuilder::create()
            ->text("Open Full Firewall Inspection View ⛶")
            ->textSize(14)
            ->textColor(config->colors.on_primary)
            ->backgroundColor(config->colors.primary)
            ->cornerRadius(config->metrics.corner_radius)
            ->onClick([this]() {
                if (m_on_request_fullscreen) m_on_request_fullscreen();
            })
            ->build();
        btn_open_full->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            44
        ));
        m_content_layout->add_view(btn_open_full);
    } else {
        // Full width inspection view: stacked vertically
        m_content_layout->add_view(create_master_switch_card());
        m_content_layout->add_view(create_traffic_policy_card());
        m_content_layout->add_view(create_profile_presets_section());
        m_content_layout->add_view(create_advanced_rules_card());
        m_content_layout->add_view(create_logs_monitor_card());
    }

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

// =============================================================================
// Status / Polkit Notification Banner
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_status_banner() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(m_is_busy ? config->colors.surface_variant : Color(0.95f, 0.45f, 0.2f, 0.15f));
    card->set_stroke(1, m_is_busy ? config->colors.primary : COLOR_DENY_AMBER);
    card->set_radius(8);
    card->set_padding(12, 10);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row->set_divider_spacing(10);

    auto lbl = TextViewBuilder::create()
        ->text(m_status_message)
        ->textSize(12)
        ->bold(true)
        ->textColor(m_is_busy ? config->colors.primary : config->colors.on_surface)
        ->build();
    LayoutParams lbl_lp;
    lbl_lp.width = 0;
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl_lp.weight = 1.0f;
    lbl_lp.gravity = Gravity::CenterVertical;
    row->add_view(lbl, lbl_lp);

    if (!m_is_busy) {
        auto btn_dismiss = ButtonBuilder::create()
            ->text("Dismiss")
            ->textSize(11)
            ->textColor(config->colors.on_surface)
            ->backgroundColor(config->colors.surface)
            ->cornerRadius(4)
            ->onClick([this]() {
                m_status_message.clear();
                setup_ui();
            })
            ->build();
        LayoutParams btn_lp;
        btn_lp.width = 68;
        btn_lp.height = 26;
        btn_lp.gravity = Gravity::CenterVertical;
        row->add_view(btn_dismiss, btn_lp);
    }

    card->add_view(row);
    return card;
}

// =============================================================================
// Preview Mode Hero Card
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_hero_preview_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(16);

    auto icon = std::make_shared<ImageView>("security-high");
    icon->set_target_size(48);
    LayoutParams icon_lp;
    icon_lp.width = 48;
    icon_lp.height = 48;
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);
    text_col->set_divider_spacing(3);

    auto title = TextViewBuilder::create()
        ->text("Firewall")
        ->bold(true)
        ->textSize(18)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto& state = FirewallBackend::get_state();
    auto sub = TextViewBuilder::create()
        ->text(state.active ? "Protection Active · Powered by UFW" : "Protection Disabled")
        ->textSize(12)
        ->textColor(state.active ? COLOR_ACTIVE_GREEN : config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub);
    row->add_view(text_col);

    auto btn_expand = ButtonBuilder::create()
        ->text("⛶")
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(6)
        ->onClick([this]() {
            if (m_on_request_fullscreen) m_on_request_fullscreen();
        })
        ->build();
    LayoutParams btn_lp;
    btn_lp.width = 36;
    btn_lp.height = 36;
    btn_lp.gravity = Gravity::CenterVertical;
    btn_expand->set_layout_params(btn_lp);
    row->add_view(btn_expand);

    card->add_view(row);
    return card;
}

// =============================================================================
// Master Switch Card
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_master_switch_card() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18, 14);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    // Left: "Master Switch"
    auto title = TextViewBuilder::create()
        ->text("Master Switch")
        ->bold(true)
        ->textSize(16)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams title_lp;
    title_lp.width = 160;
    title_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    title_lp.gravity = Gravity::CenterVertical;
    row->add_view(title, title_lp);

    // Center Indicator Dot + Label: "🟢 Firewall (Powered by UFW)"
    auto center_col = std::make_shared<LinearLayout>(Orientation::Horizontal);
    LayoutParams center_lp;
    center_lp.width = 0;
    center_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    center_lp.weight = 1.0f;
    center_lp.gravity = Gravity::CenterVertical;
    center_col->set_layout_params(center_lp);
    center_col->set_divider_spacing(8);

    auto dot = std::make_shared<FrameLayout>();
    dot->set_layout_params(LayoutParams(10, 10, Gravity::CenterVertical));
    dot->set_corner_radius(5);
    dot->set_background_color(state.active ? COLOR_ACTIVE_GREEN : COLOR_INACTIVE_GRAY);
    center_col->add_view(dot);

    auto status_lbl = TextViewBuilder::create()
        ->text(state.status_text)
        ->bold(true)
        ->textSize(14)
        ->textColor(state.active ? config->colors.on_surface : config->colors.on_surface_variant)
        ->build();
    center_col->add_view(status_lbl);
    row->add_view(center_col);

    // Right: Switch (ON / OFF) via Polkit
    auto sw = SwitchBuilder::create()
        ->checked(state.active)
        ->onCheckedChanged([this](bool checked) {
            if (m_is_busy) return;
            m_is_busy = true;
            m_status_message = "Polkit: Authenticating to change UFW service state...";
            setup_ui();

            FirewallBackend::set_active(checked, [this](bool ok, const std::string& msg) {
                m_is_busy = false;
                if (!ok) {
                    m_status_message = "Polkit authorization cancelled or failed.";
                } else {
                    m_status_message = "";
                }
                setup_ui();
            });
        })
        ->build();
    LayoutParams sw_lp;
    sw_lp.width = 54;
    sw_lp.height = 30;
    sw_lp.gravity = Gravity::CenterVertical;
    row->add_view(sw, sw_lp);

    card->add_view(row);
    return card;
}

// =============================================================================
// Traffic Policy (Baseline) Card
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_traffic_policy_card() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto container = std::make_shared<LinearLayout>(Orientation::Vertical);
    container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    container->set_divider_spacing(8);

    auto header = TextViewBuilder::create()
        ->text("Traffic Policy (Baseline)")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    container->add_view(header);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18, 14);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto card_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    card_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    card_col->set_divider_spacing(12);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(20);

    // Incoming Traffic Control using toolkit Spinner
    auto in_box = std::make_shared<LinearLayout>(Orientation::Horizontal);
    LayoutParams in_lp;
    in_lp.width = 0;
    in_lp.height = 36;
    in_lp.weight = 1.0f;
    in_box->set_layout_params(in_lp);
    in_box->set_divider_spacing(10);

    auto in_title = TextViewBuilder::create()
        ->text("Incoming Traffic")
        ->textSize(13)
        ->textColor(config->colors.on_surface)
        ->build();
    in_title->set_layout_params(LayoutParams(110, static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterVertical));
    in_box->add_view(in_title);

    int in_selected_idx = 0;
    if (state.incoming_policy.find("Reject") != std::string::npos) in_selected_idx = 1;
    else if (state.incoming_policy.find("Allow") != std::string::npos) in_selected_idx = 2;

    auto in_spinner = SpinnerBuilder::create()
        ->items({"Deny (Default)", "Reject", "Allow"})
        ->selectedIndex(in_selected_idx)
        ->textSize(12)
        ->cornerRadius(6)
        ->onItemSelected([this, in_selected_idx](int idx, const std::string& item) {
            if (m_is_busy || idx == in_selected_idx) return;

            std::string next_pol = item;
            m_is_busy = true;
            m_status_message = "Polkit: Updating default incoming traffic policy to " + next_pol + "...";
            setup_ui();

            FirewallBackend::set_incoming_policy(next_pol, [this](bool ok, const std::string& msg) {
                m_is_busy = false;
                if (!ok) {
                    m_status_message = "Polkit authorization cancelled or failed.";
                } else {
                    m_status_message = "";
                }
                setup_ui();
            });
        })
        ->build();
    LayoutParams in_spin_lp;
    in_spin_lp.width = 0;
    in_spin_lp.height = 36;
    in_spin_lp.weight = 1.0f;
    in_spin_lp.gravity = Gravity::CenterVertical;
    in_box->add_view(in_spinner, in_spin_lp);
    row->add_view(in_box);

    // Outgoing Traffic Control using toolkit Spinner
    auto out_box = std::make_shared<LinearLayout>(Orientation::Horizontal);
    LayoutParams out_lp;
    out_lp.width = 0;
    out_lp.height = 36;
    out_lp.weight = 1.0f;
    out_box->set_layout_params(out_lp);
    out_box->set_divider_spacing(10);

    auto out_title = TextViewBuilder::create()
        ->text("Outgoing Traffic")
        ->textSize(13)
        ->textColor(config->colors.on_surface)
        ->build();
    out_title->set_layout_params(LayoutParams(110, static_cast<int>(LayoutDimension::WrapContent), Gravity::CenterVertical));
    out_box->add_view(out_title);

    int out_selected_idx = 0;
    if (state.outgoing_policy.find("Deny") != std::string::npos) out_selected_idx = 1;

    auto out_spinner = SpinnerBuilder::create()
        ->items({"Allow (Default)", "Deny"})
        ->selectedIndex(out_selected_idx)
        ->textSize(12)
        ->cornerRadius(6)
        ->onItemSelected([this, out_selected_idx](int idx, const std::string& item) {
            if (m_is_busy || idx == out_selected_idx) return;

            std::string next_pol = item;
            m_is_busy = true;
            m_status_message = "Polkit: Updating default outgoing traffic policy to " + next_pol + "...";
            setup_ui();

            FirewallBackend::set_outgoing_policy(next_pol, [this](bool ok, const std::string& msg) {
                m_is_busy = false;
                if (!ok) {
                    m_status_message = "Polkit authorization cancelled or failed.";
                } else {
                    m_status_message = "";
                }
                setup_ui();
            });
        })
        ->build();
    LayoutParams out_spin_lp;
    out_spin_lp.width = 0;
    out_spin_lp.height = 36;
    out_spin_lp.weight = 1.0f;
    out_spin_lp.gravity = Gravity::CenterVertical;
    out_box->add_view(out_spinner, out_spin_lp);
    row->add_view(out_box);

    card_col->add_view(row);

    // Note
    auto note = TextViewBuilder::create()
        ->text("Note: Baseline policy sets default behavior when no custom rules match. Deny drops packets silently; Reject replies with an error.")
        ->textSize(11)
        ->textColor(config->colors.on_surface_variant)
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    card_col->add_view(note);

    card->add_view(card_col);
    container->add_view(card);
    return container;
}

// =============================================================================
// Profile Presets Section (Rich Dropdown)
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_profile_presets_section() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto container = std::make_shared<LinearLayout>(Orientation::Vertical);
    container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    container->set_divider_spacing(8);

    auto header = TextViewBuilder::create()
        ->text("Profile Presets")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    container->add_view(header);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18, 14);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto card_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    card_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    card_col->set_divider_spacing(12);

    int prof_idx = 0;
    std::string current_icon = "user-home";
    std::string current_desc = "Allows Local Subnet (e.g., Printer, NAS), SSH, KDE Connect. Denies Internet Pings.";
    switch (state.active_profile) {
        case FirewallProfile::Home:
            prof_idx = 0;
            current_icon = "user-home";
            current_desc = "Allows Local Subnet (e.g., Printer, NAS), SSH, KDE Connect. Denies Internet Pings.";
            break;
        case FirewallProfile::Work:
            prof_idx = 1;
            current_icon = "applications-office";
            current_desc = "Denies all local discovery. Allows Corp VPN (tun0) and SSH. Strict Internet filtering.";
            break;
        case FirewallProfile::Cellular:
            prof_idx = 2;
            current_icon = "network-cellular";
            current_desc = "Denies all incoming traffic. Blocks background sync & torrents on mobile hotspot.";
            break;
        case FirewallProfile::Public:
            prof_idx = 3;
            current_icon = "security-high";
            current_desc = "Maximum Lockdown. Stealth mode (drops all unsolicited packets, ignores Pings). Blocks local devices.";
            break;
        case FirewallProfile::Custom:
            prof_idx = 0;
            break;
    }

    // Top Row: Active Preset Label & Spinner Dropdown
    auto top_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    top_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    top_row->set_divider_spacing(14);

    auto icon = std::make_shared<ImageView>(current_icon);
    icon->set_target_size(24);
    LayoutParams icon_lp(24, 24, Gravity::CenterVertical);
    top_row->add_view(icon, icon_lp);

    auto label_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams lc_lp;
    lc_lp.width = 160;
    lc_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lc_lp.gravity = Gravity::CenterVertical;
    label_col->set_layout_params(lc_lp);

    auto title = TextViewBuilder::create()
        ->text("Active Preset")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    label_col->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("Applies network rule sets")
        ->textSize(11)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    label_col->add_view(sub);
    top_row->add_view(label_col);

    auto prof_spinner = SpinnerBuilder::create()
        ->items({"Home", "Work", "Cellular", "Public"})
        ->selectedIndex(prof_idx)
        ->textSize(13)
        ->cornerRadius(6)
        ->onItemSelected([this, prof_idx](int idx, const std::string& item) {
            if (m_is_busy || idx == prof_idx) return;

            FirewallProfile target_prof = FirewallProfile::Home;
            if (idx == 1) target_prof = FirewallProfile::Work;
            else if (idx == 2) target_prof = FirewallProfile::Cellular;
            else if (idx == 3) target_prof = FirewallProfile::Public;

            m_is_busy = true;
            m_status_message = "Polkit: Authenticating to activate " + item + " profile...";
            setup_ui();

            FirewallBackend::set_profile(target_prof, [this](bool ok, const std::string& msg) {
                m_is_busy = false;
                if (!ok) {
                    m_status_message = "Polkit authorization cancelled or failed.";
                } else {
                    m_status_message = "";
                }
                setup_ui();
            });
        })
        ->build();
    LayoutParams prof_lp;
    prof_lp.width = 0;
    prof_lp.height = 36;
    prof_lp.weight = 1.0f;
    prof_lp.gravity = Gravity::CenterVertical;
    top_row->add_view(prof_spinner, prof_lp);

    card_col->add_view(top_row);

    // Detail banner explaining the active profile
    auto desc_box = std::make_shared<CardView>();
    desc_box->set_card_background_color(config->colors.surface_variant);
    desc_box->set_radius(6);
    desc_box->set_padding(12, 10);
    desc_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto desc_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    desc_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    desc_col->set_divider_spacing(4);

    auto desc_head_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    desc_head_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    desc_head_row->set_divider_spacing(8);

    auto dot = std::make_shared<FrameLayout>();
    dot->set_layout_params(LayoutParams(8, 8, Gravity::CenterVertical));
    dot->set_corner_radius(4);
    dot->set_background_color(COLOR_ACTIVE_GREEN);
    desc_head_row->add_view(dot);

    std::vector<std::string> prof_names = {"Home Profile", "Work Profile", "Cellular Profile", "Public Profile"};
    std::string active_title = (prof_idx >= 0 && prof_idx < static_cast<int>(prof_names.size())) ? prof_names[prof_idx] : "Active Profile";
    auto act_label = TextViewBuilder::create()
        ->text(active_title + " Rules Active")
        ->bold(true)
        ->textSize(12)
        ->textColor(COLOR_ACTIVE_GREEN)
        ->build();
    desc_head_row->add_view(act_label);
    desc_col->add_view(desc_head_row);

    auto desc_text = TextViewBuilder::create()
        ->text(current_desc)
        ->textSize(11)
        ->textColor(config->colors.on_surface_variant)
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    desc_col->add_view(desc_text);

    desc_box->add_view(desc_col);
    card_col->add_view(desc_box);

    card->add_view(card_col);
    container->add_view(card);
    return container;
}

// =============================================================================
// Left Column: Advanced Rules & Apps
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_add_rule_inline_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(12);

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(8);

    auto title = TextViewBuilder::create()
        ->text("Add Custom UFW Rule (Requires Polkit Authentication)")
        ->bold(true)
        ->textSize(12)
        ->textColor(config->colors.primary)
        ->build();
    col->add_view(title);

    auto row_inputs = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_inputs->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row_inputs->set_divider_spacing(8);

    auto edit_port = EditTextBuilder::create()
        ->hint("Port (e.g. 8080)")
        ->padding(8, 4)
        ->build();
    LayoutParams p_lp;
    p_lp.width = 0;
    p_lp.height = 32;
    p_lp.weight = 1.0f;
    row_inputs->add_view(edit_port, p_lp);

    auto edit_source = EditTextBuilder::create()
        ->hint("Source IP / Subnet (or leave empty for Anywhere)")
        ->padding(8, 4)
        ->build();
    LayoutParams s_lp;
    s_lp.width = 0;
    s_lp.height = 32;
    s_lp.weight = 1.5f;
    row_inputs->add_view(edit_source, s_lp);

    col->add_view(row_inputs);

    auto row_actions = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_actions->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row_actions->set_divider_spacing(8);

    auto btn_save = ButtonBuilder::create()
        ->text("Save via Polkit")
        ->textSize(12)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->cornerRadius(6)
        ->onClick([this, edit_port, edit_source]() {
            if (m_is_busy) return;
            std::string port = edit_port->get_text().empty() ? "8080" : edit_port->get_text();
            std::string src = edit_source->get_text();

            FirewallRuleItem rule;
            rule.action = "Allow";
            rule.protocol = "TCP";
            rule.port = port;
            rule.source = src.empty() ? "Anywhere" : src;

            m_is_busy = true;
            m_show_add_rule = false;
            m_status_message = "Polkit: Authenticating to add rule for port " + port + "...";
            setup_ui();

            FirewallBackend::add_rule(rule, [this](bool ok, const std::string& msg) {
                m_is_busy = false;
                if (!ok) {
                    m_status_message = "Polkit authorization cancelled or rule syntax invalid.";
                } else {
                    m_status_message = "";
                }
                setup_ui();
            });
        })
        ->build();
    LayoutParams bs_lp;
    bs_lp.width = 120;
    bs_lp.height = 28;
    row_actions->add_view(btn_save, bs_lp);

    auto btn_cancel = ButtonBuilder::create()
        ->text("Cancel")
        ->textSize(12)
        ->textColor(config->colors.on_surface)
        ->backgroundColor(config->colors.surface)
        ->cornerRadius(6)
        ->onClick([this]() {
            m_show_add_rule = false;
            setup_ui();
        })
        ->build();
    LayoutParams bc_lp;
    bc_lp.width = 80;
    bc_lp.height = 28;
    row_actions->add_view(btn_cancel, bc_lp);

    col->add_view(row_actions);
    card->add_view(col);
    return card;
}

std::shared_ptr<View> FirewallDetailView::create_advanced_rules_card() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto container = std::make_shared<LinearLayout>(Orientation::Vertical);
    container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    container->set_divider_spacing(8);

    auto header = TextViewBuilder::create()
        ->text("Advanced Rules & Apps")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    container->add_view(header);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16);

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(12);

    // Quick App Toggles (strictly reflect whether rules exist in /etc/ufw/user.rules)
    auto subhead_apps = TextViewBuilder::create()
        ->text("Quick App Toggles")
        ->bold(true)
        ->textSize(13)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(subhead_apps);

    auto row_apps = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_apps->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row_apps->set_divider_spacing(12);

    auto add_quick_toggle = [&](const std::string& name, bool checked) {
        auto box = std::make_shared<LinearLayout>(Orientation::Horizontal);
        LayoutParams b_lp;
        b_lp.width = 0;
        b_lp.height = 32;
        b_lp.weight = 1.0f;
        box->set_layout_params(b_lp);
        box->set_divider_spacing(6);

        auto sw = SwitchBuilder::create()
            ->checked(checked)
            ->onCheckedChanged([this, name](bool val) {
                if (m_is_busy) return;
                m_is_busy = true;
                m_status_message = "Polkit: Updating UFW rule for " + name + "...";
                setup_ui();

                FirewallBackend::set_app_toggle(name, val, [this, name](bool ok, const std::string& msg) {
                    m_is_busy = false;
                    if (!ok) {
                        m_status_message = "Polkit authorization cancelled or failed.";
                    } else {
                        m_status_message = "";
                    }
                    setup_ui();
                });
            })
            ->build();
        LayoutParams sw_lp;
        sw_lp.width = 44;
        sw_lp.height = 24;
        sw_lp.gravity = Gravity::CenterVertical;
        box->add_view(sw, sw_lp);

        auto lbl = TextViewBuilder::create()
            ->text(name)
            ->textSize(12)
            ->textColor(config->colors.on_surface)
            ->build();
        LayoutParams lbl_lp;
        lbl_lp.width = 0;
        lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
        lbl_lp.weight = 1.0f;
        lbl_lp.gravity = Gravity::CenterVertical;
        box->add_view(lbl, lbl_lp);

        row_apps->add_view(box);
    };

    add_quick_toggle("SSH", state.ssh_enabled);
    add_quick_toggle("Plex", state.plex_enabled);
    add_quick_toggle("Samba", state.samba_enabled);
    add_quick_toggle("KDE Connect", state.kdeconnect_enabled);

    col->add_view(row_apps);

    // Divider
    auto div = std::make_shared<DividerView>();
    col->add_view(div);

    // Custom Rules Subhead + Header row
    auto row_rules_head = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_rules_head->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto subhead_rules = TextViewBuilder::create()
        ->text("Custom Rules")
        ->bold(true)
        ->textSize(13)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams shr_lp;
    shr_lp.width = 0;
    shr_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    shr_lp.weight = 1.0f;
    shr_lp.gravity = Gravity::CenterVertical;
    row_rules_head->add_view(subhead_rules, shr_lp);

    auto btn_add = ButtonBuilder::create()
        ->text("+ Add Rule")
        ->textSize(11)
        ->bold(true)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->cornerRadius(4)
        ->onClick([this]() {
            m_show_add_rule = !m_show_add_rule;
            setup_ui();
        })
        ->build();
    LayoutParams add_lp;
    add_lp.width = 84;
    add_lp.height = 26;
    row_rules_head->add_view(btn_add, add_lp);
    col->add_view(row_rules_head);

    if (m_show_add_rule) {
        col->add_view(create_add_rule_inline_card());
    }

    // Rules Table Card
    auto table_card = std::make_shared<CardView>();
    table_card->set_card_background_color(config->colors.surface_variant);
    table_card->set_radius(8);
    table_card->set_padding(10);

    auto table_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    table_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    table_col->set_divider_spacing(6);

    // If no custom rules currently configured in /etc/ufw/user.rules:
    if (state.rules.empty()) {
        auto empty_row = std::make_shared<LinearLayout>(Orientation::Vertical);
        empty_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        empty_row->set_padding(12);
        empty_row->set_divider_spacing(4);

        auto empty_title = TextViewBuilder::create()
            ->text("No Custom Rules in /etc/ufw/user.rules")
            ->bold(true)
            ->textSize(12)
            ->textColor(config->colors.on_surface)
            ->build();
        empty_row->add_view(empty_title);

        auto empty_desc = TextViewBuilder::create()
            ->text("All network traffic is currently governed strictly by the baseline policy (" + state.incoming_policy + "). Click '+ Add Rule' above to define specific port or subnet exceptions via Polkit.")
            ->textSize(11)
            ->textColor(config->colors.on_surface_variant)
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        empty_row->add_view(empty_desc);

        table_col->add_view(empty_row);
    } else {
        // Table Header
        auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        header_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            24
        ));
        header_row->set_divider_spacing(4);

        auto make_th = [&](const std::string& text, int width, float weight = 0.0f) {
            auto th = TextViewBuilder::create()
                ->text(text)
                ->bold(true)
                ->textSize(11)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams lp;
            lp.width = width;
            lp.height = static_cast<int>(LayoutDimension::WrapContent);
            lp.weight = weight;
            lp.gravity = Gravity::CenterVertical;
            header_row->add_view(th, lp);
        };

        make_th("ID", 28);
        make_th("Action", 60);
        make_th("Protocol", 65);
        make_th("Port", 55);
        make_th("Source", 0, 1.0f);
        make_th("Delete", 45);
        table_col->add_view(header_row);

        // Real Data Rows
        for (const auto& r : state.rules) {
            auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                28
            ));
            row->set_divider_spacing(4);

            auto make_td = [&](const std::string& text, int width, float weight = 0.0f, bool is_action = false) {
                auto td = TextViewBuilder::create()
                    ->text(text)
                    ->textSize(12)
                    ->textColor(is_action ? COLOR_ACTIVE_GREEN : config->colors.on_surface)
                    ->bold(is_action)
                    ->build();
                LayoutParams lp;
                lp.width = width;
                lp.height = static_cast<int>(LayoutDimension::WrapContent);
                lp.weight = weight;
                lp.gravity = Gravity::CenterVertical;
                row->add_view(td, lp);
            };

            make_td(std::to_string(r.id), 28);
            make_td(r.action, 60, 0.0f, true);
            make_td(r.protocol, 65);
            make_td(r.port, 55);
            make_td(r.source, 0, 1.0f);

            // Delete trash button (dispatches pkexec ufw --force delete <id>)
            auto btn_del = ButtonBuilder::create()
                ->text("🗑")
                ->textSize(11)
                ->textColor(COLOR_DENY_AMBER)
                ->backgroundColor(config->colors.surface)
                ->cornerRadius(4)
                ->onClick([this, rule_id = r.id]() {
                    if (m_is_busy) return;
                    m_is_busy = true;
                    m_status_message = "Polkit: Authenticating to delete UFW rule #" + std::to_string(rule_id) + "...";
                    setup_ui();

                    FirewallBackend::delete_rule(rule_id, [this](bool ok, const std::string& msg) {
                        m_is_busy = false;
                        if (!ok) {
                            m_status_message = "Polkit authorization cancelled or failed.";
                        } else {
                            m_status_message = "";
                        }
                        setup_ui();
                    });
                })
                ->build();
            LayoutParams del_lp;
            del_lp.width = 30;
            del_lp.height = 24;
            row->add_view(btn_del, del_lp);

            table_col->add_view(row);
        }
    }

    table_card->add_view(table_col);
    col->add_view(table_card);

    card->add_view(col);
    container->add_view(card);
    return container;
}

// =============================================================================
// Right Column: Logs & Monitor
// =============================================================================
std::shared_ptr<View> FirewallDetailView::create_logs_monitor_card() {
    auto config = Config::get();
    auto& state = FirewallBackend::get_state();

    auto container = std::make_shared<LinearLayout>(Orientation::Vertical);
    container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    container->set_divider_spacing(8);

    auto header = TextViewBuilder::create()
        ->text("Logs & Monitor")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    container->add_view(header);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_stroke(1, config->colors.outline_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(16);

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(10);

    // 1. Blocked Connections Header & Real Count from Journald
    auto row_blocked = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row_blocked->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto lbl_blocked = TextViewBuilder::create()
        ->text("Blocked Connections (Last Hour)")
        ->bold(true)
        ->textSize(13)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams b_lp;
    b_lp.width = 0;
    b_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    b_lp.weight = 1.0f;
    row_blocked->add_view(lbl_blocked, b_lp);

    auto count_blocked = TextViewBuilder::create()
        ->text(std::to_string(state.blocked_last_hour))
        ->bold(true)
        ->textSize(14)
        ->textColor(state.blocked_last_hour > 0 ? COLOR_ACTIVE_GREEN : config->colors.on_surface_variant)
        ->build();
    row_blocked->add_view(count_blocked);
    col->add_view(row_blocked);

    // 2. Real Telemetry Sparkline Area Chart View
    auto sparkline = std::make_shared<SparklineGraphView>(state.hourly_trend);
    col->add_view(sparkline);

    // Divider
    auto div = std::make_shared<DividerView>();
    col->add_view(div);

    // 3. Recent Log Events Subhead
    auto subhead_logs = TextViewBuilder::create()
        ->text("Recent Log Events")
        ->bold(true)
        ->textSize(13)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(subhead_logs);

    // Log list container card
    auto log_card = std::make_shared<CardView>();
    log_card->set_card_background_color(config->colors.surface_variant);
    log_card->set_radius(8);
    log_card->set_padding(10);

    auto log_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    log_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    log_col->set_divider_spacing(6);

    if (state.recent_logs.empty()) {
        auto no_logs_title = TextViewBuilder::create()
            ->text("No Packet Drops Recorded in Journald")
            ->bold(true)
            ->textSize(12)
            ->textColor(config->colors.on_surface)
            ->build();
        log_col->add_view(no_logs_title);

        std::string lvl_info = "Kernel logging is currently set to: " + state.log_level +
                               ". When unsolicited or blocked packets arrive, their source IP and port will appear here live.";
        auto no_logs_desc = TextViewBuilder::create()
            ->text(lvl_info)
            ->textSize(11)
            ->textColor(config->colors.on_surface_variant)
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        log_col->add_view(no_logs_desc);

        if (state.log_level == "low" || state.log_level == "off") {
            auto btn_more_logs = ButtonBuilder::create()
                ->text("Set Log Level to Medium (via Polkit)")
                ->textSize(11)
                ->textColor(config->colors.primary)
                ->backgroundColor(config->colors.surface)
                ->cornerRadius(4)
                ->onClick([this]() {
                    if (m_is_busy) return;
                    m_is_busy = true;
                    m_status_message = "Polkit: Setting UFW logging level to medium...";
                    setup_ui();

                    FirewallBackend::set_log_level("medium", [this](bool ok, const std::string& msg) {
                        m_is_busy = false;
                        if (!ok) {
                            m_status_message = "Polkit authorization cancelled or failed.";
                        } else {
                            m_status_message = "";
                        }
                        setup_ui();
                    });
                })
                ->build();
            LayoutParams ml_lp;
            ml_lp.width = static_cast<int>(LayoutDimension::MatchParent);
            ml_lp.height = 28;
            log_col->add_view(btn_more_logs, ml_lp);
        }
    } else {
        // Table Header
        auto log_head = std::make_shared<LinearLayout>(Orientation::Horizontal);
        log_head->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            22
        ));
        log_head->set_divider_spacing(6);

        auto th1 = TextViewBuilder::create()->text("Timestamp")->bold(true)->textSize(11)->textColor(config->colors.on_surface_variant)->build();
        LayoutParams lp1; lp1.width = 115; lp1.height = static_cast<int>(LayoutDimension::WrapContent);
        log_head->add_view(th1, lp1);

        auto th2 = TextViewBuilder::create()->text("Source IP")->bold(true)->textSize(11)->textColor(config->colors.on_surface_variant)->build();
        LayoutParams lp2; lp2.width = 0; lp2.height = static_cast<int>(LayoutDimension::WrapContent); lp2.weight = 1.0f;
        log_head->add_view(th2, lp2);

        auto th3 = TextViewBuilder::create()->text("Port")->bold(true)->textSize(11)->textColor(config->colors.on_surface_variant)->build();
        LayoutParams lp3; lp3.width = 45; lp3.height = static_cast<int>(LayoutDimension::WrapContent);
        log_head->add_view(th3, lp3);

        auto th4 = TextViewBuilder::create()->text("Action")->bold(true)->textSize(11)->textColor(config->colors.on_surface_variant)->build();
        LayoutParams lp4; lp4.width = 45; lp4.height = static_cast<int>(LayoutDimension::WrapContent);
        log_head->add_view(th4, lp4);

        log_col->add_view(log_head);

        // Real Log Entries
        for (const auto& log : state.recent_logs) {
            auto log_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            log_row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                22
            ));
            log_row->set_divider_spacing(6);

            auto ts_lbl = TextViewBuilder::create()
                ->text(log.timestamp)
                ->textSize(11)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams ts_lp;
            ts_lp.width = 115;
            ts_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            log_row->add_view(ts_lbl, ts_lp);

            auto ip_lbl = TextViewBuilder::create()
                ->text(log.source_ip)
                ->textSize(11)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams ip_lp;
            ip_lp.width = 0;
            ip_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            ip_lp.weight = 1.0f;
            log_row->add_view(ip_lbl, ip_lp);

            auto port_lbl = TextViewBuilder::create()
                ->text(log.port)
                ->textSize(11)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            LayoutParams port_lp;
            port_lp.width = 45;
            port_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            log_row->add_view(port_lbl, port_lp);

            auto act_lbl = TextViewBuilder::create()
                ->text(log.action)
                ->textSize(11)
                ->bold(true)
                ->textColor(COLOR_DENY_AMBER)
                ->build();
            LayoutParams act_lp;
            act_lp.width = 45;
            act_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            log_row->add_view(act_lbl, act_lp);

            log_col->add_view(log_row);
        }
    }

    log_card->add_view(log_col);
    col->add_view(log_card);

    card->add_view(col);
    container->add_view(card);
    return container;
}

} // namespace miqu
