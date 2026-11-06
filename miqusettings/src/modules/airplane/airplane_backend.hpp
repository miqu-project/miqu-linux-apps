#pragma once

#include "modules/airplane/airplane_types.hpp"

namespace miqu {

class AirplaneBackend {
public:
    static bool is_enabled();
    static AirplaneStatus get_status();
    static bool set_enabled(bool enabled);
    static bool toggle();
};

} // namespace miqu
