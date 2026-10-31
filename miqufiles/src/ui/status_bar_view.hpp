#pragma once

#include "model/file_item.hpp"
#include <miqutoolkit/view/view_group.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <memory>
#include <string>

namespace miqu {

class StatusBarView : public ViewGroup {
public:
    StatusBarView();
    ~StatusBarView() override = default;

    void update_stats(const std::vector<FileItem>& current_items, const FileItem* selected_item, const std::filesystem::path& current_path);

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;
    Size measure_size(int avail_width) const override;

private:
    std::string m_directory_summary;
    std::string m_selection_summary;
    std::string m_disk_summary;

    std::shared_ptr<TextView> m_tv_dir;
    std::shared_ptr<TextView> m_tv_selection;
    std::shared_ptr<TextView> m_tv_disk;
};

} // namespace miqu
