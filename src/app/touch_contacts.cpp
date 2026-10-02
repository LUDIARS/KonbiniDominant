// @implements spec/interface/mobile-platform.md Input and UI
#include "konbini/app/touch_contacts.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace konbini::app {
TouchContacts::Position TouchContacts::position() const noexcept {
    Position p;
    const Contact* first=nullptr;
    for(const auto& c:contacts_) if(c.used) {
        if(!first) {first=&c;p.x=c.x;p.y=c.y;p.pair={c.id,0,0,1};}
        else {
            p.x=(p.x+c.x)/2;p.y=(p.y+c.y)/2;
            p.pair.distancePixels=std::hypot(first->x-c.x,first->y-c.y);
            p.pair.second=c.id;p.pair.count=2;break;
        }
    }
    return p;
}
unsigned TouchContacts::liveContacts() const noexcept {
    return static_cast<unsigned>(std::count_if(contacts_.begin(),contacts_.end(),[](const auto& c){return c.used;}));
}
void TouchContacts::cancel() noexcept {
    contacts_={};pending_={};pending_.cancelled=true;pending_.timestampSeconds=lastTimestamp_;
    pinch_.reset();multiple_=false;
}
void TouchContacts::update(std::uint64_t id,TouchPhase phase,double x,double y) {
    update(TouchSample{id,phase,x,y,lastTimestamp_});
}
void TouchContacts::update(const TouchSample& sample) {
    if(!std::isfinite(sample.timestampSeconds) || sample.timestampSeconds<lastTimestamp_) {
        cancel();throw std::invalid_argument("touch timestamp went backwards");
    }
    lastTimestamp_=sample.timestampSeconds;
    if(sample.phase==TouchPhase::Cancel) {cancel();return;}
    const double x=sample.xPixels,y=sample.yPixels;
    if(!std::isfinite(x) || !std::isfinite(y)) {cancel();throw std::invalid_argument("non-finite touch position");}
    const auto before=position();
    auto found=std::find_if(contacts_.begin(),contacts_.end(),[&](const auto& c){return c.used && c.id==sample.fingerId;});
    if(sample.phase==TouchPhase::Down) {
        if(found==contacts_.end()) found=std::find_if(contacts_.begin(),contacts_.end(),[](const auto& c){return !c.used;});
        if(found==contacts_.end()) {cancel();throw std::runtime_error("too many touch contacts");}
        if(!before.pair.count) {
            pending_.pressed=true;pending_.pressXPixels=x;pending_.pressYPixels=y;
            pending_.fingerId=sample.fingerId;pending_.pressTimestampSeconds=sample.timestampSeconds;
            multiple_=false;
        }
        *found={sample.fingerId,x,y,true};
    }
    // Move / Up of a finger the table does not hold (lifted by a cancel) is
    // ignored, so a cancelled contact cannot come back as a stuck pointer.
    if(found==contacts_.end()) return;
    found->x=x;found->y=y;
    if(sample.phase==TouchPhase::Up) found->used=false;
    const auto after=position();
    pending_.xPixels=after.pair.count?after.x:x;pending_.yPixels=after.pair.count?after.y:y;
    pending_.timestampSeconds=sample.timestampSeconds;
    if(before.pair.count && after.pair.count && before.pair.first==after.pair.first && before.pair.second==after.pair.second) {
        pending_.deltaXPixels+=after.x-before.x;pending_.deltaYPixels+=after.y-before.y;
    }
    pinch_.observe(before.pair,after.pair);
    multiple_=multiple_ || after.pair.count>1;
    pending_.multipleContacts=multiple_;pending_.down=after.pair.count!=0;
    pending_.released=pending_.released || (before.pair.count && !after.pair.count);
}
PointerSample TouchContacts::consume() noexcept {
    auto sample=pending_;sample.isTouch=true;sample.pinchRatio=pinch_.consume();
    pending_.pressed=false;pending_.released=false;pending_.cancelled=false;
    pending_.deltaXPixels=0;pending_.deltaYPixels=0;
    return sample;
}
}
