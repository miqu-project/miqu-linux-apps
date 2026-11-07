#pragma once

#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <memory>
#include <string>

namespace miqu {

class WmControlDetailView : public FrameLayout {
public:
    WmControlDetailView();
    ~WmControlDetailView() override = default;

private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_reload_card();
    std::shared_ptr<View> create_shutdown_card();

std::string m_last_status = "Ready";

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
