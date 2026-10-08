#pragma once

#include "backend/security_backend.hpp"
#include <miqutoolkit/miqutoolkit.hpp>

namespace miqusecure {

class SandboxView : public miqu::ScrollView {
public:
    explicit SandboxView(const LsmInfo& info);
    ~SandboxView() override = default;

    void update_info(const LsmInfo& info);

private:
    void setup_posture_hero();
    void setup_defenses_section();
    void rebuild_flatpak_section();
    void update_status_indicator(bool active);

    LsmInfo m_info;
    std::shared_ptr<miqu::LinearLayout> m_layout;
    std::shared_ptr<miqu::FrameLayout> m_status_dot;
    std::shared_ptr<miqu::TextView> m_stack_lbl;
    std::shared_ptr<miqu::LinearLayout> m_flatpak_container;
};

} // namespace miqusecure
