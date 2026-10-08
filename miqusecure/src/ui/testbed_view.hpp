#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <functional>

namespace miqusecure {

class TestbedView : public miqu::ScrollView {
public:
    explicit TestbedView(const DistroboxInfo& info, std::function<void()> on_refresh);
    ~TestbedView() override = default;

    void update_info(const DistroboxInfo& info);

private:
    void setup_status_hero();
    void setup_create_section();
    void rebuild_boxes_list();
    void update_status_indicator(bool ready);

    DistroboxInfo m_info;
    std::function<void()> m_on_refresh;

    // Root scroll content
    std::shared_ptr<miqu::LinearLayout> m_layout;

    // Hero card
    std::shared_ptr<miqu::FrameLayout> m_status_dot;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;

    // Create section
    std::shared_ptr<miqu::LinearLayout> m_create_section;
    std::shared_ptr<miqu::EditText> m_input_name;
    std::shared_ptr<miqu::Spinner> m_spinner_image;
    std::shared_ptr<miqu::Button> m_btn_create;

    // Boxes list
    std::shared_ptr<miqu::LinearLayout> m_boxes_container;
};

} // namespace miqusecure
