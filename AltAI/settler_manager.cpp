#include "./settler_manager.h"
#include "./player_analysis.h"
#include "./gamedata_analysis.h"
#include "./city_optimiser.h"
#include "./plot_info_visitors.h"
#include "./building_info_visitors.h"
#include "./resource_info_visitors.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./iters.h"
#include "./civ_log.h"
#include "./map_log.h"
#include "./error_log.h"

namespace AltAI
{
    SettlerManager::SettlerManager(const boost::shared_ptr<MapAnalysis>& pMapAnalysis) : pMapAnalysis_(pMapAnalysis)
    {
        playerType_ = pMapAnalysis->getPlayer().getPlayerID();
        teamType_ = pMapAnalysis->getPlayer().getTeamID();
    }

    std::vector<int> SettlerManager::getBestCitySites(int minValue, int count) const
    {
        int foundCount = 0;
        std::vector<int> sites;

        // prevent settling new sites if we are nearly broke
        if (pMapAnalysis_->getPlayer().getMaxResearchRate() < 30)
        {
            return sites;
        }

        for (std::map<int, XYCoords, std::greater<int> >::const_iterator ci(bestSites_.begin()), ciEnd(bestSites_.end()); ci != ciEnd; ++ci)
        {
            if (foundCount > count)
            {
                break;
            }

            if (ci->first > minValue)
            {
                ++foundCount;
                sites.push_back(gGlobals.getMap().plotNum(ci->second.iX, ci->second.iY));
            }
        }
        return sites;
    }

    int SettlerManager::getOverseasCitySitesCount(int minValue, int count, int subAreaID) const
    {
        int foundCount = 0;

        // prevent settling new sites if we are nearly broke
        if (pMapAnalysis_->getPlayer().getMaxResearchRate() < 30)
        {
            return 0;
        }

        for (std::map<int, XYCoords, std::greater<int> >::const_iterator ci(bestSites_.begin()), ciEnd(bestSites_.end()); ci != ciEnd; ++ci)
        {
            if (foundCount > count)
            {
                break;
            }

            if (ci->first > minValue)
            {
                if (gGlobals.getMap().plot(ci->second.iX, ci->second.iY)->getSubArea() != subAreaID)
                {
                    ++foundCount;
                }
            }
        }
        return foundCount;
    }

    CvPlot* SettlerManager::getBestPlot(int subAreaID, const std::vector<CvPlot*>& ignorePlots) const
    {
        for (std::map<int, XYCoords, std::greater<int> >::const_iterator ci(bestSites_.begin()), ciEnd(bestSites_.end()); ci != ciEnd; ++ci)
        {
            if (ci->first < 50)
            {
                break;
            }

            CvPlot* pLoopPlot = gGlobals.getMap().plot(ci->second.iX, ci->second.iY);

            if (pLoopPlot->getSubArea() == subAreaID)
            {
                if (std::find(ignorePlots.begin(), ignorePlots.end(), pLoopPlot) != ignorePlots.end())
                {
                    continue;
                }

                int distance = getStepDistanceToClosestCity_(pLoopPlot, true);
                if (distance == MAX_INT || distance < 8)
                {
                    return pLoopPlot;
                }
            }
        }
        return NULL;
    }

    CvPlot* SettlerManager::getBestPlot(const CvUnitAI* pUnit, int subAreaID)
    {
        SettlerDestinationMap::iterator iter = settlerDestinationMap_.find(pUnit->getIDInfo());
        XYCoords currentDestination(-1, -1);
        int currentValue = 0;
        if (iter != settlerDestinationMap_.end())
        {
            currentDestination = iter->second.second;
            currentValue = iter->second.first;
        }

        std::map<int, XYCoords, std::greater<int> >::const_iterator bestMatchIter = bestSites_.end();

        for (std::map<int, XYCoords, std::greater<int> >::const_iterator ci(bestSites_.begin()), ciEnd(bestSites_.end()); ci != ciEnd; ++ci)
        {
            if (ci->first < 50)
            {
                break;
            }

            CvPlot* pLoopPlot = gGlobals.getMap().plot(ci->second.iX, ci->second.iY);

            if (pLoopPlot->getSubArea() == subAreaID)
            {
                // do we know where we are going?
                if (currentDestination != XYCoords(-1, -1))
                {
                    // not significantly worse than last time
                    if (ci->first * 120 > currentValue * 100)
                    {
                        if (currentDestination == ci->second)
                        {
                            bestMatchIter = ci;
                            break;
                        }
                        else if (bestMatchIter == bestSites_.end())
                        {
                            bestMatchIter = ci;
                        }
                        // closer than before
                        else if (stepDistance(ci->second.iX, ci->second.iY, currentDestination.iX, currentDestination.iY) < 
                            stepDistance(bestMatchIter->second.iX, bestMatchIter->second.iY, currentDestination.iX, currentDestination.iY)
                            && ci->first * 120 > currentValue * 100) 
                        {
                            bestMatchIter = ci;
                        }
                    }
                }
                else
                {
                    int distance = getStepDistanceToClosestCity_(pLoopPlot, true);
                    if (distance == MAX_INT || distance < 8)
                    {
                        settlerDestinationMap_[pUnit->getIDInfo()] = *ci;
                        return pLoopPlot;
                    }
                }
            }
        }

        if (bestMatchIter != bestSites_.end())
        {
            settlerDestinationMap_[pUnit->getIDInfo()] = *bestMatchIter;
            return gGlobals.getMap().plot(bestMatchIter->second.iX, bestMatchIter->second.iY);
        }
        else
        {
            return NULL;
        }
    }

