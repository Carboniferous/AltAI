#pragma once

#include "./tech_info.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const TechInfo::NullNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::BaseNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::BuildingNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::ImprovementNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::CivicNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::UnitNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::BonusNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::CommerceNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::ProcessNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::RouteNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::TradeNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::FirstToNode& node);
    std::ostream& operator << (std::ostream& os, const TechInfo::MiscEffectNode& node);
}