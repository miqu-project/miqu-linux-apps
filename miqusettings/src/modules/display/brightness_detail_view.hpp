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

class BrightnessDetailView : public FrameLayout {
public:
    BrightnessDetailView();
    ~BrightnessDetailView() override = default;


private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_slider_card();
    std::shared_ptr<View> create_presets_card();
    std::shared_ptr<View> create_hardware_card();

float m_brightness = 0.50f;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
