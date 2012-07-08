#pragma once

#include "./utils.h"
#include "./tactic_actions.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class Player;
    class City;
    class BuildingInfo;

    ConstructItem getEconomicBuildingTactics(const Player& player, BuildingTypes buildingType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    ConstructItem getMilitaryBuildingTactics(const Player& player, BuildingTypes buildingType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    ICityBuildingTacticsPtr makeCityBuildingTactics(const Player& player, const City& city, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    ILimitedBuildingTacticsPtr makeGlobalBuildingTactics(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    ILimitedBuildingTacticsPtr makeNationalBuildingTactics(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    IProcessTacticsPtr makeProcessTactics(const Player& player, ProcessTypes processType);
}