#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>

namespace miqusecure {

class TrafficView : public miqu::ScrollView {
public:
    explicit TrafficView(const TrafficReport& traffic);
    ~TrafficView() override = default;

    void update_info(const TrafficReport& traffic);

private:
    void setup_metrics_hero();
    void setup_filter_bar();
    void setup_connections_section();
    void setup_feed_section();
    void rebuild_active_connections();
    void rebuild_recent_feed();

    enum class TrafficScopeMode {
        InternetOnly = 0,
        All = 1,
        LocalOnly = 2,
        EncryptedOnly = 3
    };

    TrafficReport m_traffic;
    TrafficScopeMode m_scope_mode = TrafficScopeMode::InternetOnly;
    std::string m_search_filter;

    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::TextView> m_outbound_badge;
    std::shared_ptr<miqu::TextView> m_encrypted_badge;
    std::shared_ptr<miqu::TextView> m_bandwidth_badge;
    std::shared_ptr<miqu::TextView> m_apps_badge;

    std::shared_ptr<miqu::LinearLayout> m_feed_container;
    std::shared_ptr<miqu::LinearLayout> m_conn_container;
    std::shared_ptr<miqu::Spinner> m_spinner_scope;
    std::shared_ptr<miqu::EditText> m_search_input;
};

} // namespace miqusecure
