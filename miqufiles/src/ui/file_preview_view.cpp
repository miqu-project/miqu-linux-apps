#include "ui/file_preview_view.hpp"
#include <miqutoolkit/view/list_item_view.hpp>
#include <miqutoolkit/core/config.hpp>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cmath>

namespace miqu {

namespace {

class AspectImageView : public ImageView {
public:
    explicit AspectImageView(const std::string& path) : ImageView(path) {
        set_fit_mode(FitMode::Contain);
        set_quality_mode(ImageQuality::FullOriginal);
        if (!path.empty()) {
            gdk_pixbuf_get_file_info(path.c_str(), &m_orig_w, &m_orig_h);
        }
    }

    Size measure_size() const override {
        return measure_size(-1);
    }

    Size measure_size(int avail_width) const override {
        if (avail_width > 0 && m_orig_w > 0 && m_orig_h > 0) {
            int h = static_cast<int>(std::round(static_cast<double>(avail_width) * m_orig_h / m_orig_w));
            return Size(avail_width, h);
        }
        return ImageView::measure_size();
    }

private:
    int m_orig_w = 0;
    int m_orig_h = 0;
};

} // namespace

FilePreviewView::FilePreviewView() {
    LayoutParams lp;
    lp.width = static_cast<int>(LayoutDimension::MatchParent);
    lp.height = static_cast<int>(LayoutDimension::MatchParent);
    set_layout_params(lp);

    show_empty_state();
}

void FilePreviewView::update_preview(const FileItem* item, bool show_hidden) {
    clear_views();
    m_current_content = nullptr;
    m_text_scroll = nullptr;

    if (!item) {
        m_current_item = nullptr;
        show_empty_state("No selection");
        return;
    }

    m_current_item = std::make_shared<FileItem>(*item);

    if (item->is_dir) {
        show_directory_preview(*item, show_hidden);
    } else if (item->category == FileCategory::Image) {
        show_image_preview(*item);
    } else {
        show_file_preview(*item);
    }

    request_redraw();
}

bool FilePreviewView::scroll_preview(double delta) {
    if (m_text_scroll) {
        bool handled = m_text_scroll->on_scroll(delta);
        if (handled) {
            request_redraw();
        }
        return handled;
    }
    return false;
}

void FilePreviewView::show_empty_state(const std::string& message) {
    clear_views();

    auto config = Config::get();
    auto layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    layout->set_divider_spacing(8);

    LayoutParams layout_lp;
    layout_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    layout_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    layout_lp.gravity = Gravity::Center;
    layout->set_layout_params(layout_lp);

    auto icon = std::make_shared<ImageView>("view-preview");
    icon->set_target_size(48);
    icon->set_dim(0.35f);
    LayoutParams icon_lp;
    icon_lp.width = 48;
    icon_lp.height = 48;
    icon_lp.gravity = Gravity::CenterHorizontal;
    layout->add_view(icon, icon_lp);

    auto text = TextViewBuilder::create()
        ->text(message)
        ->textSize(12)
        ->center(true)
        ->textColor(config->colors.outline)
        ->build();
    layout->add_view(text);

    add_view(layout);
    m_current_content = layout;
}

void FilePreviewView::show_directory_preview(const FileItem& item, bool show_hidden) {
    auto config = Config::get();
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(4);
    root->set_divider_spacing(6);

    auto children = FileItem::scan_directory(item.path, show_hidden);
    if (children.empty()) {
        auto empty_label = TextViewBuilder::create()
            ->text("Empty Folder")
            ->textSize(11)
            ->center(true)
            ->textColor(config->colors.outline)
            ->build();
        root->add_view(empty_label);
    } else {
        auto list = ListViewBuilder::create()
            ->itemHeight(36)
            ->spacing(4)
            ->itemCornerRadius(6)
            ->build();
        list->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            0,
            1.0f
        ));

        for (const auto& ch : children) {
            auto item_view = ListItemViewBuilder::create()
                ->title(ch.name)
                ->trailingText(ch.size_formatted)
                ->iconSource(ch.icon_name)
                ->iconSize(18)
                ->build();
            list->add_item(item_view);
        }
        list->set_selected_index(-1);

