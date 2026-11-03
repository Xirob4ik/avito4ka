#pragma once

// Single place for user-visible product naming. Replace these (and CLIPLINE_* in CMakeLists.txt,
// and the glyph in UI/Logo.h) when the final name and logo are decided. See docs/BRANDING.md.

// Set by CMake from project(VERSION ...); the fallback only matters for ad-hoc builds.
#ifndef CLIPLINE_VERSION_STRING
 #define CLIPLINE_VERSION_STRING "0.0.0"
#endif

namespace clipline::branding
{
    inline constexpr const char* kVersion        = CLIPLINE_VERSION_STRING;
    inline constexpr const char* kProductName    = "Clipline";
    inline constexpr const char* kWordmark       = "CLIPLINE";      // drawn in the top bar
    inline constexpr const char* kCompanyName    = "Clipline Audio";
    inline constexpr const char* kUserDataFolder = "Clipline";      // %APPDATA%/<this>/Presets
    inline constexpr const char* kPresetExtension = "clpreset";
}
