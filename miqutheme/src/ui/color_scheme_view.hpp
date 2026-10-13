#pragma once

#include "backend/theme_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <functional>

namespace miqutheme {

class ColorSchemeView : public miqu::ScrollView {
public:
    ColorSchemeView(std::shared_ptr<ThemeBackend> backend, std::function<void()> on_refresh_requested);
    ~ColorSchemeView() override = default;

    void refresh_ui();

private:
    std::shared_ptr<ThemeBackend> backend_;
    std::function<void()> on_refresh_requested_;

    std::shared_ptr<miqu::LinearLayout> content_layout_;
    std::shared_ptr<miqu::Switch> mode_switch_;
    std::shared_ptr<miqu::Spinner> variant_spinner_;
    std::shared_ptr<miqu::Slider> transparency_slider_;
    std::shared_ptr<miqu::TextView> transparency_badge_;

    void build_ui();
};

} // namespace miqutheme
