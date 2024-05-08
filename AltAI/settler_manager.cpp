#include "AltAI.h"

#include "./settler_manager.h"
#include "./player_analysis.h"
#include "./gamedata_analysis.h"
#include "./city_optimiser.h"
#include "./plot_info_visitors.h"
#include "./building_info_visitors.h"
#include "./buildings_info.h"
#include "./building_tactics_deps.h"
#include "./resource_info_visitors.h"
#include "./resource_tactics.h"
#include "./game.h"
#include "./player.h"
#include "./maintenance_helper.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./tactic_selection_data.h"
#include "./civ_log.h"
#include "./map_log.h"
#include "./error_log.h"

namespace AltAI
{
    namespace
    {
        struct LessThan
        {
            explicit LessThan(const int value_) : value(value_) {}

            bool operator() (const int compareValue) const
            {
                return compareValue < value;
            }

            const int value;
        };

        struct FoundValueP
        {
            typedef XYCoords argument_type;

            FoundValueP(const CvMap& theMap_, PlayerTypes playerType_, int minFoundValue_ = 0)
                : theMap(theMap_), playerType(playerType_), minFoundValue(minFoundValue_) {}

            bool operator() (const XYCoords first) const
            {
                return theMap.plot(first)->getFoundValue(playerType) >= minFoundValue;
            }

            bool operator() (const XYCoords first, const XYCoords second) const
            {
                return theMap.plot(second)->getFoundValue(playerType) > theMap.plot(first)->getFoundValue(playerType);
            }

            const CvMap& theMap;
            PlayerTypes playerType;
            int minFoundValue;
        };

        struct FirstCityValueP
        {
            bool operator() (const SettlerManager::SiteInfo& first, const SettlerManager::SiteInfo second) const
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
        };

        struct GrowthRateValueP
        {
            explicit GrowthRateValueP(bool useSecondRing_) : useSecondRing(useSecondRing_) {}

            bool operator() (const SettlerManager::SiteInfo& first, const SettlerManager::SiteInfo second) const
            {
                return useSecondRing ? first.growthRateValues.second > second.growthRateValues.second : first.growthRateValues.first > second.growthRateValues.first;
            }

            bool useSecondRing;
        };

        struct CombinedSiteValueP
        {
            CombinedSiteValueP(bool useSecondRing_, int maxFoundValue_) : grp(useSecondRing_), maxFoundValue(maxFoundValue_) {}

            bool operator() (const SettlerManager::SiteInfo& first, const SettlerManager::SiteInfo second) const
            {
                const int bonusTotal1 = first.getBonusOutputPercentTotal(), bonusTotal2 = second.getBonusOutputPercentTotal();
                return (bonusTotal1 * maxFoundValue / 100) + first.foundValue >  (bonusTotal2 * maxFoundValue / 100) + second.foundValue;
            }

            int operator() (const SettlerManager::SiteInfo& siteInfo) const
            {
                return (siteInfo.getBonusOutputPercentTotal() * maxFoundValue / 100) + siteInfo.foundValue;
            }
            
            const GrowthRateValueP grp;
            int maxFoundValue;
        };

        struct CoordsMatcher
        {
            explicit CoordsMatcher(const XYCoords coords_) : coords(coords_) {}

            bool operator() (const SettlerManager::SiteInfo& site) const
            {
                return site.coords == coords;
            }

            const XYCoords coords;
        };
    }
    
    SettlerManager::SettlerManager(Player& player) : player_(player), pMapAnalysis_(player.getAnalysis()->getMapAnalysis()), maxFoundValue_(0), currentMaintenance_(0), turnLastCalculated_(-1)
    {
        playerType_ = player.getPlayerID();
        teamType_ = player.getTeamID();
    }

