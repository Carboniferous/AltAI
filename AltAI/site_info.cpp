#include "AltAI.h"

#include "./site_info.h"
#include "./map_analysis.h"
#include "./dot_map.h"
#include "./iters.h"

namespace AltAI
{
    bool FirstCityValueP::operator() (const SiteInfo& first, const SiteInfo second) const
    {
        int firstValue = first.foundValue + first.growthRateValues.first + first.growthRateValues.second;
        int secondValue = second.foundValue + second.growthRateValues.first + second.growthRateValues.second;
        if (firstValue == secondValue)
        {
            return YieldValueFunctor(makeYieldW(4, 2, 1))(first.projectedYield) >  YieldValueFunctor(makeYieldW(4, 2, 1))(second.projectedYield);
        }
        else
        {
            return firstValue > secondValue;
        }
    }

    GrowthRateValueP::GrowthRateValueP(bool useSecondRing_) : useSecondRing(useSecondRing_) {}

    bool GrowthRateValueP::operator() (const SiteInfo& first, const SiteInfo second) const
    {
        return useSecondRing ? first.growthRateValues.second > second.growthRateValues.second : first.growthRateValues.first > second.growthRateValues.first;
    }

    CombinedSiteValueP::CombinedSiteValueP(const MinMaxSiteInfoFinder& minMaxInfo_)
        : minMaxInfo(minMaxInfo_) {}

    bool CombinedSiteValueP::operator() (const SiteInfo& first, const SiteInfo second) const
    {
        const int bonusTotal1 = first.getBonusOutputPercentTotal(), bonusTotal2 = second.getBonusOutputPercentTotal();
        const bool firstHasGoodBonusValue = bonusTotal1 > 0;
        const bool secondHasGoodBonusValue = bonusTotal2 > 0;

        if (firstHasGoodBonusValue && !secondHasGoodBonusValue)
        {
            return true;
        }
        else if (!firstHasGoodBonusValue && secondHasGoodBonusValue)
        {
            return false;
        }
        else
        {
            return first.foundValue > second.foundValue;
        }
        //return (bonusTotal1 * maxFoundValue / 100) + first.foundValue >  (bonusTotal2 * maxFoundValue / 100) + second.foundValue;
        /*if (bonusTotal1 == bonusTotal2)
        {
            return first.positiveGrowthTotal == second.positiveGrowthTotal ? first.foundValue > second.foundValue : first.positiveGrowthTotal > second.positiveGrowthTotal;
        }
        else
        {
            return bonusTotal1 > bonusTotal2;
        }*/
        //return (bonusTotal1 * first.positiveGrowthTotal) + first.foundValue >  (bonusTotal2 * second.positiveGrowthTotal) + second.foundValue;
        //return (100 + bonusTotal1) * first.positiveGrowthTotal + first.foundValue > (100 + bonusTotal2) * second.positiveGrowthTotal + second.foundValue;
    }

    int CombinedSiteValueP::operator() (const SiteInfo& siteInfo) const
    {
        return (siteInfo.getBonusOutputPercentTotal() * minMaxInfo.maxFoundValue / 100) + siteInfo.foundValue;
    }

    bool PositiveGrowthValueP::operator() (const SiteInfo& first, const SiteInfo second) const
    {
        return first.positiveGrowthTotal == second.positiveGrowthTotal ? first.foundValue > second.foundValue : first.positiveGrowthTotal > second.positiveGrowthTotal;
    }

    bool BonusCountsValueP::operator() (const SiteInfo& first, const SiteInfo second) const
    {
        return first.getBonusCountsTotal() == second.getBonusCountsTotal() ? first.foundValue > second.foundValue : first.getBonusCountsTotal() > second.getBonusCountsTotal();
    }

    CoordsMatcher::CoordsMatcher(const XYCoords coords_) : coords(coords_) {}

    bool CoordsMatcher::operator() (const SiteInfo& site) const
    {
        return site.coords == coords;
    }

    void MinMaxSiteInfoFinder::operator() (const SiteInfo& site)
    {
        minWacd = std::min<int>(minWacd, site.weightedAverageCityDistance);
        maxWacd = std::max<int>(maxWacd, site.weightedAverageCityDistance);

        minDistance = std::min<int>(minDistance, site.nearestCityDistance);
        maxDistance = std::max<int>(maxDistance, site.nearestCityDistance);

        maxBonusCount = std::max<int>(maxBonusCount, site.getBonusCountsTotal());
        maxBonusValue = std::max<int>(maxBonusValue, site.getBonusOutputPercentTotal());
        maxFoundValue = std::max<int>(maxFoundValue, site.foundValue);

        maxPositiveGrowthTotal = std::max<int>(maxPositiveGrowthTotal, site.positiveGrowthTotal);
    }

