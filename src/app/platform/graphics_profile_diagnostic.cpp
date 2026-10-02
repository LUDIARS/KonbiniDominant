#include "konbini/app/platform/graphics_profile_diagnostic.h"

#include <fstream>
#include <stdexcept>
#include <string>

// @implements spec/interface/mobile-platform.md Surface and renderer

namespace konbini::app {

GraphicsProfileDiagnostic::GraphicsProfileDiagnostic(
    const WritablePathProvider& paths,
    const MobileGraphicsProfileSelection& selection)
    : file_(paths.resolve(WritableArea::Diagnostic,
                          kGraphicsProfileDiagnosticName)),
      selection_(selection) {
    append(describeMobileGraphicsProfileSelection(selection_), true);
}

void GraphicsProfileDiagnostic::recordThermalChange(
    const ThermalLevel level) const {
    std::string line = "thermal=";
    line += thermalLevelName(level);
    line += " graphics-profile=";
    line += mobileGraphicsProfile(selection_.profile).name;
    line += " held";
    append(line, false);
}

const std::filesystem::path& GraphicsProfileDiagnostic::file() const noexcept {
    return file_;
}

const MobileGraphicsProfileSelection& GraphicsProfileDiagnostic::selection()
    const noexcept {
    return selection_;
}

void GraphicsProfileDiagnostic::append(const std::string_view line,
                                       const bool truncate) const {
    std::ofstream output(file_, truncate ? std::ios::out | std::ios::trunc
                                         : std::ios::out | std::ios::app);
    output << line << '\n';
    output.flush();
    if (!output) {
        throw std::runtime_error("cannot write graphics profile diagnostic: " +
                                 file_.string());
    }
}

}  // namespace konbini::app
