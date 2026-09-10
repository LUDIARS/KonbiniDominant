#pragma once
#include "json_document.h"
#include "konbini/sim/skill_content.h"
namespace konbini::sim {
SkillContent parseSkillContent(const json::Value& value);
}
