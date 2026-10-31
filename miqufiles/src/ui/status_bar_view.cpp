#include "ui/status_bar_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <pango/pangocairo.h>
#include <filesystem>
#include <algorithm>

namespace miqu {

namespace fs = std::filesystem;

StatusBarView::StatusBarView() {
    LayoutParams lp;
    lp.width = static_cast<int>(LayoutDimension::MatchParent);
    lp.height = 28;
    set_layout_params(lp);
}

void StatusBarView::update_stats(const std::vector<FileItem>& current_items, const FileItem* selected_item, const fs::path& current_path) {
    size_t dirs = 0;
    size_t files = 0;
    uintmax_t total_size = 0;

    for (const auto& item : current_items) {
        if (item.is_dir) {
            dirs++;
        } else {
            files++;
            total_size += item.size_bytes;
        }
    }

    m_directory_summary = std::to_string(current_items.size()) + " items";
    if (dirs > 0 || files > 0) {
        m_directory_summary += " (" + std::to_string(dirs) + " folders, " + std::to_string(files) + " files";
        if (total_size > 0) {
            m_directory_summary += ", " + FileItem::format_file_size(total_size);
        }
        m_directory_summary += ")";
    }

    if (selected_item) {
        m_selection_summary = selected_item->name + " • " + selected_item->size_formatted + " • " + selected_item->permissions_str;
    } else {
        m_selection_summary = "No item selected";
    }

    std::error_code ec;
    auto space_info = fs::space(current_path, ec);
    if (!ec && space_info.capacity > 0) {
        m_disk_summary = "Free: " + FileItem::format_file_size(space_info.available) + " / " + FileItem::format_file_size(space_info.capacity);
    } else {
        m_disk_summary = "";
    }

    request_redraw();
}

Size StatusBarView::measure_size() const {
    return {0, 28};
}

Size StatusBarView::measure_size(int avail_width) const {
    return {avail_width >= 0 ? avail_width : 0, 28};
}

void StatusBarView::draw(cairo_t* cr, const Rect& bounds) {
    if (!cr || bounds.width <= 0 || bounds.height <= 0) return;

    auto config = Config::get();

    // Top subtle divider line
    cairo_set_source_rgba(cr, config->colors.outline_variant.r,
                              config->colors.outline_variant.g,
                              config->colors.outline_variant.b,
                              0.35f);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, bounds.x, bounds.y + 0.5);
    cairo_line_to(cr, bounds.x + bounds.width, bounds.y + 0.5);
    cairo_stroke(cr);

    std::string font_family = !config->metrics.font_family.empty() ? config->metrics.font_family : "Sans";
    int font_size = std::max(8, config->metrics.font_size - 2);
    std::string font_desc_str = font_family + " " + std::to_string(font_size);

    int pad_x = 12;
    int cur_y = bounds.y + (bounds.height - font_size - 4) / 2;

    // 1. Draw Left: Directory Summary
    if (!m_directory_summary.empty()) {
        PangoLayout* layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, m_directory_summary.c_str(), -1);
        PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        cairo_move_to(cr, bounds.x + pad_x, cur_y);
        cairo_set_source_rgba(cr, config->colors.outline.r,
                                  config->colors.outline.g,
                                  config->colors.outline.b,
                                  config->colors.outline.a);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }

    // 2. Draw Right: Disk Free Summary
    if (!m_disk_summary.empty()) {
        PangoLayout* layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, m_disk_summary.c_str(), -1);
        PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        int text_w = 0, text_h = 0;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);

        cairo_move_to(cr, bounds.x + bounds.width - pad_x - text_w, cur_y);
        cairo_set_source_rgba(cr, config->colors.outline.r,
                                  config->colors.outline.g,
                                  config->colors.outline.b,
                                  config->colors.outline.a);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
    }

    // 3. Draw Center: Selected Item Summary
    if (!m_selection_summary.empty()) {
        PangoLayout* layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, m_selection_summary.c_str(), -1);
        PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        int text_w = 0, text_h = 0;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);

        int center_x = bounds.x + (bounds.width - text_w) / 2;
        // Only draw if there's sufficient horizontal clearance
        if (center_x > bounds.x + 220 && (center_x + text_w) < (bounds.x + bounds.width - 180)) {
            cairo_move_to(cr, center_x, cur_y);
            cairo_set_source_rgba(cr, config->colors.primary.r,
                                      config->colors.primary.g,
                                      config->colors.primary.b,
                                      0.85f);
            pango_cairo_show_layout(cr, layout);
        }
        g_object_unref(layout);
    }
}

} // namespace miqu
