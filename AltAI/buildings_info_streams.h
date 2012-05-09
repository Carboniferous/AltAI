#pragma once

#include "./buildings_info.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const BuildingInfo::NullNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::IsRiver& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::MinArea& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::BuildOrCondition& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::BaseNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::YieldNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::SpecialistNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::CommerceNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::TradeNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::PowerNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::UnitExpNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::SpecialistSlotNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::BonusNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::FreeBonusNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::RemoveBonusNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::CityDefenceNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::AreaEffectNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::ReligionNode& node);
    std::ostream& operator << (std::ostream& os, const BuildingInfo::MiscEffectNode& node);
}