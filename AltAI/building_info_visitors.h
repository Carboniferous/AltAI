#pragma once

#include "./utils.h"

namespace AltAI
{
    class BuildingInfo;
    class Player;
    class City;
    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    boost::shared_ptr<BuildingInfo> makeBuildingInfo(BuildingTypes buildingType, PlayerTypes playerType);

    std::vector<TechTypes> getRequiredTechs(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    std::vector<UnitTypes> getEnabledUnits(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    Commerce getCommerceValue(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    void updateRequestData(CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    void updateGlobalRequestData(const CityDataPtr& pCityData, const CvCity* pBuiltCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
}