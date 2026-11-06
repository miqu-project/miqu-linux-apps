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
#include <functional>

namespace miqu {

class BrightnessDetailView : public FrameLayout {
public:
    BrightnessDetailView();
    ~BrightnessDetailView() override = default;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

    void set_on_request_fullscreen(std::function<void()> cb) {
        m_on_request_fullscreen = std::move(cb);
    }

private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_slider_card();
    std::shared_ptr<View> create_presets_card();
    std::shared_ptr<View> create_hardware_card();

    bool m_is_fullscreen = false;
    float m_brightness = 0.50f;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::function<void()> m_on_request_fullscreen;
};

} // namespace miqu
