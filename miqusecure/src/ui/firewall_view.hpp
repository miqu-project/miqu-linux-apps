#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <functional>

namespace miqusecure {

class FirewallView : public miqu::ScrollView {
public:
    FirewallView(const FirewallInfo& info, std::function<void()> on_rules_changed);
    ~FirewallView() override = default;

    void update_info(const FirewallInfo& info);

private:
    void setup_status_hero();
    void setup_network_profile();
    void setup_local_services();
    void setup_rules_card();
    void rebuild_rules_list();
    void show_add_rule_popup(const std::shared_ptr<miqu::View>& anchor);
    void update_status_indicator(bool active);

    FirewallInfo m_info;
    std::function<void()> m_on_changed;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::FrameLayout> m_status_dot;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::TextView> m_network_lbl;
    std::shared_ptr<miqu::Switch> m_switch_master;

    // Network profile
    std::shared_ptr<miqu::Spinner> m_spinner_mode;
    std::shared_ptr<miqu::Switch> m_switch_stealth;
    std::shared_ptr<miqu::Switch> m_switch_block_all;
    bool m_updating_ui = false;

    static int mode_to_index(NetworkMode mode);
    static NetworkMode index_to_mode(int index);

    // Service switches
    std::shared_ptr<miqu::Switch> m_switch_ssh;
    std::shared_ptr<miqu::Switch> m_switch_web;
    std::shared_ptr<miqu::Switch> m_switch_syncthing;
    std::shared_ptr<miqu::Switch> m_switch_samba;

    // Rules table container
    std::shared_ptr<miqu::LinearLayout> m_rules_table;
    std::shared_ptr<miqu::Button> m_btn_add_rule;

    // Material Design 3 docked linear progress indicator
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;

    // Active popup window reference
    std::shared_ptr<miqu::PopupWindow> m_add_popup;
};

} // namespace miqusecure
