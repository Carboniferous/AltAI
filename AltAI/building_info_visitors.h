#pragma once

#include "./city_optimiser.h"

class CvCity;

namespace AltAI
{
    class BuildingInfo;
    struct ConditionalPlotYieldEnchancingBuilding;
    class Player;
    class City;
    struct IProjectionEvent;
    typedef boost::shared_ptr<IProjectionEvent> IProjectionEventPtr;

    boost::shared_ptr<BuildingInfo> makeBuildingInfo(BuildingTypes buildingType, PlayerTypes playerType);

    void streamBuildingInfo(std::ostream& os, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    std::vector<TechTypes> getRequiredTechs(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    //BuildingInfo::BaseNode getBadNodes(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    Commerce getCommerceValue(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    void updateRequestData(CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    void updateGlobalRequestData(const CityDataPtr& pCityData, const CvCity* pBuiltCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    bool buildingHasEconomicImpact(const CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    bool buildingHasPotentialEconomicImpact(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    bool buildingHasPotentialMilitaryImpact(PlayerTypes playerType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    std::vector<ConditionalPlotYieldEnchancingBuilding> getConditionalYieldEnchancingBuildings(PlayerTypes playerType);

    std::map<XYCoords, PlotYield> getExtraConditionalYield(const std::pair<XYCoords, std::map<int, std::set<XYCoords> > >& plotData,
        const std::vector<ConditionalPlotYieldEnchancingBuilding>& conditionalYieldEnchancingBuildings, PlayerTypes playerType, int lookaheadDepth,
        std::map<BuildingTypes, PlotYield>& requiredBuildings);

    PlotYield getExtraConditionalYield(XYCoords cityCoords, XYCoords plotCoords,
        const std::vector<ConditionalPlotYieldEnchancingBuilding>& conditionalYieldEnchancingBuildings);

    //std::vector<IProjectionEventPtr> getPossibleEvents(const Player& player, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int baseEventTurn);

    bool couldConstructBuilding(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, bool ignoreRequiredBuildings);

    bool couldConstructBuilding(const Player& player, const CvPlot* pPlot, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    bool couldConstructUnitBuilding(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);    

    bool couldConstructSpecialBuilding(const Player& player, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
}