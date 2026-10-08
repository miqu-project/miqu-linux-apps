#include "traffic_view.hpp"
#include "ui_components.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>

using namespace miqu;

namespace miqusecure {

static const Color COLOR_ACTIVE_GREEN(0.188f, 0.820f, 0.345f, 1.0f); // #30d158
static const Color COLOR_WARN_AMBER(0.980f, 0.700f, 0.200f, 1.0f);   // #fab387

TrafficView::TrafficView(const TrafficReport& traffic)
    : m_traffic(traffic) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_layout->set_padding(24, 16);

    setup_metrics_hero();
    setup_filter_bar();
    setup_connections_section();
    setup_feed_section();

    set_content_view(m_layout);
    update_info(m_traffic);
}

// =============================================================================
// 1. TOP METRICS HERO INSET CARD
// =============================================================================
void TrafficView::setup_metrics_hero() {
    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 14)
        ->build();
    card->set_margin(0, 4, 0, 10);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto make_metric_col = [](const std::string& caption_text, std::shared_ptr<TextView>& out_val) {
        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        out_val = TextViewBuilder::create()->text("--")->h2()->bold(true)->build();
        out_val->set_margin(0, 0, 0, 2);

        auto lbl = TextViewBuilder::create()->text(caption_text)->caption()->muted()->build();

        col->add_view(out_val);
        col->add_view(lbl);
        return col;
    };

    row->add_view(make_metric_col("ACTIVE SOCKETS", m_outbound_badge));
    row->add_view(DividerViewBuilder::create()->orientation(Orientation::Vertical)->thickness(1)->margin(12, 4)->build());

    row->add_view(make_metric_col("TLS ENCRYPTED", m_encrypted_badge));
    row->add_view(DividerViewBuilder::create()->orientation(Orientation::Vertical)->thickness(1)->margin(12, 4)->build());

    row->add_view(make_metric_col("BANDWIDTH RX/TX", m_bandwidth_badge));
    row->add_view(DividerViewBuilder::create()->orientation(Orientation::Vertical)->thickness(1)->margin(12, 4)->build());

    row->add_view(make_metric_col("ACTIVE PROCESSES", m_apps_badge));

    card->add_view(row);
    m_layout->add_view(card);
}

// =============================================================================
// 2. SCOPE & SEARCH FILTER BAR
// =============================================================================
void TrafficView::setup_filter_bar() {
    auto sec_hdr = ui::make_section_header("TRAFFIC SCOPE & FILTERS");
    m_layout->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(14, 10)
        ->build();
    card->set_margin(0, 0, 0, 10);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    std::vector<std::string> scope_items = {
        "Internet Only",
        "All Traffic (inc. Local)",
        "Localhost Only",
        "Encrypted Only (TLS)"
    };

    m_spinner_scope = SpinnerBuilder::create()
        ->items(scope_items)
        ->selectedIndex(static_cast<int>(m_scope_mode))
        ->padding(12, 6)
        ->cornerRadius(8)
        ->onItemSelected([this](int idx, const std::string&) {
            m_scope_mode = static_cast<TrafficScopeMode>(idx);
            rebuild_active_connections();
        })
        ->build();
    m_spinner_scope->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    m_spinner_scope->set_margin(0, 0, 12, 0);
    row->add_view(m_spinner_scope);

    m_search_input = EditTextBuilder::create()
        ->hint("Filter by process, host, or port...")
        ->padding(10, 8)
        ->build();
    m_search_input->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_search_input->set_on_text_changed_listener([this](std::shared_ptr<EditText>, const std::string& text) {
        m_search_filter = text;
        rebuild_active_connections();
    });
    row->add_view(m_search_input);

    card->add_view(row);
    m_layout->add_view(card);
}

