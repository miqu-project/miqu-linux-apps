#include "ui/common/modal_overlay.hpp"
#include "modules/bluetooth/bluetooth_backend.hpp"
#include <miqutoolkit/core/config.hpp>

namespace miqu {

ModalOverlay::ModalOverlay() {
    auto config = Config::get();
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    if (config->colors.backdrop.a > 0.01f) {
        set_background_color(config->colors.backdrop);
    } else {
        set_background_color(config->colors.background.with_alpha(0.65f));
    }
}

void ModalOverlay::set_confirm_button(const std::string& text, std::function<void()> cb) {
    m_confirm_text = text;
    m_on_confirm = std::move(cb);
}

void ModalOverlay::set_cancel_button(const std::string& text, std::function<void()> cb) {
    m_cancel_text = text;
    m_on_cancel = std::move(cb);
}

static std::string format_passkey(uint32_t passkey) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%06u", passkey);
    std::string s(buf);
    if (s.length() == 6) {
        return s.substr(0, 3) + " " + s.substr(3);
    }
    return s;
}

std::shared_ptr<ModalOverlay> ModalOverlay::create_package_required_dialog(
    const std::string& package_name,
    const std::string& description,
    std::function<void()> on_close
) {
    auto modal = std::make_shared<ModalOverlay>();
    modal->set_icon("dialog-warning");
    modal->set_title("Package Required: " + package_name);
    modal->set_message(description + "\n\nPlease install it using pacman:");
    modal->set_code_block("sudo pacman -S " + package_name);
    modal->set_on_close(std::move(on_close));
    modal->build_ui();
    return modal;
}

std::shared_ptr<ModalOverlay> ModalOverlay::create_numeric_comparison_dialog(
    const std::string& device_name,
    const std::string& mac,
    uint32_t passkey,
    std::function<void()> on_confirm,
    std::function<void()> on_reject
) {
    auto modal = std::make_shared<ModalOverlay>();
    modal->set_icon("preferences-system-bluetooth");
    modal->set_title("Bluetooth Pairing Request");
    modal->set_message("\"" + device_name + "\" wants to pair with this computer.\nConfirm that the 6-digit code below matches the code displayed on your device:");
    modal->set_highlight_code(format_passkey(passkey));
    modal->set_cancel_button("Reject", [on_reject]() {
        if (on_reject) on_reject();
    });
    modal->set_confirm_button("Confirm & Pair", [on_confirm]() {
        if (on_confirm) on_confirm();
    });
    modal->build_ui();
    return modal;
}

std::shared_ptr<ModalOverlay> ModalOverlay::create_passkey_display_dialog(
    const std::string& device_name,
    const std::string& mac,
    uint32_t passkey,
    std::function<void()> on_cancel
) {
    auto modal = std::make_shared<ModalOverlay>();
    modal->set_icon("input-keyboard");
    modal->set_title("Keyboard Pairing");
    modal->set_message("Pairing with \"" + device_name + "\".\nType the 6-digit code below on the keyboard and press Enter:");
    modal->set_highlight_code(format_passkey(passkey));
    modal->set_cancel_button("Cancel", [on_cancel]() {
        if (on_cancel) on_cancel();
    });
    modal->build_ui();
    return modal;
}

std::shared_ptr<ModalOverlay> ModalOverlay::create_authorization_dialog(
    const std::string& device_name,
    const std::string& mac,
    std::function<void()> on_authorize,
    std::function<void()> on_reject
) {
    auto modal = std::make_shared<ModalOverlay>();
    modal->set_icon("security-high");
    modal->set_title("Bluetooth Authorization");
    modal->set_message("Allow \"" + device_name + "\" (" + mac + ") to connect to this computer?");
    modal->set_cancel_button("Deny", [on_reject]() {
        if (on_reject) on_reject();
    });
    modal->set_confirm_button("Allow", [on_authorize]() {
        if (on_authorize) on_authorize();
    });
    modal->build_ui();
    return modal;
}

void ModalOverlay::show_in(const std::shared_ptr<FrameLayout>& parent) {
    if (!parent) return;
    m_parent_layout = parent;
    parent->add_view(shared_from_this());
}

void ModalOverlay::dismiss() {
    if (m_on_close) {
        m_on_close();
    }
    if (auto p = m_parent_layout.lock()) {
        p->remove_view(shared_from_this());
    }
}

