#include "modules/applications/app_detail_view.hpp"
#include "modules/applications/apps_backend.hpp"
#include "ui/common/modal_overlay.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <algorithm>

namespace miqu {

AppDetailView::AppDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
}

void AppDetailView::set_app_id(const std::string& desktop_id) {
    m_desktop_id = desktop_id;
    load_data();
    setup_ui();
}

void AppDetailView::load_data() {
    if (m_desktop_id.empty()) return;

    // Reset any previously loaded app data before searching
    m_app = AppDetailInfo{};

    // Fast path: look up from the in-memory cached app list
    auto apps = AppsBackend::get_installed_apps();
    for (const auto& app : apps) {
        if (app.id == m_desktop_id) {
            m_app = app;
            break;
        }
    }
    if (m_app.id.empty()) {
        m_app.id = m_desktop_id;
        m_app.name = m_desktop_id;
        m_app.desktop_path = AppsBackend::resolve_desktop_path(m_desktop_id);
    }

    AppsBackend::populate_package_metadata(m_app);
}

void AppDetailView::setup_ui() {
    clear_views();
    auto config = Config::get();

    m_scroll = std::make_shared<ScrollView>();
    m_scroll->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    m_content_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    m_content_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    m_content_layout->set_divider_spacing(16);
    m_content_layout->set_padding(32);

    m_content_layout->add_view(create_hero_card());
    m_content_layout->add_view(create_actions_card());
    m_content_layout->add_view(create_info_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> AppDetailView::create_hero_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(16);

    auto icon = std::make_shared<ImageView>(m_app.icon_name.empty() ? "application-x-executable" : m_app.icon_name);
    icon->set_target_size(64);
    LayoutParams icon_lp;
    icon_lp.width = 64;
    icon_lp.height = 64;
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);
    text_col->set_divider_spacing(3);

    auto title = TextViewBuilder::create()
        ->text(m_app.name.empty() ? m_desktop_id : m_app.name)
        ->bold(true)
        ->textSize(20)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub = !m_app.generic_name.empty() ? m_app.generic_name : m_app.categories;
    if (!sub.empty()) {
        auto subtitle = TextViewBuilder::create()
            ->text(sub)
            ->textSize(12)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        text_col->add_view(subtitle);
    }

    if (!m_app.package_type.empty()) {
        auto badge = TextViewBuilder::create()
            ->text(m_app.package_type + (!m_app.version.empty() ? " · v" + m_app.version : ""))
            ->textSize(11)
            ->textColor(config->colors.primary)
            ->build();
        text_col->add_view(badge);
    }

    row->add_view(text_col);

    
    card->add_view(row);
    return card;
}

std::shared_ptr<View> AppDetailView::create_actions_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(14);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(12);

    auto btn_open = ButtonBuilder::create()
        ->text("▶  Open")
        ->textSize(13)
        ->textColor(config->colors.on_primary)
        ->backgroundColor(config->colors.primary)
        ->cornerRadius(6)
        ->onClick([this]() {
            on_launch_clicked();
        })
        ->build();
    LayoutParams open_lp;
    open_lp.width = 0;
    open_lp.height = 38;
    open_lp.weight = 1.0f;
    btn_open->set_layout_params(open_lp);
    row->add_view(btn_open);

    if (m_app.can_uninstall) {
        auto btn_uninstall = ButtonBuilder::create()
            ->text("🗑  Uninstall")
            ->textSize(13)
            ->textColor(Color(0.85f, 0.25f, 0.25f, 1.0f))
            ->backgroundColor(config->colors.surface_variant)
            ->cornerRadius(6)
            ->onClick([this]() {
                on_uninstall_clicked();
            })
            ->build();
        LayoutParams uninst_lp;
        uninst_lp.width = 0;
        uninst_lp.height = 38;
        uninst_lp.weight = 1.0f;
        btn_uninstall->set_layout_params(uninst_lp);
        row->add_view(btn_uninstall);
    }

    card->add_view(row);
    return card;
}

std::shared_ptr<View> AppDetailView::create_info_row(const std::string& label, const std::string& value) {
    auto config = Config::get();
    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(12);

    auto lbl = TextViewBuilder::create()
        ->text(label)
        ->textSize(12)
        ->textColor(config->colors.outline)
        ->build();
    LayoutParams lbl_lp;
    lbl_lp.width = 110;
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl->set_layout_params(lbl_lp);
    row->add_view(lbl);

    auto val = TextViewBuilder::create()
        ->text(value)
        ->textSize(12)
        ->textColor(config->colors.on_surface)
        ->build();
    LayoutParams val_lp;
    val_lp.width = 0;
    val_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    val_lp.weight = 1.0f;
    val->set_layout_params(val_lp);
    row->add_view(val);

    return row;
}

std::shared_ptr<View> AppDetailView::create_info_card() {
    auto config = Config::get();
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(18);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
    col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    col->set_divider_spacing(10);

    auto title = TextViewBuilder::create()
        ->text("Application Details")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    auto div = std::make_shared<DividerView>();
    div->set_color(config->colors.outline_variant);
    div->set_thickness(1);
    col->add_view(div);

    if (!m_app.version.empty()) {
        col->add_view(create_info_row("Version", m_app.version));
    }
    if (!m_app.installed_size.empty()) {
        col->add_view(create_info_row("Installed Size", m_app.installed_size));
    }
    if (!m_app.package_name.empty()) {
        col->add_view(create_info_row("Package", m_app.package_name));
    }
    if (!m_app.exec_cmd.empty()) {
        col->add_view(create_info_row("Command", m_app.exec_cmd));
    }
    if (!m_app.desktop_path.empty()) {
        col->add_view(create_info_row("Desktop Entry", m_app.desktop_path));
    }
    if (!m_app.categories.empty()) {
        col->add_view(create_info_row("Categories", m_app.categories));
    }
    if (!m_app.comment.empty()) {
        col->add_view(create_info_row("Description", m_app.comment));
    }

    card->add_view(col);
    return card;
}

void AppDetailView::on_launch_clicked() {
    AppsBackend::launch_app(m_app);
}

void AppDetailView::on_uninstall_clicked() {
    auto modal = std::make_shared<ModalOverlay>();
    modal->set_icon("dialog-warning");
    modal->set_title("Uninstall " + m_app.name + "?");
    modal->set_message("Are you sure you want to uninstall " + m_app.name +
                       (m_app.package_name.empty() ? "" : " (" + m_app.package_name + ")") +
                       "? This will remove the package from your system.");

    modal->set_confirm_button("Uninstall", [this, modal]() {
        modal->dismiss();
        execute_uninstall();
    });

    modal->set_cancel_button("Cancel", [modal]() {
        modal->dismiss();
    });

    modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
}

void AppDetailView::execute_uninstall() {
    std::string err;
    bool ok = AppsBackend::uninstall_app(m_app, err);
    if (ok) {
        if (m_on_app_uninstalled) {
            m_on_app_uninstalled(m_desktop_id);
        }
    } else {
        auto modal = std::make_shared<ModalOverlay>();
        modal->set_icon("dialog-error");
        modal->set_title("Uninstallation Failed");
        modal->set_message(err.empty() ? "An unexpected error occurred." : err);
        modal->set_cancel_button("Close", [modal]() { modal->dismiss(); });
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
    }
}

} // namespace miqu
