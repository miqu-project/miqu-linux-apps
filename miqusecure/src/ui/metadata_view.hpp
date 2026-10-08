#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <functional>

namespace miqusecure {

class CleanerView : public miqu::ScrollView {
public:
    CleanerView(const CleanerInfo& info, std::function<void()> on_changed = nullptr);
    ~CleanerView() override = default;

    void update_info(const CleanerInfo& info);

private:
    void setup_status_hero();
    void setup_caches_section();
    void setup_sanitizer_section();
    void update_status_indicator(bool ready);
    void clean_file(const std::string& path);

    CleanerInfo m_info;
    std::function<void()> m_on_changed;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::FrameLayout> m_status_dot;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::TextView> m_status_badge;
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;

    // Metadata file cleaning
    std::shared_ptr<miqu::EditText> m_path_input;
    std::shared_ptr<miqu::Button> m_btn_browse;
    std::shared_ptr<miqu::Button> m_btn_clean_file;
    std::shared_ptr<miqu::TextView> m_result_lbl;

    // Cache size labels
    std::shared_ptr<miqu::TextView> m_thumb_size_lbl;
    std::shared_ptr<miqu::TextView> m_browser_size_lbl;
    std::shared_ptr<miqu::TextView> m_trash_size_lbl;
    std::shared_ptr<miqu::TextView> m_history_size_lbl;
};

using MetadataView = CleanerView;

} // namespace miqusecure
