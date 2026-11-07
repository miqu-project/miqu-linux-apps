#pragma once

#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <memory>
#include <string>
#include <vector>
namespace miqu {

class TimeoutDetailView : public FrameLayout {
public:
    TimeoutDetailView();
    ~TimeoutDetailView() override = default;


private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_display_timeout_card();
    std::shared_ptr<View> create_suspend_timeout_card();
    std::shared_ptr<View> create_options_card();

int m_display_timeout_idx = 2; // 10 Minutes
    int m_suspend_timeout_idx = 1; // 30 Minutes
    bool m_dim_before_off = true;
    bool m_respect_inhibitors = true;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
