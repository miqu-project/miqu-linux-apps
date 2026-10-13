#include "color_scheme_view.hpp"
#include <cmath>

namespace miqutheme {

using namespace miqu;

class ColorDotView : public View {
public:
    ColorDotView(const std::string& hex, int diameter = 14) : diameter_(diameter) {
        color_ = Color::from_hex(hex);
        set_layout_params(LayoutParams(diameter, diameter));
        set_margin(2, 0, 2, 0);
    }

    void draw(cairo_t* cr, const Rect& bounds) override {
        cairo_save(cr);
        double cx = bounds.x + bounds.width / 2.0;
        double cy = bounds.y + bounds.height / 2.0;
        double r = std::min(bounds.width, bounds.height) / 2.0;
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        cairo_set_source_rgba(cr, color_.r, color_.g, color_.b, color_.a);
        cairo_fill(cr);

        // Subtle ring outline
        cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.2);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_restore(cr);
    }

private:
    Color color_;
    int diameter_;
};

ColorSchemeView::ColorSchemeView(std::shared_ptr<ThemeBackend> backend, std::function<void()> on_refresh_requested)
    : backend_(std::move(backend)), on_refresh_requested_(std::move(on_refresh_requested)) {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));

    content_layout_ = std::make_shared<LinearLayout>(Orientation::Vertical);
    content_layout_->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    content_layout_->set_padding(18, 12, 18, 24);

    set_content_view(content_layout_);
    build_ui();
}

void ColorSchemeView::refresh_ui() {
    build_ui();
    request_redraw();
}

