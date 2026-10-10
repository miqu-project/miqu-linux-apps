#pragma once

#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/view/slider.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include <memory>
#include <functional>

namespace miqubar {

class QuickSettingsPopupView : public miqu::CardView {
public:
    explicit QuickSettingsPopupView(std::function<void()> on_close);
    ~QuickSettingsPopupView() override = default;

    miqu::Size measure_size() const override;

private:
    void setup_ui();
    void update_audio_info();
    void set_volume(int pct);
    void update_tile_styles();

    int m_volume = 70;
    bool m_muted = false;
    bool m_wifi_on = true;
    bool m_bluetooth_on = true;
    bool m_night_light_on = false;
    bool m_power_save_on = false;

    std::shared_ptr<miqu::TextView> m_vol_label;
    std::shared_ptr<miqu::Slider> m_vol_slider;

    std::shared_ptr<miqu::Button> m_wifi_btn;
    std::shared_ptr<miqu::Button> m_bt_btn;
    std::shared_ptr<miqu::Button> m_night_btn;
    std::shared_ptr<miqu::Button> m_power_btn;

    std::function<void()> m_on_close;
};

} // namespace miqubar