// =============================================================================
// 3. ACTIVE SOCKET CONNECTIONS TABLE
// =============================================================================
void TrafficView::setup_connections_section() {
    auto sec_hdr = ui::make_section_header("ACTIVE SOCKET CONNECTIONS");
    m_layout->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 12)
        ->build();
    card->set_margin(0, 0, 0, 10);

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Table Header Row
    auto table_hdr = std::make_shared<LinearLayout>(Orientation::Horizontal);
    table_hdr->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    table_hdr->set_padding(0, 2, 0, 8);

    auto h_app = TextViewBuilder::create()->text("APPLICATION / PID")->caption()->bold(true)->muted()->build();
    h_app->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.3f));

    auto h_dest = TextViewBuilder::create()->text("REMOTE DESTINATION")->caption()->bold(true)->muted()->build();
    h_dest->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.8f));

    auto h_proto = TextViewBuilder::create()->text("PROTOCOL")->caption()->bold(true)->muted()->build();
    h_proto->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.6f));

    auto h_sec = TextViewBuilder::create()->text("SECURITY")->caption()->bold(true)->muted()->build();
    h_sec->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.8f));

    auto h_act = TextViewBuilder::create()->text("ACTIONS")->caption()->bold(true)->muted()->textAlignment(TextAlignment::Right)->build();
    h_act->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.5f));

    table_hdr->add_view(h_app);
    table_hdr->add_view(h_dest);
    table_hdr->add_view(h_proto);
    table_hdr->add_view(h_sec);
    table_hdr->add_view(h_act);
    box->add_view(table_hdr);

    box->add_view(DividerViewBuilder::create()->build());

    m_conn_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_conn_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    box->add_view(m_conn_container);

    card->add_view(box);
    m_layout->add_view(card);
}

// =============================================================================
// 4. LIVE RECENT ACTIVITY FEED
// =============================================================================
void TrafficView::setup_feed_section() {
    auto sec_hdr = ui::make_section_header("RECENT ACTIVITY FEED");
    m_layout->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 12)
        ->build();
    card->set_margin(0, 0, 0, 24);

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    m_feed_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_feed_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    box->add_view(m_feed_container);

    card->add_view(box);
    m_layout->add_view(card);
}

void TrafficView::update_info(const TrafficReport& traffic) {
    m_traffic = traffic;

    if (m_outbound_badge) {
        m_outbound_badge->set_text(std::to_string(m_traffic.total_outbound));
    }
    if (m_encrypted_badge) {
        int pct = (m_traffic.total_outbound > 0) ? (m_traffic.total_encrypted * 100 / m_traffic.total_outbound) : 100;
        m_encrypted_badge->set_text(std::to_string(pct) + "%");
    }
    if (m_bandwidth_badge) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << "↓" << m_traffic.rx_rate_kb << " ↑" << m_traffic.tx_rate_kb;
        m_bandwidth_badge->set_text(ss.str());
    }
    if (m_apps_badge) {
        m_apps_badge->set_text(std::to_string(m_traffic.total_apps));
    }

    rebuild_active_connections();
    rebuild_recent_feed();
}

void TrafficView::rebuild_active_connections() {
    if (!m_conn_container) return;
    m_conn_container->clear_views();

    std::string q = m_search_filter;
    for (char& c : q) c = std::tolower(c);

    std::vector<NetworkConnection> filtered;
    for (const auto& conn : m_traffic.active_connections) {
        if (m_scope_mode == TrafficScopeMode::InternetOnly && conn.is_loopback) {
            continue;
        } else if (m_scope_mode == TrafficScopeMode::LocalOnly && !conn.is_loopback) {
            continue;
        } else if (m_scope_mode == TrafficScopeMode::EncryptedOnly && !conn.is_encrypted) {
            continue;
        }

        if (!q.empty()) {
            std::string p_lower = conn.process_name;
            for (char& c : p_lower) c = std::tolower(c);
            std::string h_lower = conn.remote_host;
            for (char& c : h_lower) c = std::tolower(c);
            std::string a_lower = conn.remote_addr;

            if (p_lower.find(q) == std::string::npos &&
                h_lower.find(q) == std::string::npos &&
                a_lower.find(q) == std::string::npos &&
                std::to_string(conn.pid).find(q) == std::string::npos &&
                std::to_string(conn.remote_port).find(q) == std::string::npos) {
                continue;
            }
        }
        filtered.push_back(conn);
    }

    if (filtered.empty()) {
        auto empty_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        empty_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        empty_row->set_padding(0, 14);

        auto empty_tv = TextViewBuilder::create()
            ->text("No active network sockets matching the current scope and filter.")
            ->caption()
            ->muted()
            ->build();
        empty_row->add_view(empty_tv);
        m_conn_container->add_view(empty_row);
        return;
    }

    for (size_t i = 0; i < filtered.size(); ++i) {
        const auto& conn = filtered[i];
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_padding(0, 10);

        // Col 1: App + PID
        auto app_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        app_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.3f));

        auto app_tv = TextViewBuilder::create()
            ->text(conn.process_name)
            ->bold(true)
            ->ellipsize(true)
            ->build();
        auto pid_tv = TextViewBuilder::create()
            ->text(conn.pid > 0 ? ("PID " + std::to_string(conn.pid)) : "System")
            ->caption()
            ->muted()
            ->build();

        app_col->add_view(app_tv);
        app_col->add_view(pid_tv);
        row->add_view(app_col);

        // Col 2: Remote Destination
        std::string dest_desc = conn.remote_host;
        if (dest_desc.empty() || dest_desc == conn.remote_addr) {
            dest_desc = conn.remote_addr + ":" + std::to_string(conn.remote_port);
        } else {
            dest_desc += ":" + std::to_string(conn.remote_port);
        }

        auto dest_tv = TextViewBuilder::create()
            ->text(dest_desc)
            ->caption()
            ->ellipsize(true)
            ->build();
        dest_tv->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.8f));
        row->add_view(dest_tv);

        // Col 3: Protocol
        auto proto_tv = TextViewBuilder::create()
            ->text(conn.protocol)
            ->caption()
            ->muted()
            ->build();
        proto_tv->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.6f));
        row->add_view(proto_tv);

        // Col 4: Security
        auto sec_tv = TextViewBuilder::create()
            ->text(conn.is_encrypted ? "TLS" : (conn.is_loopback ? "Local" : "Plaintext"))
            ->caption()
            ->bold(true)
            ->textColor(conn.is_encrypted ? COLOR_ACTIVE_GREEN : (conn.is_loopback ? Color(0.7f, 0.7f, 0.8f, 1.0f) : COLOR_WARN_AMBER))
            ->build();
        sec_tv->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.8f));
        row->add_view(sec_tv);

        // Col 5: Actions
        auto act_box = std::make_shared<LinearLayout>(Orientation::Horizontal);
        auto act_box_lp = LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 0.5f);
        act_box_lp.gravity = Gravity::End;
        act_box->set_layout_params(act_box_lp);

        if (conn.pid > 1) {
            int pid = conn.pid;
            auto btn_end = ButtonBuilder::create()
                ->text("End")
                ->flat(true)
                ->padding(8, 4)
                ->onClick([pid]() {
                    SecurityBackend::terminate_process(pid, false);
                })
                ->build();
            act_box->add_view(btn_end);
        }
        row->add_view(act_box);

        m_conn_container->add_view(row);

        if (i + 1 < filtered.size()) {
            m_conn_container->add_view(DividerViewBuilder::create()->build());
        }
    }
}

