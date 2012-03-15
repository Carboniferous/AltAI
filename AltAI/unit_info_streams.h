#pragma once

#include "./unit_info.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const UnitInfo::NullNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::BaseNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::CargoNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::UpgradeNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::CombatNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::CityCombatNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::AirCombatNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::CollateralNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::CombatBonusNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::PromotionsNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::BuildNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::ReligionNode& node);
    std::ostream& operator << (std::ostream& os, const UnitInfo::MiscAbilityNode& node);
}