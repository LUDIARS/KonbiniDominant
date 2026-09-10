#include "konbini/app/touch_contacts.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace konbini::app {
TouchContacts::Position TouchContacts::position() const noexcept {
    Position p;
    const Contact* first=nullptr;
    for(const auto& c:contacts_) if(c.used) {
        if(!first) {first=&c;p={c.x,c.y,0,c.id,0,1};}
        else {p.x=(p.x+c.x)/2;p.y=(p.y+c.y)/2;p.distance=std::hypot(first->x-c.x,first->y-c.y);p.second=c.id;p.count=2;break;}
    }
    return p;
}
void TouchContacts::cancel() noexcept {contacts_={};pending_={};pending_.cancelled=true;multiple_=false;}
void TouchContacts::update(std::uint64_t id,TouchPhase phase,double x,double y) {
    if(phase==TouchPhase::Cancel) {cancel();return;}
    if(!std::isfinite(x) || !std::isfinite(y)) {cancel();throw std::invalid_argument("non-finite touch position");}
    const auto before=position();
    auto found=std::find_if(contacts_.begin(),contacts_.end(),[&](const auto& c){return c.used && c.id==id;});
    if(phase==TouchPhase::Down) {
        if(found==contacts_.end()) found=std::find_if(contacts_.begin(),contacts_.end(),[](const auto& c){return !c.used;});
        if(found==contacts_.end()) {cancel();throw std::runtime_error("too many touch contacts");}
        if(!before.count) {pending_.pressed=true;pending_.pressXPixels=x;pending_.pressYPixels=y;multiple_=false;}
        *found={id,x,y,true};
    }
    if(found==contacts_.end()) return;
    found->x=x;found->y=y;
    if(phase==TouchPhase::Up) found->used=false;
    const auto after=position();
    pending_.xPixels=after.count?after.x:x;pending_.yPixels=after.count?after.y:y;
    if(before.count && after.count && before.first==after.first && before.second==after.second) {
        pending_.deltaXPixels+=after.x-before.x;pending_.deltaYPixels+=after.y-before.y;
        if(before.count==2 && before.distance>1 && after.distance>1) pending_.pinchRatio*=after.distance/before.distance;
    }
    multiple_=multiple_ || after.count>1;
    pending_.multipleContacts=multiple_;pending_.down=after.count!=0;
    pending_.released=pending_.released || (before.count && !after.count);
}
PointerSample TouchContacts::consume() noexcept {
    auto sample=pending_;sample.isTouch=true;
    pending_.pressed=false;pending_.released=false;pending_.cancelled=false;
    pending_.deltaXPixels=0;pending_.deltaYPixels=0;pending_.pinchRatio=1;
    return sample;
}
}
