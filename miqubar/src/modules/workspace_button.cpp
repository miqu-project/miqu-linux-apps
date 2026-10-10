#include "workspace_button.hpp"
#include "../config/bar_config.hpp"
#include "miqutoolkit/core/config.hpp"
#include "miqutoolkit/core/window.hpp"
#include "miqutoolkit/view/card_view.hpp"
#include "miqutoolkit/view/linear_layout.hpp"
#include <cmath>
#include <iostream>

namespace miqubar {

WorkspaceButtonView::WorkspaceButtonView() {
    auto config = miqu::Config::get();
    int fs = config->metrics.font_size > 0 ? config->metrics.font_size : 11;
    set_flat(true);
    set_text_size(fs);
    set_bold(true);
    set_padding(8, 2);
    set_icon("user-desktop");
    sync_workspaces();

    set_on_click_listener([this]() {
        if (m_flyout_window) {
            hide_flyout();
        } else {
            show_flyout();
        }
    });

    auto mgr = miqu::WorkspaceManager::get();
    if (mgr) {
        mgr->on_workspaces_changed([this]() {
            sync_workspaces();
        });
    }
}

WorkspaceButtonView::~WorkspaceButtonView() {
    hide_flyout();
}

void WorkspaceButtonView::sync_workspaces() {
    auto mgr = miqu::WorkspaceManager::get();
    if (mgr) {
        m_workspaces = mgr->get_workspaces();
        const auto* act = mgr->get_active_workspace();
        if (act) {
            m_active_id = act->id;
        }
    }
    if (m_workspaces.empty()) {
        miqu::WorkspaceInfo fallback;
        fallback.id = 1;
        fallback.name = "1";
        fallback.is_active = true;
        m_workspaces.push_back(fallback);
    }
    set_text(std::to_string(m_active_id));
    request_redraw();
}

void WorkspaceButtonView::show_flyout() {
    if (m_flyout_window) {
        hide_flyout();
        return;
    }
    sync_workspaces();

    auto config = miqu::Config::get();
    auto card = std::make_shared<miqu::CardView>();
    card->set_style(miqu::CardStyle::Outlined);
    card->set_radius(config->metrics.corner_radius);
    card->set_elevation(6);

    auto row = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    row->set_divider_spacing(6);
    row->set_padding(8, 8, 8, 8);

    for (const auto& ws : m_workspaces) {
        auto btn = std::make_shared<miqu::Button>(std::to_string(ws.id));
        btn->set_text_size(11);
        btn->set_bold(true);
        btn->set_layout_params(miqu::LayoutParams(34, 34));
        if (ws.id == m_active_id) {
            btn->set_selected(true);
        } else {
            btn->set_flat(true);
        }
        btn->set_on_click_listener([this, id = ws.id]() {
            auto mgr = miqu::WorkspaceManager::get();
            if (mgr) {
                mgr->activate_workspace(id);
            }
            hide_flyout();
            sync_workspaces();
        });
        row->add_view(btn);
    }
    card->add_view(row);

    int count = static_cast<int>(m_workspaces.size());
    int flyout_w = count * 40 + 16;
    int flyout_h = 50;

    const auto& cfg = BarConfig::get();
    bool is_top = (cfg.position == "top");
    miqu::Gravity gravity = is_top ? (miqu::Gravity::Top | miqu::Gravity::Left)
                                   : (miqu::Gravity::Bottom | miqu::Gravity::Left);
    miqu::Margin margin;
    margin.left = 12;
    if (is_top) {
        margin.top = cfg.height + 8;
    } else {
        margin.bottom = cfg.height + 8;
    }

    m_flyout_window = miqu::WindowBuilder::create()
        ->role(miqu::WindowRole::LayerOverlay)
        ->layerNamespace("miqubar-workspaces")
        ->contentSize(flyout_w, flyout_h)
        ->contentGravity(gravity)
        ->contentMargin(margin)
        ->dimBackdrop(false)
        ->transparent(true)
        ->exclusiveZone(-1)
        ->closeOnClickOutside(true)
        ->closeOnEscape(true)
        ->contentView(card)
        ->onClose([this]() {
            m_flyout_window = nullptr;
            request_redraw();
        })
        ->build();

    if (m_flyout_window) m_flyout_window->show();
}

void WorkspaceButtonView::hide_flyout() {
    if (m_flyout_window) {
        m_flyout_window->close();
        m_flyout_window = nullptr;
    }
}

bool WorkspaceButtonView::on_scroll(double delta) {
    if (!BarConfig::get().scroll_switch) return false;
    if (std::abs(delta) > 0.05) {
        cycle_workspace(delta > 0 ? -1 : 1);
        return true;
    }
    return false;
}

void WorkspaceButtonView::cycle_workspace(int delta) {
    if (m_workspaces.empty()) return;
    int cur_idx = 0;
    for (size_t i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces[i].id == m_active_id) {
            cur_idx = static_cast<int>(i);
            break;
        }
    }

    int next_idx = cur_idx + delta;
    if (next_idx < 0) next_idx = static_cast<int>(m_workspaces.size()) - 1;
    if (next_idx >= static_cast<int>(m_workspaces.size())) next_idx = 0;

    size_t target_id = m_workspaces[next_idx].id;
    auto mgr = miqu::WorkspaceManager::get();
    if (mgr) {
        mgr->activate_workspace(target_id);
    }
    sync_workspaces();
}

} // namespace miqubar
