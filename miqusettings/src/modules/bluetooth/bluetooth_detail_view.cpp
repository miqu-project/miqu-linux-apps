#include "modules/bluetooth/bluetooth_detail_view.hpp"
#include "modules/bluetooth/bluetooth_backend.hpp"
#include "modules/bluetooth/bluetooth_agent.hpp"
#include "ui/common/modal_overlay.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/app_engine.hpp>
#include <miqutoolkit/view/divider_view.hpp>
#include <cstdlib>

namespace miqu {

BluetoothDetailView::BluetoothDetailView() {
    set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::MatchParent)
    ));
    init_agent();
    load_devices();
    setup_ui();
}

BluetoothDetailView::~BluetoothDetailView() {
    stop_polling();
    if (m_scanning) {
        BluetoothBackend::stop_scan();
    }
    if (m_agent) {
        m_agent->stop();
        m_agent.reset();
    }
}

void BluetoothDetailView::set_is_fullscreen(bool fullscreen) {
    m_is_fullscreen = fullscreen;
    setup_ui();
}

void BluetoothDetailView::refresh_state() {
    load_devices();
    setup_ui();
}

void BluetoothDetailView::load_devices() {
    m_adapter_info = BluetoothBackend::get_adapter_info();
    m_powered = m_adapter_info.powered;
    if (m_adapter_info.discovering) {
        m_scanning = true;
    }

    if (m_powered) {
        m_paired_devices = BluetoothBackend::get_paired_devices();
        m_available_devices = BluetoothBackend::get_available_devices();
    } else {
        m_scanning = false;
        m_paired_devices.clear();
        m_available_devices.clear();
    }
}

void BluetoothDetailView::start_polling() {
    stop_polling();
    auto engine = AppEngine::instance();
    if (!engine) return;

    m_poll_timer_id = engine->post_delayed(2000, [this]() {
        m_poll_timer_id = 0;
        if (m_scanning && m_powered) {
            if (!BluetoothBackend::is_scanning()) {
                BluetoothBackend::start_scan();
            }
            if (!m_active_pairing_modal) {
                load_devices();
                setup_ui();
            }
            start_polling();
        }
    });
}

void BluetoothDetailView::stop_polling() {
    if (m_poll_timer_id != 0) {
        if (auto engine = AppEngine::instance()) {
            engine->cancel_delayed(m_poll_timer_id);
        }
        m_poll_timer_id = 0;
    }
}

void BluetoothDetailView::init_agent() {
    m_agent = std::make_unique<BluetoothAgent>();

    m_agent->set_on_request_confirmation([this](const std::string& dev_name, const std::string& mac, uint32_t passkey, BluetoothAgent::ConfirmCallback cb) {
        if (m_active_pairing_modal) {
            m_active_pairing_modal->dismiss();
            m_active_pairing_modal.reset();
        }
        auto modal = ModalOverlay::create_numeric_comparison_dialog(
            dev_name,
            mac,
            passkey,
            [this, cb]() {
                cb(true);
                m_active_pairing_modal.reset();
                load_devices();
                setup_ui();
            },
            [this, cb]() {
                cb(false);
                m_active_pairing_modal.reset();
            }
        );
        m_active_pairing_modal = modal;
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
    });

    m_agent->set_on_display_passkey([this](const std::string& dev_name, const std::string& mac, uint32_t passkey) {
        if (m_active_pairing_modal) {
            m_active_pairing_modal->dismiss();
            m_active_pairing_modal.reset();
        }
        auto modal = ModalOverlay::create_passkey_display_dialog(
            dev_name,
            mac,
            passkey,
            [this]() {
                m_active_pairing_modal.reset();
            }
        );
        m_active_pairing_modal = modal;
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
    });

    m_agent->set_on_request_authorization([this](const std::string& dev_name, const std::string& mac, BluetoothAgent::AuthorizationCallback cb) {
        if (m_active_pairing_modal) {
            m_active_pairing_modal->dismiss();
            m_active_pairing_modal.reset();
        }
        auto modal = ModalOverlay::create_authorization_dialog(
            dev_name,
            mac,
            [this, cb]() {
                cb(true);
                m_active_pairing_modal.reset();
                load_devices();
                setup_ui();
            },
            [this, cb]() {
                cb(false);
                m_active_pairing_modal.reset();
            }
        );
        m_active_pairing_modal = modal;
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
    });

    m_agent->set_on_cancel([this]() {
        if (m_active_pairing_modal) {
            m_active_pairing_modal->dismiss();
            m_active_pairing_modal.reset();
        }
    });

    m_agent->start();
}

