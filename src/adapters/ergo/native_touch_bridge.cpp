// @implements spec/feature/pointer-controls.md
#include "konbini/adapters/ergo/native_touch_bridge.h"
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
    app::PointerSample pending;
#ifdef _WIN32
    struct Contact { DWORD id=0; double x=0,y=0; bool used=false; };
    std::array<Contact,32> contacts{};
    HWND window=nullptr;
    DWORD error=0;
    bool multi=false;
    void cancel() noexcept {
        contacts={}; pending.down=false; pending.cancelled=true; multi=false;
    }
    struct Position { double x=0,y=0,distance=0; DWORD first=0,second=0; unsigned count=0; };
    Position position() const noexcept {
        Position p;
        const Contact* first=nullptr;
        for(const auto& c:contacts) {
            if(!c.used) continue;
            if(!first) { first=&c;p.x=c.x;p.y=c.y;p.first=c.id;p.count=1; }
            else {
                p.x=(p.x+c.x)/2;p.y=(p.y+c.y)/2;
                p.distance=std::hypot(first->x-c.x,first->y-c.y);
                p.second=c.id;p.count=2;break;
            }
        }
        return p;
    }
    void touch(WPARAM wParam,LPARAM lParam) noexcept {
        const auto handle=reinterpret_cast<HTOUCHINPUT>(lParam);
        std::array<TOUCHINPUT,32> inputs{};
        const UINT count=LOWORD(wParam);
        const auto before=position();
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
                auto found=std::find_if(contacts.begin(),contacts.end(),[&](const auto& c){return c.used && c.id==input.dwID;});
                if(input.dwFlags&TOUCHEVENTF_DOWN) {
                    if(found==contacts.end()) found=std::find_if(contacts.begin(),contacts.end(),[](const auto& c){return !c.used;});
                    if(found==contacts.end()) {error=ERROR_INSUFFICIENT_BUFFER;cancel();break;}
                    if(!position().count) {
                        pending.pressed=true;
                        pending.pressXPixels=point.x;pending.pressYPixels=point.y;
                        multi=false;
                    }
                    *found={input.dwID,static_cast<double>(point.x),static_cast<double>(point.y),true};
                }
                if(found==contacts.end()) continue;
                found->x=point.x;found->y=point.y;
                pending.xPixels=point.x;pending.yPixels=point.y;
                if(input.dwFlags&TOUCHEVENTF_UP) {
                    found->used=false;
                    if(!position().count) pending.released=true;
                }
                if(position().count>1) multi=true;
            }
            const auto after=position();
            if(after.count) {pending.xPixels=after.x;pending.yPixels=after.y;}
            if(before.count && after.count && before.first==after.first && before.second==after.second) {
                pending.deltaXPixels+=after.x-before.x;
                pending.deltaYPixels+=after.y-before.y;
                if(before.count==2 && before.distance>1 && after.distance>1)
                    pending.pinchRatio*=after.distance/before.distance;
            }
            pending.down=after.count!=0;
            pending.released=pending.released || (before.count && !after.count);
            pending.multipleContacts=multi;
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
#endif
}
std::optional<app::PointerSample> NativeTouchBridge::consume() {
#ifdef _WIN32
    if(impl_->error) {
        const auto error=std::exchange(impl_->error,0);
        throw std::system_error(error,std::system_category(),"native touch input");
    }
    auto sample=impl_->pending;
    impl_->pending.pressed=false;impl_->pending.released=false;impl_->pending.cancelled=false;
    impl_->pending.deltaXPixels=0;impl_->pending.deltaYPixels=0;impl_->pending.pinchRatio=1;
    sample.isTouch=true;
    if(sample.down || sample.pressed || sample.released || sample.cancelled) return sample;
#endif
    return {};
}
}
