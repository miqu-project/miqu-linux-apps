#pragma once

#include "modules/applications/apps_types.hpp"
#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/scroll_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <memory>
#include <functional>

namespace miqu {

class AppDetailView : public FrameLayout {
public:
    AppDetailView();
    ~AppDetailView() override = default;

    void set_app_id(const std::string& desktop_id);

    void set_on_app_uninstalled(std::function<void(const std::string&)> cb) {
        m_on_app_uninstalled = std::move(cb);
    }

private:
    void setup_ui();
    void load_data();

    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_actions_card();
    std::shared_ptr<View> create_info_card();
    std::shared_ptr<View> create_info_row(const std::string& label, const std::string& value);

    void on_launch_clicked();
    void on_uninstall_clicked();
    void execute_uninstall();

    std::string m_desktop_id;
    AppDetailInfo m_app;
std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;

std::function<void(const std::string&)> m_on_app_uninstalled;
};

} // namespace miqu