void BluetoothDetailView::setup_ui() {
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
    m_content_layout->set_padding(m_is_fullscreen ? 28 : 14);

    // Dependency check: BlueZ suite
    if (!BluetoothBackend::is_bluetooth_installed()) {
        auto card = std::make_shared<CardView>();
        card->set_card_background_color(config->colors.surface_variant);
        card->set_radius(12);
        card->set_stroke(0, Color());
        card->set_padding(20);

        auto col = std::make_shared<LinearLayout>(Orientation::Vertical);
        col->set_divider_spacing(12);

        auto title = TextViewBuilder::create()
            ->text("BlueZ Package Required")
            ->bold(true)
            ->textSize(config->metrics.h2_size)
            ->textColor(config->colors.on_surface)
            ->build();
        col->add_view(title);

        auto desc = TextViewBuilder::create()
            ->text("Bluetooth management requires the official Linux BlueZ stack. Install it via pacman:")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface_variant)
            ->build();
        col->add_view(desc);

        auto code_box = std::make_shared<CardView>();
        code_box->set_card_background_color(config->colors.surface);
        code_box->set_radius(6);
        code_box->set_stroke(0, Color());
        code_box->set_padding(10);
        auto code_text = TextViewBuilder::create()
            ->text("sudo pacman -S bluez bluez-utils && sudo systemctl enable --now bluetooth")
            ->textSize(config->metrics.font_size)
            ->bold(true)
            ->textColor(config->colors.primary)
            ->build();
        code_box->add_view(code_text);
        col->add_view(code_box);

        auto btn_copy = ButtonBuilder::create()
            ->text("Copy Install Command")
            ->textSize(config->metrics.font_size)
            ->primary(true)
            ->cornerRadius(16)
            ->padding(14, 6)
            ->onClick([]() {
                BluetoothBackend::copy_to_clipboard("sudo pacman -S bluez bluez-utils && sudo systemctl enable --now bluetooth");
            })
            ->build();
        col->add_view(btn_copy);

        card->add_view(col);
        m_content_layout->add_view(card);
        m_scroll->set_content_view(m_content_layout);
        add_view(m_scroll);
        return;
    }

    // 1. Modern Adapter Hero Header
    m_content_layout->add_view(create_hero_card());

    if (m_powered) {
        // 2. Quick Action Chips (Search, Send, Receive)
        m_content_layout->add_view(create_action_row());

        // 3. Separate connected vs saved accessories
        std::vector<BluetoothDeviceItem> connected_devs;
        std::vector<BluetoothDeviceItem> saved_devs;
        for (const auto& dev : m_paired_devices) {
            if (dev.is_connected) connected_devs.push_back(dev);
            else saved_devs.push_back(dev);
        }

        if (!connected_devs.empty()) {
            m_content_layout->add_view(create_device_section("CONNECTED ACCESSORIES", connected_devs, true));
        }

        if (!saved_devs.empty()) {
            m_content_layout->add_view(create_device_section("PAIRED ACCESSORIES", saved_devs, false));
        }

        // 4. Empty state if no paired devices
        if (m_paired_devices.empty() && !m_scanning) {
            auto empty_text = TextViewBuilder::create()
                ->text("No paired accessories found.\nPut your Bluetooth headphones, mouse, keyboard, or stylus into pairing mode and click 'Search for Devices'.")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface_variant)
                ->center(true)
                ->padding(16, 20)
                ->build();
            m_content_layout->add_view(empty_text);
        }

        // 5. Available / Nearby Devices Section
        if (!m_available_devices.empty() || m_scanning) {
            m_content_layout->add_view(create_available_section());
        }

        // 6. File Transfer Information
        m_content_layout->add_view(create_file_transfer_card());
    } else {
        // Disabled state view
        m_content_layout->add_view(create_disabled_state());
    }

    m_scroll->set_content_view(m_content_layout);
    add_view(m_scroll);

    if (m_active_pairing_modal) {
        add_view(m_active_pairing_modal);
    }
}

