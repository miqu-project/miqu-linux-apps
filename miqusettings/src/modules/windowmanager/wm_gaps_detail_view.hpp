#pragma once

#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/slider.hpp>
#include <memory>
#include <string>

namespace miqu {

class WmGapsDetailView : public FrameLayout {
public:
    WmGapsDetailView();
    ~WmGapsDetailView() override = default;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_inner_gaps_card();
    std::shared_ptr<View> create_outer_gaps_card();
    std::shared_ptr<View> create_presets_card();

    bool m_is_fullscreen = false;
    int m_gaps_in = 8;
    int m_gaps_out = 12;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
