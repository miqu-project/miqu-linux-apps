#pragma once

#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include "miqutoolkit/view/button.hpp"
#include "miqutoolkit/view/text_view.hpp"
#include <memory>
#include <functional>

namespace miqubar {

class PowerPopupView : public miqu::CardView {
public:
    explicit PowerPopupView(std::function<void()> on_close);
    ~PowerPopupView() override = default;

    miqu::Size measure_size() const override;

private:
    void setup_ui();
    void trigger_action(const std::string& action_name);
    void open_full_menu();

    std::function<void()> m_on_close;
};

} // namespace miqubar