void ModalOverlay::build_ui() {
    clear_views();
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    card->set_padding(22);

    LayoutParams card_lp;
    card_lp.width = 440;
    card_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    card_lp.gravity = Gravity::Center;
    card->set_layout_params(card_lp);

    auto content = std::make_shared<LinearLayout>(Orientation::Vertical);
    content->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    content->set_divider_spacing(14);

    // 1. Header (Icon + Title)
    auto header = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header->set_divider_spacing(12);
    header->set_gravity(Gravity::CenterVertical);

    auto icon = std::make_shared<ImageView>(m_icon_name.empty() ? "dialog-warning" : m_icon_name);
    icon->set_target_size(32);
    LayoutParams icon_lp(32, 32);
    icon_lp.gravity = Gravity::CenterVertical;
    header->add_view(icon, icon_lp);

    auto title = TextViewBuilder::create()
        ->text(m_title)
        ->bold(true)
        ->textSize(config->metrics.h2_size)
        ->textColor(config->colors.on_surface)
        ->build();
    header->add_view(title);
    content->add_view(header);

    // 2. Message Body
    auto msg = TextViewBuilder::create()
        ->text(m_message)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    content->add_view(msg);

    // 3. Highlight Passkey/PIN Code card (e.g. for Bluetooth pairing)
    if (!m_highlight_code.empty()) {
        auto code_box = std::make_shared<CardView>();
        code_box->set_card_background_color(config->colors.surface_variant);
        code_box->set_radius(config->metrics.corner_radius);
        code_box->set_stroke(config->metrics.border_width, config->colors.primary);
        code_box->set_padding(18);

        auto code_lbl = TextViewBuilder::create()
            ->text(m_highlight_code)
            ->textSize(std::max(28, config->metrics.h1_size + 8))
            ->bold(true)
            ->textColor(config->colors.primary)
            ->center(true)
            ->build();
        code_box->add_view(code_lbl);
        content->add_view(code_box);
    }

    // 4. Optional Code snippet card (e.g. pacman command)
    if (!m_code_block.empty()) {
        auto code_box = std::make_shared<CardView>();
        code_box->set_card_background_color(config->colors.surface_variant);
        code_box->set_radius(std::max(4, config->metrics.corner_radius - 6));
        code_box->set_stroke(config->metrics.border_width, config->colors.outline_variant);
        code_box->set_padding(10);

        auto code_lbl = TextViewBuilder::create()
            ->text(m_code_block)
            ->textSize(config->metrics.font_size)
            ->bold(true)
            ->textColor(config->colors.primary)
            ->build();
        code_box->add_view(code_lbl);
        content->add_view(code_box);
    }

    // 5. Action Buttons row
    auto btn_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    btn_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    btn_row->set_divider_spacing(10);
    btn_row->set_gravity(Gravity::Right);

    if (!m_code_block.empty()) {
        m_btn_copy = ButtonBuilder::create()
            ->text("Copy Command")
            ->textSize(config->metrics.font_size)
            ->bordered(true)
            ->borderColor(config->colors.outline_variant)
            ->borderWidth(config->metrics.border_width)
            ->backgroundColor(config->colors.surface_variant)
            ->textColor(config->colors.on_surface)
            ->cornerRadius(btn_radius)
            ->padding(12, 6)
            ->onClick([this]() {
                BluetoothBackend::copy_to_clipboard(m_code_block);
                if (m_btn_copy) {
                    m_btn_copy->set_text("Copied ✓");
                }
            })
            ->build();
        btn_row->add_view(m_btn_copy);
    }

    // Cancel / Reject button
    if (m_on_cancel || !m_cancel_text.empty()) {
        auto btn_cancel = ButtonBuilder::create()
            ->text(m_cancel_text.empty() ? "Cancel" : m_cancel_text)
            ->textSize(config->metrics.font_size)
            ->bordered(true)
            ->borderColor(config->colors.outline_variant)
            ->borderWidth(config->metrics.border_width)
            ->backgroundColor(config->colors.surface)
            ->textColor(config->colors.on_surface)
            ->cornerRadius(btn_radius)
            ->padding(14, 6)
            ->onClick([this]() {
                if (m_on_cancel) m_on_cancel();
                dismiss();
            })
            ->build();
        btn_row->add_view(btn_cancel);
    }

    // Confirm / Action button
    if (m_on_confirm || !m_confirm_text.empty()) {
        auto btn_confirm = ButtonBuilder::create()
            ->text(m_confirm_text.empty() ? "Confirm" : m_confirm_text)
            ->primary(true)
            ->textSize(config->metrics.font_size)
            ->cornerRadius(btn_radius)
            ->padding(16, 6)
            ->onClick([this]() {
                if (m_on_confirm) m_on_confirm();
                dismiss();
            })
            ->build();
        btn_row->add_view(btn_confirm);
    } else if (m_cancel_text.empty()) {
        // Fallback default Close button
        auto btn_ok = ButtonBuilder::create()
            ->text("Close")
            ->primary(true)
            ->textSize(config->metrics.font_size)
            ->cornerRadius(btn_radius)
            ->padding(16, 6)
            ->onClick([this]() {
                dismiss();
            })
            ->build();
        btn_row->add_view(btn_ok);
    }

    content->add_view(btn_row);
    card->add_view(content);
    add_view(card);
}

} // namespace miqu
