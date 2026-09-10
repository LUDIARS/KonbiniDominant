#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include "konbini/app/native_mobile_runtime.h"
#include "pictor/surface/android_surface_provider.h"
namespace {
using konbini::app::TouchPhase;
struct Host {
    android_app* app=nullptr;
    std::unique_ptr<konbini::app::NativeMobileRuntime> game;
    std::unique_ptr<pictor::AndroidSurfaceProvider> surface;
    bool focused=false,failed=false;
    double density() const {
        const auto value=AConfiguration_getDensity(app->config);
        return value>0 && value<1000?value/160.0:1.0;
    }
    void resize() {
        if(!surface || !app->window) return;
        const int rawWidth=ANativeWindow_getWidth(app->window),rawHeight=ANativeWindow_getHeight(app->window);
        if(rawWidth<=0 || rawHeight<=0) {game->pause(true);return;}
        const auto width=static_cast<std::uint32_t>(rawWidth),height=static_cast<std::uint32_t>(rawHeight);
        surface->update_window(app->window,width,height);
        game->resize({width,height},density());
        game->pause(!focused);
    }
    void error(const char* message) {
        __android_log_print(ANDROID_LOG_ERROR,"KonbiniDominant","%s",message);
        failed=true;ANativeActivity_finish(app->activity);
    }
};
std::filesystem::path extractAssets(android_app* app) {
    const auto root=std::filesystem::path(app->activity->internalDataPath)/"konbini";
    const char* names[]={"data/content/first-playable.json","shaders/konbini_world.vert.spv","shaders/konbini_world.frag.spv",
        "shaders/konbini_composite.vert.spv","shaders/konbini_composite.frag.spv","shaders/konbini_hud.vert.spv","shaders/konbini_hud.frag.spv"};
    for(const auto* name:names) {
        std::unique_ptr<AAsset,decltype(&AAsset_close)> asset(AAssetManager_open(app->activity->assetManager,name,AASSET_MODE_STREAMING),AAsset_close);
        if(!asset) throw std::runtime_error(std::string("missing packaged asset: ")+name);
        const auto destination=root/name;
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream output(destination,std::ios::binary|std::ios::trunc);
        char buffer[16384];int count=0;
        while((count=AAsset_read(asset.get(),buffer,sizeof(buffer)))>0) output.write(buffer,count);
        if(count<0 || !output) throw std::runtime_error(std::string("cannot extract asset: ")+name);
    }
    return root;
}
void command(android_app* app,int32_t code) {
    auto& host=*static_cast<Host*>(app->userData);
    try {
        if(code==APP_CMD_INIT_WINDOW && app->window) {
            const int rawWidth=ANativeWindow_getWidth(app->window),rawHeight=ANativeWindow_getHeight(app->window);
            if(rawWidth<=0 || rawHeight<=0) throw std::runtime_error("Android window has no drawable extent");
            const auto width=static_cast<std::uint32_t>(rawWidth),height=static_cast<std::uint32_t>(rawHeight);
            if(!host.game) host.game=std::make_unique<konbini::app::NativeMobileRuntime>(extractAssets(app));
            host.game->detach();
            host.surface=std::make_unique<pictor::AndroidSurfaceProvider>(app->window,width,height);
            host.game->attach(*host.surface,{width,height},host.density());
            host.game->pause(!host.focused);
        } else if(code==APP_CMD_TERM_WINDOW) {
            if(host.game) host.game->detach();
            host.surface.reset();
        } else if(code==APP_CMD_GAINED_FOCUS || code==APP_CMD_LOST_FOCUS) {
            host.focused=code==APP_CMD_GAINED_FOCUS;
            if(host.game) host.game->pause(!host.focused);
        } else if(code==APP_CMD_WINDOW_RESIZED || code==APP_CMD_CONFIG_CHANGED) host.resize();
    } catch(const std::exception& error) {host.error(error.what());}
}
int32_t input(android_app* app,AInputEvent* event) {
    auto& host=*static_cast<Host*>(app->userData);
    if(!host.game || AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION) return 0;
    try {
        const auto action=AMotionEvent_getAction(event);
        const auto kind=action&AMOTION_EVENT_ACTION_MASK;
        if(kind==AMOTION_EVENT_ACTION_CANCEL) {host.game->touch(0,TouchPhase::Cancel,0,0);return 1;}
        const auto index=static_cast<std::size_t>((action&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        const auto count=AMotionEvent_getPointerCount(event);
        for(std::size_t i=0;i<count;++i) {
            if(kind!=AMOTION_EVENT_ACTION_MOVE && i!=index) continue;
            TouchPhase phase;
            if(kind==AMOTION_EVENT_ACTION_DOWN || kind==AMOTION_EVENT_ACTION_POINTER_DOWN) phase=TouchPhase::Down;
            else if(kind==AMOTION_EVENT_ACTION_UP || kind==AMOTION_EVENT_ACTION_POINTER_UP) phase=TouchPhase::Up;
            else if(kind==AMOTION_EVENT_ACTION_MOVE) phase=TouchPhase::Move;
            else continue;
            host.game->touch(static_cast<std::uint64_t>(AMotionEvent_getPointerId(event,i)),phase,AMotionEvent_getX(event,i),AMotionEvent_getY(event,i));
        }
    } catch(const std::exception& error) {host.error(error.what());}
    return 1;
}
}
extern "C" void android_main(android_app* app) {
    app_dummy();
    Host host;host.app=app;
    app->userData=&host;app->onAppCmd=command;app->onInputEvent=input;
    while(!app->destroyRequested) {
        int events=0;android_poll_source* source=nullptr;
        const int timeout=host.surface && host.focused && !host.failed?0:-1;
        const auto result=ALooper_pollOnce(timeout,nullptr,&events,reinterpret_cast<void**>(&source));
        if(result>=0 && source) source->process(app,source);
        if(app->destroyRequested) break;
        if(host.surface && host.focused && !host.failed) try {
            const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            host.game->frame(now);
        } catch(const std::exception& error) {host.error(error.what());}
    }
    if(host.game) host.game->detach();
}
