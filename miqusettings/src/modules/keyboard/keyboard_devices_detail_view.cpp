#include "modules/keyboard/keyboard_devices_detail_view.hpp"
#include "modules/keyboard/keyboard_backend.hpp"
#include <miqutoolkit/core/config.hpp>

namespace miqu {

KeyboardDevicesDetailView::KeyboardDevicesDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void KeyboardDevicesDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void KeyboardDevicesDetailView::setup_ui() {
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
    m_content_layout->set_padding(m_is_fullscreen ? 32 : 16);

    m_content_layout->add_view(create_hero_card());
    m_content_layout->add_view(create_keyboards_list_card());
    m_content_layout->add_view(create_test_input_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> KeyboardDevicesDetailView::create_hero_card() {
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

    auto icon = std::make_shared<ImageView>("input-keyboard");
    icon->set_target_size(m_is_fullscreen ? 64 : 48);
    LayoutParams icon_lp;
    icon_lp.width = m_is_fullscreen ? 64 : 48;
    icon_lp.height = m_is_fullscreen ? 64 : 48;
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
        ->text("Keyboards & Keymap")
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    InputDevicesInfo info = KeyboardBackend::get_devices_info();
    size_t count = info.keyboards.size();
    auto subtitle = TextViewBuilder::create()
        ->text(std::to_string(count) + " hardware keyboard" + (count == 1 ? "" : "s") + " detected")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("wlroots libinput Hardware Device Introspection ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);
    card->add_view(row);
    return card;
}

std::shared_ptr<View> KeyboardDevicesDetailView::create_keyboards_list_card() {
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
    col->set_divider_spacing(12);

    auto title = TextViewBuilder::create()
        ->text("Detected Keyboards")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    InputDevicesInfo info = KeyboardBackend::get_devices_info();
    if (info.keyboards.empty()) {
        auto empty_lbl = TextViewBuilder::create()
            ->text("No physical keyboards detected via miquctl devices")
            ->textSize(13)
            ->textColor(config->colors.outline)
            ->build();
        col->add_view(empty_lbl);
    } else {
        for (const auto& kb : info.keyboards) {
            auto kb_card = std::make_shared<CardView>();
            kb_card->set_card_background_color(config->colors.surface_variant);
            kb_card->set_radius(std::max(4, config->metrics.corner_radius - 4));
            kb_card->set_padding(12);
            kb_card->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));

            auto kb_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
            kb_row->set_layout_params(LayoutParams(
                static_cast<int>(LayoutDimension::MatchParent),
                static_cast<int>(LayoutDimension::WrapContent)
            ));
            kb_row->set_divider_spacing(12);

            auto kb_icon = std::make_shared<ImageView>("input-keyboard");
            kb_icon->set_target_size(24);
            LayoutParams ki_lp;
            ki_lp.width = 24;
            ki_lp.height = 24;
            ki_lp.gravity = Gravity::CenterVertical;
            kb_row->add_view(kb_icon, ki_lp);

            auto kb_info_col = std::make_shared<LinearLayout>(Orientation::Vertical);
            LayoutParams kic_lp;
            kic_lp.width = 0;
            kic_lp.height = static_cast<int>(LayoutDimension::WrapContent);
            kic_lp.weight = 1.0f;
            kb_info_col->set_layout_params(kic_lp);

            auto kb_name = TextViewBuilder::create()
                ->text(kb.name)
                ->bold(true)
                ->textSize(13)
                ->textColor(config->colors.on_surface)
                ->build();
            kb_info_col->add_view(kb_name);

            auto kb_sub = TextViewBuilder::create()
                ->text("Active Keymap: " + kb.layout + " · Repeat: " + std::to_string(kb.repeat_rate) + "/s (" + std::to_string(kb.repeat_delay) + "ms delay)")
                ->textSize(11)
                ->textColor(config->colors.on_surface_variant)
                ->build();
            kb_info_col->add_view(kb_sub);

            kb_row->add_view(kb_info_col);
            kb_card->add_view(kb_row);
            col->add_view(kb_card);
        }
    }

    card->add_view(col);
    return card;
}

std::shared_ptr<View> KeyboardDevicesDetailView::create_test_input_card() {
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
    col->set_divider_spacing(8);

    auto title = TextViewBuilder::create()
        ->text("Active Keymap Layout Selection")
        ->bold(true)
        ->textSize(14)
        ->textColor(config->colors.on_surface)
        ->build();
    col->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text("Switch default XKB keymap layout across all applications:")
        ->textSize(12)
        ->textColor(config->colors.outline)
        ->build();
    col->add_view(desc);

    auto btn_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    btn_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    btn_row->set_divider_spacing(8);

    std::vector<std::string> layouts = {"us", "gb", "de", "fr", "es", "ru", "jp"};
    for (const auto& l : layouts) {
        auto btn = std::make_shared<Button>();
        btn->set_text(l);
        btn->set_style(ButtonStyle::Standard);
        btn->set_on_click_listener([l]() {
            KeyboardBackend::set_keyboard_layout(l);
        });
        btn_row->add_view(btn);
    }
    col->add_view(btn_row);

    card->add_view(col);
    return card;
}

} // namespace miqu
