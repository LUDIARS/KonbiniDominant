// @implements spec/feature/pointer-controls.md
// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/adapters/ergo/native_touch_bridge.h"
#include "konbini/app/touch_contacts.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <system_error>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef NOGDI
#undef NOGDI
#endif
#include <windows.h>
#include <commctrl.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

namespace konbini::adapters::ergo {
struct NativeTouchBridge::Impl {
#ifdef _WIN32
    // The contact table, edge keeping and pinch / drag accumulation are the
    // shared game-owned `TouchContacts`; this adapter only converts WM_TOUCH
    // into normalized `TouchSample`s.
    app::TouchContacts contacts;
    HWND window=nullptr;
    DWORD error=0;
    DWORD lastTick=0;
    double elapsedSeconds=0;
    bool hasTick=false;
    void cancel() noexcept { contacts.cancel(); }
    // TOUCHINPUT::dwTime is a 32-bit millisecond tick that wraps after ~49
    // days. Unsigned differences keep the timestamp monotonic across a wrap.
    double timestamp(DWORD tick) noexcept {
        if(hasTick) elapsedSeconds+=static_cast<DWORD>(tick-lastTick)/1000.0;
        lastTick=tick;hasTick=true;
        return elapsedSeconds;
    }
    void touch(WPARAM wParam,LPARAM lParam) noexcept {
        const auto handle=reinterpret_cast<HTOUCHINPUT>(lParam);
        std::array<TOUCHINPUT,app::TouchContacts::kCapacity> inputs{};
        const UINT count=LOWORD(wParam);
        if(count>inputs.size() || !GetTouchInputInfo(handle,count,inputs.data(),sizeof(TOUCHINPUT))) {
            error=count>inputs.size()?ERROR_INSUFFICIENT_BUFFER:GetLastError();
            if(!error) error=ERROR_INVALID_DATA;
            cancel();
        } else {
            for(UINT i=0;i<count;++i) {
                const auto& input=inputs[i];
                POINT point{input.x/100,input.y/100};
                if(!ScreenToClient(window,&point)) {
                    error=GetLastError();if(!error) error=ERROR_INVALID_DATA;cancel();break;
                }
                const auto phase=(input.dwFlags&TOUCHEVENTF_DOWN)?app::TouchPhase::Down:
                    (input.dwFlags&TOUCHEVENTF_UP)?app::TouchPhase::Up:app::TouchPhase::Move;
                try {
                    contacts.update(app::normalizeTouchSample(input.dwID,phase,point.x,point.y,timestamp(input.dwTime)));
                } catch(const std::runtime_error&) {
                    error=ERROR_INSUFFICIENT_BUFFER;break;
                } catch(...) {
                    error=ERROR_INVALID_DATA;break;
                }
            }
        }
        // Every handled WM_TOUCH owns its handle, including malformed/error packets.
        if(!CloseTouchInputHandle(handle) && !error) error=GetLastError();
    }
    static LRESULT CALLBACK callback(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam,
                                      UINT_PTR id,DWORD_PTR reference) noexcept {
        auto& self=*reinterpret_cast<Impl*>(reference);
        if(message==WM_TOUCH) {self.touch(wParam,lParam);return 0;}
        // Windows also promotes touch to mouse. Consume those messages once here.
        if(message>=WM_MOUSEFIRST && message<=WM_MOUSELAST &&
           (static_cast<ULONG_PTR>(GetMessageExtraInfo())&0xFFFFFF80u)==0xFF515780u) return 0;
        if(message==WM_CANCELMODE || message==WM_KILLFOCUS) self.cancel();
        if(message==WM_NCDESTROY) {
            self.cancel(); self.window=nullptr;
            RemoveWindowSubclass(hwnd,callback,id);
        }
        return DefSubclassProc(hwnd,message,wParam,lParam);
    }
#endif
};
NativeTouchBridge::NativeTouchBridge():impl_(std::make_unique<Impl>()) {}
NativeTouchBridge::~NativeTouchBridge() {detach();}
void NativeTouchBridge::attach(GLFWwindow* window) {
#ifdef _WIN32
    if(!window || impl_->window) throw std::invalid_argument("invalid touch bridge attachment");
    const auto handle=glfwGetWin32Window(window);
    if(!RegisterTouchWindow(handle,0)) throw std::system_error(GetLastError(),std::system_category(),"RegisterTouchWindow");
    if(!SetWindowSubclass(handle,Impl::callback,reinterpret_cast<UINT_PTR>(impl_.get()),reinterpret_cast<DWORD_PTR>(impl_.get()))) {
        const auto error=GetLastError();
        UnregisterTouchWindow(handle);
        throw std::system_error(error,std::system_category(),"SetWindowSubclass");
    }
    impl_->window=handle;
#else
    // The distributed native touch backend targets Windows; GLFW mouse input
    // remains available on other platforms without claiming native multitouch.
    (void)window;
#endif
}
void NativeTouchBridge::detach() noexcept {
#ifdef _WIN32
    if(impl_->window) {
        RemoveWindowSubclass(impl_->window,Impl::callback,reinterpret_cast<UINT_PTR>(impl_.get()));
        UnregisterTouchWindow(impl_->window);
        impl_->window=nullptr;
    }
    impl_->error=0;
    impl_->cancel();
    impl_->hasTick=false;
#endif
}
std::optional<app::PointerSample> NativeTouchBridge::consume() {
#ifdef _WIN32
    if(impl_->error) {
        const auto error=std::exchange(impl_->error,0);
        throw std::system_error(error,std::system_category(),"native touch input");
    }
    const auto sample=impl_->contacts.consume();
    if(sample.down || sample.pressed || sample.released || sample.cancelled) return sample;
#endif
    return {};
}
}