    std::vector<int> SettlerManager::getBestCitySites(int minValue, int count)
    {
        int foundCount = 0;
        std::vector<int> sites;
        const CvMap& theMap = gGlobals.getMap();

        // prevent settling new sites if we are nearly broke
        if (pMapAnalysis_->getPlayer().getMaxResearchPercent() < 30 || sitesInfo_.empty())
        {
            return sites;
        }

        // find up to count values > min found value using max found value + max grv (second ring)
        //sitesInfo_.sort(FirstCityValueP());  // assume sorted already
        int maxGrvAndFoundValue = sitesInfo_.begin()->foundValue + sitesInfo_.begin()->growthRateValues.second;
        for (std::list<SiteInfo>::const_iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
        {
            if (foundCount > count)
            {
                break;
            }
            if (sitesIter->foundValue > minValue)
            {
                ++foundCount;
                sites.push_back(theMap.plotNum(sitesIter->coords.iX, sitesIter->coords.iY));
            }
        }

        return sites;
    }

    int SettlerManager::getOverseasCitySitesCount(int minValue, int count, int subAreaID) const
    {
        int foundCount = 0;

        // prevent settling new sites if we are nearly broke
        if (pMapAnalysis_->getPlayer().getMaxResearchPercent() < 30)
        {
            return 0;
        }

        for (std::list<SiteInfo>::const_iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
        {
            if (foundCount > count)
            {
                break;
            }

            if (sitesIter->foundValue > minValue)
            {
                if (gGlobals.getMap().plot(sitesIter->coords)->getSubArea() != subAreaID)
                {
                    ++foundCount;
                }
            }
        }
        return foundCount;
    }

    void SettlerManager::analysePlotValues()
    {
        const int thisGameTurn = gGlobals.getGame().getGameTurn();
        if (thisGameTurn == turnLastCalculated_ && !pMapAnalysis_->plotValuesDirty())
        {
            return;
        }

        turnLastCalculated_ = thisGameTurn;

        clearFoundValues_();
        populateDotMap_();
        populateBonusMap_();

        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
        const Player& player_ = *gGlobals.getGame().getAltAI()->getPlayer(playerType_);
        const CvCity* pCapitalCity = player.getCapitalCity();        

        std::set<BonusTypes> existingBonuses;
        int baseHappy = 0, baseHealthy = 0;
        if (pCapitalCity)
        {
            for (int bonusType = 0, count = gGlobals.getNumBonusInfos(); bonusType < count; ++bonusType)
            {
                if (pCapitalCity->getNumBonuses((BonusTypes)bonusType) > 0)
                {
                    existingBonuses.insert((BonusTypes)bonusType);
                    baseHappy += gGlobals.getBonusInfo((BonusTypes)bonusType).getHappiness();
                    baseHealthy += gGlobals.getBonusInfo((BonusTypes)bonusType).getHealth();
                }
            }
        }
        else
        {
            baseHappy = 1;  // to allow for palace
        }

        // todo - factor in expected free buildings happy in case of no cities (hence the additional one to level happiness in lieu of palace +1)
        const int baseHappyPopulation = baseHappy + gGlobals.getHandicapInfo(player.getHandicapType()).getHappyBonus();
        const int baseHealthPopulation = baseHealthy + gGlobals.getHandicapInfo(player.getHandicapType()).getHealthBonus();
        const int timeHorizon = player_.getAnalysis()->getTimeHorizon();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(player)->getStream();
        os << "\nanalysePlotValues - using baseHappyPopulation: " << baseHappyPopulation << ", baseHealthPopulation: " << baseHealthPopulation;
#endif  
        CvMap& theMap = gGlobals.getMap(); 
        for (DotMap::iterator iter(dotMap_.begin()), endIter(dotMap_.end()); iter != endIter; ++iter)
        {
            std::vector<BonusTypes> newBonuses = iter->getNewBonuses(existingBonuses);
            int extraHappy = 0, extraHealth = 0;
            for (int bonusType = 0, count = newBonuses.size(); bonusType < count; ++bonusType)
            {
                extraHappy += gGlobals.getBonusInfo((BonusTypes)bonusType).getHappiness();
                extraHealth += gGlobals.getBonusInfo((BonusTypes)bonusType).getHealth();
            }
            extraHealth += iter->getBaseFeatureHealth();

            DotMapOptimiser opt(*iter, playerType_);
            std::vector<YieldTypes> yieldTypes(boost::assign::list_of(YIELD_FOOD)(YIELD_PRODUCTION)(YIELD_COMMERCE));
            //opt.optimise(yieldTypes, std::min<int>(1 + baseHappyPopulation + extraHappy, iter->plotDataSet.size()));
            opt.optimise(std::vector<YieldWeights>());
            iter->calcOutput(player_, 2 * player_.getAnalysis()->getTimeHorizon(), baseHealthPopulation + extraHealth);
            theMap.plot(iter->coords.iX, iter->coords.iY)->setFoundValue(playerType_, iter->getFoundValue(player_));
        }

        findBestCitySites_();

#ifdef ALTAI_DEBUG
        //if (!(gGlobals.getGame().getGameTurn() % 10))
        {
            debugDotMap();
        }
#endif
    }

    void SettlerManager::debugDotMap() const
    {
#ifdef ALTAI_DEBUG
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
        std::ostream& os = CivLog::getLog(player)->getStream();

        std::map<XYCoords, int> coordMap;
        std::multimap<int, XYCoords, std::greater<int> > sites;

        YieldValueFunctor valueF(makeYieldW(2, 2, 1));

        for (DotMap::const_iterator ci(dotMap_.begin()), ciEnd(dotMap_.end()); ci != ciEnd; ++ci)
        {
            std::pair<PlotYield, int> plotYieldAndSurplusFood = ci->getOutput();
            //os << "\noutput for coords: " << ci->coords << " = " << plotYieldAndSurplusFood.first << ", " << plotYieldAndSurplusFood.second;

            int value = valueF(plotYieldAndSurplusFood.first);
            sites.insert(std::make_pair(value, ci->coords));
            coordMap.insert(std::make_pair(ci->coords, value));
        }

        {
            int iNetCommerce = 1 + player.getCommerceRate(COMMERCE_GOLD) + player.getCommerceRate(COMMERCE_RESEARCH) + std::max(0, player.getGoldPerTurn());
            int iNetExpenses = player.calculateInflatedCosts() + std::min(0, player.getGoldPerTurn());        

            os << "\nMinimum found value = " << player.AI_getMinFoundValue();
            os << "\nNet Commerce = " << iNetCommerce << ", net expenses: " << iNetExpenses;
        }

        int count = 0;
        for (std::multimap<int, XYCoords, std::greater<int> >::const_iterator ci(sites.begin()), ciEnd(sites.end()); ci != ciEnd; ++ci)
        {
            if (count++ < 10)
            {
                os << "\n" << ci->second << " value = " << ci->first;
                dotMap_.find(DotMapItem(ci->second, PlotYield()))->debugOutputs(player_, os);
            }
            else
            {
                break;
            }
        }
        MapLog::getLog(player)->init();
        MapLog::getLog(player)->logMap(coordMap);
#endif
    }

    void SettlerManager::debugPlot(XYCoords coords, std::ostream& os) const
    {
        DotMap::const_iterator ci = dotMap_.find(DotMapItem(coords, PlotYield()));
        if (ci != dotMap_.end())
        {
            ci->debugOutputs(player_, os);
        }
    }

    XYCoords SettlerManager::getBestPlot() const
    {
        // simplistic version for use by tactics to evaluate upcoming city builds for required techs, etc...
        // todo - remove hardcoded min value
        return sitesInfo_.empty() || sitesInfo_.begin()->foundValue < 50 ? XYCoords() : sitesInfo_.begin()->coords;
    }

    CvPlot* SettlerManager::getBestPlot(int subAreaID, const std::vector<CvPlot*>& ignorePlots)
    {
        analysePlotValues();

        for (std::list<SiteInfo>::const_iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
        {
            CvPlot* pLoopPlot = gGlobals.getMap().plot(sitesIter->coords);

            if (pLoopPlot->getSubArea() == subAreaID)
            {
                if (std::find(ignorePlots.begin(), ignorePlots.end(), pLoopPlot) != ignorePlots.end())
                {
                    continue;
                }

                const CvPlot* pClosestCityPlot = pMapAnalysis_->getClosestCity(pLoopPlot, pLoopPlot->getSubArea(), false);
                int distance = pClosestCityPlot ? stepDistance(pClosestCityPlot->getX(), pClosestCityPlot->getY(), pLoopPlot->getX(), pLoopPlot->getY()) : MAX_INT;
                //int distance = getStepDistanceToClosestCity_(pLoopPlot, true);
                //int weight = getWeightedAverageCityDistance_(pLoopPlot, true);

                if (distance == MAX_INT || distance < 8)
                {
                    return pLoopPlot;
                }
            }
        }

        return NULL;
    }

    CvPlot* SettlerManager::getBestPlot(const CvUnit* pUnit, int subAreaID)
    {
#ifdef ALTAI_DEBUG
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
        std::ostream& os = CivLog::getLog(player)->getStream();
        os << "\n" << __FUNCTION__ << " unit: " << pUnit->getIDInfo() << " sub area: " << subAreaID << "\n";

        CityIter cityIter(player);
        int areaAverageWeightedDistance = 0, cityCount = 0, subAreaCityCount = 0;
        const CvCity* pLoopCity;
        while (pLoopCity = cityIter())
        {
            if (pLoopCity->plot()->getSubArea() == subAreaID)
            {
                os << ' ' << safeGetCityName(pLoopCity) << " wcd = " << getWeightedAverageCityDistance_(pLoopCity->plot(), true);
                ++subAreaCityCount;
                areaAverageWeightedDistance += getWeightedAverageCityDistance_(pLoopCity->plot(), true);
            }
        }
        if (subAreaCityCount > 0)
        {
            areaAverageWeightedDistance /= subAreaCityCount;
        }
        os << " current area average wcd = " << areaAverageWeightedDistance;

        os << "\nBest sites: (turn = " << gGlobals.getGame().getGameTurn() << ")";
        for (std::list<SiteInfo>::const_iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
        {
            os << "\n" << sitesIter->coords << " " << sitesIter->foundValue << " grv: " << sitesIter->growthRateValues.first << ", " << sitesIter->growthRateValues.second
                << ", wacd: " << sitesIter->weightedAverageCityDistance;
            for (std::map<BonusTypes, int>::const_iterator bIter(sitesIter->bonusPercentOutputChanges.begin()), bEndIter(sitesIter->bonusPercentOutputChanges.end()); bIter != bEndIter; ++bIter)
            {
                os << "\t:" << gGlobals.getBonusInfo(bIter->first).getType() << " %output change: " << bIter->second;
            }

            os << "\n";

            DotMap::const_iterator dotMapIter = dotMap_.find(DotMapItem(sitesIter->coords, PlotYield()));
            dotMapIter->debugOutputs(player_, os);
        }
        os << "\nDest map: ";
        for (SettlerDestinationMap::const_iterator ci(settlerDestinationMap_.begin()), ciEnd(settlerDestinationMap_.end()); ci != ciEnd; ++ci)
        {
            os << ci->first << " " << ci->second.first << " " << ci->second.second << ", ";
        }
        os << '\n';
#endif

        // see if have a destination already for this settler (initially this will be empty)
        SettlerDestinationMap::iterator iter = settlerDestinationMap_.find(pUnit->getIDInfo());
        XYCoords currentDestination(-1, -1);
        int currentValue = 0;
        if (iter != settlerDestinationMap_.end())  // have a current target plot for this settler
        {
            currentDestination = iter->second.second;
            currentValue = iter->second.first;

            for (std::list<SiteInfo>::const_iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
            {
                CvPlot* pPlot = gGlobals.getMap().plot(sitesIter->coords);

                if (sitesIter->coords == currentDestination) // entry still in map
                {
                    if (pUnit->canFound(pPlot))
                    {
                        return gGlobals.getMap().plot(currentDestination.iX, currentDestination.iY);
                    }
                }

                // adjacent - assume max has shifted
                if (stepDistance(sitesIter->coords.iX, sitesIter->coords.iY, currentDestination.iX, currentDestination.iY) == 1)
                {
#ifdef ALTAI_DEBUG
                    DotMap::const_iterator dotMapIter = dotMap_.find(DotMapItem(currentDestination, PlotYield()));
                    if (dotMapIter != dotMap_.end())
                    {
                        os << "\nold destination dotmap: \n";
                        dotMapIter->debugOutputs(player_, os);
                    }
                    dotMapIter = dotMap_.find(DotMapItem(sitesIter->coords, PlotYield()));
                    if (dotMapIter != dotMap_.end())
                    {
                        os << "\nnew destination dotmap: \n";
                        dotMapIter->debugOutputs(player_, os);
                    }

#endif
                    if (pUnit->canFound(pPlot))
                    {
#ifdef ALTAI_DEBUG
                        os << " updating destination for settler: " << pUnit->getIDInfo() << " from: " << currentDestination << " to: " << sitesIter->coords;
#endif
                        // update our destination
                        settlerDestinationMap_[pUnit->getIDInfo()] = std::make_pair(sitesIter->foundValue + sitesIter->growthRateValues.second, sitesIter->coords);
                        return pPlot;
                    }
                }
            }
#ifdef ALTAI_DEBUG
            os << " erasing current destination: " << currentDestination << " for settler: " << pUnit->getIDInfo();
#endif
            settlerDestinationMap_.erase(pUnit->getIDInfo());
        }   

        // find closest un-targetted 'best plot':
        std::vector<XYCoords> targetedPlots;
        XYCoords bestClosestSite(-1, -1); 
        int bestClosestSiteValue = 0;
        for (;;)
        {
            boost::tie(bestClosestSiteValue, bestClosestSite) = getClosestSite_(subAreaID, 70, targetedPlots, pUnit->plot());

            if (bestClosestSite == XYCoords(-1, -1))  // nothing found
            {
                break;
            }

            bool alreadyTargeted = false;
            for (SettlerDestinationMap::const_iterator si(settlerDestinationMap_.begin()), siEnd(settlerDestinationMap_.end()); si != siEnd; ++si)
            {
                if (stepDistance(si->second.second.iX, si->second.second.iY, bestClosestSite.iX, bestClosestSite.iY) <= 2)
                {
                    targetedPlots.push_back(bestClosestSite);
                    alreadyTargeted = true;
                    break;
                }
            }

            if (!alreadyTargeted)
            {
                break;
            }
        }

        CvPlot* pBestPlot = NULL;
        if (bestClosestSite != XYCoords(-1, -1))
        {
            pBestPlot = gGlobals.getMap().plot(bestClosestSite.iX, bestClosestSite.iY);
            //int distance = getStepDistanceToClosestCity_(pBestPlot, true);
            const CvPlot* pClosestCityPlot = pMapAnalysis_->getClosestCity(pBestPlot, pBestPlot->getSubArea(), false);
            int distance = pClosestCityPlot ? stepDistance(pClosestCityPlot->getX(), pClosestCityPlot->getY(), pBestPlot->getX(), pBestPlot->getY()) : MAX_INT;
            int weight = getWeightedAverageCityDistance_(pBestPlot, true);
#ifdef ALTAI_DEBUG
            os << pUnit->getIDInfo() << " setting destination to: " << bestClosestSite << " step distance to nearest city = " << distance << ", weight = " << weight;
#endif
            settlerDestinationMap_[pUnit->getIDInfo()] = std::make_pair(bestClosestSiteValue, bestClosestSite);
        }

        return pBestPlot;
    }

    void SettlerManager::eraseUnit(IDInfo unit)
    {
        settlerDestinationMap_.erase(unit);
    }

    DotMapItem SettlerManager::getPlotDotMap(XYCoords coords) const
    {
        DotMap::const_iterator ci = dotMap_.find(DotMapItem(coords, PlotYield()));
        if (ci != dotMap_.end())
        {
            return *ci;
        }
        return DotMapItem(coords, PlotYield());
    }

    void SettlerManager::write(FDataStreamBase* pStream) const
    {
    }

    void SettlerManager::read(FDataStreamBase* pStream)
    {
    }

    SettlerManager::SiteInfo::SiteInfo(DotMapItem& dotMapItem, int foundValue_, int weightedAverageCityDistance_, std::pair<int, int> growthRateValues_)
        : weightedAverageCityDistance(weightedAverageCityDistance_),
          coords(dotMapItem.coords), foundValue(foundValue_), baseYield(dotMapItem.getOutput().first), projectedYield(dotMapItem.projectedYield),
          requiredBuildingsAndYields(dotMapItem.requiredAdditionalYieldBuildings),
          growthRateValues(growthRateValues_)
    {
    }

    int SettlerManager::SiteInfo::getBonusOutputPercentTotal() const
    {
        int total = 0;
        for (std::map<BonusTypes, int>::const_iterator bonusIter(bonusPercentOutputChanges.begin()), endBonusIter(bonusPercentOutputChanges.end()); bonusIter != endBonusIter; ++bonusIter)
        {
            total += bonusIter->second;
        }
        return total;
    }

    std::ostream& SettlerManager::SiteInfo::debug(std::ostream& os) const
    {
        os << "\n\t" << coords << " found value: " << foundValue << ", baseYield: " << baseYield << ", projected: " << projectedYield << ", grv_1: " << growthRateValues.first << ", grv_2: " << growthRateValues.second;
        for (size_t i = 0, count = requiredBuildingsAndYields.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getBuildingInfo(requiredBuildingsAndYields[i].first).getType() << ": " << requiredBuildingsAndYields[i].second;
        }
        for (std::map<BonusTypes, int>::const_iterator ci(bonusPercentOutputChanges.begin()), ciEnd(bonusPercentOutputChanges.end()); ci != ciEnd; ++ci)
        {
            os << " " << gGlobals.getBonusInfo(ci->first).getType() << " %c: " << ci->second;
        }
        return os;
    }

    std::list<SettlerManager::SiteInfo>::iterator SettlerManager::findSite_(XYCoords coords)
    {
        std::list<SiteInfo>::iterator siteIter = std::find_if(sitesInfo_.begin(), sitesInfo_.end(), CoordsMatcher(coords));
        if (siteIter == sitesInfo_.end())
        {
            std::pair<int, int> growthRateData = calculateGrowthRateData_(coords, gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getAnalysis()->getTimeHorizon());
            CvPlot* pPlot = gGlobals.getMap().plot(coords);
            DotMap::iterator dotMapIter = dotMap_.find(DotMapItem(coords, PlotYield()));
            if (dotMapIter != dotMap_.end())
            {
                sitesInfo_.push_front(SiteInfo(*dotMapIter, pPlot->getFoundValue(playerType_), getWeightedAverageCityDistance_(pPlot, true), growthRateData));
            }
            //sitesInfo_.push_front(SiteInfo(coords, pPlot->getFoundValue(playerType_), getWeightedAverageCityDistance_(pPlot, true), growthRateData));
            siteIter = sitesInfo_.begin();
            // add maintenance data?
        }
        return siteIter;
    }

    SettlerManager::SiteInfo SettlerManager::getMaxSiteInfoData_() const
    {
        /*
        XYCoords coords;
            int foundValue, weightedAverageCityDistance, consumedWorkerCount, healthLevel;
            PlotYield baseYield, projectedYield;
            std::vector<std::pair<BuildingTypes, PlotYield> > requiredBuildingsAndYields;
            std::pair<int, int> growthRateValues;  // first, second ring
            std::map<BonusTypes, int> bonusPercentOutputChanges;
        */

        SettlerManager::SiteInfo maxSiteInfo;

        for (std::list<SiteInfo>::const_iterator siteIter(sitesInfo_.begin()), siteEndIter(sitesInfo_.end()); siteIter != siteEndIter; ++siteIter)
        {
            maxSiteInfo.foundValue = std::max<int>(siteIter->foundValue, maxSiteInfo.foundValue);
            maxSiteInfo.growthRateValues.first = std::max<int>(siteIter->growthRateValues.first, maxSiteInfo.growthRateValues.first);
            maxSiteInfo.growthRateValues.second = std::max<int>(siteIter->growthRateValues.second, maxSiteInfo.growthRateValues.second);
            for (int i = 0; i < NUM_YIELD_TYPES; ++i)
            {
                maxSiteInfo.baseYield[i] = std::max<int>(siteIter->baseYield[i], maxSiteInfo.baseYield[i]);
                maxSiteInfo.projectedYield[i] = std::max<int>(siteIter->projectedYield[i], maxSiteInfo.projectedYield[i]);
            }
            
            for (std::map<BonusTypes, int>::const_iterator bonusIter(siteIter->bonusPercentOutputChanges.begin()), endBonusIter(siteIter->bonusPercentOutputChanges.end());
                bonusIter != endBonusIter; ++bonusIter)
            {
                maxSiteInfo.bonusPercentOutputChanges[bonusIter->first] = std::max<int>(bonusIter->second, maxSiteInfo.bonusPercentOutputChanges[bonusIter->first]);
            }
        }

        return maxSiteInfo;
    }

    DotMapItem SettlerManager::analysePlotValue_(const MapAnalysis::PlotValues& plotValues, MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator ci)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player_.getCvPlayer())->getStream();
//#endif
        CvMap& theMap = gGlobals.getMap();
        CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);

        CvPlot* pPlot = theMap.plot(ci->first.iX, ci->first.iY);
        const PlotInfo::PlotInfoNode& plotInfoNode = pMapAnalysis_->getPlotInfoNode(pPlot);

        DotMapItem dotMapItem(ci->first, getPlotCityYield(plotInfoNode, playerType_), pPlot->isFreshWater(), pPlot->getBonusType(player.getTeam()));
//#ifdef ALTAI_DEBUG
//        const bool debug = ci->first == XYCoords(75, 38);
//#endif

        std::map<BuildingTypes, PlotYield> requiredBuildings;
        std::vector<ConditionalPlotYieldEnchancingBuilding> conditionalEnhancements = GameDataAnalysis::getInstance()->getConditionalPlotYieldEnhancingBuildings(playerType_);
        std::map<XYCoords, PlotYield> extraYieldsMap = getExtraConditionalYield(*ci, conditionalEnhancements, playerType_, 4, requiredBuildings);
        std::copy(requiredBuildings.begin(), requiredBuildings.end(), std::back_inserter(dotMapItem.requiredAdditionalYieldBuildings));

        // in case city plot has a conditional yield (may be so in the case of dykes)
        std::map<XYCoords, PlotYield>::const_iterator yi(extraYieldsMap.find(ci->first));
        if (yi != extraYieldsMap.end())
        {
            dotMapItem.cityPlotYield += yi->second;
            dotMapItem.conditionalYield = yi->second;
        }

        //if (!extraYieldsMap.empty())
        //{
        //    PlotYield totalExtraYield;
        //    for (std::map<XYCoords, PlotYield>::const_iterator ci(extraYieldsMap.begin()), ciEnd(extraYieldsMap.end()); ci != ciEnd; ++ci)
        //    {
        //        totalExtraYield += ci->second;
        //    }

        //    //std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType_))->getStream();
        //    //os << "\nextra plotyield = " << totalExtraYield;
        //}
        
        // todo - consider a version of this fn which takes into account only explored plots
        dotMapItem.numDeadLockedBonuses = player.AI_countDeadlockedBonuses(pPlot);

        // iterate over all plot keys
        for (MapAnalysis::PlotValues::PlotKeyCoordsMap::const_iterator mi(ci->second.begin()), miEnd(ci->second.end()); mi != miEnd; ++mi)
        {
            // iterator over all coords with this plot key
            for (std::set<XYCoords>::const_iterator si(mi->second.begin()), siEnd(mi->second.end()); si != siEnd; ++si)
            {
                const CvPlot* pLoopPlot = theMap.plot(si->iX, si->iY);

                if (!pLoopPlot->isRevealed(teamType_, false) || excludedCoords_.find(*si) != excludedCoords_.end())
                {
                    continue;
                }

                //if (*si == ci->first)  // todo - do we still need this?
                //{
                //    boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(CvPlayerAI::getPlayer(playerType_));
                //    std::ostream& os = pErrorLog->getStream();
                //    os << "\nFound duplicate plot: " << *si;
                //}

                // todo - move most of this into ctor
                DotMapItem::DotMapPlotData plotData;
                plotData.coords = *si;
                // oddly can't actually trust getYield here! Yields on unsettled land seem to sometimes just be the base yield without bonus yields taken into account
                // instead use possible imp data which has AltAI calculated yields for each imp type (incl. none)
                //plotData.currentYield = pLoopPlot->getYield();
                ImprovementTypes currentImp = pLoopPlot->getImprovementType();
                boost::tie(plotData.neighbourCityCount, plotData.workedByNeighbour) = getNeighbourCityData_(pLoopPlot);

                if (plotData.neighbourCityCount > 1)
                {
                    continue;  // don't bother if this plot is already shared
                }

                // already worked - not a very stable criterion - might want to rethink
                if (plotData.neighbourCityCount > 0 && plotData.workedByNeighbour && pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
                {
//#ifdef ALTAI_DEBUG
//                    if (debug)
//                    {
//                        os << "\nskipping plot: " << *si << " as worked by neighbour - turn = " << gGlobals.getGame().getGameTurn();
//                    }
//#endif
                    continue;
                }

                // check for extra yields for this coordinate
                std::map<XYCoords, PlotYield>::const_iterator yi(extraYieldsMap.find(*si));
                MapAnalysis::PlotValues::KeysValueMap::const_iterator ki(plotValues.keysValueMap.find(mi->first));

                if (ki != plotValues.keysValueMap.end())
                {
                    for (size_t i = 0, count = ki->second.size(); i < count; ++i)
                    {
                        plotData.possibleImprovements.push_back(ki->second[i]);

                        if (yi != extraYieldsMap.end())  // add any extra yields on
                        {
                            plotData.possibleImprovements.rbegin()->first += yi->second;
                            // store conditional yield separately too, so can account for what yields require building construction
                            plotData.conditionalYield = yi->second; 
                        }
                        if (getBaseImprovement(ki->second[i].second) == getBaseImprovement(currentImp))
                        {
                            // set current yield from calculated imp data - even if we have no available imps: -1 (no_imp) should still show up
                            plotData.currentYield = ki->second[i].first;
                        }
                    }
                }
                else
                {
                    std::ostream& os = ErrorLog::getLog(CvPlayerAI::getPlayer(playerType_))->getStream();
                    os << "\nTurn: " << gGlobals.getGame().getGameTurn() << " (analysePlotValue_): Failed to find plot key: " << mi->first << " for coords: " << *si;
                }
                
                //BonusTypes bonusType = boost::get<PlotInfo::BaseNode>(pMapAnalysis_->getPlotInfoNode(pLoopPlot)).bonusType;
                BonusTypes bonusType = pLoopPlot->getBonusType(player.getTeam());
                if (bonusType != NO_BONUS)
                {
                    dotMapItem.bonusTypes.insert(bonusType);
                    plotData.bonusType = bonusType;
                }

                dotMapItem.plotDataSet.insert(plotData);
            }
        }
        return dotMapItem;
    }

    void SettlerManager::findBestCitySites_()
    {
        CvMap& theMap = gGlobals.getMap();
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
        const PlayerPtr& pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
        const int timeHorizon = pPlayer->getAnalysis()->getTimeHorizon();

        const int MAX_DISTANCE_CITY_MAINTENANCE_ = gGlobals.getDefineINT("MAX_DISTANCE_CITY_MAINTENANCE");
        const int numCities = player.getNumCities();
        std::map<int, int> areaTotals;

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
#endif
        sitesInfo_.clear();
        bestSites_.clear();
        bestBonusSites_.clear();
        growthRateValues_.clear();
        checkedNeighbourCoords_.clear();

        populateMaxFoundValues_();
        populateBestBonusSites_();
        updateBonusValues_();

        std::set<XYCoords> fvmCoords;
        for (std::list<std::pair<XYCoords, int> >::const_iterator fvmIter = foundValueMaxes_.begin(), fvmEndIter = foundValueMaxes_.end(); fvmIter != fvmEndIter; ++fvmIter)
        {
            fvmCoords.insert(fvmIter->first);
            // add to site info list
            DotMap::iterator dotMapIter = dotMap_.find(DotMapItem(fvmIter->first, PlotYield()));
            if (dotMapIter != dotMap_.end())
            {
                XYCoords debugCoords(fvmIter->first);
                sitesInfo_.push_back(SiteInfo(*dotMapIter, fvmIter->second, getWeightedAverageCityDistance_(theMap.plot(fvmIter->first), true), growthRateValues_[fvmIter->first]));
            }
            //SiteInfo siteInfo(fvmIter->first, fvmIter->second, getWeightedAverageCityDistance_(theMap.plot(fvmIter->first), true));
            //sitesInfo_.push_back(siteInfo);
        }

        populateMaintenanceDeltas_(fvmCoords);
        populateGrowthRatesData_();

        const int maxGoldWithProcesses = pPlayer->getMaxGoldRateWithProcesses();
        maxGrowthIter_ = std::max_element(sitesInfo_.begin(), sitesInfo_.end(), GrowthRateValueP(true));
        if (maxGrowthIter_ == sitesInfo_.end())
        {
            return;
        }

#ifdef ALTAI_DEBUG
        os << "\n\t found growth rate value max: " << maxGrowthIter_->coords << " fv = " << maxGrowthIter_->foundValue 
           << " grv = " << maxGrowthIter_->growthRateValues.first << ", " << maxGrowthIter_->growthRateValues.second
           << ", max found value = " << maxFoundValue_ << ", max yields: " << maxYields_;
#endif

        if (numCities == 0)
        {
            findFirstCitySite_();
            return;
        }

        // consider bonus sites
        std::set<XYCoords> bonusSitesToConsider;
        {
            // map stores output % change for each output type for each available bonus
            for (std::map<BonusTypes, TotalOutput>::const_iterator bvIter(bonusValuesMap_.begin()), bvEndIter(bonusValuesMap_.end()); bvIter != bvEndIter; ++bvIter)
            {
                int maxPercentIncrease = std::max<int>(0, *std::max_element(bvIter->second.data.begin(), bvIter->second.data.end()));
                // e.g. if a resource gives 20% output bonus, and we have 3 cities, then threshold = 50% max found value
                // i.e. a site's found value has to be within this threshold of the max found value to be considered
                // set a lower bound of 30% to avoid considering really poor sites
                // with 6 or more cities, threshold will always be 30% - may want to rethink this
                int maxFoundValueThreshold = std::max<int>(30, 100 - maxPercentIncrease - (10 * numCities));
                
                std::map<BonusTypes, std::list<XYCoords> >::const_iterator bonusSitesIter = bestBonusSitesMap_.find(bvIter->first);
                if (bonusSitesIter != bestBonusSitesMap_.end())
                {
                    for (std::list<XYCoords>::const_iterator coordsIter(bonusSitesIter->second.begin()), coordsEndIter(bonusSitesIter->second.end()); coordsIter != coordsEndIter; ++coordsIter)
                    {
                        CvPlot* pSitePlot = theMap.plot(*coordsIter);
                        // only consider plots which are within %age max found value                        
                        if (((100 * pSitePlot->getFoundValue(playerType_)) / maxFoundValue_) >= maxFoundValueThreshold)
                        {
                            bonusSitesToConsider.insert(*coordsIter);
                            std::list<SiteInfo>::iterator siteInfoIter = findSite_(*coordsIter);
                            siteInfoIter->bonusPercentOutputChanges[bvIter->first] = maxPercentIncrease;
#ifdef ALTAI_DEBUG
                            os << "\n\t adding bonus site for: " << gGlobals.getBonusInfo(bvIter->first).getType() << " at: " << *coordsIter << " % inc. = " << maxPercentIncrease;
#endif
                        }
                    }
                }
            }
        }

        populateMaintenanceDeltas_(bonusSitesToConsider);
        //std::map<BonusTypes, std::map<XYCoords, int> > possibleBonusSitesMap;

        sitesInfo_.sort(CombinedSiteValueP(true, maxFoundValue_));
    }

    void SettlerManager::findFirstCitySite_()
    {
        const PlayerPtr& pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
        const int timeHorizon = pPlayer->getAnalysis()->getTimeHorizon();
        CvMap& theMap = gGlobals.getMap();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
#endif
        std::list<XYCoords> finalCandidates;
        std::list<SiteInfo>::iterator maxGIter = sitesInfo_.begin();
        for (std::list<SiteInfo>::iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
        {
            if (sitesIter->growthRateValues.second > maxGIter->growthRateValues.second)
            {
                maxGIter = sitesIter;
            }
#ifdef ALTAI_DEBUG
            os << "\n\t found value max: " << sitesIter->coords << " = " << sitesIter->foundValue 
               << " growth rate value = " << sitesIter->growthRateValues.first << ", " << sitesIter->growthRateValues.second;
#endif
        }

        SiteInfo maxSiteInfo = getMaxSiteInfoData_();
        os << "\n site info maxes: ";
        maxSiteInfo.debug(os);

        // loop over found value max locations' growth rate values - checking neighbour plots and storing either original site's or neighbour's found value + growth rate value
        for (std::list<SiteInfo>::iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
        {
            if (sitesIter->growthRateValues.second > maxGIter->growthRateValues.second * 4 / 5)
            {                
#ifdef ALTAI_DEBUG
                os << "\n checking neighbour sites for: " << sitesIter->coords << " growth rate = " << sitesIter->growthRateValues.second << " found value = " << sitesIter->foundValue;
#endif
                CvPlot* pSitePlot = theMap.plot(sitesIter->coords);
                int thisSitesMaxValue = sitesIter->growthRateValues.second + sitesIter->foundValue;
                XYCoords maxValueCoords;
                NeighbourPlotIter nIter(pSitePlot);
                while (IterPlot pLoopPlot = nIter())
                {
                    const XYCoords neighbourCoords = pLoopPlot->getCoords();

                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(pPlayer->getTeamID(), false) && pLoopPlot->getSubArea() == pSitePlot->getSubArea())
                    {
                        std::list<SiteInfo>::iterator neighbourSiteIter = std::find_if(sitesInfo_.begin(), sitesInfo_.end(), CoordsMatcher(neighbourCoords));
                        if (neighbourSiteIter == sitesInfo_.end())
                        {
                            DotMap::iterator dotMapIter = dotMap_.find(DotMapItem(neighbourCoords, PlotYield()));
                            if (dotMapIter != dotMap_.end())
                            {
                                sitesInfo_.push_front(SiteInfo(*dotMapIter, pLoopPlot->getFoundValue(playerType_), getWeightedAverageCityDistance_(pLoopPlot, true), calculateGrowthRateData_(neighbourCoords, timeHorizon)));
                            }

                            //sitesInfo_.push_front(SiteInfo(neighbourCoords, pLoopPlot->getFoundValue(playerType_), 
                            //    getWeightedAverageCityDistance_(pLoopPlot, true), calculateGrowthRateData_(neighbourCoords, timeHorizon)));
                            neighbourSiteIter = sitesInfo_.begin();
                        }
#ifdef ALTAI_DEBUG
                        os << "\n\t " << neighbourCoords << " grv: " << neighbourSiteIter->growthRateValues.second << " v. " << sitesIter->growthRateValues.second << " found value: " << neighbourSiteIter->foundValue;
#endif
                        int neighbourValue = neighbourSiteIter->growthRateValues.second + neighbourSiteIter->foundValue;
                        if (neighbourValue > thisSitesMaxValue)
                        {
                            maxValueCoords = neighbourCoords;
                            thisSitesMaxValue = neighbourValue;
#ifdef ALTAI_DEBUG
                            os << "\n\t set new max value: " << neighbourValue << " at: " << neighbourCoords;
#endif
                        }
                    }
                }
            }
        }
        sitesInfo_.sort(FirstCityValueP());
#ifdef ALTAI_DEBUG
        os << "\nSorted site infos: \n";
        for (std::list<SiteInfo>::const_iterator ci(sitesInfo_.begin()), ciEnd(sitesInfo_.end()); ci != ciEnd; ++ci)
        {
            ci->debug(os);
        }
#endif
    }

    void SettlerManager::clearFoundValues_()
    {
        const CvMap& theMap = gGlobals.getMap();
        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
        {
            theMap.plotByIndex(i)->setFoundValue(playerType_, 0);
        }
    }

    void SettlerManager::populateDotMap_()
    {
        dotMap_.clear();

        const MapAnalysis::PlotValues& plotValues = pMapAnalysis_->getPlotValues();

        for (MapAnalysis::PlotValues::PlotValueMap::const_iterator ci(plotValues.plotValueMap.begin()), ciEnd(plotValues.plotValueMap.end()); ci != ciEnd; ++ci)
        {
            for (MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator ci2(ci->second.begin()), ci2End(ci->second.end()); ci2 != ci2End; ++ci2)
            {
                dotMap_.insert(analysePlotValue_(plotValues, ci2));
            }
        }
    }

    void SettlerManager::populateMaxFoundValues_()
    {
        const PlayerPtr& pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);

#ifdef ALTAI_DEBUG        
        std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
#endif
        CvMap& theMap = gGlobals.getMap();
        foundValueMaxes_.clear();
        maxFoundValue_ = 0;
        maxYields_ = PlotYield();

        for (DotMap::const_iterator ci(dotMap_.begin()), ciEnd(dotMap_.end()); ci != ciEnd; ++ci)
        {
            for (int i = 0; i < NUM_YIELD_TYPES; ++i)
            {
                maxYields_[i] = std::max<int>(maxYields_[i], ci->projectedYield[i]);
            }

            int foundValue = ci->getFoundValue(*pPlayer); //  YieldValueFunctor(makeYieldW(6, 4, 3))(ci->projectedYield);
            maxFoundValue_ = std::max<int>(maxFoundValue_, foundValue);

            /*CvPlot* pPlot = theMap.plot(ci->coords.iX, ci->coords.iY);
            int value = pPlot->getFoundValue(playerType_);
            bool isMax = true;

            CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)1);
            while (IterPlot pLoopPlot = plotIter())
            {
                int thisPlotsValue = pLoopPlot->getFoundValue(playerType_);

                if (thisPlotsValue > value)
                {
                    isMax = false;
                    break;
                }
            }                

            if (isMax)
            {
                maxFoundValue_ = std::max<int>(maxFoundValue_, value);
                foundValueMaxes_.push_back(std::make_pair(ci->coords, value));
            }*/
        }

#ifdef ALTAI_DEBUG
        os << "\npopulateMaxFoundValues_(): max found value = " << maxFoundValue_ << ", max yields = " << maxYields_;
#endif

        // filter out values with less than 50% of maxYields_ for any type
        for (DotMap::const_iterator ci(dotMap_.begin()), ciEnd(dotMap_.end()); ci != ciEnd; ++ci)
        {
            bool lowYieldSite = false;
            for (int i = 0; i < NUM_YIELD_TYPES; ++i)
            {
                // set threshold at 2/5 max (40%)
                if (5 * ci->projectedYield[i] < 2 * maxYields_[i])
                {
                    lowYieldSite = true;
                    break;
                }
            }
            if (!lowYieldSite)
            {
                foundValueMaxes_.push_back(std::make_pair(ci->coords, ci->getFoundValue(*pPlayer))); // YieldValueFunctor(makeYieldW(6, 4, 3))(ci->projectedYield)));
#ifdef ALTAI_DEBUG
                os << "\nkeeping: " << ci->coords << " with yield: " << ci->projectedYield;
#endif
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\nskipping: " << ci->coords << " with yield: " << ci->projectedYield;
#endif
            }
        }

        // keep values more than 1/3 max value found
        //filterFoundValues_(LessThan(maxFoundValue_ / 3));
    }

    void SettlerManager::populateMaxBonusValues_()
    {
        for (BestSitesMap::iterator bonusSitesIter(bestBonusSites_.begin()), bonusSitesEndIter(bestBonusSites_.end()); bonusSitesIter != bonusSitesEndIter; ++bonusSitesIter)
        {
        }
    }

    void SettlerManager::populateBonusMap_()
    {
        // might be more efficient to build this from resource data in MapAnalysis
        // although would need to check each plot around each bonus in dotMap_
        bonusSitesMap_.clear();
        for (DotMap::const_iterator ci(dotMap_.begin()), ciEnd(dotMap_.end()); ci != ciEnd; ++ci)
        {
            for (DotMapItem::PlotDataConstIter plotIter(ci->plotDataSet.begin()), endPlotIter(ci->plotDataSet.end()); plotIter != endPlotIter; ++plotIter)
            {
                if (plotIter->bonusType != NO_BONUS)
                {
                    bonusSitesMap_[plotIter->bonusType].insert(ci->coords);
                }
            }
        }
    }

    void SettlerManager::populateBestBonusSites_()
    {
        CvMap& theMap = gGlobals.getMap();
        bestBonusSitesMap_.clear();

        for (std::map<BonusTypes, std::set<XYCoords> >::const_iterator ci(bonusSitesMap_.begin()), ciEnd(bonusSitesMap_.end()); ci != ciEnd; ++ci)
        {
            std::set<XYCoords>::const_iterator bestCoordsIter = std::max_element(ci->second.begin(), ci->second.end(), FoundValueP(theMap, playerType_));
            if (bestCoordsIter != ci->second.end())
            {
                int bestFoundValue = theMap.plot(*bestCoordsIter)->getFoundValue(playerType_);
                // track sites which are at least 70% of the best found value for this bonus
                std::remove_copy_if(ci->second.begin(), ci->second.end(), std::back_inserter(bestBonusSitesMap_[ci->first]), std::not1(FoundValueP(theMap, playerType_, (70 * bestFoundValue) / 100)));
            }
        }
    }

    void SettlerManager::populateMaintenanceDeltas_(const std::set<XYCoords>& coords)
    {
        for (std::set<XYCoords>::const_iterator ci(coords.begin()), ciEnd(coords.end()); ci != ciEnd; ++ci)
        {
             // zero out entry for coord in case we already calculated it (todo - skip calc or erase from passed list?)
             siteMaintenanceChangesMap_[*ci] = 0;
        }

        currentMaintenance_ = 0;
        CityIter iter(CvPlayerAI::getPlayer(playerType_));
        CvCity* pLoopCity;
        while (pLoopCity = iter())
        {
            MaintenanceHelper helper(pLoopCity);
            currentMaintenance_ += helper.getMaintenance();  // for each current city, add on its maintenance cost...
            for (std::set<XYCoords>::const_iterator ci(coords.begin()), ciEnd(coords.end()); ci != ciEnd; ++ci)
            {
                // ...and then for each site, add in the extra cost that that city will incur as a result of the new city
                // result is each entry in siteMaintenanceChangesMap_ has the cost of founding that city
                siteMaintenanceChangesMap_[*ci] += helper.getMaintenanceWithCity(*ci);
            }
        }

        // finally, calculate the new city's cost for each potential site and store as a delta from current maintenance cost
        for (std::set<XYCoords>::const_iterator ci(coords.begin()), ciEnd(coords.end()); ci != ciEnd; ++ci)
        {
            MaintenanceHelper helper(*ci, playerType_);
            siteMaintenanceChangesMap_[*ci] = helper.getMaintenance() - currentMaintenance_;
        }

#ifdef ALTAI_DEBUG
        // debug
        const PlayerPtr& pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);        
        std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
        os << "\nChecking best sites. Current maintenance = " << currentMaintenance_ << ", max gold = " << pPlayer->getMaxGoldRate() 
           << ", max gold with processes = " << pPlayer->getMaxGoldRateWithProcesses();
#endif
    }

    void SettlerManager::populateGrowthRatesData_()
    {
        const PlayerPtr& pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
        const int timeHorizon = pPlayer->getAnalysis()->getTimeHorizon();

        for (std::list<std::pair<XYCoords, int> >::const_iterator ci(foundValueMaxes_.begin()), ciEnd(foundValueMaxes_.end()); ci != ciEnd; ++ci)
        {
            growthRateValues_[ci->first] = calculateGrowthRateData_(ci->first, timeHorizon);
        }

        for (std::list<SiteInfo>::iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
        {
            sitesIter->growthRateValues = calculateGrowthRateData_(sitesIter->coords, timeHorizon);
        }
    }

    void SettlerManager::updateBonusValues_()
    {
        bonusValuesMap_.clear();

        const PlayerPtr& pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
        TotalOutput currentOutput = pPlayer->getCurrentProjectedOutput();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
        os << '\n' << __FUNCTION__ << " current civ output = " << currentOutput;
#endif

        for (std::map<BonusTypes, std::set<XYCoords> >::const_iterator bonusIter(bonusSitesMap_.begin()), endBonusIter(bonusSitesMap_.end()); bonusIter != endBonusIter; ++bonusIter)
        {
            int currentCount = pMapAnalysis_->getControlledResourceCount(bonusIter->first);
            if (currentCount == 0)
            {
                TacticSelectionData& selectionData = pPlayer->getAnalysis()->getPlayerTactics()->getBaseTacticSelectionData();
                PlayerTactics::ResourceTacticsMap::const_iterator bonusTacticIter = pPlayer->getAnalysis()->getPlayerTactics()->resourceTacticsMap_.find(bonusIter->first);
                if (bonusTacticIter != pPlayer->getAnalysis()->getPlayerTactics()->resourceTacticsMap_.end())
                {
                    bonusTacticIter->second->apply(selectionData);
                    TechTypes resourceWorkTech = bonusTacticIter->second->getTechDependency()->getResearchTech();

                    if (!isEmpty(selectionData.potentialResourceOutputDeltas[bonusIter->first].second))
                    {
                        bonusValuesMap_[bonusIter->first] = selectionData.potentialResourceOutputDeltas[bonusIter->first].second;
#ifdef ALTAI_DEBUG
                        TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));

                        os << "\n\t" << gGlobals.getBonusInfo(bonusIter->first).getType();
                        os << ", resource value: " << valueF(selectionData.potentialResourceOutputDeltas[bonusIter->first].first) / 100 
                           << ", projected output delta: " << selectionData.potentialResourceOutputDeltas[bonusIter->first].first 
                           << ", %age change: " << selectionData.potentialResourceOutputDeltas[bonusIter->first].second;
                        if (resourceWorkTech != NO_TECH && pPlayer->getTechResearchDepth(resourceWorkTech) > 0)
                        {
                            os << " required tech = " << gGlobals.getTechInfo(resourceWorkTech).getType() << " depth = " << pPlayer->getTechResearchDepth(resourceWorkTech);
                        }
#endif
                    }
                }
            }
        }

#ifdef ALTAI_DEBUG
        if (!bonusValuesMap_.empty())
        {
            os << "\nbonus economic value map: ";
        }
        for (std::map<BonusTypes, TotalOutput>::const_iterator ci(bonusValuesMap_.begin()), ciEnd(bonusValuesMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\n" << gGlobals.getBonusInfo(ci->first).getType() << " = " << ci->second;
        }
#endif
    }

    // todo - move this into DotMap class perhaps?
    std::pair<int, int> SettlerManager::calculateGrowthRateData_(XYCoords coords, const int timeHorizon)
    {
        const CvPlayer& player = *gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getCvPlayer();
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(player)->getStream();
#endif
        const CvCity* pCapitalCity = player.getCapitalCity();        

        // todo make this general
        std::set<BonusTypes> existingBonuses;
        int baseHappy = 0, baseHealthy = 0;
        if (pCapitalCity)
        {
            for (int bonusType = 0, count = gGlobals.getNumBonusInfos(); bonusType < count; ++bonusType)
            {
                if (pCapitalCity->getNumBonuses((BonusTypes)bonusType) > 0)
                {
                    existingBonuses.insert((BonusTypes)bonusType);
                    baseHappy += gGlobals.getBonusInfo((BonusTypes)bonusType).getHappiness();
                    baseHealthy += gGlobals.getBonusInfo((BonusTypes)bonusType).getHealth();
                }
            }
        }
        else
        {
            baseHappy = 1;  // to allow for palace
        }
        const int baseHealthPopulation = baseHealthy + gGlobals.getHandicapInfo(player.getHandicapType()).getHealthBonus();

        DotMap::iterator dotMapIter = dotMap_.find(DotMapItem(coords, PlotYield()));
        if (dotMapIter != dotMap_.end())
        {
            std::vector<int> firstRingGrowthTurns = dotMapIter->getGrowthRates(player, dotMapIter->plotDataSet.size(), baseHealthPopulation, 1), 
                secondRingGrowthTurns = dotMapIter->getGrowthRates(player, dotMapIter->plotDataSet.size(), baseHealthPopulation, 2);

            int firstRingValue = 0, secondRingValue = 0;
            for (size_t i = 0, count = firstRingGrowthTurns.size(); i < count; ++i)
            {
                firstRingValue += (i + 1) * std::max<int>(0, timeHorizon - firstRingGrowthTurns[i]);
            }
            for (size_t i = 0, count = secondRingGrowthTurns.size(); i < count; ++i)
            {
                secondRingValue += (i + 1) * std::max<int>(0, timeHorizon - secondRingGrowthTurns[i]);
            }

#ifdef ALTAI_DEBUG
            os << "\nDotmap for: " << coords;
            dotMapIter->debugOutputs(player_, os);
            os << "\n\tgrowth at turns: (c1)";
            for (size_t i = 0, count = firstRingGrowthTurns.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << firstRingGrowthTurns[i];
            }
            os << "\n\tgrowth at turns: (c2)";
            for (size_t i = 0, count = secondRingGrowthTurns.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << secondRingGrowthTurns[i];
            }
            os << "\ngrowth rate values = " << firstRingValue << ", " << secondRingValue;
#endif

            return std::make_pair(firstRingValue, secondRingValue);
        }
        else
        {
            return std::make_pair(0, 0);
        }
    }

    void SettlerManager::findMaxGrowthRateSite_(bool includeSecondRing)
    {
        // todo
    }

    void SettlerManager::populatePlotCountAndResources_(XYCoords coords, int& improveablePlotCount, std::map<BonusTypes, int>& resourcesMap)
    {
        improveablePlotCount = 0;
        resourcesMap.clear();
        DotMap::const_iterator siteIter(dotMap_.find(DotMapItem(coords, PlotYield())));
        if (siteIter == dotMap_.end())
        {
            return;
        }

        for (DotMapItem::PlotDataConstIter plotIter(siteIter->plotDataSet.begin()), endPlotIter(siteIter->plotDataSet.end()); plotIter != endPlotIter; ++plotIter)
        {
            if (plotIter->getWorkedImprovement() != NO_IMPROVEMENT)
            {
                ++improveablePlotCount;
            }

            if (plotIter->bonusType != NO_BONUS)
            {
                ++resourcesMap[plotIter->bonusType];
            }
        }
    }

    int SettlerManager::doSiteValueAdjustment_(XYCoords coords, int baseValue, int maintenanceDelta, int improveablePlotCount, const std::map<BonusTypes, int>& bonusCounts)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
#ifdef ALTAI_DEBUG
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
        std::ostream& os = CivLog::getLog(player)->getStream();
#endif
        int finalSiteValue = baseValue;

#ifdef ALTAI_DEBUG
        os << "\nsite: " << coords << " initial site value: " << baseValue;
        os << " maintenance adjustment: " << -maintenanceDelta / 5;  // prob should remove this and just use weighted distance metric
        //os << " improveable plot count adjustment: " << 2 * improveablePlotCount;
#endif
        // todo - add value of new trade routes in here too
        finalSiteValue -= maintenanceDelta / 5;
        //finalSiteValue += 2 * improveablePlotCount;

        for (std::map<BonusTypes, int>::const_iterator bonusIter(bonusCounts.begin()), endBonusIter(bonusCounts.end()); bonusIter != endBonusIter; ++bonusIter)
        {
            int currentCount = pMapAnalysis_->getControlledResourceCount(bonusIter->first);
#ifdef ALTAI_DEBUG
            os << " " << bonusIter->second << " " << gGlobals.getBonusInfo(bonusIter->first).getType() << (currentCount == 0 ? " (new) +" : "+");
            os << bonusIter->second * (currentCount == 0 ? 4 : 2);
#endif
            finalSiteValue += bonusIter->second * (currentCount == 0 ? 4 : 2);

            boost::shared_ptr<ResourceInfo> pResourceInfo = pPlayer->getAnalysis()->getResourceInfo(bonusIter->first);
            // todo - handle military resource site selection separately
            std::pair<int, int> unitCounts = getResourceMilitaryUnitCount(pResourceInfo);

            if (unitCounts.first > 0 || unitCounts.second > 0)
            {
#ifdef ALTAI_DEBUG
                os << " r/o for: " << unitCounts.first << ", " << unitCounts.second << " units";
                os << " +" << std::min<int>(10, 2 * unitCounts.first + unitCounts.second);
#endif
                finalSiteValue += std::min<int>(10, 2 * unitCounts.first + unitCounts.second);// + 
            }
        }
#ifdef ALTAI_DEBUG
        os << "\nfinal site value = " << finalSiteValue;
#endif
        return finalSiteValue;
    }

    // todo - make into a utility fn?
    std::pair<int, bool> SettlerManager::getNeighbourCityData_(const CvPlot* pPlot) const
    {
        int count = 0;
        bool workedByNeighbour = false;

        for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
        {
            CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
            while (IterPlot pLoopPlot = plotIter())
            {
                if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType_, false))
                {
                    // count only our cities (maybe revisit this?)
                    if (pLoopPlot->isCity() && pLoopPlot->getOwner() == playerType_)                    
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

    // possibly a utility fn?
    int SettlerManager::getWeightedAverageCityDistance_(const CvPlot* pPlot, bool sameArea) const
    {
        const int areaID = pPlot->getArea();

        CityIter cityIter(CvPlayerAI::getPlayer(playerType_));
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

    std::pair<int, XYCoords> SettlerManager::getClosestSite_(const int subArea, const int percentThreshold, const std::vector<XYCoords>& ignorePlots, const CvPlot* pPlot) const
    {
#ifdef ALTAI_DEBUG
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
        std::ostream& os = CivLog::getLog(player)->getStream();
        os << "\n" << __FUNCTION__ << " threshold = " << percentThreshold;
        if (!ignorePlots.empty())
        {
            os << "\n\tignore plots: ";
            for (size_t ignorePlotIndex = 0, ignorePlotCount = ignorePlots.size(); ignorePlotIndex < ignorePlotCount; ++ignorePlotIndex)
            {
                if (ignorePlotIndex > 0) os << ", ";
                os << ignorePlots[ignorePlotIndex];
            }
        }
#endif
        XYCoords site(-1, -1);
        int maxSiteValue = 0, minSiteDistance = MAX_INT, closestSiteValue = 0;
        for (std::list<SiteInfo>::const_iterator sitesIter(sitesInfo_.begin()), sitesEndIter(sitesInfo_.end()); sitesIter != sitesEndIter; ++sitesIter)
        {
            bool skipThisSite = false;
            for (size_t ignorePlotIndex = 0, ignorePlotCount = ignorePlots.size(); ignorePlotIndex < ignorePlotCount; ++ignorePlotIndex)
            {
                if (stepDistance(sitesIter->coords.iX, sitesIter->coords.iY, ignorePlots[ignorePlotIndex].iX, ignorePlots[ignorePlotIndex].iY) <= 3)
                {
                    skipThisSite = true;
#ifdef ALTAI_DEBUG
                    os << "\n\tskipping: " << sitesIter->coords << " as too close: " << stepDistance(sitesIter->coords.iX, sitesIter->coords.iY, ignorePlots[ignorePlotIndex].iX, ignorePlots[ignorePlotIndex].iY)
                        << " to ignore plot: " << ignorePlots[ignorePlotIndex];
#endif
                    break;
                }
            }

            if (skipThisSite)
            {
                continue;
            }

            CvPlot* pLoopPlot = gGlobals.getMap().plot(sitesIter->coords);

            // since sitesInfo_ is sorted by value + growth rate value, first site we find will have the highest value
            if (pLoopPlot->getSubArea() == subArea)
            {
                // not going to find any more sites to consider
                int siteValue = CombinedSiteValueP(true, maxFoundValue_)(*sitesIter);
                if (siteValue < (percentThreshold * maxFoundValue_) / 100)
                {
#ifdef ALTAI_DEBUG
                    os << "\nquitting search as site: " << sitesIter->coords << " with site value: " << siteValue << " less than threshold %";
#endif
                    break;
                }

                //int thisSiteDistance = getStepDistanceToClosestCity_(pLoopPlot, true);
                int weight = getWeightedAverageCityDistance_(pLoopPlot, true);
                if (weight < minSiteDistance)
                {
                    minSiteDistance = weight;
                    closestSiteValue = siteValue;
                    site = sitesIter->coords;
#ifdef ALTAI_DEBUG
                    os << "\nsite: " << sitesIter->coords << " value = " << siteValue;
#endif
                }
                if (weight == 0)  // indicates this is the first city being considered
                {
                    // found existing settler plot (and no existing targeted settlers)
                    // want to compare siteValue of this plot to see if it's worth moving
                    if (ignorePlots.empty() && sitesIter->coords == pPlot->getCoords())
                    {
                        // is current plot's site value at least 95% of best value?
                        if ((closestSiteValue * 95) / 100 < siteValue)
                        {
#ifdef ALTAI_DEBUG
                            os << "\nselecting current plot over moving as site value is: " << (100 * siteValue) / closestSiteValue << "% of max found site value";
#endif
                            // maybe also check if we lose/gain bonuses?
                            closestSiteValue = siteValue;
                            site = sitesIter->coords;
                        }
                        break;
                    }                    
                }
            }
        }

        return std::make_pair(closestSiteValue, site);
    }
}