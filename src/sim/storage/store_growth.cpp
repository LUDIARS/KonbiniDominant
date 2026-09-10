#include "konbini/sim/store_table.h"
#include <stdexcept>
#include <cmath>
namespace konbini::sim {
// @implements spec/feature/full-campaign-baseline.md
std::optional<std::size_t> StoreTable::findAt(const FacilityId facility,const std::uint32_t slot) const noexcept {
    for(std::size_t i=0;i<size();++i)
        if(isActive_[i] && facilityIds_[i]==facility && verticalSlots_[i]==slot) return i;
    return {};
}
std::uint32_t StoreTable::firstEmptySlot(const FacilityId facility,const std::uint32_t limit) const noexcept {
    for(std::uint32_t slot=0;slot<limit;++slot) if(!findAt(facility,slot)) return slot;
    return limit;
}
void StoreTable::setZocRadius(const std::size_t index, const double radius) {
    if(index>=size() || !std::isfinite(radius) || radius<=0)
        throw std::invalid_argument("invalid store radius");
    zocRadiiMeters_[index]=radius;
}
void StoreTable::setFaith(const std::size_t index,const std::uint32_t faith) {
    if(index>=size() || faith>100) throw std::invalid_argument("invalid faith");
    faiths_[index]=faith;
}
void StoreTable::markAntiStore(const std::size_t index) {
    if(index>=size() || !isActive_[index] || antiStores_[index]) throw std::invalid_argument("invalid inversion target");
    antiStores_[index]=1;
}
}