    void SettlerManager::analysePlotValues()
    {
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        const CvMap& theMap = gGlobals.getMap();
        for (int i = 0, count = theMap.numPlots(); i < count; ++i)
	    {
            theMap.plotByIndex(i)->setFoundValue(playerType_, 0);
        }

        dotMap_.clear();

        const MapAnalysis::PlotValues& plotValues = pMapAnalysis_->getPlotValues();

        for (MapAnalysis::PlotValues::PlotValueMap::const_iterator ci(plotValues.plotValueMap.begin()), ciEnd(plotValues.plotValueMap.end()); ci != ciEnd; ++ci)
        {
            for (MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator ci2(ci->second.begin()), ci2End(ci->second.end()); ci2 != ci2End; ++ci2)
            {
                dotMap_.insert(analysePlotValue_(ci2));
            }
        }

//#ifdef ALTAI_DEBUG
        // debug
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
        std::ostream& os = CivLog::getLog(player)->getStream();
//#endif

        for (DotMap::iterator iter(dotMap_.begin()), endIter(dotMap_.end()); iter != endIter; ++iter)
        {
            DotMapOptimiser opt(*iter, playerType_);
            //opt.optimise();
            std::vector<YieldTypes> yieldTypes(boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE));
            opt.optimise(yieldTypes, iter->plotData.size());
            iter->debugOutputs(os);
        }

        YieldValueFunctor valueF(makeYieldW(0, 6, 2));
        for (DotMap::const_iterator ci(dotMap_.begin()), ciEnd(dotMap_.end()); ci != ciEnd; ++ci)
        {
            std::pair<PlotYield, int> plotYieldAndSurplusFood = ci->getOutput(playerType_);
            int value = valueF(plotYieldAndSurplusFood.first);
            value += plotYieldAndSurplusFood.second * 8;

            PlotYield maxYield;
            ImprovementTypes bestImprovement;
            boost::tie(maxYield, bestImprovement) = getMaxYield(pMapAnalysis_->getPlotInfoNode(theMap.plot(ci->coords.iX, ci->coords.iY)), playerType_, 3);

            const DotMapItem::PlotData cityPlotData = ci->getPlotData(ci->coords);
            bool onFoodSpecial = cityPlotData.bonusType != NO_BONUS && maxYield[YIELD_FOOD] > 4 && bestImprovement != NO_IMPROVEMENT;
            bool onGoodSpecial = cityPlotData.bonusType != NO_BONUS && maxYield[YIELD_FOOD] >= foodPerPop && (maxYield[YIELD_PRODUCTION] > 3 || maxYield[YIELD_COMMERCE] > 6);

            if (onFoodSpecial || onGoodSpecial)
            {
                os << "\nTreating plot: " << ci->coords << " as good/food special: maxYield = " << maxYield;
                value *= 4;
                value /= 5;
            }

            if (ci->numDeadLockedBonuses > 0)
            {
                value /= 2;
            }

            value = std::max<int>(0, value);

            theMap.plot(ci->coords.iX, ci->coords.iY)->setFoundValue(playerType_, value);
        }

