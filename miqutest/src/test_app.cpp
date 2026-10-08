#include "test_app.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>

using namespace miqu;

namespace miqutest {

TestApp::TestApp(std::shared_ptr<AppEngine> engine)
    : m_engine(std::move(engine)) {}

bool TestApp::init() {
    if (!m_engine) return false;
    setup_ui();
    return m_window != nullptr;
}

int TestApp::run() {
    if (!m_engine) return 1;
    return m_engine->enter_loop();
}

void TestApp::log_activity(const std::string& msg) {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", tm);

    std::string line = "[" + std::string(buf) + "] " + msg;
    std::cout << line << std::endl;

    if (m_status_text) {
        m_status_text->set_text(line);
    }
    if (m_window) {
        m_window->schedule_redraw();
    }
}

void TestApp::setup_ui() {
    // 1. Root Vertical Container
    auto root = std::make_shared<LinearLayout>(Orientation::Vertical);
    root->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    root->set_padding(24, 20);
    root->set_divider_spacing(18);

    // =========================================================================
    // 2. Header Bar (Title + Subtitle + 3-Dot Overflow Menu)
    // =========================================================================
    auto header = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header->set_gravity(Gravity::CenterVertical);

    auto title_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams title_params(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    );
    title_params.weight = 1.0f;
    title_col->set_layout_params(title_params);

    auto title = TextViewBuilder::create()
        ->text("Action Menu & Popup Showcase")
        ->textSize(18)
        ->bold(true)
        ->build();
    title_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Interactive testing for 3-dot menus, checkables, radios, and gravities")
        ->textSize(12)
        ->muted(true)
        ->build();
    title_col->add_view(subtitle);
    header->add_view(title_col);

    // Header 3-Dot Overflow Button
    auto header_dots_btn = std::make_shared<ImageButton>("view-more-symbolic");
    header_dots_btn->set_circle(true);
    header_dots_btn->set_icon_size(18);
    header_dots_btn->set_padding(8);
    header_dots_btn->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Build Header Overflow Menu
    m_header_menu = PopupMenuBuilder::create()
        ->section("Application")
        ->item("Preferences", "preferences-system-symbolic", "Ctrl+,", [this]() {
            log_activity("Header: 'Preferences' triggered");
        })
        ->item("Keyboard Shortcuts", "input-keyboard-symbolic", [this]() {
            log_activity("Header: 'Keyboard Shortcuts' triggered");
        })
        ->separator()
        ->section("View Options")
        ->checkable("Dark Theme Preview", true, [this](bool checked) {
            log_activity(std::string("Header: 'Dark Theme' toggled -> ") + (checked ? "ON" : "OFF"));
        })
        ->checkable("Show Grid Overlay", false, [this](bool checked) {
            log_activity(std::string("Header: 'Grid Overlay' toggled -> ") + (checked ? "ON" : "OFF"));
        })
        ->separator()
        ->section("Density")
        ->radio("Compact", 1, false, [this](bool checked) {
            if (checked) log_activity("Header: Density set to Compact");
        })
        ->radio("Normal", 1, true, [this](bool checked) {
            if (checked) log_activity("Header: Density set to Normal");
        })
        ->radio("Comfortable", 1, false, [this](bool checked) {
            if (checked) log_activity("Header: Density set to Comfortable");
        })
        ->separator()
        ->destructive("Purge Cache", "user-trash-symbolic", [this]() {
            log_activity("Header Destructive: 'Purge Cache' executed");
        })
        ->item("Quit", "application-exit-symbolic", "Ctrl+Q", [this]() {
            log_activity("Header: 'Quit' triggered");
            if (m_engine) m_engine->quit();
        })
        ->build();

    header_dots_btn->set_on_click_listener([this, header_dots_btn]() {
        m_header_menu->show_as_dropdown(header_dots_btn, PopupGravity::BottomEnd);
    });

    header->add_view(header_dots_btn);
    root->add_view(header);

    // Subtle divider
    root->add_view(DividerViewBuilder::create()->thickness(1)->build());

    // =========================================================================
    // 3. Section 1: Item Cards with 3-Dot Overflow Menus
    // =========================================================================
    auto sec1_label = TextViewBuilder::create()
        ->text("Item Cards with Contextual 3-Dot Overflow Menus")
        ->textSize(13)
        ->bold(true)
        ->build();
    root->add_view(sec1_label);

    auto cards_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    cards_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    cards_row->set_divider_spacing(16);

    // --- Card 1: Project Document ---
    auto card1 = CardViewBuilder::create()
        ->bordered(true)
        ->cornerRadius(10)
        ->padding(14, 12)
        ->build();
    LayoutParams c1_params(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    );
    c1_params.weight = 1.0f;
    card1->set_layout_params(c1_params);

    auto card1_layout = std::make_shared<LinearLayout>(Orientation::Horizontal);
    card1_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    card1_layout->set_gravity(Gravity::CenterVertical);

    auto c1_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams c1_text_params(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    );
    c1_text_params.weight = 1.0f;
    c1_text_col->set_layout_params(c1_text_params);

    auto c1_title = TextViewBuilder::create()
        ->text("Q3_Financial_Plan.pdf")
        ->textSize(13)
        ->bold(true)
        ->build();
    c1_text_col->add_view(c1_title);

    auto c1_sub = TextViewBuilder::create()
        ->text("PDF • 3.2 MB • Modified 10m ago")
        ->textSize(11)
        ->muted(true)
        ->build();
    c1_text_col->add_view(c1_sub);
    card1_layout->add_view(c1_text_col);

    auto c1_dots_btn = std::make_shared<ImageButton>("view-more-symbolic");
    c1_dots_btn->set_circle(true);
    c1_dots_btn->set_icon_size(16);
    c1_dots_btn->set_padding(6);
    c1_dots_btn->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    m_doc_menu = PopupMenuBuilder::create()
        ->item("Open Document", "document-open-symbolic", [this]() {
            log_activity("Card 1: 'Open Document' clicked");
        })
        ->item("Edit Properties", "document-edit-symbolic", [this]() {
            log_activity("Card 1: 'Edit Properties' clicked");
        })
        ->item("Duplicate", "edit-copy-symbolic", "Ctrl+D", [this]() {
            log_activity("Card 1: 'Duplicate' clicked");
        })
        ->separator()
        ->checkable("Star as Favorite", true, [this](bool checked) {
            log_activity(std::string("Card 1: Star Favorite -> ") + (checked ? "ON" : "OFF"));
        })
        ->checkable("Offline Sync", false, [this](bool checked) {
            log_activity(std::string("Card 1: Offline Sync -> ") + (checked ? "ON" : "OFF"));
        })
        ->separator()
        ->destructive("Move to Trash", "user-trash-symbolic", [this]() {
            log_activity("Card 1 Destructive: 'Move to Trash' clicked");
        })
        ->build();

    c1_dots_btn->set_on_click_listener([this, c1_dots_btn]() {
        m_doc_menu->show_as_dropdown(c1_dots_btn, PopupGravity::BottomEnd);
    });
    card1_layout->add_view(c1_dots_btn);
    card1->add_view(card1_layout);
    cards_row->add_view(card1);

    // --- Card 2: Backup Archive ---
    auto card2 = CardViewBuilder::create()
        ->bordered(true)
        ->cornerRadius(10)
        ->padding(14, 12)
        ->build();
    LayoutParams c2_params(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    );
    c2_params.weight = 1.0f;
    card2->set_layout_params(c2_params);

    auto card2_layout = std::make_shared<LinearLayout>(Orientation::Horizontal);
    card2_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    card2_layout->set_gravity(Gravity::CenterVertical);

    auto c2_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams c2_text_params(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    );
    c2_text_params.weight = 1.0f;
    c2_text_col->set_layout_params(c2_text_params);

    auto c2_title = TextViewBuilder::create()
        ->text("system_backup_2026.tar.gz")
        ->textSize(13)
        ->bold(true)
        ->build();
    c2_text_col->add_view(c2_title);

    auto c2_sub = TextViewBuilder::create()
        ->text("Archive • 420 MB • Created today")
        ->textSize(11)
        ->muted(true)
        ->build();
    c2_text_col->add_view(c2_sub);
    card2_layout->add_view(c2_text_col);

    auto c2_dots_btn = std::make_shared<ImageButton>("view-more-symbolic");
    c2_dots_btn->set_circle(true);
    c2_dots_btn->set_icon_size(16);
    c2_dots_btn->set_padding(6);
    c2_dots_btn->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    m_archive_menu = PopupMenuBuilder::create()
        ->item("Extract Archive", "package-x-generic-symbolic", [this]() {
            log_activity("Card 2: 'Extract Archive' clicked");
        })
        ->item("Verify Checksum", "dialog-information-symbolic", [this]() {
            log_activity("Card 2: 'Verify Checksum' clicked");
        })
        ->item("Share File", "emblem-shared-symbolic", [this]() {
            log_activity("Card 2: 'Share File' clicked");
        })
        ->separator()
        ->destructive("Delete Permanently", "user-trash-symbolic", [this]() {
            log_activity("Card 2 Destructive: 'Delete Permanently' clicked");
        })
        ->build();

    c2_dots_btn->set_on_click_listener([this, c2_dots_btn]() {
        m_archive_menu->show_as_dropdown(c2_dots_btn, PopupGravity::BottomEnd);
    });
    card2_layout->add_view(c2_dots_btn);
    card2->add_view(card2_layout);
    cards_row->add_view(card2);

    root->add_view(cards_row);

    // =========================================================================
    // 4. Section 2: Dropdown Buttons & Multi-Axis Gravity Tests
    // =========================================================================
    auto sec2_label = TextViewBuilder::create()
        ->text("Dropdown Menus & Multi-Axis Placement Gravity")
        ->textSize(13)
        ->bold(true)
        ->build();
    root->add_view(sec2_label);

    auto btn_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    btn_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    btn_row->set_divider_spacing(12);

    // Button 1: "Actions ▾" (Primary style, BottomStart gravity)
    auto actions_btn = ButtonBuilder::create()
        ->text("Actions ▾")
        ->primary(true)
        ->padding(14, 8)
        ->cornerRadius(8)
        ->build();

    m_actions_menu = PopupMenuBuilder::create()
        ->section("Quick Actions")
        ->item("New Document", "document-new-symbolic", "Ctrl+N", [this]() {
            log_activity("Actions Menu: 'New Document' clicked");
        })
        ->item("New Folder", "folder-new-symbolic", [this]() {
            log_activity("Actions Menu: 'New Folder' clicked");
        })
        ->separator()
        ->item("Import from Device...", "document-open-symbolic", [this]() {
            log_activity("Actions Menu: 'Import' clicked");
        })
        ->item("Export All Projects...", "document-save-symbolic", [this]() {
            log_activity("Actions Menu: 'Export All' clicked");
        })
        ->build();

    actions_btn->set_on_click_listener([this, actions_btn]() {
        m_actions_menu->show_as_dropdown(actions_btn, PopupGravity::BottomStart);
    });
    btn_row->add_view(actions_btn);

    // Button 2: "Export As ▾" (Standard style, BottomEnd gravity)
    auto export_btn = ButtonBuilder::create()
        ->text("Export As ▾")
        ->padding(14, 8)
        ->cornerRadius(8)
        ->build();

    m_export_menu = PopupMenuBuilder::create()
        ->item("Portable Document (.pdf)", "x-office-document-symbolic", [this]() {
            log_activity("Export Menu: Selected PDF");
        })
        ->item("Vector Graphic (.svg)", "image-x-generic-symbolic", [this]() {
            log_activity("Export Menu: Selected SVG");
        })
        ->item("Markdown (.md)", "text-x-generic-symbolic", [this]() {
            log_activity("Export Menu: Selected Markdown");
        })
        ->separator()
        ->checkable("High-Resolution 2x", true, [this](bool checked) {
            log_activity(std::string("Export Menu: High-Res 2x -> ") + (checked ? "ON" : "OFF"));
        })
        ->build();

    export_btn->set_on_click_listener([this, export_btn]() {
        m_export_menu->show_as_dropdown(export_btn, PopupGravity::BottomEnd);
    });
    btn_row->add_view(export_btn);

    // Button 3: "Popup Above ▴" (Outlined style, TopStart gravity)
    auto above_btn = ButtonBuilder::create()
        ->text("Popup Above ▴")
        ->outlined(true)
        ->padding(14, 8)
        ->cornerRadius(8)
        ->build();

    m_above_menu = PopupMenuBuilder::create()
        ->section("Top-Anchored Menu")
        ->item("Gravity: TopStart", [this]() {
            log_activity("Above Menu: TopStart item selected");
        })
        ->item("Flipped Upward", [this]() {
            log_activity("Above Menu: Flipped Upward selected");
        })
        ->separator()
        ->item("Auto-clamp Test", [this]() {
            log_activity("Above Menu: Auto-clamp Test selected");
        })
        ->build();

    above_btn->set_on_click_listener([this, above_btn]() {
        m_above_menu->show_as_dropdown(above_btn, PopupGravity::TopStart);
    });
    btn_row->add_view(above_btn);

    // Spinner: Dropdown Spinner (powered by PopupMenu under the hood)
    m_test_spinner = SpinnerBuilder::create()
        ->prompt("Select Theme")
        ->items({"Dynamic Material You", "Nord Dark", "Catppuccin Mocha", "Solarized Light"})
        ->cornerRadius(8)
        ->padding(12, 8)
        ->onItemSelected([this](int idx, const std::string& item) {
            log_activity("Spinner Selection: [" + std::to_string(idx) + "] " + item);
        })
        ->build();
    m_test_spinner->set_layout_params(LayoutParams(
        180,
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    btn_row->add_view(m_test_spinner);

    root->add_view(btn_row);

    // =========================================================================
    // 5. Section 3: Live Activity Console (Bottom Card)
    // =========================================================================
    auto console_card = CardViewBuilder::create()
        ->bordered(true)
        ->cornerRadius(10)
        ->padding(16, 12)
        ->build();
    console_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto console_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    console_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    console_layout->set_divider_spacing(6);

    auto console_header = std::make_shared<LinearLayout>(Orientation::Horizontal);
    console_header->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    console_header->set_gravity(Gravity::CenterVertical);

    auto console_title = TextViewBuilder::create()
        ->text("LIVE ACTIVITY CONSOLE")
        ->textSize(11)
        ->bold(true)
        ->muted(true)
        ->build();
    LayoutParams c_title_params(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    );
    c_title_params.weight = 1.0f;
    console_title->set_layout_params(c_title_params);
    console_header->add_view(console_title);

    auto clear_btn = ButtonBuilder::create()
        ->text("Clear")
        ->flat(true)
        ->padding(8, 4)
        ->textSize(11)
        ->onClick([this]() {
            if (m_status_text) {
                m_status_text->set_text("[Console cleared]");
            }
            if (m_window) m_window->schedule_redraw();
        })
        ->build();
    console_header->add_view(clear_btn);
    console_layout->add_view(console_header);

    m_status_text = TextViewBuilder::create()
        ->text("Ready. Click any 3-dot overflow menu or dropdown button to test.")
        ->textSize(12)
        ->build();
    console_layout->add_view(m_status_text);

    console_card->add_view(console_layout);
    root->add_view(console_card);

    // =========================================================================
    // 6. Build Window
    // =========================================================================
    m_window = WindowBuilder::create()
        ->title("MiQuToolkit - Action Menu & Popup Showcase")
        ->appId("miqutest")
        ->role(WindowRole::Toplevel)
        ->preferredSize(820, 560)
        ->closeOnEscape(true)
        ->contentView(root)
        ->onClose([this]() {
            if (m_engine) m_engine->quit();
        })
        ->build();

    if (m_window) {
        m_window->show();
    }
}

} // namespace miqutest
