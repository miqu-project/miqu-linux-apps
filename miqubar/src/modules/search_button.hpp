#pragma once

#include "miqutoolkit/view/button.hpp"

namespace miqubar {

class SearchButtonView : public miqu::Button {
public:
    SearchButtonView();
    ~SearchButtonView() override = default;

    void sync_style();

private:
    void launch_search();
};

} // namespace miqubar