        findBestCitySites_();

#ifdef ALTAI_DEBUG
        //if (!(gGlobals.getGame().getGameTurn() % 10))
        //{
        //    debugDotMap();
        //}
#endif
    }

    DotMapItem SettlerManager::analysePlotValue_(MapAnalysis::PlotValues::SubAreaPlotValueMap::const_iterator ci)
    {
        CvMap& theMap = gGlobals.getMap(); 
        const MapAnalysis::PlotValues& plotValues = pMapAnalysis_->getPlotValues();

        CvPlot* pPlot = theMap.plot(ci->first.iX, ci->first.iY);
        const PlotInfo::PlotInfoNode& plotInfoNode = pMapAnalysis_->getPlotInfoNode(pPlot);

        DotMapItem dotMapItem(ci->first, getPlotCityYield(plotInfoNode, playerType_));

        std::vector<ConditionalPlotYieldEnchancingBuilding> conditionalEnhancements = GameDataAnalysis::getInstance()->getConditionalPlotYieldEnhancingBuildings(playerType_);
        std::map<XYCoords, PlotYield> extraYieldsMap = getExtraConditionalYield(*ci, conditionalEnhancements, playerType_, 4);

        // debug
        if (!extraYieldsMap.empty())
        {
            PlotYield totalExtraYield;
            for (std::map<XYCoords, PlotYield>::const_iterator ci(extraYieldsMap.begin()), ciEnd(extraYieldsMap.end()); ci != ciEnd; ++ci)
            {
                totalExtraYield += ci->second;
            }

            //std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType_))->getStream();
            //os << "\nextra plotyield = " << totalExtraYield;
        }
        
        dotMapItem.numDeadLockedBonuses = CvPlayerAI::getPlayer(playerType_).AI_countDeadlockedBonuses(pPlot);

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

                DotMapItem::PlotData plotData;
                plotData.coords = *si;
                boost::tie(plotData.neighbourCityCount, plotData.workedByNeighbour) = getNeighbourCityData_(pLoopPlot);

                if (plotData.neighbourCityCount > 1)
                {
                    continue;  // don't bother if this plot is already shared
                }

                // already worked
                if (plotData.neighbourCityCount > 0 && plotData.workedByNeighbour && pLoopPlot->getImprovementType() != NO_IMPROVEMENT)
                {
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
                        }
                    }
                }
                else
                {
                    std::ostream& os = ErrorLog::getLog(CvPlayerAI::getPlayer(playerType_))->getStream();
                    os << "\n(analysePlotValue_): Failed to find plot key: " << mi->first << " for coords: " << *si;
                }
                
                BonusTypes bonusType = boost::get<PlotInfo::BaseNode>(pMapAnalysis_->getPlotInfoNode(pLoopPlot)).bonusType;
                if (bonusType != NO_BONUS)
                {
                    dotMapItem.bonusTypes.insert(bonusType);
                    plotData.bonusType = bonusType;
                }

                dotMapItem.plotData.insert(plotData);
            }
        }
        return dotMapItem;
    }

    void SettlerManager::populateBestCitySitesMap_(std::map<XYCoords, int>& bestSitesMap)
    {
        CvMap& theMap = gGlobals.getMap();

        for (DotMap::const_iterator ci(dotMap_.begin()), ciEnd(dotMap_.end()); ci != ciEnd; ++ci)
        {
            CvPlot* pPlot = theMap.plot(ci->coords.iX, ci->coords.iY);
            int value = pPlot->getFoundValue(playerType_);
            bool isMax = true;

            int i = 1;
            //for (int i = 1; i <= CITY_PLOTS_RADIUS; ++i)
            //{
                CultureRangePlotIter plotIter(pPlot, (CultureLevelTypes)i);
                while (IterPlot pLoopPlot = plotIter())
                {
                    int thisPlotsValue = pLoopPlot->getFoundValue(playerType_);

                    if (thisPlotsValue > value)
                    {
                        isMax = false;
                        break;
                    }
                }
                //if (!isMax)
                //{
                //    break;
                //}
            //}

            if (isMax && value > 50)
            {
                bestSitesMap.insert(std::make_pair(ci->coords, value));
            }
        }
    }

    void SettlerManager::populatePlotCountAndResources_(XYCoords coords, std::map<XYCoords, int>& plotCountMap, std::map<XYCoords, std::map<BonusTypes, int> >& resourcesMap)
    {
        int improveablePlotCount = 0;
        DotMap::const_iterator siteIter(dotMap_.find(DotMapItem(coords, PlotYield())));
        if (siteIter == dotMap_.end())
        {
            return;
        }

        for (DotMapItem::PlotDataConstIter plotIter(siteIter->plotData.begin()), endPlotIter(siteIter->plotData.end()); plotIter != endPlotIter; ++plotIter)
        {
            if (plotIter->getWorkedImprovement() != NO_IMPROVEMENT)
            {
                ++improveablePlotCount;
            }

            if (plotIter->bonusType != NO_BONUS)
            {
                ++resourcesMap[coords][plotIter->bonusType];
            }
        }
        plotCountMap.insert(std::make_pair(coords, improveablePlotCount));
    }

    int SettlerManager::doSiteValueAdjustment_(XYCoords coords, int baseValue, int maintenanceDelta,
        const std::map<XYCoords, int>& plotCountMap, const std::map<XYCoords, std::map<BonusTypes, int> >& resourcesMap)
    {
        boost::shared_ptr<Player> pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
//#ifdef ALTAI_DEBUG
//        // debug
//        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);
//        std::ostream& os = CivLog::getLog(player)->getStream();
//#endif
        int finalSiteValue = baseValue;

//#ifdef ALTAI_DEBUG
//      //if (!(i % 10)) os << "\n";
//      os << "\n";
//      os << coords << " = " << baseValue << ", ";
//      os << " maintenance delta = " << maintenanceIter->second - currentMaintenance;
//      // todo - add value of new trade routes in here too
//      os << " new max research rate = " << gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getMaxResearchRate(
//            std::make_pair(fixedIncome, fixedExpenses + maintenanceIter->second - currentMaintenance));
//#endif
        
        // todo - check how much rounding is going to matter here
        finalSiteValue -= 10 * (maintenanceDelta / 100);

        std::map<XYCoords, int>::const_iterator plotCountIter(plotCountMap.find(coords));

//#ifdef ALTAI_DEBUG
//      os << " improveable plot count = " << plotCountIter->second << " ";
//#endif

        finalSiteValue += 2 * plotCountIter->second;

        std::map<XYCoords, std::map<BonusTypes, int> >::const_iterator resourceMapIter(resourcesMap.find(coords));
        if (resourceMapIter != resourcesMap.end())
        {
            for (std::map<BonusTypes, int>::const_iterator bonusIter(resourceMapIter->second.begin()), endBonusIter(resourceMapIter->second.end()); bonusIter != endBonusIter; ++bonusIter)
            {
                int currentCount = pMapAnalysis_->getControlledResourceCount(bonusIter->first);
//#ifdef ALTAI_DEBUG
//              os << "\n" << gGlobals.getBonusInfo(bonusIter->first).getType() << " = " << bonusIter->second << (currentCount == 0 ? " (new)" : "")<< ", ";
//#endif
                finalSiteValue += bonusIter->second * (currentCount == 0 ? 4 : 2);

                boost::shared_ptr<ResourceInfo> pResourceInfo = pPlayer->getAnalysis()->getResourceInfo(bonusIter->first);
                std::pair<int, int> unitCounts = getResourceMilitaryUnitCount(pResourceInfo);

                ResourceHappyInfo happyInfo = getResourceHappyInfo(pResourceInfo);
                ResourceHealthInfo healthInfo = getResourceHealthInfo(pResourceInfo);

                if (currentCount == 0)
                {
                    finalSiteValue += std::min<int>(10, 2 * unitCounts.first + unitCounts.second) + std::min<int>(10, happyInfo.actualHappy + healthInfo.actualHealth);
                }
//#ifdef ALTAI_DEBUG
//                os << "\n needed for: " << unitCounts.first << ", optional for: " << unitCounts.second;
//
//                os << "\n actual happy = " << happyInfo.actualHappy << " potential happy = " << happyInfo.potentialHappy << " unused happy = " << happyInfo.unusedHappy;
//                os << "\n actual health = " << healthInfo.actualHealth << " potential health = " << healthInfo.potentialHealth << " unused health = " << healthInfo.unusedHealth;
//#endif
            }
        }
        return finalSiteValue;
    }

    void SettlerManager::findBestCitySites_()
    {
        CvMap& theMap = gGlobals.getMap();
        const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType_);

        const int MAX_DISTANCE_CITY_MAINTENANCE_ = gGlobals.getDefineINT("MAX_DISTANCE_CITY_MAINTENANCE");
        std::map<XYCoords, int> bestSitesMap;
        std::map<int, int> areaTotals;

        bestSites_.clear();

        populateBestCitySitesMap_(bestSitesMap);

        // maintenance delta calc
        int currentMaintenance = 0;
        std::map<XYCoords, int> newMaintenanceMap;

        for (std::map<XYCoords, int>::const_iterator ci(bestSitesMap.begin()), ciEnd(bestSitesMap.end()); ci != ciEnd; ++ci)
        {
            MaintenanceHelper helper(ci->first, playerType_);
            newMaintenanceMap[ci->first] = helper.getMaintenance();
        }

        CityIter iter(CvPlayerAI::getPlayer(playerType_));
        CvCity* pLoopCity;
        while (pLoopCity = iter())
        {
            MaintenanceHelper helper(pLoopCity);
            currentMaintenance += helper.getMaintenance();
            //for (std::map<int, XYCoords, std::greater<int> >::const_iterator ci(bestSites_.begin()), ciEnd(bestSites_.end()); ci != ciEnd; ++ci)
            for (std::map<XYCoords, int>::const_iterator ci(bestSitesMap.begin()), ciEnd(bestSitesMap.end()); ci != ciEnd; ++ci)
            {
                newMaintenanceMap[ci->first] += helper.getMaintenanceWithCity(ci->first);
            }
        }

        // improvable plot count and new resources
        std::map<XYCoords, int> plotCountMap;
        std::map<XYCoords, std::map<BonusTypes, int> > resourcesMap;
        for (std::map<XYCoords, int>::const_iterator ci(bestSitesMap.begin()), ciEnd(bestSitesMap.end()); ci != ciEnd; ++ci)
        {
            populatePlotCountAndResources_(ci->first, plotCountMap, resourcesMap);
        }

