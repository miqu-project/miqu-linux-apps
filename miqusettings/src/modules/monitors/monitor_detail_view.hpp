#pragma once

#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <miqutoolkit/view/switch.hpp>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace miqu {

class MonitorDetailView : public FrameLayout {
public:
    MonitorDetailView();
    ~MonitorDetailView() override = default;

    void set_monitor_info(const std::string& name, const std::string& subtitle);
    void set_is_fullscreen(bool fullscreen);
    bool is_fullscreen() const { return m_is_fullscreen; }

    void set_on_request_fullscreen(std::function<void()> cb) {
        m_on_request_fullscreen = std::move(cb);
    }

private:
    void setup_ui();
    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_primary_card();
    std::shared_ptr<View> create_resolution_card();
    std::shared_ptr<View> create_scaling_card();
    std::shared_ptr<View> create_orientation_card();
    std::shared_ptr<View> create_actions_card();

    bool m_is_fullscreen = false;
    bool m_is_primary = true;
    std::string m_selected_monitor = "eDP-1";
    std::string m_subtitle = "1920x1080 @ 60Hz · Built-in Display (Primary)";
    std::string m_current_res = "1920x1080";
    std::string m_current_hz = "60 Hz";
    std::string m_current_scale = "100%";
    std::string m_current_orient = "Landscape (Standard)";

    std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;
    std::function<void()> m_on_request_fullscreen;
};

} // namespace miqu
