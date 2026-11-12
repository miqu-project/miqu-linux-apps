#pragma once

#include "settings_model.hpp"
#include <string>
#include <vector>
#include <functional>

namespace miqu {

class XdgBackend {
public:
    // Initialize or refresh in-memory XDG MIME mappings cache (~0.1ms)
    static void init_cache();

    // Query the current default desktop ID for a given primary MIME type
    static std::string get_default_desktop_id(const std::string& primary_mime);

    // Query friendly display name for current default app of a primary MIME type
    static std::string get_default_app_name(const std::string& primary_mime, const std::string& fallback_name = "");

    // Populate candidate applications for a role dynamically (on demand)
    static void populate_role_candidates(DefaultAppRole& role);

    // Set the default application for a role (MIME types)
    // Synchronous execution (calls xdg-mime and updates in-memory cache)
    static bool set_default_app(const std::string& desktop_id, const std::string& mime_types_csv, std::string& error_msg);

    // Asynchronous execution of set_default_app
    static void set_default_app_async(const std::string& desktop_id, const std::string& mime_types_csv, std::function<void(bool success, const std::string& msg)> on_complete = nullptr);

    // Helper to resolve desktop path (e.g. "firefox.desktop" -> "/usr/share/applications/firefox.desktop")
    static std::string resolve_desktop_path(const std::string& desktop_id);

    // Helper to get friendly name from desktop file
    static std::string get_app_name_from_desktop(const std::string& desktop_id);

    // Helper to get app icon glyph from name/categories
    static std::string get_app_icon(const std::string& desktop_id, const std::string& name, const std::string& categories);
};

} // namespace miqu
