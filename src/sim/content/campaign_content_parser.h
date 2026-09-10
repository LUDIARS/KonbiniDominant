#pragma once
#include "json_document.h"
#include "konbini/sim/campaign_content.h"
namespace konbini::sim {
CampaignContent parseCampaignContent(const json::Value& value);
}
