#pragma once

#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <miqutoolkit/view/slider.hpp>
#include <memory>
#include <string>
#include <functional>

namespace miqu {

class NightLightDetailView : public FrameLayout {
public:
    NightLightDetailView();
    ~NightLightDetailView() override = default;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

    void set_on_request_fullscreen(std::function<void()> cb) {
        m_on_request_fullscreen = std::move(cb);
    }

private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_toggle_card();
    std::shared_ptr<View> create_warmth_card();
    std::shared_ptr<View> create_schedule_card();
    std::shared_ptr<View> create_engine_card();

    bool m_is_fullscreen = false;
    bool m_enabled = true;
    float m_warmth = 0.60f; // corresponds to ~4500K
    int m_schedule_mode = 0; // 0 = Sunset to Sunrise, 1 = Always On

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::function<void()> m_on_request_fullscreen;
};

} // namespace miqu
