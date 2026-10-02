#include <android_native_app_glue.h>
#include <android/log.h>

#include <exception>

#include "android_host.h"

// NativeActivity entry point: owns the looper only. Every callback goes to
// `AndroidHost`, which forwards it to the shared platform boundary.
namespace {

void onCommand(android_app* app, const int32_t command) {
    static_cast<konbini::android_host::AndroidHost*>(app->userData)->onCommand(command);
}

int32_t onInput(android_app* app, AInputEvent* event) {
    return static_cast<konbini::android_host::AndroidHost*>(app->userData)->onInput(event);
}

}  // namespace

extern "C" void android_main(android_app* app) {
    konbini::android_host::AndroidHost host(app);
    try {
        host.boot();
    } catch (const std::exception& error) {
        // Missing assets, writable roots or city generation: no game starts.
        __android_log_print(ANDROID_LOG_ERROR, "KonbiniDominant", "boot failed: %s", error.what());
        ANativeActivity_finish(app->activity);
    }
    app->userData = &host;
    app->onAppCmd = onCommand;
    app->onInputEvent = onInput;
    while (!app->destroyRequested) {
        int events = 0;
        android_poll_source* source = nullptr;
        const int timeout = host.wantsFrames() ? 0 : -1;
        const int result = ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void**>(&source));
        if (result >= 0 && source != nullptr) {
            source->process(app, source);
        }
        if (app->destroyRequested) {
            break;
        }
        host.step();
    }
}
