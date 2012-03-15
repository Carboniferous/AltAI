#pragma once

#include "./buildings_info.h"
#include "./city_optimiser.h"

class CvCity;

namespace AltAI
{
    class BuildingInfo;
    struct ConditionalPlotYieldEnchancingBuilding;
    class Player;
    class City;

    boost::shared_ptr<BuildingInfo> makeBuildingInfo(BuildingTypes buildingType, PlayerTypes playerType);

    void streamBuildingInfo(std::ostream& os, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    std::vector<TechTypes> getRequiredTechs(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    BuildingInfo::BaseNode getBadNodes(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    Commerce getCommerceValue(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    void updateRequestData(CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    bool buildingHasEconomicImpact(const CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    bool buildingHasPotentialEconomicImpact(const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
    bool buildingHasPotentialMilitaryImpact(PlayerTypes playerType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    // todo - generalise to multiple conditions?
    struct ConditionalPlotYieldEnchancingBuilding
    {
        explicit ConditionalPlotYieldEnchancingBuilding(BuildingTypes buildingType_ = NO_BUILDING) : buildingType(buildingType_) {}
        BuildingTypes buildingType;
        std::vector<BuildingInfo::BuildCondition> buildConditions;
        std::vector<std::pair<CvPlotFnPtr, PlotYield> > conditionalYieldChanges;
    };

    std::vector<ConditionalPlotYieldEnchancingBuilding> getConditionalYieldEnchancingBuildings(PlayerTypes playerType);

    std::map<XYCoords, PlotYield> getExtraConditionalYield(const std::pair<XYCoords, std::map<int, std::set<XYCoords> > >& plotData,
        const std::vector<ConditionalPlotYieldEnchancingBuilding>& conditionalYieldEnchancingBuildings, PlayerTypes playerType, int lookaheadDepth);

    PlotYield getExtraConditionalYield(XYCoords cityCoords, XYCoords plotCoords,
        const std::vector<ConditionalPlotYieldEnchancingBuilding>& conditionalYieldEnchancingBuildings);

    bool couldConstructBuilding(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
}