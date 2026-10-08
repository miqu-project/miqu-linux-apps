#include "testbed_view.hpp"
#include "ui_components.hpp"
#include <thread>

using namespace miqu;

namespace miqusecure {

static const Color COLOR_ACTIVE_GREEN(0.188f, 0.820f, 0.345f, 1.0f); // #30d158
static const Color COLOR_INACTIVE_GRAY(0.545f, 0.545f, 0.600f, 1.0f); // #8b8b99

// =============================================================================
// CONSTRUCTOR
// =============================================================================
TestbedView::TestbedView(const DistroboxInfo& info, std::function<void()> on_refresh)
    : m_info(info), m_on_refresh(std::move(on_refresh)) {
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
    setup_create_section();

    auto sec_testbeds_hdr = ui::make_section_header("CONFIGURED TESTBEDS");
    m_layout->add_view(sec_testbeds_hdr);

    m_boxes_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_boxes_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_boxes_container->set_margin(0, 0, 0, 24);
    m_layout->add_view(m_boxes_container);

    set_content_view(m_layout);
    rebuild_boxes_list();
}

// =============================================================================
// 1. STATUS HERO CARD
// =============================================================================
void TestbedView::setup_status_hero() {
    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 14)
        ->build();
    card->set_margin(0, 4, 0, 6);

    auto status_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    status_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    // 8px indicator dot
    bool ready = (m_info.distrobox_installed && m_info.podman_installed);
    m_status_dot = std::make_shared<FrameLayout>();
    m_status_dot->set_layout_params(LayoutParams(8, 8, Gravity::CenterVertical));
    m_status_dot->set_corner_radius(4);
    m_status_dot->set_margin(0, 0, 14, 0);
    update_status_indicator(ready);
    status_row->add_view(m_status_dot);

    auto info_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    info_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto title_tv = TextViewBuilder::create()
        ->text("Disposable Testing Environments")
        ->h3()
        ->bold(true)
        ->build();
    title_tv->set_margin(0, 0, 0, 3);

    std::string st_str = ready ?
        "Podman & Distrobox ready • Run GUI and CLI apps in isolated containers" :
        "Container engine needed • Install Podman and Distrobox to enable";
    m_status_lbl = TextViewBuilder::create()
        ->text(st_str)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();

    info_col->add_view(title_tv);
    info_col->add_view(m_status_lbl);
    status_row->add_view(info_col);

    card->add_view(status_row);
    m_layout->add_view(card);

    // Docked 3px indeterminate progress bar
    m_progress_bar = ProgressBarBuilder::create()
        ->style(ProgressBarStyle::Linear)
        ->indeterminate(true)
        ->trackHeight(3)
        ->build();
    m_progress_bar->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        3
    ));
    m_progress_bar->set_visibility(Visibility::Invisible);
    m_progress_bar->set_margin(0, 0, 0, 4);
    m_layout->add_view(m_progress_bar);
}

