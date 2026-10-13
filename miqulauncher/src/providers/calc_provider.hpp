#pragma once

#include "model/launcher_item.hpp"
#include <string>
#include <optional>

namespace miqu {

class CalcProvider {
public:
    CalcProvider() = default;

    std::optional<LauncherItem> evaluate(const std::string& query);
    void activate(const LauncherItem& item);

private:
    static bool is_math_query(const std::string& query);
    static std::string format_number(double val);
};

} // namespace miqu
