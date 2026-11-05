#include "modules/bluetooth/bluetooth_detail_view.hpp"
#include "modules/bluetooth/bluetooth_backend.hpp"
#include "modules/bluetooth/bluetooth_agent.hpp"
#include "ui/common/modal_overlay.hpp"
#include <miqutoolkit/core/config.hpp>
#include <miqutoolkit/core/app_engine.hpp>
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
        card->set_card_background_color(config->colors.surface);
        card->set_radius(config->metrics.corner_radius);
        card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
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
        code_box->set_card_background_color(config->colors.surface_variant);
        code_box->set_radius(std::max(4, config->metrics.corner_radius - 4));
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

    // 1. Adapter Hero
    m_content_layout->add_view(create_hero_card());

    if (m_powered) {
        // 2. Action Row (Send File, Receive Files, Pair Device)
        m_content_layout->add_view(create_action_row());

        // 3. Discovery Banner if scanning
        if (m_scanning) {
            m_content_layout->add_view(create_discovery_card());
        }

        // 4. Paired Device Groups
        bool has_audio = false;
        bool has_input = false;
        bool has_other = false;
        for (const auto& dev : m_paired_devices) {
            if (dev.category == BluetoothDeviceCategory::Audio) has_audio = true;
            else if (dev.category == BluetoothDeviceCategory::Input) has_input = true;
            else has_other = true;
        }

        if (has_audio) {
            m_content_layout->add_view(create_category_section("AUDIO DEVICES", BluetoothDeviceCategory::Audio));
        }
        if (has_input) {
            m_content_layout->add_view(create_category_section("MOUSE, KEYBOARD & PEN", BluetoothDeviceCategory::Input));
        }
        if (has_other) {
            m_content_layout->add_view(create_category_section("OTHER PAIRED DEVICES", BluetoothDeviceCategory::Other));
        }

        // 5. Empty state if no paired devices
        if (m_paired_devices.empty() && !m_scanning) {
            auto card = std::make_shared<CardView>();
            card->set_card_background_color(config->colors.surface);
            card->set_radius(config->metrics.corner_radius);
            card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
            card->set_padding(18);

            auto empty_text = TextViewBuilder::create()
                ->text("No paired devices found.\nPut your Bluetooth headphones, mouse, keyboard, or stylus into pairing mode and click 'Scan for Devices' to pair.")
                ->textSize(config->metrics.font_size)
                ->textColor(config->colors.on_surface_variant)
                ->center(true)
                ->build();
            card->add_view(empty_text);
            m_content_layout->add_view(card);
        }

        // 6. Available Devices Section
        if (!m_available_devices.empty() || m_scanning) {
            m_content_layout->add_view(create_available_section());
        }

        // 7. File Transfer Information & Settings
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

std::shared_ptr<View> BluetoothDetailView::create_hero_card() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto hero = std::make_shared<CardView>();
    hero->set_card_background_color(config->colors.surface);
    hero->set_radius(config->metrics.corner_radius);
    hero->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    hero->set_padding(16);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(14);
    row->set_gravity(Gravity::CenterVertical);

    // Bluetooth Icon
    auto icon = std::make_shared<ImageView>(m_powered ? "bluetooth-active" : "bluetooth");
    icon->set_target_size(m_is_fullscreen ? 38 : 32);
    LayoutParams icon_lp(m_is_fullscreen ? 38 : 32, m_is_fullscreen ? 38 : 32);
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    // Text column
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text("Bluetooth")
        ->bold(true)
        ->textSize(m_is_fullscreen ? config->metrics.h1_size : config->metrics.h2_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub;
    if (!m_adapter_info.available) {
        sub = "No controller detected · Connect a Bluetooth adapter";
    } else if (m_powered) {
        sub = "Discoverable as \"" + m_adapter_info.name + "\" · Active";
    } else {
        sub = "Disabled · Radios Powered Off";
    }

    auto subtitle = TextViewBuilder::create()
        ->text(sub)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
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
    row->add_view(sw);

    // Fullscreen toggle button if in 3-pillar mode
    if (!m_is_fullscreen) {
        auto btn_expand = ButtonBuilder::create()
            ->text("⛶")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.primary)
            ->backgroundColor(config->colors.surface_variant)
            ->cornerRadius(btn_radius)
            ->onClick([this]() {
                if (m_on_request_fullscreen) m_on_request_fullscreen();
            })
            ->build();
        LayoutParams btn_lp(32, 32);
        btn_expand->set_layout_params(btn_lp);
        row->add_view(btn_expand);
    }

    hero->add_view(row);
    return hero;
}

