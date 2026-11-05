#pragma once

#include "model/file_item.hpp"
#include <miqutoolkit/view/view_group.hpp>
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/list_view.hpp>
#include <memory>
#include <functional>

namespace miqu {

class FilePreviewView : public FrameLayout {
public:
    FilePreviewView();
    ~FilePreviewView() override = default;

    void update_preview(const FileItem* item, bool show_hidden = false);
    bool scroll_preview(double delta);
    void set_on_item_click_listener(std::function<void(const FileItem&)> cb) {
        m_on_item_click = std::move(cb);
    }
    void set_on_preview_click_listener(std::function<void(const FileItem&)> cb) {
        m_on_preview_click = std::move(cb);
    }
    void set_on_context_click_listener(std::function<void(const FileItem&, int, int)> cb) {
        m_on_context_click = std::move(cb);
    }

    bool on_mouse_button(int lx, int ly, MouseButton button, bool pressed, const Rect& bounds) override;

    std::filesystem::path get_current_path() const {
        return m_current_item ? m_current_item->path : std::filesystem::path();
    }

private:
    void show_empty_state(const std::string& message = "No selection");
    void show_directory_preview(const FileItem& item, bool show_hidden);
    void show_image_preview(const FileItem& item);
    void show_file_preview(const FileItem& item);

    std::shared_ptr<View> m_current_content;
    std::shared_ptr<ScrollView> m_text_scroll;
    std::function<void(const FileItem&)> m_on_item_click;
    std::function<void(const FileItem&)> m_on_preview_click;
    std::function<void(const FileItem&, int, int)> m_on_context_click;
    std::shared_ptr<FileItem> m_current_item;
    bool m_pressed = false;

    std::chrono::steady_clock::time_point m_last_item_click_time{};
    int m_last_item_clicked_index = -1;
    std::chrono::steady_clock::time_point m_last_preview_click_time{};
};

} // namespace miqu
