#include "settings_view.hpp"

using namespace miqu;

namespace miqumusic {

std::shared_ptr<View> SettingsView::create(
    MpdClient& client,
    const std::string& host,
    unsigned int port,
    std::function<void(bool connected)> on_conn_changed
) {
    auto settings_scroll_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    settings_scroll_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    settings_scroll_layout->set_padding(8, 6, 8, 18);

    auto make_section_card = [](const std::string& title_text, const std::string& icon_text) {
        auto card = std::make_shared<CardView>();
        card->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        card->set_corner_radius(12);
        card->set_padding(14, 12);
        card->set_margin(0, 4, 0, 10);

        auto card_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        card_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        header_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        header_row->set_margin(0, 0, 0, 8);

        auto header_icon = TextViewBuilder::create()
            ->text(icon_text)
            ->textSize(16)
            ->build();
        header_icon->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        header_icon->set_margin(0, 0, 8, 0);

        auto header_title = TextViewBuilder::create()
            ->text(title_text)
            ->bold(true)
            ->textSize(14)
            ->build();
        header_title->set_layout_params(LayoutParams(
            0,
            static_cast<int>(LayoutDimension::WrapContent),
            1.0f
        ));

        header_row->add_view(header_icon);
        header_row->add_view(header_title);
        card_col->add_view(header_row);

        return std::make_pair(card, card_col);
    };

    // 1. Connection Card
    auto [conn_card, conn_col] = make_section_card("MPD Server Connection", "📡");

    std::string server_str = (host.empty() ? "localhost" : host) + ":" + std::to_string(port > 0 ? port : 6600);
    auto conn_status_lbl = TextViewBuilder::create()
        ->text(client.is_connected() ? ("Status: Connected to " + server_str) : ("Status: Disconnected from " + server_str))
        ->caption(true)
        ->muted(true)
        ->build();
    conn_status_lbl->set_margin(0, 0, 0, 10);
    conn_col->add_view(conn_status_lbl);

    auto conn_btn_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    conn_btn_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto btn_reconnect = ButtonBuilder::create()
        ->text("↻ Reconnect")
        ->primary(true)
        ->textSize(12)
        ->padding(12, 6)
        ->cornerRadius(8)
        ->build();
    btn_reconnect->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    btn_reconnect->set_margin(0, 0, 8, 0);

    auto btn_update_db = ButtonBuilder::create()
        ->text("🔄 Rescan Library")
        ->flat(true)
        ->textSize(12)
        ->padding(12, 6)
        ->cornerRadius(8)
        ->build();
    btn_update_db->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    conn_btn_row->add_view(btn_reconnect);
    conn_btn_row->add_view(btn_update_db);
    conn_col->add_view(conn_btn_row);
    conn_card->add_view(conn_col);
    settings_scroll_layout->add_view(conn_card);

    // 2. Playback Modes Card
    auto [modes_card, modes_col] = make_section_card("Playback Preferences", "🎛️");

    auto make_switch_row = [](const std::string& label, const std::string& desc, bool initial_val, std::function<void(bool)> on_toggle) {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_margin(0, 4, 0, 8);

        auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        text_col->set_layout_params(LayoutParams(
            0,
            static_cast<int>(LayoutDimension::WrapContent),
            1.0f
        ));
        text_col->set_margin(0, 0, 8, 0);

        auto l_lbl = TextViewBuilder::create()->text(label)->bold(true)->build();
        auto d_lbl = TextViewBuilder::create()->text(desc)->caption(true)->muted(true)->build();
        text_col->add_view(l_lbl);
        text_col->add_view(d_lbl);

        auto sw = SwitchBuilder::create()
            ->checked(initial_val)
            ->onCheckedChanged(std::move(on_toggle))
            ->build();
        sw->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        row->add_view(text_col);
        row->add_view(sw);
        return row;
    };

    MpdStatus cur_st = client.get_status();

    modes_col->add_view(make_switch_row(
        "Consume Mode",
        "Automatically remove songs from queue after playing",
        cur_st.consume,
        [&client](bool checked) {
            client.set_consume(checked);
        }
    ));

    modes_col->add_view(make_switch_row(
        "Single Track Mode",
        "Stop playback or repeat only current track",
        cur_st.single,
        [&client](bool checked) {
            client.set_single(checked);
        }
    ));

    modes_col->add_view(make_switch_row(
        "Shuffle / Random",
        "Play queue in randomized order",
        cur_st.random,
        [&client](bool checked) {
            client.set_random(checked);
        }
    ));

    modes_col->add_view(make_switch_row(
        "Repeat Queue",
        "Loop playback continuously when queue reaches end",
        cur_st.repeat,
        [&client](bool checked) {
            client.set_repeat(checked);
        }
    ));

    modes_card->add_view(modes_col);
    settings_scroll_layout->add_view(modes_card);

    // 3. About Card
    auto [about_card, about_col] = make_section_card("About Miquland Music", "ℹ️");

    auto app_name_lbl = TextViewBuilder::create()
        ->text("miqumusic v1.0.0")
        ->bold(true)
        ->build();
    auto app_desc_lbl = TextViewBuilder::create()
        ->text("Native Wayland client for Music Player Daemon (MPD)\nBuilt with miqutoolkit & libmpdclient")
        ->caption(true)
        ->muted(true)
        ->build();
    app_desc_lbl->set_margin(0, 2, 0, 0);

    about_col->add_view(app_name_lbl);
    about_col->add_view(app_desc_lbl);
    about_card->add_view(about_col);
    settings_scroll_layout->add_view(about_card);

    // Reconnect and update db callbacks
    btn_reconnect->set_on_click_listener([&client, host, port, conn_status_lbl, server_str, on_conn_changed]() {
        bool ok = client.connect(host, port);
        conn_status_lbl->set_text(ok ? ("Status: Connected to " + server_str) : ("Status: Disconnected from " + server_str));
        if (on_conn_changed) on_conn_changed(ok);
    });

    btn_update_db->set_on_click_listener([&client, btn_update_db]() {
        bool ok = client.update_database();
        btn_update_db->set_text(ok ? "✓ Scan started" : "✗ Scan failed");
    });

    auto settings_scroll = ScrollViewBuilder::create()
        ->contentView(settings_scroll_layout)
        ->scrollbar(true)
        ->build();
    settings_scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    return settings_scroll;
}

} // namespace miqumusic