std::shared_ptr<View> BluetoothDetailView::create_action_row() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto action_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    action_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    action_row->set_divider_spacing(10);

    // 1. Send File button
    auto btn_send = ButtonBuilder::create()
        ->text("↗ Send File")
        ->icon("document-send")
        ->iconSize(16)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->backgroundColor(config->colors.surface)
        ->bordered(true)
        ->borderColor(config->colors.outline_variant)
        ->borderWidth(config->metrics.border_width)
        ->cornerRadius(btn_radius)
        ->padding(12, 8)
        ->onClick([this]() { on_send_file(); })
        ->build();
    LayoutParams send_lp(0, 38, 1.0f);
    btn_send->set_layout_params(send_lp);
    action_row->add_view(btn_send);

    // 2. Receive Files button
    auto btn_recv = ButtonBuilder::create()
        ->text("↙ Receive Files")
        ->icon("folder-download")
        ->iconSize(16)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->backgroundColor(config->colors.surface)
        ->bordered(true)
        ->borderColor(config->colors.outline_variant)
        ->borderWidth(config->metrics.border_width)
        ->cornerRadius(btn_radius)
        ->padding(12, 8)
        ->onClick([this]() { on_receive_file(); })
        ->build();
    LayoutParams recv_lp(0, 38, 1.0f);
    btn_recv->set_layout_params(recv_lp);
    action_row->add_view(btn_recv);

    // 3. Scan for Devices button
    auto btn_pair = ButtonBuilder::create()
        ->text(m_scanning ? "󰐕 Scanning..." : "󰐕 Scan for Devices")
        ->icon(m_scanning ? "network-wireless-acquiring" : "list-add")
        ->iconSize(16)
        ->textSize(config->metrics.font_size)
        ->primary(!m_scanning)
        ->bordered(m_scanning)
        ->borderColor(config->colors.primary)
        ->borderWidth(config->metrics.border_width)
        ->cornerRadius(btn_radius)
        ->padding(12, 8)
        ->onClick([this]() { toggle_scan(); })
        ->build();
    LayoutParams pair_lp(0, 38, 1.0f);
    btn_pair->set_layout_params(pair_lp);
    action_row->add_view(btn_pair);

    return action_row;
}

std::shared_ptr<View> BluetoothDetailView::create_category_section(const std::string& title, BluetoothDeviceCategory cat) {
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
        ->textColor(config->colors.on_surface_variant)
        ->build();
    sec_container->add_view(lbl);

    // Card wrapping device rows
    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    card->set_padding(8);

    auto list_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    list_box->set_divider_spacing(8);

    for (auto& dev : m_paired_devices) {
        if (dev.category == cat) {
            list_box->add_view(create_device_row(dev));
        }
    }

    card->add_view(list_box);
    sec_container->add_view(card);
    return sec_container;
}

std::shared_ptr<View> BluetoothDetailView::create_device_row(BluetoothDeviceItem& dev) {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_padding(8, 6);
    row->set_divider_spacing(10);
    row->set_gravity(Gravity::CenterVertical);

    // Device Icon
    auto icon = std::make_shared<ImageView>(dev.icon_name.empty() ? "bluetooth" : dev.icon_name);
    icon->set_target_size(24);
    LayoutParams icon_lp(24, 24);
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    // Name & Subtitle
    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text(dev.name)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto sub_view = TextViewBuilder::create()
        ->text(dev.connection_info)
        ->textSize(config->metrics.caption_size)
        ->textColor(dev.is_connected ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub_view);
    row->add_view(text_col);

    // Connect / Disconnect button
    auto action_btn = ButtonBuilder::create()
        ->text(dev.is_connected ? "Disconnect" : "Connect")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->padding(10, 4)
        ->onClick([this, mac = dev.mac]() {
            toggle_connection(mac);
        });

    if (dev.is_connected) {
        action_btn->bordered(true)
                  ->borderColor(config->colors.outline_variant)
                  ->borderWidth(config->metrics.border_width)
                  ->backgroundColor(config->colors.surface_variant)
                  ->textColor(config->colors.on_surface);
    } else {
        action_btn->primary(true);
    }

    auto btn_view = action_btn->build();
    LayoutParams btn_lp(90, 30);
    btn_lp.gravity = Gravity::CenterVertical;
    btn_view->set_layout_params(btn_lp);
    row->add_view(btn_view);

    // Forget button
    auto btn_forget = ButtonBuilder::create()
        ->text("✕")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->backgroundColor(config->colors.surface_variant)
        ->textColor(config->colors.on_surface_variant)
        ->onClick([this, mac = dev.mac]() {
            on_forget_device(mac);
        })
        ->build();
    LayoutParams forget_lp(30, 30);
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

    auto lbl = TextViewBuilder::create()
        ->text("AVAILABLE DEVICES (NEARBY)")
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    sec_container->add_view(lbl);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    card->set_padding(8);

    auto list_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    list_box->set_divider_spacing(8);

    if (m_available_devices.empty()) {
        auto msg = TextViewBuilder::create()
            ->text(m_scanning ? "Scanning for nearby discoverable Bluetooth devices..." : "No new devices found nearby.")
            ->textSize(config->metrics.font_size)
            ->textColor(config->colors.on_surface_variant)
            ->padding(8)
            ->build();
        list_box->add_view(msg);
    } else {
        for (auto& dev : m_available_devices) {
            list_box->add_view(create_available_row(dev));
        }
    }

    card->add_view(list_box);
    sec_container->add_view(card);
    return sec_container;
}

