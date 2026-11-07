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
    if (m_alive) {
        *m_alive = false;
    }
    stop_polling();
    if (m_scanning) {
        BluetoothBackend::stop_scan();
    }
    if (m_agent) {
        m_agent->stop();
        m_agent.reset();
    }
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

    m_poll_timer_id = engine->post_delayed(2000, [this, alive = m_alive]() {
        if (!*alive) return;
        m_poll_timer_id = 0;
        if (m_scanning && m_powered) {
            m_scan_ticks++;
            if (m_scan_ticks >= 10) { // Bounded scan window: auto-stop after 20s for 0% idle dormancy
                m_scanning = false;
                m_scan_ticks = 0;
                BluetoothBackend::stop_scan();
                load_devices();
                setup_ui();
                return;
            }
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

    m_agent->set_on_request_confirmation([this, alive = m_alive](const std::string& dev_name, const std::string& mac, uint32_t passkey, BluetoothAgent::ConfirmCallback cb) {
        if (!*alive) return;
        if (m_active_pairing_modal) {
            m_active_pairing_modal->dismiss();
            m_active_pairing_modal.reset();
        }
        auto modal = ModalOverlay::create_numeric_comparison_dialog(
            dev_name,
            mac,
            passkey,
            [this, alive, cb]() {
                if (!*alive) return;
                cb(true);
                m_active_pairing_modal.reset();
                load_devices();
                setup_ui();
            },
            [this, alive, cb]() {
                if (!*alive) return;
                cb(false);
                m_active_pairing_modal.reset();
            }
        );
        m_active_pairing_modal = modal;
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
    });

    m_agent->set_on_display_passkey([this, alive = m_alive](const std::string& dev_name, const std::string& mac, uint32_t passkey) {
        if (!*alive) return;
        if (m_active_pairing_modal) {
            m_active_pairing_modal->dismiss();
            m_active_pairing_modal.reset();
        }
        auto modal = ModalOverlay::create_passkey_display_dialog(
            dev_name,
            mac,
            passkey,
            [this, alive]() {
                if (!*alive) return;
                m_active_pairing_modal.reset();
            }
        );
        m_active_pairing_modal = modal;
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
    });

    m_agent->set_on_request_authorization([this, alive = m_alive](const std::string& dev_name, const std::string& mac, BluetoothAgent::AuthorizationCallback cb) {
        if (!*alive) return;
        if (m_active_pairing_modal) {
            m_active_pairing_modal->dismiss();
            m_active_pairing_modal.reset();
        }
        auto modal = ModalOverlay::create_authorization_dialog(
            dev_name,
            mac,
            [this, alive, cb]() {
                if (!*alive) return;
                cb(true);
                m_active_pairing_modal.reset();
                load_devices();
                setup_ui();
            },
            [this, alive, cb]() {
                if (!*alive) return;
                cb(false);
                m_active_pairing_modal.reset();
            }
        );
        m_active_pairing_modal = modal;
        modal->show_in(std::dynamic_pointer_cast<FrameLayout>(shared_from_this()));
    });

    m_agent->set_on_cancel([this, alive = m_alive]() {
        if (!*alive) return;
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
    m_content_layout->set_padding(28);

    // Dependency check: BlueZ suite
    if (!BluetoothBackend::is_bluetooth_installed()) {
        auto card = std::make_shared<CardView>();
        card->set_card_background_color(config->colors.surface);
        card->set_radius(config->metrics.corner_radius);
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
        // 2. Paired Devices Section (Clean, unified list in one tonal card)
        m_content_layout->add_view(create_paired_section());

        // 3. Available Devices Section (Scanning / Nearby devices)
        if (!m_available_devices.empty() || m_scanning) {
            m_content_layout->add_view(create_available_section());
        }

        // 4. File Transfers & Sharing (Send file + Open Folder together in proper place)
        m_content_layout->add_view(create_file_transfer_section());
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
    hero->set_padding(16);

    auto hero_layout = std::make_shared<LinearLayout>(Orientation::Vertical);
    hero_layout->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    hero_layout->set_divider_spacing(12);

    auto row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    row->set_divider_spacing(14);
    row->set_gravity(Gravity::CenterVertical);

    // Bluetooth Icon
    auto icon = std::make_shared<ImageView>(m_powered ? "bluetooth-active" : "bluetooth");
    icon->set_target_size(38);
    LayoutParams icon_lp(38, 38);
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
        ->textSize(config->metrics.h1_size)
        ->textColor(config->colors.on_surface)
        ->build();
    text_col->add_view(title);

    std::string sub;
    if (!m_adapter_info.available) {
        sub = "No controller detected · Connect a Bluetooth adapter";
    } else if (m_powered) {
        sub = "Discoverable as \"" + m_adapter_info.name + "\"";
    } else {
        sub = "Disabled · Radios powered off";
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

        
    hero_layout->add_view(row);

    // Android-style prominent 'Pair new device' action pill inside hero card
    if (m_powered) {
        auto btn_pair = ButtonBuilder::create()
            ->text(m_scanning ? "󰑐 Scanning for Nearby Devices... (Tap to Stop)" : "+ Pair New Device")
            ->textSize(config->metrics.font_size)
            ->primary(!m_scanning)
            ->backgroundColor(m_scanning ? config->colors.surface_variant : config->colors.primary)
            ->textColor(m_scanning ? config->colors.primary : config->colors.on_primary)
            ->cornerRadius(btn_radius + 2)
            ->padding(14, 8)
            ->onClick([this]() { toggle_scan(); })
            ->build();
        btn_pair->set_content_gravity(Gravity::Center);
        LayoutParams pair_lp(static_cast<int>(LayoutDimension::MatchParent), 38);
        btn_pair->set_layout_params(pair_lp);
        hero_layout->add_view(btn_pair);
    }

    hero->add_view(hero_layout);
    return hero;
}

std::shared_ptr<View> BluetoothDetailView::create_paired_section() {
    auto config = Config::get();
    auto sec_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    sec_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sec_container->set_divider_spacing(6);

    auto lbl = TextViewBuilder::create()
        ->text("PAIRED DEVICES")
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    sec_container->add_view(lbl);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(8);

    auto list_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    list_box->set_divider_spacing(6);

    if (m_paired_devices.empty()) {
        auto empty_text = TextViewBuilder::create()
            ->text("No paired devices yet. Tap '+ Pair New Device' above to search for nearby Bluetooth devices.")
            ->textSize(config->metrics.caption_size)
            ->textColor(config->colors.on_surface_variant)
            ->padding(8)
            ->build();
        list_box->add_view(empty_text);
    } else {
        for (auto& dev : m_paired_devices) {
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
        action_btn->backgroundColor(config->colors.surface_variant)
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
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto sec_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    sec_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sec_container->set_divider_spacing(6);

    // Header row with title and optional Stop action
    auto header_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    header_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    header_row->set_gravity(Gravity::CenterVertical);

    std::string header_text = m_scanning ? "AVAILABLE DEVICES (SCANNING...)" : "AVAILABLE DEVICES";
    auto lbl = TextViewBuilder::create()
        ->text(header_text)
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(m_scanning ? config->colors.primary : config->colors.on_surface_variant)
        ->build();
    LayoutParams lbl_lp;
    lbl_lp.width = 0;
    lbl_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    lbl_lp.weight = 1.0f;
    lbl->set_layout_params(lbl_lp);
    header_row->add_view(lbl);

    if (m_scanning) {
        auto btn_stop = ButtonBuilder::create()
            ->text("Stop")
            ->textSize(config->metrics.caption_size)
            ->cornerRadius(btn_radius)
            ->backgroundColor(config->colors.surface_variant)
            ->textColor(config->colors.on_surface)
            ->padding(10, 2)
            ->onClick([this]() { toggle_scan(); })
            ->build();
        LayoutParams stop_lp(56, 26);
        btn_stop->set_layout_params(stop_lp);
        header_row->add_view(btn_stop);
    }

    sec_container->add_view(header_row);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(8);

    auto list_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    list_box->set_divider_spacing(6);

    if (m_available_devices.empty()) {
        auto msg = TextViewBuilder::create()
            ->text(m_scanning ? "Scanning for nearby discoverable Bluetooth devices..." : "No new devices found nearby.")
            ->textSize(config->metrics.caption_size)
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
        ->text("Pair")
        ->primary(true)
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->padding(10, 4)
        ->onClick([this, mac = dev.mac]() {
            on_pair_device(mac);
        })
        ->build();
    LayoutParams btn_lp(70, 30);
    btn_lp.gravity = Gravity::CenterVertical;
    btn_pair->set_layout_params(btn_lp);
    row->add_view(btn_pair);

    return row;
}

std::shared_ptr<View> BluetoothDetailView::create_file_transfer_section() {
    auto config = Config::get();
    int btn_radius = config->metrics.corner_radius > 0 ? std::max(4, config->metrics.corner_radius - 4) : 8;

    auto sec_container = std::make_shared<LinearLayout>(Orientation::Vertical);
    sec_container->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    sec_container->set_divider_spacing(6);

    auto lbl = TextViewBuilder::create()
        ->text("FILES & SHARING")
        ->bold(true)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    sec_container->add_view(lbl);

    auto card = std::make_shared<CardView>();
    card->set_card_background_color(config->colors.surface);
    card->set_radius(config->metrics.corner_radius);
    card->set_padding(8);

    auto list_box = std::make_shared<LinearLayout>(Orientation::Vertical);
    list_box->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    list_box->set_divider_spacing(6);

    // Row 1: Send Files
    auto send_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    send_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    send_row->set_padding(8, 6);
    send_row->set_divider_spacing(10);
    send_row->set_gravity(Gravity::CenterVertical);

    auto send_icon = std::make_shared<ImageView>("document-send");
    send_icon->set_target_size(24);
    LayoutParams s_icon_lp(24, 24);
    s_icon_lp.gravity = Gravity::CenterVertical;
    send_row->add_view(send_icon, s_icon_lp);

    auto send_text = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams s_text_lp;
    s_text_lp.width = 0;
    s_text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    s_text_lp.weight = 1.0f;
    send_text->set_layout_params(s_text_lp);

    auto send_title = TextViewBuilder::create()
        ->text("Send Files via Bluetooth")
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    send_text->add_view(send_title);

    auto send_sub = TextViewBuilder::create()
        ->text("Transfer photos, documents, and media to a paired device")
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    send_text->add_view(send_sub);
    send_row->add_view(send_text);

    auto btn_send = ButtonBuilder::create()
        ->text("Send Files")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->backgroundColor(config->colors.surface_variant)
        ->textColor(config->colors.on_surface)
        ->padding(12, 4)
        ->onClick([this]() { on_send_file(); })
        ->build();
    LayoutParams send_btn_lp(100, 30);
    send_btn_lp.gravity = Gravity::CenterVertical;
    btn_send->set_layout_params(send_btn_lp);
    send_row->add_view(btn_send);

    list_box->add_view(send_row);

    // Row 2: Received Files
    auto recv_row = std::make_shared<LinearLayout>(Orientation::Horizontal);
    recv_row->set_layout_params(LayoutParams(
        static_cast<int>(LayoutDimension::MatchParent),
        static_cast<int>(LayoutDimension::WrapContent)
    ));
    recv_row->set_padding(8, 6);
    recv_row->set_divider_spacing(10);
    recv_row->set_gravity(Gravity::CenterVertical);

    auto recv_icon = std::make_shared<ImageView>("folder-download");
    recv_icon->set_target_size(24);
    LayoutParams r_icon_lp(24, 24);
    r_icon_lp.gravity = Gravity::CenterVertical;
    recv_row->add_view(recv_icon, r_icon_lp);

    auto recv_text = std::make_shared<LinearLayout>(Orientation::Vertical);
    LayoutParams r_text_lp;
    r_text_lp.width = 0;
    r_text_lp.height = static_cast<int>(LayoutDimension::WrapContent);
    r_text_lp.weight = 1.0f;
    recv_text->set_layout_params(r_text_lp);

    auto recv_title = TextViewBuilder::create()
        ->text("Received Files Folder")
        ->bold(true)
        ->textSize(config->metrics.font_size)
        ->textColor(config->colors.on_surface)
        ->build();
    recv_text->add_view(recv_title);

    std::string dl_dir = BluetoothBackend::get_received_files_directory();
    auto recv_sub = TextViewBuilder::create()
        ->text(dl_dir)
        ->textSize(config->metrics.caption_size)
        ->textColor(config->colors.on_surface_variant)
        ->build();
    recv_text->add_view(recv_sub);
    recv_row->add_view(recv_text);

    auto btn_recv = ButtonBuilder::create()
        ->text("Open Folder")
        ->textSize(config->metrics.caption_size)
        ->cornerRadius(btn_radius)
        ->backgroundColor(config->colors.surface_variant)
        ->textColor(config->colors.on_surface)
        ->padding(12, 4)
        ->onClick([dl_dir]() {
            std::string cmd = "xdg-open \"" + dl_dir + "\" 2>/dev/null &";
            std::system(cmd.c_str());
        })
        ->build();
    LayoutParams recv_btn_lp(100, 30);
    recv_btn_lp.gravity = Gravity::CenterVertical;
    btn_recv->set_layout_params(recv_btn_lp);
    recv_row->add_view(btn_recv);

    list_box->add_view(recv_row);

    card->add_view(list_box);
    sec_container->add_view(card);
    return sec_container;
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
        engine->post_delayed(3500, [this, alive = m_alive]() {
            if (!*alive) return;
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
        m_scan_ticks = 0;
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
