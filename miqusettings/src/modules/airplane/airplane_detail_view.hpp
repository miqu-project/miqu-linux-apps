#pragma once

#include "modules/airplane/airplane_types.hpp"
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <memory>
namespace miqu {

class AirplaneDetailView : public FrameLayout {
public:
    AirplaneDetailView();
    ~AirplaneDetailView() override;


    void refresh_state();

private:
    void setup_ui();
    void load_data();

    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_radios_card();
    std::shared_ptr<View> create_radio_row(const std::string& icon, const std::string& title,
                                          const std::string& subtitle, bool is_active);
    std::shared_ptr<View> create_info_card();

    void on_toggle_mode(bool enabled);

    AirplaneStatus m_status;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
};

} // namespace miqu
