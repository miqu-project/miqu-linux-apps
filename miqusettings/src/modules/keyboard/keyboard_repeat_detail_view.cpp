#include "modules/keyboard/keyboard_repeat_detail_view.hpp"
#include "modules/keyboard/keyboard_backend.hpp"
#include <miqutoolkit/core/config.hpp>
#include <algorithm>

namespace miqu {

KeyboardRepeatDetailView::KeyboardRepeatDetailView() {
    InputDevicesInfo info = KeyboardBackend::get_devices_info();
    if (!info.keyboards.empty()) {
        m_repeat_rate = info.keyboards[0].repeat_rate;
        m_repeat_delay = info.keyboards[0].repeat_delay;
    }

    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    setup_ui();
}

void KeyboardRepeatDetailView::set_is_fullscreen(bool fullscreen) {
    if (m_is_fullscreen != fullscreen) {
        m_is_fullscreen = fullscreen;
        setup_ui();
    }
}

void KeyboardRepeatDetailView::setup_ui() {
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
    m_content_layout->add_view(create_repeat_delay_card());
    m_content_layout->add_view(create_repeat_rate_card());

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);
}

std::shared_ptr<View> KeyboardRepeatDetailView::create_hero_card() {
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
        ->text("Typing & Repeat Speed")
        ->bold(true)
        ->textSize(m_is_fullscreen ? 20 : 17)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto subtitle = TextViewBuilder::create()
        ->text("Delay: " + std::to_string(m_repeat_delay) + "ms · Rate: " + std::to_string(m_repeat_rate) + " characters/second")
        ->textSize(12)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(subtitle);

    auto badge = TextViewBuilder::create()
        ->text("Wayland Seat Auto-Repeat Binding ✓")
        ->textSize(11)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(badge);

    row->add_view(text_col);
    card->add_view(row);
    return card;
}

std::shared_ptr<View> KeyboardRepeatDetailView::create_repeat_delay_card() {
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

    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto title = TextViewBuilder::create()
        ->text("INITIAL REPEAT DELAY (HOLD DURATION)")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    LayoutParams t_lp;
    t_lp.width = 0;
    t_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    t_lp.weight = 1.0f;
    title->set_layout_params(t_lp);
    header_row->add_view(title);

    auto val_lbl = TextViewBuilder::create()
        ->text(std::to_string(m_repeat_delay) + " ms")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->build();
    header_row->add_view(val_lbl);
    col->add_view(header_row);

    // Range: 150ms to 800ms
    float slider_val = std::clamp((m_repeat_delay - 150) / 650.0f, 0.0f, 1.0f);
    auto slider = SliderBuilder::create()
        ->value(slider_val)
        ->onValueChanged([this, val_lbl](float val, bool) {
            m_repeat_delay = 150 + static_cast<int>(val * 650.0f + 0.5f);
            val_lbl->set_text(std::to_string(m_repeat_delay) + " ms");
            val_lbl->request_redraw();
            KeyboardBackend::set_repeat_info(m_repeat_rate, m_repeat_delay);
        })
        ->build();
    LayoutParams sl_lp;
    sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sl_lp.height = 30;
    slider->set_layout_params(sl_lp);
    col->add_view(slider);

    auto labels_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    labels_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    auto lbl_short = TextViewBuilder::create()->text("Short (150ms)")->textSize(11)->textColor(config->colors.outline)->build();
    LayoutParams ls_lp;
    ls_lp.width = 0;
    ls_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    ls_lp.weight = 1.0f;
    lbl_short->set_layout_params(ls_lp);
    labels_row->add_view(lbl_short);

    auto lbl_long = TextViewBuilder::create()->text("Long (800ms)")->textSize(11)->textColor(config->colors.outline)->build();
    labels_row->add_view(lbl_long);
    col->add_view(labels_row);

    card->add_view(col);
    return card;
}

std::shared_ptr<View> KeyboardRepeatDetailView::create_repeat_rate_card() {
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

    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto title = TextViewBuilder::create()
        ->text("AUTO-REPEAT SPEED (CHARACTERS PER SECOND)")
        ->bold(true)
        ->textSize(11)
        ->textColor(config->colors.outline)
        ->build();
    LayoutParams t_lp;
    t_lp.width = 0;
    t_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    t_lp.weight = 1.0f;
    title->set_layout_params(t_lp);
    header_row->add_view(title);

    auto val_lbl = TextViewBuilder::create()
        ->text(std::to_string(m_repeat_rate) + " / sec")
        ->bold(true)
        ->textSize(15)
        ->textColor(config->colors.primary)
        ->build();
    header_row->add_view(val_lbl);
    col->add_view(header_row);

    // Range: 10 to 60 / sec
    float slider_val = std::clamp((m_repeat_rate - 10) / 50.0f, 0.0f, 1.0f);
    auto slider = SliderBuilder::create()
        ->value(slider_val)
        ->onValueChanged([this, val_lbl](float val, bool) {
            m_repeat_rate = 10 + static_cast<int>(val * 50.0f + 0.5f);
            val_lbl->set_text(std::to_string(m_repeat_rate) + " / sec");
            val_lbl->request_redraw();
            KeyboardBackend::set_repeat_info(m_repeat_rate, m_repeat_delay);
        })
        ->build();
    LayoutParams sl_lp;
    sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sl_lp.height = 30;
    slider->set_layout_params(sl_lp);
    col->add_view(slider);

    auto labels_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    labels_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    auto lbl_slow = TextViewBuilder::create()->text("Slow (10/s)")->textSize(11)->textColor(config->colors.outline)->build();
    LayoutParams ls_lp;
    ls_lp.width = 0;
    ls_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    ls_lp.weight = 1.0f;
    lbl_slow->set_layout_params(ls_lp);
    labels_row->add_view(lbl_slow);

    auto lbl_fast = TextViewBuilder::create()->text("Fast (60/s)")->textSize(11)->textColor(config->colors.outline)->build();
    labels_row->add_view(lbl_fast);
    col->add_view(labels_row);

    card->add_view(col);
    return card;
}

} // namespace miqu