void TrafficView::rebuild_recent_feed() {
    if (!m_feed_container) return;
    m_feed_container->clear_views();

    if (m_traffic.recent_events.empty()) {
        auto empty_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        empty_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        empty_row->set_padding(0, 14);

        auto empty_tv = TextViewBuilder::create()
            ->text("Listening for outbound requests... New socket connections will appear here in real time.")
            ->caption()
            ->muted()
            ->build();
        empty_row->add_view(empty_tv);
        m_feed_container->add_view(empty_row);
        return;
    }

    size_t count = std::min(m_traffic.recent_events.size(), static_cast<size_t>(10));
    for (size_t i = 0; i < count; ++i) {
        const auto& event = m_traffic.recent_events[i];
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_padding(0, 10);

        auto time_tv = TextViewBuilder::create()->text(event.timestamp)->caption()->muted()->build();
        time_tv->set_margin(0, 0, 12, 0);
        row->add_view(time_tv);

        auto proc_tv = TextViewBuilder::create()->text(event.process_name)->bold(true)->caption()->build();
        proc_tv->set_margin(0, 0, 10, 0);
        row->add_view(proc_tv);

        auto arrow_tv = TextViewBuilder::create()->text("→")->caption()->muted()->build();
        arrow_tv->set_margin(0, 0, 10, 0);
        row->add_view(arrow_tv);

        std::string dest_label = event.remote_host;
        if (dest_label.empty() || dest_label == event.remote_addr) {
            dest_label = event.remote_addr + ":" + std::to_string(event.remote_port);
        } else {
            dest_label += " (" + event.remote_addr + ":" + std::to_string(event.remote_port) + ")";
        }

        auto target_tv = TextViewBuilder::create()
            ->text(dest_label)
            ->caption()
            ->ellipsize(true)
            ->build();
        target_tv->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
        row->add_view(target_tv);

        auto sec_tv = TextViewBuilder::create()
            ->text(event.is_encrypted ? "TLS" : "Plain")
            ->caption()
            ->bold(true)
            ->textColor(event.is_encrypted ? COLOR_ACTIVE_GREEN : COLOR_WARN_AMBER)
            ->build();
        row->add_view(sec_tv);

        m_feed_container->add_view(row);

        if (i + 1 < count) {
            m_feed_container->add_view(DividerViewBuilder::create()->build());
        }
    }
}

} // namespace miqusecure
