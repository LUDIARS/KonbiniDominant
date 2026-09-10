#pragma once
#include "konbini/sim/dimension_system.h"
namespace konbini::sim {
enum class CampaignFailure : std::uint8_t {
    None, NotUnlocked, WrongChain, NoTarget, Collapsed, NotRival,
    FaithRequired, AlreadyInverted, InsufficientCash, Cooldown, DimensionCapacity, ChooseUpgradeFirst, InvalidUpgrade
};
struct CampaignCommandResult {
    CampaignCommand command{};
    CampaignFailure failure=CampaignFailure::None;
};
CampaignFailure applyCampaignCommand(CampaignWorld world,const CampaignCommand& command);
}
