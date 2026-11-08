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

class WmEffectsDetailView : public FrameLayout {
public:
    WmEffectsDetailView();
    ~WmEffectsDetailView() override = default;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_opacity_card();
    std::shared_ptr<View> create_toggles_card();

    bool m_is_fullscreen = false;
    float m_opacity_active = 1.0f;
    float m_opacity_inactive = 1.0f;
    bool m_animations = true;
    bool m_blur = true;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
