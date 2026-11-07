#include "modules/keyboard/pointers_detail_view.hpp"
#include "modules/keyboard/keyboard_backend.hpp"
#include <miqutoolkit/core/config.hpp>

namespace miqu {

PointersDetailView::PointersDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}


void PointersDetailView::setup_ui() {
    clear_views();

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
    m_content_layout->add_view(create_pointers_card());
    m_content_layout->add_view(create_touch_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> PointersDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("input-mouse");
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
        ->text("Mice, Pointers & Touch Devices")
        ->bold(true)
        ->textSize(20)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    InputDevicesInfo info = KeyboardBackend::get_devices_info();
    size_t ptr_count = info.pointers.size();
    size_t touch_count = info.touch_devices.size();
    auto subtitle = TextViewBuilder::create()
        ->text(std::to_string(ptr_count) + " pointer(s) · " + std::to_string(touch_count) + " touch device(s)")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("libinput Hardware Device Enumeration ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);
    card->add_view(row);
    return card;
}

std::shared_ptr<View> PointersDetailView::create_pointers_card() {
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
        ->text("Connected Pointer Devices")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    InputDevicesInfo info = KeyboardBackend::get_devices_info();
    if (info.pointers.empty()) {
        auto empty_lbl = TextViewBuilder::create()
            ->text("No mouse or touchpad devices detected via miquctl devices")
            ->textSize(13)
            ->textColor(config->colors.outline)
            ->build();
        col->add_view(empty_lbl);
    } else {
        for (const auto& ptr : info.pointers) {
            auto item_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            item_row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));
            item_row->set_divider_spacing(10);

            auto p_icon = std::make_shared<ImageView>("input-mouse");
            p_icon->set_target_size(20);
            LayoutParams pi_lp;
            pi_lp.width = 20;
            pi_lp.height = 20;
            pi_lp.gravity = Gravity::CenterVertical;
            item_row->add_view(p_icon, pi_lp);

            auto p_name = TextViewBuilder::create()
                ->text(ptr.name)
                ->textSize(13)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams pn_lp;
            pn_lp.gravity = Gravity::CenterVertical;
            item_row->add_view(p_name, pn_lp);

            col->add_view(item_row);
        }
    }

    card->add_view(col);
    return card;
}

std::shared_ptr<View> PointersDetailView::create_touch_card() {
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
        ->text("Touchscreens & Drawing Tablets")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    InputDevicesInfo info = KeyboardBackend::get_devices_info();
    if (info.touch_devices.empty()) {
        auto empty_lbl = TextViewBuilder::create()
            ->text("No touchscreen or graphics tablets detected via miquctl devices")
            ->textSize(13)
            ->textColor(config->colors.outline)
            ->build();
        col->add_view(empty_lbl);
    } else {
        for (const auto& tch : info.touch_devices) {
            auto item_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            item_row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));
            item_row->set_divider_spacing(10);

            auto t_icon = std::make_shared<ImageView>("input-touchpad");
            t_icon->set_target_size(20);
            LayoutParams ti_lp;
            ti_lp.width = 20;
            ti_lp.height = 20;
            ti_lp.gravity = Gravity::CenterVertical;
            item_row->add_view(t_icon, ti_lp);

            auto t_name = TextViewBuilder::create()
                ->text(tch.name)
                ->textSize(13)
                ->textColor(config->colors.on_surface)
                ->build();
            LayoutParams tn_lp;
            tn_lp.gravity = Gravity::CenterVertical;
            item_row->add_view(t_name, tn_lp);

            col->add_view(item_row);
        }
    }

    card->add_view(col);
    return card;
}

} // namespace miqu
