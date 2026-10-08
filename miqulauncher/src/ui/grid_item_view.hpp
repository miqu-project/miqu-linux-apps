#pragma once

#include "model/launcher_item.hpp"
#include <miqutoolkit/view/grid_item_view.hpp>
#include <memory>

namespace miqu {

class LauncherGridItemView : public GridItemView {
public:
    explicit LauncherGridItemView(LauncherItem data);
    ~LauncherGridItemView() override = default;

    const LauncherItem& get_data() const { return m_data; }
    LauncherItem& get_data() { return m_data; }
    void set_data(LauncherItem data);

private:
    LauncherItem m_data;
};

} // namespace miqu
