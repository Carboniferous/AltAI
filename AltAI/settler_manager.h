#pragma once

#include "./utils.h"
#include "./dot_map.h"
#include "./map_analysis.h"

namespace AltAI
{
    class SettlerManager
    {
    public:
        explicit SettlerManager(Player& player);

        std::vector<int> getBestCitySites(int minValue, int count);
        int getOverseasCitySitesCount(int minValue, int count, int subAreaID) const;
        void analysePlotValues();
        void makePlotDirty(XYCoords coords);  // todo
        void debugDotMap() const;
        void debugPlot(XYCoords coords, std::ostream& os) const;

        XYCoords getBestPlot() const;
        CvPlot* getBestPlot(int subAreaID, const std::vector<CvPlot*>& ignorePlots);
        CvPlot* getBestPlot(const CvUnit* pUnit, int subAreaID);
        void eraseUnit(IDInfo unit);

        DotMapItem getPlotDotMap(XYCoords coords) const;

        //std::set<BonusTypes> getBonusesForSites(int siteCount) const;
        //std::set<ImprovementTypes> getImprovementTypesForSites(int siteCount) const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        struct SiteInfo
        {
            SiteInfo() : foundValue(0), weightedAverageCityDistance(-1), consumedWorkerCount(0), healthLevel(0) {}
            SiteInfo(XYCoords coords_, int foundValue_, int weightedAverageCityDistance_)
                : coords(coords_), foundValue(foundValue_), weightedAverageCityDistance(weightedAverageCityDistance_) {}
            SiteInfo(XYCoords coords_, int foundValue_, int weightedAverageCityDistance_, std::pair<int, int> growthRateValues_) 
                : coords(coords_), foundValue(foundValue_), weightedAverageCityDistance(weightedAverageCityDistance_), growthRateValues(growthRateValues_) {}
            SiteInfo(DotMapItem& dotMapItem, int foundValue_, int weightedAverageCityDistance_, std::pair<int, int> growthRateValues_);

            int getBonusOutputPercentTotal() const;

            XYCoords coords;
            int foundValue, weightedAverageCityDistance, consumedWorkerCount, healthLevel;
            PlotYield baseYield, projectedYield;
            std::vector<std::pair<BuildingTypes, PlotYield> > requiredBuildingsAndYields;
            std::pair<int, int> growthRateValues;  // first, second ring
            std::map<BonusTypes, int> bonusPercentOutputChanges;

            std::ostream& debug(std::ostream& os) const;
        };

    private:
        DotMapItem analysePlotValue_(const MapAnalysis::PlotValues& plotValues, MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator ci);

        void findBestCitySites_();
        void findFirstCitySite_();
        void clearFoundValues_();
        void populateDotMap_();
        void populateMaxFoundValues_();
        void populateMaxBonusValues_();
        void populateBonusMap_();
        void populateBestBonusSites_();
        void populateMaintenanceDeltas_(const std::set<XYCoords>& coords);
        void populateGrowthRatesData_();
        void updateBonusValues_();

        std::pair<int, int> calculateGrowthRateData_(XYCoords coords, const int timeHorizon);
        void findMaxGrowthRateSite_(bool includeSecondRing);
        void populatePlotCountAndResources_(XYCoords coords, int& improveablePlotCount, std::map<BonusTypes, int>& resourcesMap);
        int doSiteValueAdjustment_(XYCoords coords, int baseValue, int maintenanceDelta, int improveablePlotCount, const std::map<BonusTypes, int>& bonusCounts);

        std::pair<int, bool> getNeighbourCityData_(const CvPlot* pPlot) const;
        int getWeightedAverageCityDistance_(const CvPlot* pPlot, bool sameArea) const;
        std::pair<int, XYCoords> getClosestSite_(const int subArea, const int percentThreshold, const std::vector<XYCoords>& ignorePlots, const CvPlot* pPlot) const;

        template <typename Pred>
            void filterFoundValues_(Pred pred)
        {
            for (std::list<std::pair<XYCoords, int> >::iterator iter(foundValueMaxes_.begin()), endIter(foundValueMaxes_.end()); iter != endIter;)
            {
                if (pred(iter->second))
                {
                    foundValueMaxes_.erase(iter++);
                }
                else
                {
                    ++iter;
                }
            }
        }

        Player& player_;
        boost::shared_ptr<MapAnalysis> pMapAnalysis_;
        PlayerTypes playerType_;
        TeamTypes teamType_;

        std::list<SiteInfo>::iterator findSite_(XYCoords coords);
        SiteInfo getMaxSiteInfoData_() const;
        
        typedef std::set<DotMapItem> DotMap;
        DotMap dotMap_;
        typedef std::multimap<int, XYCoords, std::greater<int> > BestSitesMap;
        BestSitesMap bestSites_, bestBonusSites_;
        std::list<SiteInfo> sitesInfo_;
        std::list<SiteInfo>::iterator maxGrowthIter_;
        int maxFoundValue_;
        PlotYield maxYields_;
        std::list<std::pair<XYCoords, int> > foundValueMaxes_;
        std::map<XYCoords, std::pair<int, int> > growthRateValues_;
        std::set<XYCoords> excludedCoords_, checkedNeighbourCoords_;

        std::map<BonusTypes, std::set<XYCoords> > bonusSitesMap_;
        std::map<BonusTypes, std::list<XYCoords> > bestBonusSitesMap_;
        std::map<BonusTypes, TotalOutput> bonusValuesMap_;  // store %age deltas
        int currentMaintenance_;
        std::map<XYCoords, int> siteMaintenanceChangesMap_;

        typedef std::map<IDInfo, std::pair<int, XYCoords> > SettlerDestinationMap;
        SettlerDestinationMap settlerDestinationMap_;
        int turnLastCalculated_;
    };
}