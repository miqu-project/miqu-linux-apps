#pragma once

#include <miqutoolkit/view/card_view.hpp>

namespace miqu {

class ColumnCardView : public CardView {
public:
    ColumnCardView() = default;
    ~ColumnCardView() override = default;

    bool on_mouse_move(int lx, int ly, const Rect& bounds) override {
        bool was_hovered = m_hovered;
        m_hovered = bounds.contains(Point(lx, ly));
        bool child_handled = CardView::on_mouse_move(lx, ly, bounds);
        return (m_hovered != was_hovered) || m_hovered || child_handled;
    }

    bool on_scroll(double delta) override {
        if (!m_hovered) {
            return false;
        }
        CardView::on_scroll(delta);
        return true; // Exclusively consume scroll so events never leak across columns
    }

    bool on_key(const KeyPressEvent& event) override {
        // Prevent child views from intercepting global keys from ViewGroup::on_key;
        // FilesWindow explicitly controls navigation in the active column
        return false;
    }

    bool is_hovered() const { return m_hovered; }

private:
    bool m_hovered = false;
};

} // namespace miqu
