#pragma once

#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/view/slider.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include <memory>
#include <functional>
#include <string>

namespace miqubar {

class BrightnessControlPopupView : public miqu::CardView {
public:
    explicit BrightnessControlPopupView(std::function<void()> on_close);
    ~BrightnessControlPopupView() override = default;

    miqu::Size measure_size() const override;

private:
    void setup_ui();
    void update_brightness_info();
    void set_brightness(int pct);

    int m_brightness = 100;
    std::string m_backlight_path;

    std::shared_ptr<miqu::TextView> m_label;
    std::shared_ptr<miqu::Slider> m_slider;
    std::shared_ptr<miqu::Button> m_icon_btn;

    std::function<void()> m_on_close;
};

} // namespace miqubar
