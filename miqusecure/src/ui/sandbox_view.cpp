#include "sandbox_view.hpp"
#include "ui_components.hpp"

using namespace miqu;

namespace miqusecure {

static const Color COLOR_ACTIVE_GREEN(0.188f, 0.820f, 0.345f, 1.0f); // #30d158
static const Color COLOR_INACTIVE_GRAY(0.545f, 0.545f, 0.600f, 1.0f); // #8b8b99

SandboxView::SandboxView(const LsmInfo& info) : m_info(info) {
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

    setup_posture_hero();
    setup_defenses_section();

    m_flatpak_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_flatpak_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_flatpak_container->set_margin(0, 0, 0, 24);
    m_layout->add_view(m_flatpak_container);

    rebuild_flatpak_section();
    set_content_view(m_layout);
}

// =============================================================================
// 1. KERNEL DEFENSE POSTURE HERO INSET CARD
// =============================================================================
void SandboxView::setup_posture_hero() {
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

    // 8px indicator dot
    bool has_lsm = !m_info.active_lsms.empty();
    m_status_dot = std::make_shared<FrameLayout>();
    m_status_dot->set_layout_params(LayoutParams(8, 8, Gravity::CenterVertical));
    m_status_dot->set_corner_radius(4);
    m_status_dot->set_margin(0, 0, 14, 0);
    update_status_indicator(has_lsm);
    row->add_view(m_status_dot);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    text_col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

    auto title = TextViewBuilder::create()->text("Application Sandboxing & Kernel Defense")->h3()->bold(true)->build();
    title->set_margin(0, 0, 0, 3);

    std::string lsm_str = "";
    for (size_t i = 0; i < m_info.active_lsms.size(); ++i) {
        if (i > 0) lsm_str += ", ";
        lsm_str += m_info.active_lsms[i];
    }
    if (lsm_str.empty()) lsm_str = "None detected";

    m_stack_lbl = TextViewBuilder::create()
        ->text("Active Kernel Modules: " + lsm_str)
        ->caption()
        ->muted()
        ->multiline(true)
        ->ellipsize(false)
        ->build();

    text_col->add_view(title);
    text_col->add_view(m_stack_lbl);
    row->add_view(text_col);

    card->add_view(row);
    m_layout->add_view(card);
}

// =============================================================================
// 2. ACTIVE KERNEL DEFENSES INSET CARD
// =============================================================================
void SandboxView::setup_defenses_section() {
    auto sec_kernel_hdr = ui::make_section_header("ACTIVE KERNEL DEFENSES");
    m_layout->add_view(sec_kernel_hdr);

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

    auto make_module_row = [](const std::string& name,
                              const std::string& desc,
                              const std::string& status_txt,
                              bool is_active) {
        auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        row->set_padding(0, 10);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

        auto name_tv = TextViewBuilder::create()->text(name)->bold(true)->build();
        auto desc_tv = TextViewBuilder::create()
            ->text(desc)
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        desc_tv->set_margin(0, 2, 0, 0);

        col->add_view(name_tv);
        col->add_view(desc_tv);
        row->add_view(col);

        std::shared_ptr<TextView> out_lbl;
        auto pill = ui::make_status_pill(status_txt, out_lbl, is_active ? Color(0.188f, 0.820f, 0.345f, 0.15f) : Color::transparent());
        row->add_view(pill);

        return row;
    };

    // Landlock
    std::string landlock_status = m_info.landlock_active ? "ACTIVE" : "INACTIVE";
    box->add_view(make_module_row("Landlock LSM (Process Isolation)",
        "Allows apps to voluntarily restrict their own filesystem and network access without root privileges",
        landlock_status, m_info.landlock_active));
    box->add_view(DividerViewBuilder::create()->build());

    // YAMA
    std::string yama_status = m_info.yama_active ? "PROTECTED" : "INACTIVE";
    box->add_view(make_module_row("YAMA Memory Guard (Ptrace Protection)",
        "Restricts ptrace debugging to prevent background processes from reading passwords or injecting code",
        yama_status, m_info.yama_active));
    box->add_view(DividerViewBuilder::create()->build());

    // AppArmor
    std::string aa_status = "INACTIVE";
    if (m_info.apparmor_active) {
        aa_status = "ACTIVE (" + std::to_string(m_info.enforcing_profiles) + " PROFILES)";
    }
    box->add_view(make_module_row("AppArmor Mandatory Access Control",
        "Enforces confinement profiles restricting application paths and network capabilities",
        aa_status, m_info.apparmor_active));

    card->add_view(box);
    m_layout->add_view(card);
}

// =============================================================================
// 3. FLATPAK SANDBOXED APPS SECTION
// =============================================================================
void SandboxView::rebuild_flatpak_section() {
    if (!m_flatpak_container) return;
    m_flatpak_container->clear_views();

    auto sec_hdr = ui::make_section_header("SANDBOXED APPLICATIONS (FLATPAK)");
    m_flatpak_container->add_view(sec_hdr);

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

    if (!m_info.flatpak_installed) {
        auto empty_row = std::make_shared<LinearLayout>(Orientation::Vertical);
        empty_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        empty_row->set_padding(0, 6);

        auto t = TextViewBuilder::create()->text("Flatpak Sandboxing Engine Not Installed")->bold(true)->build();
        auto d = TextViewBuilder::create()
            ->text("Flatpak runs desktop applications inside isolated Bubblewrap containers. To install on Arch/Miqu: sudo pacman -S flatpak")
            ->caption()
            ->muted()
            ->multiline(true)
            ->ellipsize(false)
            ->build();
        d->set_margin(0, 2, 0, 0);

        empty_row->add_view(t);
        empty_row->add_view(d);
        box->add_view(empty_row);
    } else if (m_info.flatpak_apps.empty()) {
        auto empty_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        empty_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        empty_row->set_padding(0, 10);

        auto empty_tv = TextViewBuilder::create()
            ->text("No Flatpak sandboxed applications currently installed.")
            ->caption()
            ->muted()
            ->build();
        empty_row->add_view(empty_tv);
        box->add_view(empty_row);
    } else {
        for (size_t i = 0; i < m_info.flatpak_apps.size(); ++i) {
            const auto& app = m_info.flatpak_apps[i];
            auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent),
                Gravity::CenterVertical
            ));
            row->set_padding(0, 8);

            auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
            col->set_layout_params(LayoutParams(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f));

            auto name_tv = TextViewBuilder::create()->text(app.name)->bold(true)->build();
            auto id_tv = TextViewBuilder::create()->text(app.id)->caption()->muted()->build();
            id_tv->set_margin(0, 2, 0, 0);

            col->add_view(name_tv);
            col->add_view(id_tv);
            row->add_view(col);

            std::shared_ptr<TextView> out_lbl;
            auto pill = ui::make_status_pill("SANDBOXED", out_lbl, Color(0.188f, 0.820f, 0.345f, 0.15f));
            row->add_view(pill);

            box->add_view(row);

            if (i + 1 < m_info.flatpak_apps.size()) {
                box->add_view(DividerViewBuilder::create()->build());
            }
        }
    }

    card->add_view(box);
    m_flatpak_container->add_view(card);
}

void SandboxView::update_status_indicator(bool active) {
    if (!m_status_dot) return;
    m_status_dot->set_background_color(active ? COLOR_ACTIVE_GREEN : COLOR_INACTIVE_GRAY);
    m_status_dot->request_redraw();
}

void SandboxView::update_info(const LsmInfo& info) {
    m_info = info;

    bool has_lsm = !m_info.active_lsms.empty();
    update_status_indicator(has_lsm);

    if (m_stack_lbl) {
        std::string lsm_str = "";
        for (size_t i = 0; i < m_info.active_lsms.size(); ++i) {
            if (i > 0) lsm_str += ", ";
            lsm_str += m_info.active_lsms[i];
        }
        if (lsm_str.empty()) lsm_str = "None detected";
        m_stack_lbl->set_text("Active Kernel Modules: " + lsm_str);
    }

    rebuild_flatpak_section();
}

} // namespace miqusecure
