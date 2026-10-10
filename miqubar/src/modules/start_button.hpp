#pragma once

#include "miqutoolkit/view/image_button.hpp"

namespace miqubar {

class StartButtonView : public miqu::ImageButton {
public:
    StartButtonView();
    ~StartButtonView() override = default;

private:
    void launch_menu();
};

} // namespace miqubar
