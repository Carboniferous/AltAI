#pragma once

#include "./resource_info.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const ResourceInfo::NullNode& node);
    std::ostream& operator << (std::ostream& os, const ResourceInfo::BaseNode& node);
    std::ostream& operator << (std::ostream& os, const ResourceInfo::BuildingNode& node);
    std::ostream& operator << (std::ostream& os, const ResourceInfo::RouteNode& node);
    std::ostream& operator << (std::ostream& os, const ResourceInfo::UnitNode& node);
}