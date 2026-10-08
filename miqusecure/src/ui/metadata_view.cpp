#include "metadata_view.hpp"
#include "ui_components.hpp"
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;
using namespace miqu;

namespace miqusecure {

static const Color COLOR_ACTIVE_GREEN(0.188f, 0.820f, 0.345f, 1.0f); // #30d158
static const Color COLOR_INACTIVE_GRAY(0.545f, 0.545f, 0.600f, 1.0f); // #8b8b99

CleanerView::CleanerView(const CleanerInfo& info, std::function<void()> on_changed)
    : m_info(info), m_on_changed(std::move(on_changed)) {
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

    setup_status_hero();
    setup_caches_section();
    setup_sanitizer_section();

    set_content_view(m_layout);
}

// =============================================================================
// 1. STATUS HERO INSET CARD
// =============================================================================
void CleanerView::setup_status_hero() {
    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 14)
        ->build();
    card->set_margin(0, 4, 0, 6);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    // 8px indicator dot
    m_status_dot = std::make_shared<FrameLayout>();
    m_status_dot->set_layout_params(LayoutParams(8, 8, Gravity::CenterVertical));
    m_status_dot->set_corner_radius(4);
    m_status_dot->set_margin(0, 0, 14, 0);
    update_status_indicator(m_info.mat2_installed);
    row->add_view(m_status_dot);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    m_status_lbl = TextViewBuilder::create()
        ->text("System Caches & Metadata Sanitizer")
        ->h3()
        ->bold(true)
        ->build();
    m_status_lbl->set_margin(0, 0, 0, 3);

    std::string badge_text = m_info.mat2_installed ?
        ("mat2 engine active" + (m_info.version.empty() ? "" : (" (" + m_info.version + ")")) + " • Privacy scrubbing ready") :
        "mat2 engine not installed (metadata stripping unavailable)";

    m_status_badge = TextViewBuilder::create()
        ->text(badge_text)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();

    text_col->add_view(m_status_lbl);
    text_col->add_view(m_status_badge);
    row->add_view(text_col);

    auto btn_clean_all = ButtonBuilder::create()
        ->text("Clean All Caches")
        ->primary(true)
        ->padding(16, 8)
        ->onClick([this]() {
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            std::thread([this]() {
                std::string log;
                SecurityBackend::clean_all_caches(log);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, log]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (m_result_lbl) m_result_lbl->set_text("✔ " + log);
                        if (m_on_changed) m_on_changed();
                    });
                }
            }).detach();
        })
        ->build();
    btn_clean_all->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    row->add_view(btn_clean_all);
    card->add_view(row);
    m_layout->add_view(card);

    // Docked 3px progress bar
    m_progress_bar = ProgressBarBuilder::create()
        ->style(ProgressBarStyle::Linear)
        ->indeterminate(true)
        ->trackHeight(3)
        ->build();
    m_progress_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        3
    ));
    m_progress_bar->set_margin(0, 0, 0, 10);
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_layout->add_view(m_progress_bar);
}

// =============================================================================
// 2. SYSTEM & APPLICATION CACHES INSET CARD
// =============================================================================
void CleanerView::setup_caches_section() {
    auto sec_hdr = ui::make_section_header("SYSTEM & APPLICATION CACHES");
    m_layout->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 6)
        ->build();
    card->set_margin(0, 0, 0, 10);

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto make_cache_row = [this](const std::string& title,
                                 const std::string& desc,
                                 const std::string& initial_size,
                                 std::shared_ptr<TextView>& out_size_lbl,
                                 std::function<bool(std::string&)> clean_action) {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_padding(0, 10);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto title_r = std::make_shared<LinearLayout>(Orientation::Horizontal);
        title_r->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        auto title_tv = TextViewBuilder::create()->text(title)->bold(true)->build();
        title_tv->set_margin(0, 0, 8, 0);
        title_r->add_view(title_tv);

        auto pill = ui::make_status_pill(initial_size, out_size_lbl);
        title_r->add_view(pill);
        col->add_view(title_r);

        auto desc_tv = TextViewBuilder::create()
            ->text(desc)
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        desc_tv->set_margin(0, 2, 0, 0);
        col->add_view(desc_tv);
        row->add_view(col);

        auto clean_btn = ButtonBuilder::create()
            ->text("Clean")
            ->flat(true)
            ->padding(12, 6)
            ->onClick([this, clean_action]() {
                if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
                std::thread([this, clean_action]() {
                    std::string log;
                    bool ok = clean_action(log);
                    if (auto engine = AppEngine::instance()) {
                        engine->post([this, ok, log]() {
                            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                            if (m_result_lbl) m_result_lbl->set_text(ok ? ("✔ " + log) : ("Error: " + log));
                            if (m_on_changed) m_on_changed();
                        });
                    }
                }).detach();
            })
            ->build();
        clean_btn->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->add_view(clean_btn);

        return row;
    };

    // 1. Thumbnails
    box->add_view(make_cache_row("Thumbnail Previews",
        "Purge generated thumbnail files from image and video folders",
        m_info.thumbnails_size, m_thumb_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_thumbnails(log); }));
    box->add_view(DividerViewBuilder::create()->build());

    // 2. Browser Caches
    box->add_view(make_cache_row("Web Browser Caches",
        "Purge Firefox, Chrome and Chromium temporary web assets and caches",
        m_info.browser_cache_size, m_browser_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_browser_caches(log); }));
    box->add_view(DividerViewBuilder::create()->build());

    // 3. Trash & Temp
    box->add_view(make_cache_row("Trash Bin & Temporary Files",
        "Empty user trash files and GPU shader cache folders",
        m_info.trash_size, m_trash_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_trash_and_temp(log); }));
    box->add_view(DividerViewBuilder::create()->build());

    // 4. Shell History
    box->add_view(make_cache_row("Terminal Command Histories",
        "Clear recorded shell history files (~/.bash_history, ~/.zsh_history)",
        m_info.bash_history_size, m_history_size_lbl,
        [](std::string& log) { return SecurityBackend::clean_shell_history(log); }));

    card->add_view(box);
    m_layout->add_view(card);
}

