#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <functional>

namespace miqusecure {

class DnsView : public miqu::ScrollView {
public:
    DnsView(const DnsInfo& info, std::function<void()> on_changed);
    ~DnsView() override = default;

    void update_info(const DnsInfo& info);

private:
    void setup_master_card();
    void setup_resolver_card();
    void show_custom_dns_popup(const std::shared_ptr<miqu::View>& anchor);
    void update_status_indicator(bool encrypted);
    void update_custom_dns_label();

    static int provider_to_index(const std::string& provider_name);

    DnsInfo m_info;
    std::function<void()> m_on_changed;
    bool m_updating_ui = false;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::FrameLayout> m_status_dot;
    std::shared_ptr<miqu::TextView> m_status_lbl;
    std::shared_ptr<miqu::TextView> m_subtitle_lbl;
    std::shared_ptr<miqu::Switch> m_switch_master;
    std::shared_ptr<miqu::Spinner> m_spinner_provider;
    std::shared_ptr<miqu::TextView> m_custom_dns_lbl;
    std::shared_ptr<miqu::Button> m_btn_custom_dns;
    std::shared_ptr<miqu::ProgressBar> m_progress_bar;

    // Active popup window reference
    std::shared_ptr<miqu::PopupWindow> m_custom_popup;
};

} // namespace miqusecure
