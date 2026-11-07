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

class ScreenLockDetailView : public FrameLayout {
public:
    ScreenLockDetailView();
    ~ScreenLockDetailView() override = default;


private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_lock_triggers_card();
    std::shared_ptr<View> create_grace_period_card();
    std::shared_ptr<View> create_appearance_card();
    std::shared_ptr<View> create_actions_card();

bool m_lock_on_sleep = true;
    bool m_lock_on_display_off = true;
    bool m_show_power_buttons = true;
    int m_grace_period_idx = 0; // 0 = Immediately

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
