#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace miqu {

enum class SettingType {
    Category,
    Toggle,
    SliderControl,
    Selection,
    Info
};

struct SettingItem : public std::enable_shared_from_this<SettingItem> {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string icon_name;
    SettingType type = SettingType::Category;

    bool bool_val = false;
    float float_val = 0.5f;
    std::vector<std::string> options;
    int selected_option_idx = 0;

    std::vector<std::shared_ptr<SettingItem>> children;
    std::weak_ptr<SettingItem> parent;

    bool is_leaf() const {
        return children.empty() && type != SettingType::Category;
    }

    std::string get_status_text() const;

    static std::shared_ptr<SettingItem> create_default_tree();
};

} // namespace miqu
