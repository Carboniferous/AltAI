#pragma once

#include "./utils.h"

namespace AltAI
{
    class BuildingInfo;
    class Player;
    class City;

    struct CityConditionYieldHelperImpl;
    typedef boost::shared_ptr<CityConditionYieldHelperImpl> CityConditionYieldHelperImplPtr;

    struct CityConditionYieldHelper
    {
        explicit CityConditionYieldHelper(PlayerTypes playerType);
        PlotYield getExtraConditionalYield(XYCoords cityCoords, XYCoords plotCoords);

        CityConditionYieldHelperImplPtr pImpl;
    };

    struct ConditionalPlotYieldEnchancingBuilding;

    std::vector<ConditionalPlotYieldEnchancingBuilding> getConditionalYieldEnchancingBuildings(PlayerTypes playerType);

    std::map<XYCoords, PlotYield> getExtraConditionalYield(PlayerTypes playerType, int lookaheadDepth, 
        const std::pair<XYCoords, std::map<int, std::set<XYCoords> > >& plotData,       
        std::map<BuildingTypes, PlotYield>& requiredBuildings);   


    bool couldConstructBuilding(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, bool ignoreRequiredBuildings);

    bool couldConstructBuilding(const Player& player, const CvPlot* pPlot, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

    bool couldConstructUnitBuilding(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);    

    bool couldConstructSpecialBuilding(const Player& player, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
}