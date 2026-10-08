#pragma once

#include "sys_info.hpp"
#include <miqutoolkit/miqutoolkit.hpp>
#include <memory>
#include <string>

namespace miquinfo {

class PageBuilder {
public:
    static std::shared_ptr<miqu::LinearLayout> create_spec_row(const std::string& label, const std::string& value);

    static std::shared_ptr<miqu::LinearLayout> create_circular_gauge_item(
        const std::string& title,
        float fraction,
        const std::string& percent_str,
        const std::string& detail_str,
        bool invert_threshold = false
    );

    static void populate_overview(const std::shared_ptr<miqu::LinearLayout>& col, const SystemInfo& info);
    static void populate_hardware(const std::shared_ptr<miqu::LinearLayout>& col, const SystemInfo& info);
    static void populate_thermals(const std::shared_ptr<miqu::LinearLayout>& col, const SystemInfo& info);
    static void populate_storage(const std::shared_ptr<miqu::LinearLayout>& col, const SystemInfo& info);
    static void populate_system(const std::shared_ptr<miqu::LinearLayout>& col, const SystemInfo& info);

    static std::shared_ptr<miqu::View> create_overview_page(const SystemInfo& info);
    static std::shared_ptr<miqu::View> create_hardware_page(const SystemInfo& info);
    static std::shared_ptr<miqu::View> create_thermals_page(const SystemInfo& info);
    static std::shared_ptr<miqu::View> create_storage_page(const SystemInfo& info);
    static std::shared_ptr<miqu::View> create_system_page(const SystemInfo& info);
};

} // namespace miquinfo