void ColorSchemeView::build_ui() {
    content_layout_->clear_views();

    ThemeMode mode = backend_->get_current_mode();
    std::string current_scheme = backend_->get_current_scheme(mode);

    // =========================================================================
    // 1. THEME MODE CARD (Full Width)
    // =========================================================================
    auto mode_card = std::make_shared<CardView>();
    mode_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    mode_card->set_padding(16, 14, 16, 14);
    mode_card->set_margin(0, 0, 0, 14);

    auto mode_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    mode_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto mode_text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    mode_text_col->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));

    auto mode_title = TextViewBuilder::create()
        ->text("Theme Mode")
        ->h2()
        ->build();

    auto mode_sub = TextViewBuilder::create()
        ->text(mode == ThemeMode::Dark ? "Dark theme active" : "Light theme active")
        ->caption()
        ->muted()
        ->build();
    mode_sub->set_margin(0, 2, 0, 0);

    mode_text_col->add_view(mode_title);
    mode_text_col->add_view(mode_sub);
    mode_row->add_view(mode_text_col);

    auto mode_switch_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    mode_switch_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto dark_label = TextViewBuilder::create()
        ->text("Dark")
        ->caption()
        ->build();
    dark_label->set_margin(0, 0, 6, 0);

    mode_switch_ = SwitchBuilder::create()
        ->checked(mode == ThemeMode::Light)
        ->onCheckedChanged([this](bool checked) {
            ThemeMode new_mode = checked ? ThemeMode::Light : ThemeMode::Dark;
            backend_->set_mode(new_mode);
            if (on_refresh_requested_) on_refresh_requested_();
        })
        ->build();

    auto light_label = TextViewBuilder::create()
        ->text("Light")
        ->caption()
        ->build();
    light_label->set_margin(6, 0, 0, 0);

    mode_switch_row->add_view(dark_label);
    mode_switch_row->add_view(mode_switch_);
    mode_switch_row->add_view(light_label);

    mode_row->add_view(mode_switch_row);
    mode_card->add_view(mode_row);
    content_layout_->add_view(mode_card);

    // =========================================================================
    // 2. SECTION TITLE: COLOR SCHEMES
    // =========================================================================
    auto schemes_header = TextViewBuilder::create()
        ->text("Color Schemes")
        ->h2()
        ->build();
    schemes_header->set_margin(0, 6, 0, 8);
    content_layout_->add_view(schemes_header);

    // =========================================================================
    // 3. FEATURED FULL-WIDTH CARD: MATERIAL YOU (DYNAMIC WALLPAPER)
    // =========================================================================
    bool is_dynamic_active = (current_scheme == "dynamic-matugen");
    auto matugen_card = std::make_shared<CardView>();
    matugen_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    matugen_card->set_padding(16, 14, 16, 14);
    matugen_card->set_margin(0, 0, 0, 14);
    matugen_card->set_bordered(is_dynamic_active);

    auto matugen_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    matugen_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    // Top Row: Selection Radio + Title + Status
    auto matugen_top_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    matugen_top_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto matugen_radio = TextViewBuilder::create()
        ->text(is_dynamic_active ? "🔘" : "⚪")
        ->textSize(16)
        ->build();
    matugen_radio->set_margin(0, 0, 8, 0);

    auto matugen_title = TextViewBuilder::create()
        ->text("Material You (Dynamic Wallpaper)")
        ->h2()
        ->build();

    auto matugen_title_box = std::make_shared<LinearLayout>(Orientation::Horizontal);
    LayoutParams lp_matugen_title(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    lp_matugen_title.gravity = Gravity::CenterVertical;
    matugen_title_box->set_layout_params(lp_matugen_title);
    matugen_title_box->add_view(matugen_radio);
    matugen_title_box->add_view(matugen_title);
    matugen_top_row->add_view(matugen_title_box);

    // Dynamic Swatches
    SchemeInfo dynamic_info = backend_->get_dynamic_scheme_info(mode);
    auto matugen_swatches_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    matugen_swatches_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::WrapContent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    for (const auto& hex : dynamic_info.swatches) {
        matugen_swatches_row->add_view(std::make_shared<ColorDotView>(hex, 16));
    }
    matugen_top_row->add_view(matugen_swatches_row);
    matugen_col->add_view(matugen_top_row);

    // Sub-category Row: Palette Variant Dropdown
    auto variant_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    variant_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));
    variant_row->set_margin(24, 10, 0, 0);

    auto variant_label = TextViewBuilder::create()
        ->text("Palette Variant:")
        ->caption()
        ->muted()
        ->build();
    variant_label->set_margin(0, 0, 10, 0);
    variant_row->add_view(variant_label);

    const auto& variants = ThemeBackend::get_matugen_variants();
    std::vector<std::string> variant_labels;
    int selected_variant_idx = 0;
    std::string current_type = backend_->get_matugen_type();

    for (size_t i = 0; i < variants.size(); ++i) {
        variant_labels.push_back(variants[i].first);
        if (variants[i].second == current_type) {
            selected_variant_idx = static_cast<int>(i);
        }
    }

    variant_spinner_ = SpinnerBuilder::create()
        ->items(variant_labels)
        ->selectedIndex(selected_variant_idx)
        ->onItemSelected([this, variants](int idx, const std::string&) {
            if (idx >= 0 && idx < static_cast<int>(variants.size())) {
                backend_->set_matugen_type(variants[idx].second);
                if (on_refresh_requested_) on_refresh_requested_();
            }
        })
        ->build();
    variant_spinner_->set_layout_params(LayoutParams(
        200,
        32
    ));
    variant_row->add_view(variant_spinner_);

    matugen_col->add_view(variant_row);
    matugen_card->add_view(matugen_col);

    matugen_card->set_on_click_listener([this, current_type]() {
        backend_->apply_scheme("dynamic-matugen", current_type);
    });

    content_layout_->add_view(matugen_card);

    // =========================================================================
    // 4. TWO-COLUMN GRID: PRESET COLOR SCHEMES
    // =========================================================================
    std::vector<SchemeInfo> presets = backend_->get_available_schemes(mode);

    auto two_col_layout = std::make_shared<LinearLayout>(Orientation::Horizontal);
    two_col_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    two_col_layout->set_margin(0, 0, 0, 14);

    auto col_left = std::make_shared<LinearLayout>(Orientation::Vertical);
    col_left->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    col_left->set_margin(0, 0, 6, 0);

    auto col_right = std::make_shared<LinearLayout>(Orientation::Vertical);
    col_right->set_layout_params(LayoutParams(
        0,
        static_cast<int>(LayoutDimension::WrapContent),
        1.0f
    ));
    col_right->set_margin(6, 0, 0, 0);

    for (size_t i = 0; i < presets.size(); ++i) {
        const auto& preset = presets[i];
        bool is_active = (current_scheme == preset.name);

        auto card = std::make_shared<CardView>();
        card->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));
        card->set_padding(14, 12, 14, 12);
        card->set_margin(0, 0, 0, 10);
        card->set_bordered(is_active);

        auto card_col = std::make_shared<LinearLayout>(Orientation::Vertical);
        card_col->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent)
        ));

        // Header: Radio + Title
        auto card_header = std::make_shared<LinearLayout>(Orientation::Horizontal);
        card_header->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::MatchParent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));

        auto radio = TextViewBuilder::create()
            ->text(is_active ? "🔘" : "⚪")
            ->textSize(14)
            ->build();
        radio->set_margin(0, 0, 6, 0);

        auto title = TextViewBuilder::create()
            ->text(preset.display_name)
            ->bold()
            ->build();

        card_header->add_view(radio);
        card_header->add_view(title);
        card_col->add_view(card_header);

        // Color chips row
        auto swatches_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
        swatches_row->set_layout_params(LayoutParams(
            static_cast<int>(LayoutDimension::WrapContent),
            static_cast<int>(LayoutDimension::WrapContent),
            Gravity::CenterVertical
        ));
        swatches_row->set_margin(20, 8, 0, 0);
        for (const auto& hex : preset.swatches) {
            swatches_row->add_view(std::make_shared<ColorDotView>(hex, 14));
        }
        card_col->add_view(swatches_row);

        card->add_view(card_col);

        std::string s_name = preset.name;
        card->set_on_click_listener([this, s_name]() {
            backend_->apply_scheme(s_name);
        });

        if (i % 2 == 0) {
            col_left->add_view(card);
        } else {
            col_right->add_view(card);
        }
    }

    two_col_layout->add_view(col_left);
    two_col_layout->add_view(col_right);
    content_layout_->add_view(two_col_layout);

    // =========================================================================
    // 5. FULL-WIDTH CARD: WINDOW TRANSPARENCY
    // =========================================================================
    int curr_transparency = backend_->get_transparency();

    auto trans_card = std::make_shared<CardView>();
    trans_card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    trans_card->set_padding(16, 14, 16, 14);

    auto trans_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    trans_col->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto trans_header = std::make_shared<LinearLayout>(Orientation::Horizontal);
    trans_header->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent),
        Gravity::CenterVertical
    ));

    auto trans_title = TextViewBuilder::create()
        ->text("Window Transparency")
        ->h2()
        ->build();

    auto trans_title_box = std::make_shared<LinearLayout>(Orientation::Horizontal);
    LayoutParams lp_trans_title(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    lp_trans_title.gravity = Gravity::CenterVertical;
    trans_title_box->set_layout_params(lp_trans_title);
    trans_title_box->add_view(trans_title);
    trans_header->add_view(trans_title_box);

    transparency_badge_ = TextViewBuilder::create()
        ->text(std::to_string(curr_transparency) + "%")
        ->bold()
        ->build();
    trans_header->add_view(transparency_badge_);
    trans_col->add_view(trans_header);

    transparency_slider_ = SliderBuilder::create()
        ->value(curr_transparency / 100.0f)
        ->onValueChanged([this](float val, bool from_user) {
            int pct = std::clamp(static_cast<int>(std::round(val * 100.0f)), 0, 100);
            if (transparency_badge_) {
                transparency_badge_->set_text(std::to_string(pct) + "%");
            }
            if (from_user) {
                backend_->apply_transparency(pct, false);
            }
        })
        ->build();
    transparency_slider_->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        28
    ));
    transparency_slider_->set_margin(0, 10, 0, 0);
    trans_col->add_view(transparency_slider_);

    trans_card->add_view(trans_col);
    content_layout_->add_view(trans_card);
}

} // namespace miqutheme
