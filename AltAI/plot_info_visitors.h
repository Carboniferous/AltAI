#pragma once

#include "./plot_info.h"
#include "./city_data.h"

namespace AltAI
{
    class CityData;

    struct AvailableImprovements
    {
        struct ImprovementData
        {
            ImprovementData() : improvementType(NO_IMPROVEMENT) {}
            ImprovementData(ImprovementTypes improvementType_, PlotYield plotYield_)
                : improvementType(improvementType_), plotYield(plotYield_) {}

            ImprovementTypes improvementType;
            PlotYield plotYield;
        };

        explicit AvailableImprovements(FeatureTypes featureType = NO_FEATURE);

        typedef std::vector<ImprovementData> ListT;
        ListT improvements, removeFeatureImprovements;
        FeatureTypes featureType;

        std::vector<boost::tuple<FeatureTypes, ImprovementTypes, PlotYield> > getAsList() const;
        std::vector<std::pair<FeatureTypes, ImprovementTypes> > getAsListNoYields() const;
        bool empty() const;
    };

    // does plot have any yield (including available through later technologies)
    bool hasPossibleYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerIndex);

    // max yield including future techs and civics (to given depth)
    std::pair<PlotYield, ImprovementTypes> getMaxYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerIndex, int lookaheadDepth = 0);

    // just current yield (should match CvPlot->getYield() - although with the advantage of being const)
    PlotYield getYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, ImprovementTypes improvementType, FeatureTypes featureType, RouteTypes routeType, bool isGoldenAge = false);

    // list of improvements and yields
    std::vector<std::pair<PlotYield, ImprovementTypes> > getYields(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, int lookaheadDepth = 0);

    // list of routetypes and yield changes
    std::vector<std::pair<RouteTypes, PlotYield> > getRouteYieldChanges(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, ImprovementTypes improvementType, FeatureTypes featureType);

    // yield of upgraded improvement
    PlotYield getUpgradedImprovementYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, ImprovementTypes improvementType);

    std::list<PlotData::UpgradeData::Upgrade> 
        getUpgradedImprovementsData(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, ImprovementTypes improvementType, int turnsUntilUpgrade, int timeHorizon, int upgradeRate);

    // list of possible improvements (with current tech) for given city
    PlotsAndImprovements getPossibleImprovements(const CityData& data, bool ignoreExisting);

    // list of best improvements by plots with yields, including features which need to be removed
    std::vector<std::pair<XYCoords, boost::tuple<FeatureTypes, ImprovementTypes, PlotYield> > > getBestImprovements(const CityData& data, YieldValueFunctor valueF, bool ignoreExisting);

    // update city data with new improvement
    void updateCityOutputData(CityData& data, PlotData& plotData, FeatureTypes removedFeatureType, RouteTypes newRouteType, ImprovementTypes newImprovementType);

    // get city square yield
    PlotYield getPlotCityYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType);
}