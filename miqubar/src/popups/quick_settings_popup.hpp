#pragma once

#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/view/slider.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include <memory>
#include <functional>

namespace miqubar {

class SoundControlPopupView : public miqu::CardView {
public:
    explicit SoundControlPopupView(std::function<void()> on_close);
    ~SoundControlPopupView() override = default;

    miqu::Size measure_size() const override;

private:
    void setup_ui();
    void update_audio_info();
    void set_volume(int pct);
    void toggle_mute();

    int m_volume = 70;
    bool m_muted = false;

    std::shared_ptr<miqu::TextView> m_vol_label;
    std::shared_ptr<miqu::Slider> m_vol_slider;
    std::shared_ptr<miqu::Button> m_mute_btn;

    std::function<void()> m_on_close;
};

// Backward-compatibility alias
using QuickSettingsPopupView = SoundControlPopupView;

} // namespace miqubar