// =============================================================================
// 2. CREATE NEW TESTBED SECTION
// =============================================================================
void TestbedView::setup_create_section() {
    bool ready = (m_info.distrobox_installed && m_info.podman_installed);

    // Wrap header + card in a single container for visibility toggling
    m_create_section = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_create_section->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_create_section->set_visibility(ready ? Visibility::Visible : Visibility::Gone);

    auto sec_hdr = ui::make_section_header("NEW TESTBED");
    m_create_section->add_view(sec_hdr);

    auto card = CardViewBuilder::create()
        ->style(CardStyle::Outlined)
        ->padding(18, 12)
        ->build();
    card->set_margin(0, 0, 0, 6);

    auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    form_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    m_input_name = EditTextBuilder::create()
        ->hint("Container name (e.g. testbox)...")
        ->padding(10, 8)
        ->build();
    m_input_name->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
    m_input_name->set_margin(0, 0, 10, 0);
    form_row->add_view(m_input_name);

    // Image selector — common distrobox-supported base images
    static const std::vector<std::string> IMAGE_LABELS = {
        "Arch Linux",
        "Ubuntu",
        "Fedora",
        "Debian",
        "openSUSE",
        "Alpine"
    };
    static const std::vector<std::string> IMAGE_TAGS = {
        "archlinux:latest",
        "ubuntu:latest",
        "fedora:latest",
        "debian:latest",
        "opensuse/tumbleweed:latest",
        "alpine:latest"
    };

    m_spinner_image = SpinnerBuilder::create()
        ->items(IMAGE_LABELS)
        ->selectedIndex(0)
        ->padding(12, 6)
        ->cornerRadius(8)
        ->build();
    m_spinner_image->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    m_spinner_image->set_margin(0, 0, 10, 0);
    form_row->add_view(m_spinner_image);

    m_btn_create = ButtonBuilder::create()
        ->text("+ Create Testbed")
        ->primary(true)
        ->padding(16, 8)
        ->build();

    std::weak_ptr<Button> weak_btn = m_btn_create;
    std::weak_ptr<EditText> weak_name = m_input_name;
    std::weak_ptr<Spinner> weak_spinner = m_spinner_image;

    m_btn_create->set_on_click_listener([this, weak_btn, weak_name, weak_spinner]() {
        auto name_inp = weak_name.lock();
        auto spinner = weak_spinner.lock();
        auto btn = weak_btn.lock();
        if (!name_inp || !spinner || !btn) return;

        // Read and trim name — default to "testbox"
        std::string name = name_inp->get_text();
        if (auto f = name.find_first_not_of(" \t\n\r"); f != std::string::npos) {
            name = name.substr(f, name.find_last_not_of(" \t\n\r") - f + 1);
        } else {
            name = "testbox";
        }

        // Resolve image from spinner selection
        int idx = spinner->get_selected_index();
        std::string image = (idx >= 0 && idx < static_cast<int>(IMAGE_TAGS.size()))
            ? IMAGE_TAGS[idx] : IMAGE_TAGS[0];

        btn->set_text("Creating...");
        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);

        std::thread([this, name, image, weak_btn]() {
            SecurityBackend::create_testbed(name, image);
            if (auto engine = AppEngine::instance()) {
                engine->post([this, weak_btn]() {
                    if (auto b = weak_btn.lock()) {
                        b->set_text("+ Create Testbed");
                    }
                    if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                    if (m_on_refresh) m_on_refresh();
                });
            }
        }).detach();
    });

    form_row->add_view(m_btn_create);

    card->add_view(form_row);
    m_create_section->add_view(card);
    m_layout->add_view(m_create_section);
}

void TestbedView::update_status_indicator(bool ready) {
    if (!m_status_dot) return;
    m_status_dot->set_background_color(ready ? COLOR_ACTIVE_GREEN : COLOR_INACTIVE_GRAY);
    m_status_dot->request_redraw();
}

// =============================================================================
// UPDATE INFO (called when real data arrives or on refresh)
// =============================================================================
void TestbedView::update_info(const DistroboxInfo& info) {
    m_info = info;

    bool ready = (m_info.distrobox_installed && m_info.podman_installed);
    update_status_indicator(ready);

    if (m_status_lbl) {
        std::string st_str = ready ?
            "Podman & Distrobox ready • Run GUI and CLI apps in isolated containers" :
            "Container engine needed • Install Podman and Distrobox to enable";
        m_status_lbl->set_text(st_str);
    }

    // Show or hide the creation form based on container engine availability
    if (m_create_section) {
        m_create_section->set_visibility(ready ? Visibility::Visible : Visibility::Gone);
    }

    rebuild_boxes_list();
}