#ifdef ALTAI_DEBUG
        // debug
        std::ostream& os = CivLog::getLog(player)->getStream();
#endif

        //const int fixedIncome = std::max<int>(0, player.getGoldPerTurn());
        //const int fixedExpenses = player.calculateInflatedCosts() + std::min<int>(0, player.getGoldPerTurn());

        int i = 0;
        for (std::map<XYCoords, int>::const_iterator ci(bestSitesMap.begin()), ciEnd(bestSitesMap.end()); ci != ciEnd; ++ci)
        {
            const int maintenanceDelta = newMaintenanceMap[ci->first] - currentMaintenance;
            int finalSiteValue = doSiteValueAdjustment_(ci->first, ci->second, maintenanceDelta, plotCountMap, resourcesMap);
            XYCoords finalSiteCoords(ci->first);

            CvPlot* pSitePlot = theMap.plot(ci->first.iX, ci->first.iY);
            // on a resource - can we move to better site?
            //if (pSitePlot->getBonusType(player.getTeam()) != NO_BONUS)
            //{
            //    os << "\nOn Bonus - need to check neighbours, site = " << ci->first << " with value: " << finalSiteValue;
                NeighbourPlotIter iter(pSitePlot);

                int bestBaseValue = pSitePlot->getFoundValue(player.getID());

                while (IterPlot pLoopPlot = iter())
                {
                    if (pLoopPlot.valid() && pLoopPlot->isRevealed(player.getTeam(), false) && pLoopPlot->getSubArea() == pSitePlot->getSubArea())
                    {
                        int thisBaseValue = pLoopPlot->getFoundValue(player.getID());
                        if (thisBaseValue < (2 * bestBaseValue) / 3)
                        {
                            continue;
                        }

                        XYCoords thisPlot(pLoopPlot->getX(), pLoopPlot->getY());

                        populatePlotCountAndResources_(thisPlot, plotCountMap, resourcesMap);

                        // use same maintenance delta
                        int thisPlotFinalValue = doSiteValueAdjustment_(thisPlot, thisBaseValue, maintenanceDelta, plotCountMap, resourcesMap);

                        if (thisPlotFinalValue > finalSiteValue)
                        {
                            finalSiteValue = thisPlotFinalValue;
                            finalSiteCoords = thisPlot;
#ifdef ALTAI_DEBUG
                            os << "\nSelected site: " << thisPlot << " with value: " << thisPlotFinalValue << " over original.";
#endif
                        }
                        else
                        {
#ifdef ALTAI_DEBUG
                            //os << "\nSkipping site: " << thisPlot << " with value: " << thisPlotFinalValue;
#endif
                        }
                    }
                }
            //}

            // debug
            /*DotMap::const_iterator dotMapIter = dotMap_.find(DotMapItem(finalSiteCoords, PlotYield()));
            if (dotMapIter != dotMap_.end())
            {
                dotMapIter->debugOutputs(os);
            }*/
#ifdef ALTAI_DEBUG
            os << "\nfinal site value = " << finalSiteValue;
#endif
            bestSites_.insert(std::make_pair(finalSiteValue, finalSiteCoords));
            areaTotals[pSitePlot->getArea()] += finalSiteValue;
        }