static std::shared_ptr<View> create_icon_badge(const std::string& icon_name, const Color& bg_color, int size = 36, int icon_sz = 20) {
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(bg_color);
    card->set_radius(size / 2);
    card->set_stroke(0, Color());
    card->set_elevation(0);
    LayoutParams lp(size, size);
    lp.gravity = Gravity::CenterVertical;
    card->set_layout_params(lp);

    auto img = std::make_shared<ImageView>(icon_name.empty() ? "bluetooth" : icon_name);
    img->set_target_size(icon_sz);
    LayoutParams img_lp(icon_sz, icon_sz);
    img_lp.gravity = Gravity::Center;
    card->add_view(img, img_lp);
    return card;
}

std::shared_ptr<View> BluetoothDetailView::create_hero_card() {
    auto config = Config::get();

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_radius(16);
    card->set_stroke(0, Color());
    card->set_elevation(0);
    card->set_padding(16, 16);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(14);
    row->set_gravity(Gravity::CenterVertical);

    // Circular glyph badge (44x44)
    Color badge_bg = m_powered ? config->colors.primary_container : config->colors.surface;
    std::string icon_src = m_powered ? "bluetooth-active" : "bluetooth";
    auto badge = create_icon_badge(icon_src, badge_bg, 44, 24);
    row->add_view(badge);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    text_col->set_layout_params(text_lp);
    text_col->set_divider_spacing(2);

    auto title = TextViewBuilder::create()
        ->text("Bluetooth")
        ->bold(true)
        ->textSize(m_is_fullscreen ? config->metrics.h1_size : config->metrics.h2_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub_text;
    Color sub_color = config->colors.on_surface_variant;
    if (!m_adapter_info.available) {
        sub_text = "No controller hardware detected";
    } else if (m_powered) {
        std::string alias = m_adapter_info.name.empty() ? "This Device" : m_adapter_info.name;
        if (m_scanning) {
            sub_text = "● Searching for nearby accessories...";
            sub_color = config->colors.primary;
        } else {
            sub_text = "● Discoverable as \"" + alias + "\"";
        }
    } else {
        sub_text = "Turn on to connect headphones, mouse, keyboard & accessories";
    }

    auto subtitle = TextViewBuilder::create()
        ->text(sub_text)
        ->textSize(config->metrics.caption_size)
        ->textColor(sub_color)
        ->build();
    text_col->add_view(subtitle);
    row->add_view(text_col);

    // Power Toggle Switch
    auto sw = SwitchBuilder::create()
        ->checked(m_powered)
        ->onCheckedChanged([this](bool checked) {
            toggle_power(checked);
        })
        ->build();
    LayoutParams sw_lp;
    sw_lp.gravity = Gravity::CenterVertical;
    sw->set_layout_params(sw_lp);
    row->add_view(sw);

    // Fullscreen toggle button if in 3-pillar mode
    if (!m_is_fullscreen) {
        auto btn_expand = ButtonBuilder::create()
            ->text("⛶")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->flat(true)
            ->cornerRadius(6)
            ->onClick([this]() {
                if (m_on_request_fullscreen) m_on_request_fullscreen();
            })
            ->build();
        LayoutParams btn_lp(32, 32);
        btn_expand->set_layout_params(btn_lp);
        row->add_view(btn_expand);
    }

    card->add_view(row);
    return card;
}

std::shared_ptr<View> BluetoothDetailView::create_action_row() {
    auto config = Config::get();

    auto action_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    action_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    action_row->set_divider_spacing(8);

    // 1. Scan for Devices button
    auto btn_pair = ButtonBuilder::create()
        ->text(m_scanning ? "Stop Searching" : "Search for Devices")
        ->icon(m_scanning ? "network-wireless-acquiring" : "list-add")
        ->iconSize(14)
        ->textSize(config->metrics.font_size)
        ->primary(!m_scanning)
        ->flat(m_scanning)
        ->cornerRadius(16)
        ->padding(12, 6)
        ->onClick([this]() { toggle_scan(); })
        ->build();
    LayoutParams pair_lp(0, 36, 1.2f);
    btn_pair->set_layout_params(pair_lp);
    action_row->add_view(btn_pair);

    // 2. Send File button
    auto btn_send = ButtonBuilder::create()
        ->text("Send File")
        ->icon("document-send")
        ->iconSize(14)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(16)
        ->padding(12, 6)
        ->onClick([this]() { on_send_file(); })
        ->build();
    LayoutParams send_lp(0, 36, 1.0f);
    btn_send->set_layout_params(send_lp);
    action_row->add_view(btn_send);

    // 3. Receive Files button
    auto btn_recv = ButtonBuilder::create()
        ->text("Receive")
        ->icon("folder-download")
        ->iconSize(14)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->backgroundColor(config->colors.surface_variant)
        ->cornerRadius(16)
        ->padding(12, 6)
        ->onClick([this]() { on_receive_file(); })
        ->build();
    LayoutParams recv_lp(0, 36, 1.0f);
    btn_recv->set_layout_params(recv_lp);
    action_row->add_view(btn_recv);

    return action_row;
}

std::shared_ptr<View> BluetoothDetailView::create_device_section(
    const std::string& title,
    const std::vector<BluetoothDeviceItem>& devs,
    bool is_connected_group
) {
    if (devs.empty()) return nullptr;
    auto config = Config::get();

    auto sec_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    sec_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sec_container->set_divider_spacing(6);

    // Section header label
    auto lbl = TextViewBuilder::create()
        ->text(title)
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(is_connected_group ? config->colors.primary : config->colors.on_surface_variant)
        ->padding(4, 2)
        ->build();
    sec_container->add_view(lbl);

    // Inset grouped card container (Apple/Android modern style)
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_radius(14);
    card->set_stroke(0, Color());
    card->set_elevation(0);
    card->set_padding(8, 4);

    auto list_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    bool first = true;
    for (const auto& dev : devs) {
        if (!first) {
            auto div = std::make_shared<DividerView>(Orientation::Horizontal, 1);
            div->set_color(config->colors.outline_variant);
            div->set_margin(Margin(48, 2, 8, 2));
            list_box->add_view(div);
        }
        first = false;
        list_box->add_view(create_device_row(dev, is_connected_group));
    }

    card->add_view(list_box);
    sec_container->add_view(card);
    return sec_container;
}

std::shared_ptr<View> BluetoothDetailView::create_device_row(const BluetoothDeviceItem& dev, bool is_connected_group) {
    auto config = Config::get();

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_padding(8, 6);
    row->set_divider_spacing(12);
    row->set_gravity(Gravity::CenterVertical);

    // Circular glyph badge (36x36)
    Color badge_bg = is_connected_group ? config->colors.primary_container : config->colors.surface;
    auto badge = create_icon_badge(dev.icon_name, badge_bg, 36, 20);
    row->add_view(badge);

    // Name & Status
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    text_col->set_layout_params(text_lp);
    text_col->set_divider_spacing(2);

    auto title = TextViewBuilder::create()
        ->text(dev.name)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string status_text;
    Color status_color = config->colors.on_surface_variant;
    if (dev.is_connected) {
        status_text = "● Connected";
        status_color = config->colors.primary;
        if (dev.battery_percentage >= 0) {
            status_text += " · " + std::to_string(dev.battery_percentage) + "% Battery";
        }
    } else {
        status_text = "Paired accessory";
    }

    auto sub_view = TextViewBuilder::create()
        ->text(status_text)
        ->textSize(config->metrics.caption_size)
        ->textColor(status_color)
        ->build();
    text_col->add_view(sub_view);
    row->add_view(text_col);

    // Connect / Disconnect button
    auto action_btn = ButtonBuilder::create()
        ->text(dev.is_connected ? "Disconnect" : "Connect")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(14)
        ->padding(12, 4)
        ->onClick([this, mac = dev.mac]() {
            toggle_connection(mac);
        });

    if (dev.is_connected) {
        action_btn->backgroundColor(config->colors.surface)
                  ->textColor(config->colors.on_surface);
    } else {
        action_btn->primary(true);
    }

    auto btn_view = action_btn->build();
    LayoutParams btn_lp(84, 28);
    btn_lp.gravity = Gravity::CenterVertical;
    btn_view->set_layout_params(btn_lp);
    row->add_view(btn_view);

    // Forget button
    auto btn_forget = ButtonBuilder::create()
        ->text("✕")
        ->textSize(config->metrics.caption_size)
        ->flat(true)
        ->textColor(config->colors.on_surface_variant)
        ->onClick([this, mac = dev.mac]() {
            on_forget_device(mac);
        })
        ->build();
    LayoutParams forget_lp(28, 28);
    forget_lp.gravity = Gravity::CenterVertical;
    btn_forget->set_layout_params(forget_lp);
    row->add_view(btn_forget);

    return row;
}

std::shared_ptr<View> BluetoothDetailView::create_available_section() {
    auto config = Config::get();
    auto sec_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    sec_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sec_container->set_divider_spacing(6);

    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header_row->set_gravity(Gravity::CenterVertical);

    auto lbl = TextViewBuilder::create()
        ->text("NEARBY DEVICES")
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->padding(4, 2)
        ->build();
    header_row->add_view(lbl);

    if (m_scanning) {
        auto scan_lbl = TextViewBuilder::create()
            ->text("Searching...")
            ->textSize(config->metrics.caption_size)
            ->textColor(config->colors.primary)
            ->padding(8, 2)
            ->build();
        header_row->add_view(scan_lbl);
    }
    sec_container->add_view(header_row);

    // Inset grouped card
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_radius(14);
    card->set_stroke(0, Color());
    card->set_elevation(0);
    card->set_padding(8, 4);

    auto list_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    if (m_available_devices.empty()) {
        auto msg = TextViewBuilder::create()
            ->text(m_scanning ? "Looking for nearby discoverable Bluetooth accessories..." : "No new devices found nearby.")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface_variant)
            ->padding(12, 12)
            ->build();
        list_box->add_view(msg);
    } else {
        bool first = true;
        for (const auto& dev : m_available_devices) {
            if (!first) {
                auto div = std::make_shared<DividerView>(Orientation::Horizontal, 1);
                div->set_color(config->colors.outline_variant);
                div->set_margin(Margin(48, 2, 8, 2));
                list_box->add_view(div);
            }
            first = false;
            list_box->add_view(create_available_row(dev));
        }
    }

    card->add_view(list_box);
    sec_container->add_view(card);
    return sec_container;
}

