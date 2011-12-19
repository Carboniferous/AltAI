#pragma once

#include "./utils.h"

namespace AltAI
{
    class ResourceInfo
    {
    public:
        ResourceInfo(BonusTypes bonusType, PlayerTypes playerType);

        BonusTypes getBonusType() const
        {
            return bonusType_;
        }

        PlayerTypes getPlayerType() const
        {
            return playerType_;
        }

        struct NullNode
        {
            NullNode() {}
        };

        struct BaseNode;

        struct BuildingNode
        {
            BuildingNode() : buildingType(NO_BUILDING), productionModifier(0), bonusHealth(0), bonusHappy(0) {}
            BuildingTypes buildingType;
            int productionModifier;
            int bonusHealth;
            int bonusHappy;
            YieldModifier yieldModifier;
        };

        struct UnitNode
        {
            UnitNode() : unitType(NO_UNIT), isAndRequirement(false) {}
            ::UnitTypes unitType;
            bool isAndRequirement;
        };

        struct RouteNode
        {
            RouteNode() : routeType(NO_ROUTE) {}
            RouteTypes routeType;
        };

        typedef boost::variant<NullNode, boost::recursive_wrapper<BaseNode>, BuildingNode, UnitNode, RouteNode> ResourceInfoNode;

        struct BaseNode
        {
            BaseNode() : bonusType(NO_BONUS), revealTech(NO_TECH), obsoleteTech(NO_TECH), baseHealth(0), baseHappy(0)
            {
            }
            BonusTypes bonusType;
            TechTypes revealTech, obsoleteTech;
            int baseHealth, baseHappy;
            std::vector<BuildingNode> buildingNodes;
            std::vector<UnitNode> unitNodes;
            std::vector<RouteNode> routeNodes;
        };

        const ResourceInfoNode& getInfo() const;

    private:
        void init_();

        BonusTypes bonusType_;
        PlayerTypes playerType_;
        ResourceInfoNode infoNode_;
    };
}