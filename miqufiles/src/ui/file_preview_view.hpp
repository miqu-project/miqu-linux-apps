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
    void set_on_open_listener(std::function<void(const std::filesystem::path&)> cb) {
        m_on_open = std::move(cb);
    }

private:
    void show_empty_state(const std::string& message = "No selection");
    void show_directory_preview(const FileItem& item, bool show_hidden);
    void show_image_preview(const FileItem& item);
    void show_text_preview(const FileItem& item);
    void show_binary_preview(const FileItem& item);

    std::shared_ptr<View> m_current_content;
    std::function<void(const std::filesystem::path&)> m_on_open;
};

} // namespace miqu
