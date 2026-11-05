#pragma once

#include <miqutoolkit/view/frame_layout.hpp>
#include <miqutoolkit/view/card_view.hpp>
#include <miqutoolkit/view/linear_layout.hpp>
#include <miqutoolkit/view/text_view.hpp>
#include <miqutoolkit/view/image_view.hpp>
#include <miqutoolkit/view/button.hpp>
#include <memory>
#include <string>
#include <functional>

namespace miqu {

class ModalOverlay : public FrameLayout {
public:
    static std::shared_ptr<ModalOverlay> create_package_required_dialog(
        const std::string& package_name,
        const std::string& description,
        std::function<void()> on_close = nullptr
    );

    static std::shared_ptr<ModalOverlay> create_numeric_comparison_dialog(
        const std::string& device_name,
        const std::string& mac,
        uint32_t passkey,
        std::function<void()> on_confirm,
        std::function<void()> on_reject = nullptr
    );

    static std::shared_ptr<ModalOverlay> create_passkey_display_dialog(
        const std::string& device_name,
        const std::string& mac,
        uint32_t passkey,
        std::function<void()> on_cancel = nullptr
    );

    static std::shared_ptr<ModalOverlay> create_authorization_dialog(
        const std::string& device_name,
        const std::string& mac,
        std::function<void()> on_authorize,
        std::function<void()> on_reject = nullptr
    );

    ModalOverlay();
    ~ModalOverlay() override = default;

    void set_icon(const std::string& icon_name) { m_icon_name = icon_name; }
    void set_title(const std::string& title) { m_title = title; }
    void set_message(const std::string& message) { m_message = message; }
    void set_code_block(const std::string& code) { m_code_block = code; }
    void set_highlight_code(const std::string& code) { m_highlight_code = code; }
    void set_confirm_button(const std::string& text, std::function<void()> cb);
    void set_cancel_button(const std::string& text, std::function<void()> cb);
    void set_on_close(std::function<void()> cb) { m_on_close = std::move(cb); }

    void show_in(const std::shared_ptr<FrameLayout>& parent);
    void dismiss();

private:
    void build_ui();

    std::string m_icon_name = "dialog-warning";
    std::string m_title;
    std::string m_message;
    std::string m_code_block;
    std::string m_highlight_code;

    std::string m_confirm_text;
    std::function<void()> m_on_confirm;
    std::string m_cancel_text;
    std::function<void()> m_on_cancel;
    std::function<void()> m_on_close;

    std::weak_ptr<FrameLayout> m_parent_layout;
    std::shared_ptr<Button> m_btn_copy;
};

} // namespace miqu
