#pragma once

#include "./utils.h"
#include "./dot_map.h"

namespace AltAI
{
    struct SiteInfo;

    struct MinMaxSiteInfoFinder
    {
        MinMaxSiteInfoFinder()
            : minWacd(MAX_INT), maxWacd(-1), minDistance(MAX_INT), maxDistance(0), maxBonusCount(0), maxBonusValue(0), maxFoundValue(0), maxPositiveGrowthTotal(0)
        {}

        void operator() (const SiteInfo& site);

        void debug(std::ostream& os) const;

        int minWacd, maxWacd;
        int minDistance, maxDistance;
        int maxBonusCount;
        int maxBonusValue;
        int maxFoundValue;
        int maxPositiveGrowthTotal;
    };

    struct FirstCityValueP
    {
        bool operator() (const SiteInfo& first, const SiteInfo second) const;
    };

    struct GrowthRateValueP
    {
        explicit GrowthRateValueP(bool useSecondRing_);

        bool operator() (const SiteInfo& first, const SiteInfo second) const;

        bool useSecondRing;
    };

    struct CombinedSiteValueP
    {
        CombinedSiteValueP(const MinMaxSiteInfoFinder& minMaxInfo_);

        bool operator() (const SiteInfo& first, const SiteInfo second) const;
        int operator() (const SiteInfo& siteInfo) const;

        const MinMaxSiteInfoFinder& minMaxInfo;
    };

    struct PositiveGrowthValueP
    {
        bool operator() (const SiteInfo& first, const SiteInfo second) const;
    };

    struct BonusCountsValueP
    {
        bool operator() (const SiteInfo& first, const SiteInfo second) const;
    };

    struct CoordsMatcher
    {
        explicit CoordsMatcher(const XYCoords coords_);

        bool operator() (const SiteInfo& site) const;

        const XYCoords coords;
    };

    

    int getWeightedAverageCityDistance(PlayerTypes playerType, const CvPlot* pPlot, bool sameArea);
    std::pair<int, bool> getNeighbourCityData(PlayerTypes playerType, TeamTypes teamType, const CvPlot* pPlot);

    struct SiteInfo
    {
        SiteInfo() : foundValue(0), weightedAverageCityDistance(-1), consumedWorkerCount(0), healthLevel(0), positiveGrowthTotal(0), nearestCityDistance(-1) {}

        SiteInfo(XYCoords coords_, int foundValue_, int weightedAverageCityDistance_)
            : coords(coords_), foundValue(foundValue_), weightedAverageCityDistance(weightedAverageCityDistance_), positiveGrowthTotal(0), nearestCityDistance(-1) {}

        SiteInfo(XYCoords coords_, int foundValue_, int weightedAverageCityDistance_, std::pair<int, int> growthRateValues_) 
            : coords(coords_), foundValue(foundValue_), weightedAverageCityDistance(weightedAverageCityDistance_), 
              growthRateValues(growthRateValues_), positiveGrowthTotal(0), nearestCityDistance(-1) {}

        SiteInfo(const DotMapItem& dotMapItem, int foundValue_, int weightedAverageCityDistance_, 
            std::pair<int, int> growthRateValues_, int positiveGrowthTotal_, int nearestCityDistance_);

        int getBonusOutputPercentTotal() const;
        int getBonusCountsTotal() const;

        XYCoords coords;
        int foundValue, weightedAverageCityDistance, consumedWorkerCount, healthLevel, positiveGrowthTotal, nearestCityDistance;
        PlotYield baseYield, projectedYield;
        std::vector<std::pair<BuildingTypes, PlotYield> > requiredBuildingsAndYields;
        std::pair<int, int> growthRateValues;  // first, second ring
        std::map<BonusTypes, int> bonusPercentOutputChanges;
        std::map<BonusTypes, int> bonusCounts;

        std::ostream& debug(std::ostream& os) const;
    };
}