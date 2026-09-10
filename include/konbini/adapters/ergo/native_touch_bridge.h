#pragma once
#include <memory>
#include <optional>
#include "konbini/app/pointer_sample.h"
struct GLFWwindow;
namespace konbini::adapters::ergo {
// Owns only the native touch registration/subclass. GLFW retains window ownership.
class NativeTouchBridge {
public:
    NativeTouchBridge();
    ~NativeTouchBridge();
    NativeTouchBridge(const NativeTouchBridge&)=delete;
    NativeTouchBridge& operator=(const NativeTouchBridge&)=delete;
    void attach(GLFWwindow* window);
    void detach() noexcept;
    std::optional<app::PointerSample> consume();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
