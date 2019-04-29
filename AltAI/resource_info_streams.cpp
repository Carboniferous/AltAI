#include "AltAI.h"

#include "./resource_info_streams.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const ResourceInfo::NullNode& node)
    {
        return os;
    }

    std::ostream& operator << (std::ostream& os, const ResourceInfo::BaseNode& node)
    {
        if (node.bonusType != NO_BONUS)
        {
            os << gGlobals.getBonusInfo(node.bonusType).getType();

            if (node.baseHappy != 0)
            {
                os << " Happy =  " << node.baseHappy << " ";
            }

            if (node.baseHealth != 0)
            {
                os << " Health =  " << node.baseHealth << " ";
            }

            if (node.revealTech != NO_TECH)
            {
                os << " Revealed by: " << gGlobals.getTechInfo(node.revealTech).getType() << " ";
            }

            if (node.obsoleteTech != NO_TECH)
            {
                os << " Obsoleted by: " << gGlobals.getTechInfo(node.obsoleteTech).getType() << " ";
            }

            for (size_t i = 0, count = node.buildingNodes.size(); i < count; ++i)
            {
                if (i == 0) os << "\n\t";
                os << node.buildingNodes[i] << " ";
            }

            for (size_t i = 0, count = node.unitNodes.size(); i < count; ++i)
            {
                if (i == 0) os << "\n\t";
                os << node.unitNodes[i] << " ";
            }

        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const ResourceInfo::BuildingNode& node)
    {
        os << gGlobals.getBuildingInfo(node.buildingType).getType() << " ";

        if (node.bonusHappy != 0)
        {
            os << "Happy = " << node.bonusHappy << " ";
        }

        if (node.bonusHealth != 0)
        {
            os << "Health = " << node.bonusHealth << " ";
        }

        if (node.productionModifier != 0)
        {
            os << "Production Modifier: " << node.productionModifier << " ";
        }

        if (node.yieldModifier != YieldModifier())
        {
            os << "Yield Modifier: " << node.yieldModifier << " ";
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const ResourceInfo::UnitNode& node)
    {
        os << gGlobals.getUnitInfo(node.unitType).getType() << " ";
        return os;
    }

    std::ostream& operator << (std::ostream& os, const ResourceInfo::RouteNode& node)
    {
        os << " allows route: " << gGlobals.getRouteInfo(node.routeType).getType();
        return os;
    }
}
