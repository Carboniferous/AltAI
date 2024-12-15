#pragma once

#include "./utils.h"

namespace AltAI
{
    class BuildingInfo;
    class Player;
    class City;
    class CityData;

    bool buildingHasEconomicImpact(const CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    bool buildingHasPotentialEconomicImpact(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    bool buildingHasPotentialMilitaryImpact(PlayerTypes playerType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
}