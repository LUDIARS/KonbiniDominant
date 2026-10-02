#include "android_thermal_monitor.h"

#include <dlfcn.h>

#include "konbini/app/platform/android_host_mapping.h"
#include "konbini/app/platform/lifecycle_event.h"

namespace konbini::android_host {

// Signatures of android/thermal.h (API 30). The status enum is an int.
struct AndroidThermalMonitor::Api {
    using Callback = void (*)(void*, int);
    void* (*acquire)() = nullptr;
    void (*release)(void*) = nullptr;
    int (*currentStatus)(void*) = nullptr;
    int (*registerListener)(void*, Callback, void*) = nullptr;
    int (*unregisterListener)(void*, Callback, void*) = nullptr;
};

namespace {

template <typename Function>
bool resolve(void* library, const char* name, Function& function) {
    function = reinterpret_cast<Function>(dlsym(library, name));
    return function != nullptr;
}

const AndroidThermalMonitor::Api* loadApi() {
    // libandroid stays loaded for the process; the handle is never closed.
    static const AndroidThermalMonitor::Api* api = []() -> const AndroidThermalMonitor::Api* {
        void* library = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
        if (library == nullptr) {
            return nullptr;
        }
        static AndroidThermalMonitor::Api resolved;
        const bool complete = resolve(library, "AThermal_acquireManager", resolved.acquire) &&
                              resolve(library, "AThermal_releaseManager", resolved.release) &&
                              resolve(library, "AThermal_getCurrentThermalStatus", resolved.currentStatus) &&
                              resolve(library, "AThermal_registerThermalStatusListener",
                                      resolved.registerListener) &&
                              resolve(library, "AThermal_unregisterThermalStatusListener",
                                      resolved.unregisterListener);
        return complete ? &resolved : nullptr;
    }();
    return api;
}

}  // namespace

AndroidThermalMonitor::AndroidThermalMonitor(app::LifecycleEventQueue& events)
    : events_(events), api_(loadApi()) {
    if (api_ == nullptr) {
        return;
    }
    manager_ = api_->acquire();
    if (manager_ != nullptr) {
        listening_ = api_->registerListener(manager_, &AndroidThermalMonitor::onStatus, this) == 0;
    }
}

AndroidThermalMonitor::~AndroidThermalMonitor() {
    if (manager_ == nullptr) {
        return;
    }
    if (listening_) {
        api_->unregisterListener(manager_, &AndroidThermalMonitor::onStatus, this);
    }
    api_->release(manager_);
}

std::optional<app::ThermalLevel> AndroidThermalMonitor::current() const noexcept {
    if (manager_ == nullptr) {
        return std::nullopt;
    }
    return app::thermalLevelFromAndroidStatus(api_->currentStatus(manager_));
}

bool AndroidThermalMonitor::available() const noexcept {
    return manager_ != nullptr;
}

void AndroidThermalMonitor::onStatus(void* self, const int status) {
    const auto level = app::thermalLevelFromAndroidStatus(status);
    if (level) {
        // A full queue latches overflow; the owner thread fails on drain.
        static_cast<void>(static_cast<AndroidThermalMonitor*>(self)->events_.push(
            app::LifecycleEvent::thermalPressure(*level)));
    }
}

}  // namespace konbini::android_host
