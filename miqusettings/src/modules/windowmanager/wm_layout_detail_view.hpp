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
#include <functional>

namespace miqu {

class WmLayoutDetailView : public FrameLayout {
public:
    WmLayoutDetailView();
    ~WmLayoutDetailView() override = default;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_layout_modes_card();
    std::shared_ptr<View> create_smart_gaps_card();
    std::shared_ptr<View> create_actions_card();

    bool m_is_fullscreen = false;
    std::string m_current_layout = "dwindle";
    bool m_smart_gaps = false;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