std::shared_ptr<View> BluetoothDetailView::create_available_row(BluetoothDeviceItem& dev) {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_padding(8, 6);
    row->set_divider_spacing(10);
    row->set_gravity(Gravity::CenterVertical);

    auto icon = std::make_shared<ImageView>("bluetooth");
    icon->set_target_size(24);
    LayoutParams icon_lp(24, 24);
    icon_lp.gravity = Gravity::CenterVertical;
    row->add_view(icon, icon_lp);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text(dev.name)
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("MAC: " + dev.mac)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub);
    row->add_view(text_col);

    auto btn_pair = ButtonBuilder::create()
        ->text("Pair & Connect")
        ->primary(true)
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->padding(10, 4)
        ->onClick([this, mac = dev.mac]() {
            on_pair_device(mac);
        })
        ->build();
    LayoutParams btn_lp(110, 30);
    btn_lp.gravity = Gravity::CenterVertical;
    btn_pair->set_layout_params(btn_lp);
    row->add_view(btn_pair);

    return row;
}

std::shared_ptr<View> BluetoothDetailView::create_discovery_card() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface_variant);
    card->set_radius(config->metrics.corner_radius);
    card->set_stroke(config->metrics.border_width, config->colors.primary);
    card->set_padding(12);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(12);
    row->set_gravity(Gravity::CenterVertical);

    auto icon = std::make_shared<ImageView>("network-wireless-acquiring");
    icon->set_target_size(24);
    row->add_view(icon);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text("Scanning for Nearby Devices...")
        ->bold(true)
        ->textSize(config->metrics.h3_size)
        ->textColor(config->colors.primary)
        ->build();
    text_col->add_view(title);

    auto sub = TextViewBuilder::create()
        ->text("Put your Bluetooth mouse, headphones, or stylus into pairing mode.")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    text_col->add_view(sub);
    row->add_view(text_col);

    auto btn_stop = ButtonBuilder::create()
        ->text("Stop")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->padding(8, 4)
        ->onClick([this]() { toggle_scan(); })
        ->build();
    LayoutParams btn_lp(64, 28);
    btn_stop->set_layout_params(btn_lp);
    row->add_view(btn_stop);

    card->add_view(row);
    return card;
}

std::shared_ptr<View> BluetoothDetailView::create_file_transfer_card() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_stroke(config->metrics.border_width, config->colors.outline_variant);
    card->set_padding(14);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(12);
    row->set_gravity(Gravity::CenterVertical);

    auto icon = std::make_shared<ImageView>("folder-download");
    icon->set_target_size(24);
    row->add_view(icon);

    auto text_col = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams text_lp;
    text_lp.width = 0;
    text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    text_lp.weight = 1.0f;
    text_col->set_layout_params(text_lp);

    auto title = TextViewBuilder::create()
        ->text("Bluetooth File Transfers")
        ->bold(true)
        ->textSize(config->metrics.h3_size)
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
        ->cornerRadius(btn_radius)
        ->padding(10, 4)
        ->bordered(true)
        ->borderColor(config->colors.outline_variant)
        ->borderWidth(config->metrics.border_width)
        ->backgroundColor(config->colors.surface_variant)
        ->textColor(config->colors.on_surface)
        ->onClick([dl_dir]() {
            std::string cmd = "xdg-open \"" + dl_dir + "\" 2>/dev/null &";
            std::system(cmd.c_str());
        })
        ->build();
    LayoutParams btn_lp(100, 30);
    btn_open->set_layout_params(btn_lp);
    row->add_view(btn_open);

    card->add_view(row);
    return card;
}

std::shared_ptr<View> BluetoothDetailView::create_disabled_state() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto box = std::make_shared<LinearLayout>(Orientation::Vertical);
    box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    box->set_divider_spacing(14);
    box->set_padding(24);
    box->set_gravity(Gravity::CenterHorizontal);

    auto empty_icon = std::make_shared<ImageView>("bluetooth");
    empty_icon->set_target_size(48);
    empty_icon->set_dim(0.3f);
    box->add_view(empty_icon);

    auto title = TextViewBuilder::create()
        ->text("Bluetooth is Turned Off")
        ->bold(true)
        ->textSize(config->metrics.h2_size)
        ->center(true)
        ->textColor(config->colors.on_surface)
        ->build();
    box->add_view(title);

    auto desc = TextViewBuilder::create()
        ->text("Turn on Bluetooth to discover and connect your headphones, mouse, keyboard, or transfer files.")
        ->textSize(config->metrics.font_size)
        ->center(true)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    box->add_view(desc);

    auto btn_on = ButtonBuilder::create()
        ->text("Turn On Bluetooth")
        ->primary(true)
        ->textSize(config->metrics.font_size)
        ->cornerRadius(btn_radius)
        ->padding(16, 8)
        ->onClick([this]() {
            toggle_power(true);
        })
        ->build();
    box->add_view(btn_on);

    return box;
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
