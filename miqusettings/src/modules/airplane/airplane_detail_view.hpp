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
#include <functional>

namespace miqu {

class AirplaneDetailView : public FrameLayout {
public:
    AirplaneDetailView();
    ~AirplaneDetailView() override;

    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

    void set_on_request_fullscreen(std::function<void()> cb) {
        m_on_request_fullscreen = std::move(cb);
    }

    void refresh_state();

private:
    void setup_ui();
    void load_data();
    void start_polling();
    void stop_polling();

    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_radios_card();
    std::shared_ptr<View> create_radio_row(const std::string& icon, const std::string& title,
                                          const std::string& subtitle, bool is_active);
    std::shared_ptr<View> create_info_card();

    void on_toggle_mode(bool enabled);

    bool m_is_fullscreen = false;
    uint64_t m_poll_timer_id = 0;
    AirplaneStatus m_status;

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::function<void()> m_on_request_fullscreen;
};

} // namespace miqu