// =============================================================================
// 3. CONFIGURED TESTBEDS LIST
// =============================================================================
void TestbedView::rebuild_boxes_list() {
    if (!m_boxes_container) return;
    m_boxes_container->clear_views();

    // Not-installed state: show install guidance card
    if (!m_info.distrobox_installed || !m_info.podman_installed) {
        auto card = CardViewBuilder::create()
            ->style(CardStyle::Outlined)
            ->padding(18, 14)
            ->build();

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        auto t = TextViewBuilder::create()
            ->text("Container Tools Required")
            ->bold(true)
            ->build();
        auto d = TextViewBuilder::create()
            ->text("Distrobox and Podman enable disposable Linux environments for safe app testing. "
                   "GUI apps run seamlessly on the host display while packages remain fully isolated."
                   "\n\nTo install, run: sudo pacman -S distrobox podman")
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        d->set_margin(0, 3, 0, 0);

        col->add_view(t);
        col->add_view(d);
        card->add_view(col);
        m_boxes_container->add_view(card);
        return;
    }

    // Empty state: no boxes configured yet
    if (m_info.boxes.empty()) {
        auto card = CardViewBuilder::create()
            ->style(CardStyle::Outlined)
            ->padding(18, 14)
            ->build();

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        auto t = TextViewBuilder::create()
            ->text("No Active Testbeds")
            ->bold(true)
            ->build();
        auto msg = TextViewBuilder::create()
            ->text("Use the form above to spin up a clean container for safe app testing.")
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        msg->set_margin(0, 3, 0, 0);

        col->add_view(t);
        col->add_view(msg);
        card->add_view(col);
        m_boxes_container->add_view(card);
        return;
    }

    // Active boxes
    for (size_t i = 0; i < m_info.boxes.size(); ++i) {
        const auto& box = m_info.boxes[i];
        std::string b_name = box.name;

        auto card = CardViewBuilder::create()
            ->style(CardStyle::Outlined)
            ->padding(18, 14)
            ->build();
        card->set_margin(0, 0, 0, 12);

        auto box_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        box_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        // --- Row 1: Name + Image + Actions (Terminal, Destroy) ---
        auto row_top = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row_top->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row_top->set_margin(0, 0, 0, 8);

        auto name_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        name_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto name_tv = TextViewBuilder::create()->text(box.name)->h3()->bold(true)->build();
        std::string desc = box.image + " • " + (box.status.empty() ? "Ready" : box.status);
        auto desc_tv = TextViewBuilder::create()
            ->text(desc)
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        desc_tv->set_margin(0, 2, 0, 0);

        name_col->add_view(name_tv);
        name_col->add_view(desc_tv);
        row_top->add_view(name_col);

        auto btn_term = ButtonBuilder::create()
            ->text("Terminal")
            ->flat(true)
            ->padding(12, 6)
            ->onClick([b_name]() {
                SecurityBackend::launch_testbed_terminal(b_name);
            })
            ->build();
        btn_term->set_margin(0, 0, 8, 0);
        row_top->add_view(btn_term);

        auto btn_destroy = ButtonBuilder::create()
            ->text("Destroy")
            ->flat(true)
            ->padding(12, 6)
            ->build();
        std::weak_ptr<Button> weak_destroy = btn_destroy;
        btn_destroy->set_on_click_listener([this, b_name, weak_destroy]() {
            if (auto b = weak_destroy.lock()) {
                b->set_text("Destroying...");
            }
            if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Visible);
            std::thread([this, b_name]() {
                SecurityBackend::destroy_testbed(b_name);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this]() {
                        if (m_progress_bar) m_progress_bar->set_visibility(Visibility::Invisible);
                        if (m_on_refresh) m_on_refresh();
                    });
                }
            }).detach();
        });
        row_top->add_view(btn_destroy);
        box_col->add_view(row_top);

        box_col->add_view(DividerViewBuilder::create()->build());

        // --- Row 2: Package Installer ---
        auto form_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        form_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        form_row->set_margin(0, 10, 0, 8);

        auto input_pkg = EditTextBuilder::create()
            ->hint("Package name (e.g. gimp, mpv, vlc, htop)...")
            ->padding(10, 8)
            ->build();
        input_pkg->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));
        input_pkg->set_margin(0, 0, 10, 0);
        form_row->add_view(input_pkg);

        auto box_progress = ProgressBarBuilder::create()
            ->style(ProgressBarStyle::Linear)
            ->indeterminate(true)
            ->trackHeight(3)
            ->build();
        box_progress->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            3
        ));
        box_progress->set_visibility(Visibility::Invisible);
        box_progress->set_margin(0, 0, 0, 6);

        auto btn_install = ButtonBuilder::create()
            ->text("Install")
            ->primary(true)
            ->padding(14, 8)
            ->build();
        btn_install->set_margin(0, 0, 8, 0);

        std::weak_ptr<Button> weak_install = btn_install;
        std::weak_ptr<EditText> weak_input = input_pkg;
        std::weak_ptr<ProgressBar> weak_prog = box_progress;

        btn_install->set_on_click_listener([this, b_name, weak_install, weak_input, weak_prog]() {
            auto inp = weak_input.lock();
            auto btn = weak_install.lock();
            auto prg = weak_prog.lock();
            if (!inp || !btn) return;

            std::string pkg = inp->get_text();
            size_t first = pkg.find_first_not_of(" \t\n\r");
            if (first == std::string::npos) return;
            size_t last = pkg.find_last_not_of(" \t\n\r");
            pkg = pkg.substr(first, (last - first + 1));
            if (pkg.empty()) return;

            btn->set_text("Installing...");
            if (prg) prg->set_visibility(Visibility::Visible);

            std::thread([this, b_name, pkg, weak_install, weak_input, weak_prog]() {
                std::string err;
                bool ok = SecurityBackend::install_testbed_package(b_name, pkg, err);
                if (auto engine = AppEngine::instance()) {
                    engine->post([this, ok, weak_install, weak_input, weak_prog]() {
                        if (auto p = weak_prog.lock()) {
                            p->set_visibility(Visibility::Invisible);
                        }
                        if (auto i = weak_input.lock()) {
                            if (ok) i->set_text("");
                        }
                        if (auto b = weak_install.lock()) {
                            b->set_text(ok ? "Installed" : "Failed");
                        }
                        if (ok && m_on_refresh) {
                            m_on_refresh();
                        }
                    });
                }
            }).detach();
        });
        form_row->add_view(btn_install);

        auto btn_run = ButtonBuilder::create()
            ->text("Run")
            ->flat(true)
            ->padding(12, 8)
            ->onClick([b_name, input_pkg]() {
                std::string app = input_pkg->get_text();
                size_t first = app.find_first_not_of(" \t\n\r");
                if (first == std::string::npos) return;
                size_t last = app.find_last_not_of(" \t\n\r");
                app = app.substr(first, (last - first + 1));
                if (!app.empty()) {
                    SecurityBackend::launch_testbed_app(b_name, app);
                }
            })
            ->build();
        form_row->add_view(btn_run);

        box_col->add_view(form_row);
        box_col->add_view(box_progress);

        // --- Row 3: Installed Sandbox Apps ---
        if (!box.installed_packages.empty()) {
            box_col->add_view(DividerViewBuilder::create()->build());

            auto apps_list = std::make_shared<LinearLayout>(Orientation::Vertical);
            apps_list->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));

            for (size_t j = 0; j < box.installed_packages.size(); ++j) {
                const auto& pkg = box.installed_packages[j];
                std::string p_name = pkg.name;

                auto app_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
                app_row->set_layout_params(LayoutParams(
                    static_cast<int>(LayoutDimension::MatchParent),
                    static_cast<int>(LayoutDimension::WrapContent),
                    Gravity::CenterVertical
                ));
                app_row->set_padding(0, 8);

                auto a_col = std::make_shared<LinearLayout>(Orientation::Vertical);
                a_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

                auto a_name = TextViewBuilder::create()->text(pkg.name)->bold(true)->build();
                auto a_ver = TextViewBuilder::create()
                    ->text("Version: " + pkg.version)
                    ->caption()
                    ->muted()
                    ->multiline(true)
                    ->ellipsize(false)
                    ->build();
                a_col->add_view(a_name);
                a_col->add_view(a_ver);
                app_row->add_view(a_col);

                auto btn_launch = ButtonBuilder::create()
                    ->text("Launch GUI")
                    ->flat(true)
                    ->padding(10, 6)
                    ->onClick([b_name, p_name]() {
                        SecurityBackend::launch_testbed_app(b_name, p_name);
                    })
                    ->build();
                btn_launch->set_margin(0, 0, 6, 0);
                app_row->add_view(btn_launch);

                auto btn_uninstall = ButtonBuilder::create()
                    ->text("Remove")
                    ->flat(true)
                    ->padding(10, 6)
                    ->build();
                std::weak_ptr<Button> weak_uninst = btn_uninstall;
                btn_uninstall->set_on_click_listener([this, b_name, p_name, weak_uninst]() {
                    if (auto b = weak_uninst.lock()) {
                        b->set_text("Removing...");
                    }
                    std::thread([this, b_name, p_name]() {
                        SecurityBackend::remove_testbed_package(b_name, p_name);
                        if (auto engine = AppEngine::instance()) {
                            engine->post([this]() {
                                if (m_on_refresh) m_on_refresh();
                            });
                        }
                    }).detach();
                });
                app_row->add_view(btn_uninstall);

                apps_list->add_view(app_row);

                if (j + 1 < box.installed_packages.size()) {
                    apps_list->add_view(DividerViewBuilder::create()->build());
                }
            }
            box_col->add_view(apps_list);
        }

        card->add_view(box_col);
        m_boxes_container->add_view(card);
    }
}

} // namespace miqusecure
