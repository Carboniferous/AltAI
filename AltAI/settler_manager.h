#pragma once

#include "./utils.h"
#include "./dot_map.h"
#include "./map_analysis.h"

namespace AltAI
{
    class SettlerManager
    {
    public:
        explicit SettlerManager(const boost::shared_ptr<MapAnalysis>& pMapAnalysis);

        std::vector<int> getBestCitySites(int minValue, int count) const;
        int getOverseasCitySitesCount(int minValue, int count, int subAreaID) const;
        void analysePlotValues();
        void debugDotMap() const;

        CvPlot* getBestPlot(int subAreaID, const std::vector<CvPlot*>& ignorePlots) const;

        CvPlot* getBestPlot(const CvUnitAI* pUnit, int subAreaID);

        std::set<BonusTypes> getBonusesForSites(int siteCount) const;
        std::set<ImprovementTypes> getImprovementTypesForSites(int siteCount) const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:

        DotMapItem analysePlotValue_(MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator ci);
        void findBestCitySites_();
        void populateBestCitySitesMap_(std::map<XYCoords, int>& bestSitesMap);
        void populatePlotCountAndResources_(XYCoords coords, std::map<XYCoords, int>& plotCountMap, std::map<XYCoords, std::map<BonusTypes, int> >& resourcesMap);
        int doSiteValueAdjustment_(XYCoords coords, int baseValue, int maintenanceDelta,
                                   const std::map<XYCoords, int>& plotCountMap, const std::map<XYCoords, std::map<BonusTypes, int> >& resourcesMap);
        std::pair<int, bool> getNeighbourCityData_(const CvPlot* pPlot) const;
        int getStepDistanceToClosestCity_(const CvPlot* pPlot, bool sameArea) const;

        boost::shared_ptr<MapAnalysis> pMapAnalysis_;
        PlayerTypes playerType_;
        TeamTypes teamType_;

        typedef std::set<DotMapItem> DotMap;
        DotMap dotMap_;
        std::map<int, XYCoords, std::greater<int> > bestSites_;
        std::set<XYCoords> excludedCoords_;

        typedef std::map<IDInfo, std::pair<int, XYCoords> > SettlerDestinationMap;
        SettlerDestinationMap settlerDestinationMap_;
    };
}