        list->set_on_item_click_listener([this, children, list_weak = std::weak_ptr<ListView>(list)](size_t idx, std::shared_ptr<View>) {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_item_click_time).count();
            if (m_last_item_clicked_index == static_cast<int>(idx) && diff < 400) {
                if (idx < children.size() && m_on_item_click) {
                    m_on_item_click(children[idx]);
                }
            } else {
                m_last_item_click_time = now;
                m_last_item_clicked_index = static_cast<int>(idx);
                if (auto l = list_weak.lock()) {
                    l->set_selected_index(static_cast<int>(idx));
                }
            }
        });

        list->set_on_item_context_click_listener([this, children](size_t idx, std::shared_ptr<View>, int x, int y) {
            if (idx < children.size() && m_on_context_click) {
                m_on_context_click(children[idx], x, y);
            }
        });

        root->add_view(list);
    }

    add_view(root);
    m_current_content = root;
}

void FilePreviewView::show_image_preview(const FileItem& item) {
    auto config = Config::get();

    auto scroll = std::make_shared<ScrollView>();
    scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    auto aspect_img = std::make_shared<AspectImageView>(item.path.string());
    aspect_img->set_corner_radius(config->metrics.corner_radius);
    aspect_img->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    aspect_img->set_on_context_click_listener([this, item](int x, int y) {
        if (m_on_context_click) {
            m_on_context_click(item, x, y);
        }
    });

    scroll->set_content_view(aspect_img);

    m_text_scroll = scroll;
    add_view(scroll);
    m_current_content = scroll;
}

void FilePreviewView::show_file_preview(const FileItem& item) {
    auto config = Config::get();

    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams root_lp;
    root_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    root_lp.height = static_cast<int>(LayoutDimension::MatchParent);
    root_lp.gravity = Gravity::Center;
    root->set_layout_params(root_lp);
    root->set_padding(24);
    root->set_divider_spacing(16);

    auto icon = std::make_shared<ImageView>(item.icon_name);
    icon->set_target_size(56);
    LayoutParams icon_lp;
    icon_lp.width = 56;
    icon_lp.height = 56;
    icon_lp.gravity = Gravity::CenterHorizontal;
    root->add_view(icon, icon_lp);

    auto name_tv = TextViewBuilder::create()
        ->text(item.name)
        ->bold(true)
        ->textSize(13)
        ->center(true)
        ->textColor(config->colors.on_surface)
        ->wrap(true)
        ->build();
    LayoutParams name_lp;
    name_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    name_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    name_lp.gravity = Gravity::CenterHorizontal;
    name_tv->set_layout_params(name_lp);
    root->add_view(name_tv);

    add_view(root);
    m_current_content = root;
}

bool FilePreviewView::on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) {
    if (!is_visible()) return false;

    // First allow child views (e.g. directory ListView or ScrollView thumb) to handle mouse button
    if (FrameLayout::on_mouse_button(lx, ly, button, pressed, bounds)) {
        return true;
    }

    // If not consumed by a child view, handle clicking on single file / image preview
    if (bounds.contains(Point(lx, ly))) {
        if (button == MouseButton::Right && pressed) {
            if (m_current_item && m_on_context_click) {
                m_on_context_click(*m_current_item, lx, ly);
                return true;
            }
        } else if (button == MouseButton::Left) {
            if (pressed) {
                m_pressed = true;
                return true;
            } else if (m_pressed) {
                m_pressed = false;
                auto now = std::chrono::steady_clock::now();
                auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_preview_click_time).count();
                if (diff < 400) {
                    if (m_current_item && !m_current_item->is_dir && m_on_preview_click) {
                        m_on_preview_click(*m_current_item);
                    }
                } else {
                    m_last_preview_click_time = now;
                }
                return true;
            }
        }
    }

    if (!pressed) {
        m_pressed = false;
    }

    return false;
}

} // namespace miqu
