#include "miller_view.hpp"
#include "preview_common.hpp"
#include <miqutoolkit/view/popup_window.hpp>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace miqu {
std::shared_ptr<LinearLayout> MillerView::create_form_card(const std::shared_ptr<Config>& config) {
    return LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->padding(16, 16)
        ->spacing(12)
        ->backgroundColor(config->colors.surface)
        ->stroke(1, config->colors.outline.with_alpha(0.3f))
        ->cornerRadius(get_card_radius(config))
        ->build();
}

void MillerView::add_form_divider(const std::shared_ptr<LinearLayout>& card, const std::shared_ptr<Config>& config) {
    auto divider = LinearLayoutBuilder::create()
        ->backgroundColor(config->colors.outline.with_alpha(0.12f))
        ->build();
    LayoutParams dlp;
    dlp.width = static_cast<int>(LayoutDimension::MatchParent);
    dlp.height = 1;
    card->add_view(divider, dlp);
}

void MillerView::add_form_toggle(
    const std::shared_ptr<LinearLayout>& card,
    const std::shared_ptr<Config>& config,
    const std::string& title,
    const std::string& hint,
    bool checked,
    std::function<void(bool)> on_toggle)
{
    auto row = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->padding(10, 8)
        ->spacing(12)
        ->backgroundColor(Color::transparent())
        ->cornerRadius(get_inner_radius(config))
        ->build();

    auto box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    box->add_view(title_view);

    if (!hint.empty()) {
        auto hint_view = TextViewBuilder::create()
            ->text(hint)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        box->add_view(hint_view);
    }

    LayoutParams box_lp;
    box_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    box_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    box_lp.weight = 1.0f;
    box_lp.gravity = Gravity::CenterVertical;
    row->add_view(box, box_lp);

    auto sw = SwitchBuilder::create()
        ->checked(checked)
        ->onCheckedChanged([this, on_toggle](bool val) {
            if (on_toggle) on_toggle(val);
            notify_state_changed();
        })
        ->build();
    row->add_view(sw);
    card->add_view(row);
}

void MillerView::add_form_slider(
    const std::shared_ptr<LinearLayout>& card,
    const std::shared_ptr<Config>& config,
    const std::string& title,
    const std::string& hint,
    float initial_val,
    const std::string& initial_label,
    std::function<std::string(float)> on_change)
{
    auto block = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(4)
        ->padding(10, 8)
        ->backgroundColor(Color::transparent())
        ->cornerRadius(get_inner_radius(config))
        ->build();

    auto header = LinearLayoutBuilder::create()
        ->orientation(Orientation::Horizontal)
        ->gravity(Gravity::CenterVertical)
        ->build();

    auto box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    box->add_view(title_view);

    if (!hint.empty()) {
        auto hint_view = TextViewBuilder::create()
            ->text(hint)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        box->add_view(hint_view);
    }

    LayoutParams box_lp;
    box_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    box_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    box_lp.weight = 1.0f;
    box_lp.gravity = Gravity::CenterVertical;
    header->add_view(box, box_lp);

    auto val_label = TextViewBuilder::create()
        ->text(initial_label)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.primary)
        ->build();
    LayoutParams val_lp;
    val_lp.width = static_cast<int>(LayoutDimension::WrapContent);
    val_lp.gravity = Gravity::CenterVertical;
    header->add_view(val_label, val_lp);
    block->add_view(header);

    auto slider = SliderBuilder::create()
        ->value(initial_val)
        ->onValueChanged([this, val_label, on_change](float val, bool user) {
            (void)user;
            if (on_change) {
                std::string text = on_change(val);
                val_label->set_text(text);
                val_label->request_redraw();
            }
            notify_state_changed();
        })
        ->build();
    LayoutParams sl_lp;
    sl_lp.width = static_cast<int>(LayoutDimension::MatchParent);
    sl_lp.height = 24;
    block->add_view(slider, sl_lp);

    card->add_view(block);
}

void MillerView::add_form_spinner(
    const std::shared_ptr<LinearLayout>& card,
    const std::shared_ptr<Config>& config,
    const std::string& title,
    const std::string& hint,
    const std::vector<std::string>& items,
    int cur_idx,
    std::function<void(int, const std::string&)> on_select)
{
    auto block = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(6)
        ->padding(10, 8)
        ->backgroundColor(Color::transparent())
        ->cornerRadius(get_inner_radius(config))
        ->build();

    auto box = LinearLayoutBuilder::create()
        ->orientation(Orientation::Vertical)
        ->spacing(2)
        ->build();

    auto title_view = TextViewBuilder::create()
        ->text(title)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    box->add_view(title_view);

    if (!hint.empty()) {
        auto hint_view = TextViewBuilder::create()
            ->text(hint)
            ->caption(true)
            ->textColor(config->colors.on_surface_variant.with_alpha(0.7f))
            ->build();
        box->add_view(hint_view);
    }
    block->add_view(box);

    auto spinner = SpinnerBuilder::create()
        ->items(items)
        ->selectedIndex(cur_idx)
        ->fontSize(config->metrics.font_size)
        ->padding(12, 6)
        ->cornerRadius(get_inner_radius(config))
        ->onItemSelected([this, on_select](int idx, const std::string& val) {
            if (on_select) on_select(idx, val);
            notify_state_changed();
        })
        ->build();
    LayoutParams sp_lp(static_cast<int>(LayoutDimension::WrapContent), 36);
    block->add_view(spinner, sp_lp);
    card->add_view(block);
}



// Modular Domain Preview Renderers
// ==============================================================================


} // namespace miqu