#ifdef ALTAI_DEBUG
        os << "\n";

        os << "\n(Turn = " << gGlobals.getGame().getGameTurn() << ") Found " << bestSites_.size() << " potential sites" << " (current maintenance = " << currentMaintenance << ")";
        for (std::map<int, XYCoords, std::greater<int> >::const_iterator ci(bestSites_.begin()), ciEnd(bestSites_.end()); ci != ciEnd; ++ci)
        {
            os << "\n" << ci->second << " = " << ci->first;
        }

        for (std::map<int, int>::const_iterator ci(areaTotals.begin()), ciEnd(areaTotals.end()); ci != ciEnd; ++ci)
        {
            os << "\nArea = " << ci->first << ", total = " << ci->second;
        }
#endif
    }

    // todo - check if we get city plot bonus counted here
    std::set<BonusTypes> SettlerManager::getBonusesForSites(int siteCount) const
    {
        std::set<BonusTypes> bonuses;
        int foundCount = 0;
        for (std::map<int, XYCoords, std::greater<int> >::const_iterator ci(bestSites_.begin()), ciEnd(bestSites_.end()); ci != ciEnd; ++ci)
        {
            ++foundCount;
            if (foundCount > siteCount)
            {
                break;
            }

            DotMap::const_iterator siteIter(dotMap_.find(DotMapItem(XYCoords(ci->second.iX, ci->second.iY), PlotYield())));
            for (DotMapItem::PlotDataConstIter plotIter(siteIter->plotData.begin()), endPlotIter(siteIter->plotData.end()); plotIter != endPlotIter; ++plotIter)
            {
                if (plotIter->bonusType != NO_BONUS)
                {
                    bonuses.insert(plotIter->bonusType);
                }
            }
        }
        return bonuses;
    }

    std::set<ImprovementTypes> SettlerManager::getImprovementTypesForSites(int siteCount) const
    {
        std::set<ImprovementTypes> improvements;
        int foundCount = 0;
        for (std::map<int, XYCoords, std::greater<int> >::const_iterator ci(bestSites_.begin()), ciEnd(bestSites_.end()); ci != ciEnd; ++ci)
        {
            ++foundCount;
            if (foundCount > siteCount)
            {
                break;
            }

            DotMap::const_iterator siteIter(dotMap_.find(DotMapItem(XYCoords(ci->second.iX, ci->second.iY), PlotYield())));
            for (DotMapItem::PlotDataConstIter plotIter(siteIter->plotData.begin()), endPlotIter(siteIter->plotData.end()); plotIter != endPlotIter; ++plotIter)
            {
                if (plotIter->getWorkedImprovement() != NO_IMPROVEMENT)
                {
                    improvements.insert(plotIter->getWorkedImprovement());
                }
            }
        }
        return improvements;
    }

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
                        const CvCity* pCity = pLoopPlot->getPlotCity();
                        //happyCap = pCity->getPopulation() + std::min<int>(0, -pCity->angryPopulation());
                        workedByNeighbour = workedByNeighbour || pCity->isWorkingPlot(pLoopPlot);
                    }
                }
            }
        }

        return std::make_pair(count, workedByNeighbour);
    }

    int SettlerManager::getStepDistanceToClosestCity_(const CvPlot* pPlot, bool sameArea) const
    {
        const int areaID = pPlot->getArea();

        CityIter cityIter(CvPlayerAI::getPlayer(playerType_));
        int minDistance = MAX_INT;

        const CvCity* pLoopCity;
        while (pLoopCity = cityIter())
        {
            if (!sameArea || pLoopCity->getArea() == areaID)
            {
                minDistance = std::min<int>(minDistance, stepDistance(pPlot->getX(), pPlot->getY(), pLoopCity->getX(), pLoopCity->getY()));
            }
        }
        return minDistance;
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
            std::pair<PlotYield, int> plotYieldAndSurplusFood = ci->getOutput(playerType_);
            os << "\noutput for coords: " << ci->coords << " = " << plotYieldAndSurplusFood.first << ", " << plotYieldAndSurplusFood.second;

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
                dotMap_.find(DotMapItem(ci->second, PlotYield()))->debugOutputs(os);
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

    void SettlerManager::write(FDataStreamBase* pStream) const
    {
    }

    void SettlerManager::read(FDataStreamBase* pStream)
    {
    }
}