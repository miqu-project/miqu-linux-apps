#pragma once

#include "sound_backend.hpp"
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/slider.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>

namespace miqu {

class SoundDetailView : public FrameLayout {
public:
    SoundDetailView();
    ~SoundDetailView() override = default;


private:
void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_output_volume_card();
    std::shared_ptr<View> create_output_devices_card();
    std::shared_ptr<View> create_input_volume_card();
    std::shared_ptr<View> create_input_devices_card();

SoundState m_state;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::shared_ptr<TextView> m_out_vol_label;
    std::shared_ptr<Slider> m_out_slider;
    std::shared_ptr<TextView> m_in_vol_label;
    std::shared_ptr<Slider> m_in_slider;
};

} // namespace miqu