    void MinMaxSiteInfoFinder::debug(std::ostream& os) const
    {
        os << "\n min wacd: " << minWacd << ", max wacd: " << maxWacd
           << ", min dist: " << minDistance << ", max dist: " << maxDistance
           << ", max pgv: " << maxPositiveGrowthTotal
           << ", max bonus count: " << maxBonusCount << ", max bonus value: " << maxBonusValue << ", max found value: " << maxFoundValue;
    }

    int getWeightedAverageCityDistance(PlayerTypes playerType, const CvPlot* pPlot, bool sameArea)
    {
        const int areaID = pPlot->getArea();

        CityIter cityIter(CvPlayerAI::getPlayer(playerType));
        int total = 0;

        const CvCity* pLoopCity;
        while (pLoopCity = cityIter())
        {
            if (!sameArea || pLoopCity->getArea() == areaID)
            {
                total += stepDistance(pPlot->getX(), pPlot->getY(), pLoopCity->getX(), pLoopCity->getY()) * pLoopCity->getPopulation();
            }
        }
        return total;
    }

    std::pair<int, bool> getNeighbourCityData(PlayerTypes playerType, TeamTypes teamType, const CvPlot* pPlot)
    {
        int count = 0;
        bool workedByNeighbour = false;

        for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
        {
            CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
            while (IterPlot pLoopPlot = plotIter())
            {
                if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType, false))
                {
                    // count only our cities (maybe revisit this?)
                    if (pLoopPlot->isCity() && pLoopPlot->getOwner() == playerType)                    
                    {
                        ++count;
                        if (!workedByNeighbour)
                        {
                            const CvCity* pCity = pLoopPlot->getPlotCity();
                            workedByNeighbour = pCity->isWorkingPlot(pLoopPlot);
                        }
                    }
                }
            }
        }

        return std::make_pair(count, workedByNeighbour);
    }

    SiteInfo::SiteInfo(const DotMapItem& dotMapItem, int foundValue_, int weightedAverageCityDistance_, std::pair<int, int> growthRateValues_, int positiveGrowthTotal_, int nearestCityDistance_)
        : weightedAverageCityDistance(weightedAverageCityDistance_),
          coords(dotMapItem.coords), foundValue(foundValue_), baseYield(dotMapItem.getOutput().first), projectedYield(dotMapItem.projectedYield),
          requiredBuildingsAndYields(dotMapItem.requiredAdditionalYieldBuildings),
          growthRateValues(growthRateValues_),
          positiveGrowthTotal(positiveGrowthTotal_),
          nearestCityDistance(nearestCityDistance_)
    {
    }

    int SiteInfo::getBonusOutputPercentTotal() const
    {
        int total = 0;
        for (std::map<BonusTypes, int>::const_iterator bonusIter(bonusPercentOutputChanges.begin()), endBonusIter(bonusPercentOutputChanges.end()); bonusIter != endBonusIter; ++bonusIter)
        {
            total += bonusIter->second;
        }
        return total;
    }

    int SiteInfo::getBonusCountsTotal() const
    {
        int total = 0;
        for (std::map<BonusTypes, int>::const_iterator bonusIter(bonusCounts.begin()), endBonusIter(bonusCounts.end()); bonusIter != endBonusIter; ++bonusIter)
        {
            total += bonusIter->second;
        }
        return total;
    }

    std::ostream& SiteInfo::debug(std::ostream& os) const
    {
        os << "\n\t" << coords << " found value: " << foundValue << ", bonus value = " << getBonusOutputPercentTotal()
           << ", bonus count: " << getBonusCountsTotal() << ", distinct: " << bonusCounts.size()
           << ", pgv: " << positiveGrowthTotal << ", dist: " << nearestCityDistance << ", wacd: " << weightedAverageCityDistance;
        os << ", baseYield: " << baseYield << /*", projected: " << projectedYield << */ ", grv_1: " << growthRateValues.first << ", grv_2: " << growthRateValues.second;
        for (size_t i = 0, count = requiredBuildingsAndYields.size(); i < count; ++i)
        {
            if (i > 0) os << ", "; else os << " ";
            os << gGlobals.getBuildingInfo(requiredBuildingsAndYields[i].first).getType() << ": " << requiredBuildingsAndYields[i].second;
        }
        for (std::map<BonusTypes, int>::const_iterator ci(bonusPercentOutputChanges.begin()), ciEnd(bonusPercentOutputChanges.end()); ci != ciEnd; ++ci)
        {
            os << " " << gGlobals.getBonusInfo(ci->first).getType() << " %c: " << ci->second;
        }
        return os;
    }
}
