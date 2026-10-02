#pragma once

#include <filesystem>
#include <string_view>

#include "konbini/app/platform/memory_pressure.h"
#include "konbini/app/platform/mobile_graphics_profile.h"
#include "konbini/app/platform/writable_paths.h"

// @implements spec/interface/mobile-platform.md Surface and renderer

namespace konbini::app {

inline constexpr std::string_view kGraphicsProfileDiagnosticName =
    "graphics-profile.log";

// Writes the selected mobile profile to the platform diagnostic root, then
// appends later thermal changes. Thermal changes never switch the profile at
// runtime: the facility mesh level is fixed at boot, so the record says the
// profile was held. A failed write throws `std::runtime_error`; a diagnostic
// the device validation cannot read is not a successful boot.
// @implements spec/interface/mobile-platform.md Surface and renderer
class GraphicsProfileDiagnostic {
public:
    // Truncates the record and writes the selection line.
    GraphicsProfileDiagnostic(const WritablePathProvider& paths,
                              const MobileGraphicsProfileSelection& selection);

    void recordThermalChange(ThermalLevel level) const;

    [[nodiscard]] const std::filesystem::path& file() const noexcept;
    [[nodiscard]] const MobileGraphicsProfileSelection& selection()
        const noexcept;

private:
    void append(std::string_view line, bool truncate) const;

    std::filesystem::path file_;
    MobileGraphicsProfileSelection selection_;
};

}  // namespace konbini::app