std::shared_ptr<View> BluetoothDetailView::create_available_row(const BluetoothDeviceItem& dev) {
    auto config = Config::get();

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_padding(8, 6);
    row->set_divider_spacing(12);
    row->set_gravity(Gravity::CenterVertical);

    // Circular glyph badge (36x36)
    auto badge = create_icon_badge(dev.icon_name.empty() ? "bluetooth" : dev.icon_name, config->colors.surface, 36, 20);
    row->add_view(badge);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    text_col->set_layout_params(text_lp);
    text_col->set_divider_spacing(2);

    auto title = TextViewBuilder::create()
        ->text(dev.name)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("Ready to pair · " + dev.mac)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub);
    row->add_view(text_col);

    auto btn_pair = ButtonBuilder::create()
        ->text("Pair")
        ->primary(true)
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(14)
        ->padding(14, 4)
        ->onClick([this, mac = dev.mac]() {
            on_pair_device(mac);
        })
        ->build();
    LayoutParams btn_lp(72, 28);
    btn_lp.gravity = Gravity::CenterVertical;
    btn_pair->set_layout_params(btn_lp);
    row->add_view(btn_pair);

    return row;
}

std::shared_ptr<View> BluetoothDetailView::create_file_transfer_card() {
    auto config = Config::get();

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_radius(14);
    card->set_stroke(0, Color());
    card->set_padding(12, 14);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(12);
    row->set_gravity(Gravity::CenterVertical);

    auto badge = create_icon_badge("folder-download", config->colors.surface, 36, 20);
    row->add_view(badge);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp(0, static_cast<int>(LayoutDimension::WrapContent), 1.0f);
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text("Bluetooth File Transfers")
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string dl_dir = BluetoothBackend::get_received_files_directory();
    auto sub = TextViewBuilder::create()
        ->text("Received files are stored in " + dl_dir + " automatically")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub);
    row->add_view(text_col);

    auto btn_open = ButtonBuilder::create()
        ->text("Open Folder")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(14)
        ->padding(12, 4)
        ->backgroundColor(config->colors.surface)
        ->textColor(config->colors.on_surface)
        ->onClick([dl_dir]() {
            std::string cmd = "xdg-open \"" + dl_dir + "\" 2>/dev/null &";
            std::system(cmd.c_str());
        })
        ->build();
    LayoutParams btn_lp(96, 28);
    btn_open->set_layout_params(btn_lp);
    row->add_view(btn_open);

    card->add_view(row);
    return card;
}