// =============================================================================
// 3. DOCUMENT & MEDIA SANITIZER (MAT2)
// =============================================================================
void CleanerView::setup_sanitizer_section() {
    auto sec_hdr = ui::make_section_header("DOCUMENT & MEDIA SANITIZER (MAT2)");
    m_layout->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 14)
        ->build();
    card->set_margin(0, 0, 0, 24);

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto m_title = TextViewBuilder::create()->text("Sanitize File Metadata")->bold(true)->build();
    auto m_desc = TextViewBuilder::create()
        ->text("Strips hidden GPS coordinates, camera serial numbers, author tags, and edit history from images, PDFs, and Office documents.")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    m_desc->set_margin(0, 3, 0, 12);
    box->add_view(m_title);
    box->add_view(m_desc);

    // File path picker/input row
    auto file_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    file_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    file_row->set_margin(0, 0, 0, 8);

    m_path_input = EditTextBuilder::create()
        ->hint("Absolute path to file (e.g. /home/user/photo.jpg)")
        ->padding(12, 8)
        ->build();
    m_path_input->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    m_path_input->set_margin(0, 0, 12, 0);
    file_row->add_view(m_path_input);

    auto btn_clean_file = ButtonBuilder::create()
        ->text("Strip Metadata")
        ->primary(true)
        ->padding(16, 8)
        ->onClick([this]() {
            std::string path = m_path_input->get_text();
            if (!path.empty()) {
                clean_file(path);
            }
        })
        ->build();
    btn_clean_file->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    file_row->add_view(btn_clean_file);
    box->add_view(file_row);

    m_result_lbl = TextViewBuilder::create()
        ->text("")
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();
    box->add_view(m_result_lbl);

    card->add_view(box);
    m_layout->add_view(card);
}

void CleanerView::clean_file(const std::string& path) {
    if (!m_info.mat2_installed) {
        if (m_result_lbl) m_result_lbl->set_text("Error: mat2 is not installed. Run: sudo pacman -S mat2");
        return;
    }

    if (path.empty()) {
        if (m_result_lbl) m_result_lbl->set_text("Please enter a valid file path.");
        return;
    }

    if (!fs::exists(path)) {
        if (m_result_lbl) m_result_lbl->set_text("Error: File does not exist: " + path);
        return;
    }

    if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
    if (m_result_lbl) m_result_lbl->set_text("Scrubbing metadata with mat2...");

    std::thread([this, path]() {
        std::string log;
        bool ok = SecurityBackend::clean_metadata(path, log);
        if (auto engine = AppEngine::instance()) {
            engine->post([this, ok, log]() {
                if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                if (m_result_lbl) {
                    m_result_lbl->set_text(ok ? ("✔ Metadata scrubbed successfully: " + log) : ("Error: " + log));
                }
                if (ok && m_path_input) m_path_input->set_text("");
                if (m_on_changed) m_on_changed();
            });
        }
    }).detach();
}

void CleanerView::update_status_indicator(bool ready) {
    if (!m_status_dot) return;
    m_status_dot->set_background_color(ready ? COLOR_ACTIVE_GREEN : COLOR_INACTIVE_GRAY);
    m_status_dot->request_redraw();
}

void CleanerView::update_info(const CleanerInfo& info) {
    m_info = info;

    update_status_indicator(m_info.mat2_installed);

    if (m_status_badge) {
        std::string badge_text = m_info.mat2_installed ?
            ("mat2 engine active" + (m_info.version.empty() ? "" : (" (" + m_info.version + ")")) + " • Privacy scrubbing ready") :
            "mat2 engine not installed (metadata stripping unavailable)";
        m_status_badge->set_text(badge_text);
    }

    if (m_thumb_size_lbl) m_thumb_size_lbl->set_text(m_info.thumbnails_size);
    if (m_browser_size_lbl) m_browser_size_lbl->set_text(m_info.browser_cache_size);
    if (m_trash_size_lbl) m_trash_size_lbl->set_text(m_info.trash_size);
    if (m_history_size_lbl) m_history_size_lbl->set_text(m_info.bash_history_size);
}

} // namespace miqusecure
