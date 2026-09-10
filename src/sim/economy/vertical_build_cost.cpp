#include "konbini/sim/vertical_placement.h"
#include <limits>
#include <stdexcept>
namespace konbini::sim {
std::int64_t verticalBuildCost(const std::int64_t base,const std::uint32_t slot,const VerticalContent& rules) {
    if(base<=0 || slot>=rules.slots) throw std::invalid_argument("invalid vertical price input");
    const std::int64_t increment=static_cast<std::int64_t>(slot)*rules.costStepPermille;
    const auto remainder=((base%1000)*increment+999)/1000;
    const auto limit=std::numeric_limits<std::int64_t>::max()-base;
    if(remainder>limit || (increment && base/1000>(limit-remainder)/increment))
        throw std::overflow_error("vertical build price overflow");
    return base+(base/1000)*increment+remainder;
}
}
