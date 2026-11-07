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

class DefaultAppDetailView : public FrameLayout {
public:
    DefaultAppDetailView();
    ~DefaultAppDetailView() override = default;

    void set_role_id(const std::string& role_id);

    void set_on_default_changed(std::function<void(const std::string&, const std::string&)> cb) {
        m_on_default_changed = std::move(cb);
    }

private:
    void setup_ui();
    void load_data();

    std::shared_ptr<View> create_hero_card();
    std::shared_ptr<View> create_current_default_card();
    std::shared_ptr<View> create_candidates_card();
    std::shared_ptr<View> create_candidate_row(const AppDetailInfo& app, bool is_current);

    void select_candidate(const std::string& desktop_id);

    std::string m_role_id;
    DefaultRoleSpec m_role;
std::shared_ptr<ScrollView> m_scroll;
    std::shared_ptr<LinearLayout> m_content_layout;

std::function<void(const std::string&, const std::string&)> m_on_default_changed;
};

} // namespace miqu
