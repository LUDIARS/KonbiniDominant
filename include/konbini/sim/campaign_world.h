#pragma once
#include "konbini/sim/placement_system.h"
#include "konbini/sim/population_cell_table.h"
#include "konbini/sim/world_entity_ids.h"
namespace konbini::sim {
// References to one already-staged tick transaction; owns no authoritative state.
struct CampaignWorld {
    GameState& state;
    const FirstPlayableContent& content;
    FacilityTable& facilities;
    StoreTable& stores;
    PopulationCellTable& population;
    ChainEconomyTable& economy;
    WorldEntityIds& identities;
};
}
