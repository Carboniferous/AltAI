#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;
    class BuildingInfo;

    ConstructItem getEconomicBuildingTactics(const Player& player, BuildingTypes buildingType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    ConstructItem getMilitaryBuildingTactics(const Player& player, BuildingTypes buildingType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    TotalOutput getProjectedEconomicImpact(const Player& player, const City& city, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int selectedEconomicFlags);
}