std::shared_ptr<View> BluetoothDetailView::create_disabled_state() {
    auto config = Config::get();

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_radius(16);
    card->set_stroke(0, Color());
    card->set_padding(32, 28);
    card->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    box->set_divider_spacing(12);
    box->set_gravity(Gravity::CenterHorizontal);

    auto badge = create_icon_badge("bluetooth", config->colors.surface, 56, 32);
    box->add_view(badge);

    auto title = TextViewBuilder::create()
        ->text("Bluetooth is Turned Off")
        ->bold(true)
        ->textSize(config->metrics.h2_size)
        ->center(true)
        ->textColor(config->colors.on_surface)
        ->build();
    box->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text("Turn on Bluetooth to discover and connect your wireless headphones, mice, keyboards, and other accessories.")
        ->textSize(config->metrics.font_size)
        ->center(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    box->add_view(desc);

    auto btn_on = ButtonBuilder::create()
        ->text("Turn On Bluetooth")
        ->primary(true)
        ->textSize(config->metrics.font_size)
        ->cornerRadius(18)
        ->padding(20, 8)
        ->onClick([this]() {
            toggle_power(true);
        })
        ->build();
    box->add_view(btn_on);

    card->add_view(box);
    return card;
}

void BluetoothDetailView::toggle_power(bool power) {
    m_powered = power;
    BluetoothBackend::set_powered(power);
    if (!power && m_scanning) {
        toggle_scan();
    }
    load_devices();
    setup_ui();
}

void BluetoothDetailView::toggle_connection(const std::string& mac) {
    for (auto& dev : m_paired_devices) {
        if (dev.mac == mac) {
            dev.is_connected = !dev.is_connected;
            dev.connection_info = dev.is_connected ? "Connecting..." : "Disconnecting...";

            if (dev.is_connected) {
                BluetoothBackend::connect_device(mac);
            } else {
                BluetoothBackend::disconnect_device(mac);
            }
            break;
        }
    }
    setup_ui();
}

void BluetoothDetailView::on_pair_device(const std::string& mac) {
    BluetoothBackend::pair_and_connect(mac);
    // Refresh after delay if no modal is active
    if (auto engine = AppEngine::instance()) {
        engine->post_delayed(3500, [this]() {
            if (!m_active_pairing_modal) {
                load_devices();
                setup_ui();
            }
        });
    }
}

void BluetoothDetailView::on_forget_device(const std::string& mac) {
    BluetoothBackend::forget_device(mac);
    load_devices();
    setup_ui();
}

void BluetoothDetailView::on_send_file() {
    if (!BluetoothBackend::is_obex_installed()) {
        auto modal = ModalOverlay::create_package_required_dialog(
            "bluez-obex",
            "Sending files over Bluetooth requires the BlueZ OBEX daemon and tools."
        );
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
        return;
    }
    if (m_paired_devices.empty()) {
        auto modal = std::make_shared<ModalOverlay>();
        modal->set_icon("dialog-information");
        modal->set_title("No Paired Devices Found");
        modal->set_message("To send a file via Bluetooth, first pair your smartphone, tablet, or another computer.");
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
        return;
    }
    BluetoothBackend::launch_send_file();
}

void BluetoothDetailView::on_receive_file() {
    if (!BluetoothBackend::is_obex_installed()) {
        auto modal = ModalOverlay::create_package_required_dialog(
            "bluez-obex",
            "Receiving files over Bluetooth requires the BlueZ OBEX background service."
        );
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
        return;
    }
    std::system("systemctl --user start obex.service 2>/dev/null; xdg-open \"$HOME/Downloads\" 2>/dev/null &");
}

void BluetoothDetailView::toggle_scan() {
    m_scanning = !m_scanning;
    if (m_scanning) {
        BluetoothBackend::start_scan();
        start_polling();
    } else {
        BluetoothBackend::stop_scan();
        stop_polling();
    }
    if (m_powered) {
        m_available_devices = BluetoothBackend::get_available_devices();
    }
    setup_ui();
}

} // namespace miqu
