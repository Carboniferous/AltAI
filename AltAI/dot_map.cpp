#include "AltAI.h"
#include "./dot_map.h"
#include "./plot_info.h"
#include "./plot_info_visitors.h"
#include "./helper_fns.h"
#include "./gamedata_analysis.h"
#include "./game.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./civ_log.h"

namespace AltAI
{
    namespace
    {
        DotMapItem::SortedPlots makePlotSet()
        {
            YieldPriority yieldPriority;
            yieldPriority.assign(-1);
            yieldPriority[0] = YIELD_FOOD;

            return DotMapItem::SortedPlots(MixedOutputOrderFunctor<PlotYield>(yieldPriority, makeYieldW(4, 2, 1)));
        }
    }

    DotMapItem::DotMapPlotData::DotMapPlotData(const PlotData& plotData, PlayerTypes playerType, int lookAheadDepth) :
        workedImprovement(-1), neighbourCityCount(0), workedByNeighbour(false), isPinned(false), isSelected(true), improvementMakesBonusValid(false)
    {
        TeamTypes teamType = PlayerIDToTeamID(playerType);

        coords = plotData.coords;
        possibleImprovements = getYields(plotData.pPlotInfo->getInfo(), playerType, false, lookAheadDepth);
        bonusType = plotData.bonusType;
        featureType = plotData.featureType;

        ImprovementTypes currentImprovement = plotData.improvementType;
        if (currentImprovement == NO_IMPROVEMENT)
        {
            // if we have an improvement yield will be got from its possibleImprovements entry
            currentYield = plotData.plotYield;
        }

        for (size_t i = 0, count = possibleImprovements.size(); i < count; ++i)
        {
            // todo - check featuretype too
            if (getBaseImprovement(possibleImprovements[i].second) == getBaseImprovement(currentImprovement))
            {
                workedImprovement = i;
                currentYield = possibleImprovements[i].first;
                // leave the most upgraded improvement here, but set the actual type in city improvement manager as it will be built
                //if (possibleImprovements[i].second != currentImprovement)
                //{
                //    // set the upgraded improvement type and yield
                //    possibleImprovements[i] = std::make_pair(plotData.plotYield, currentImprovement);
                //}

                if (currentImprovement != NO_IMPROVEMENT && bonusType != NO_BONUS && 
                    gGlobals.getImprovementInfo(currentImprovement).isImprovementBonusMakesValid(bonusType))
                {
                    improvementMakesBonusValid = true;
                }
                break;
            }
        }
    }

    DotMapItem::DotMapPlotData DotMapItem::getPlotData(XYCoords coords) const
    {
        DotMapPlotData key(coords);
        PlotDataSet::const_iterator ci = plotDataSet.find(key);
        return ci != plotDataSet.end() ? *ci : DotMapPlotData();
    }

    PlotYield DotMapItem::DotMapPlotData::getProjectedPlotYield(const Player& player, const int timeHorizon) const
    {
        if (workedImprovement == -1)
        {
            return currentYield;
        }
        else
        {
            PlotYield extraYield;
            if (possibleImprovements[workedImprovement].second != NO_IMPROVEMENT && 
                (ImprovementTypes)gGlobals.getImprovementInfo(possibleImprovements[workedImprovement].second).getImprovementUpgrade() != NO_IMPROVEMENT)
            {
                ImprovementTypes currentImpType = possibleImprovements[workedImprovement].second, nextImpType = NO_IMPROVEMENT;
                const int upgradeRate = player.getCvPlayer()->getImprovementUpgradeRate();
                int remainingTurns = timeHorizon;
                // upgrade time > 0 means we can upgrade
                int upgradeTime = gGlobals.getGame().getImprovementUpgradeTime(currentImpType);
                while (upgradeTime > 0)
                {
                    int turnsUntilUpgrade = getActualUpgradeTurns(upgradeTime, upgradeRate);
                    if (turnsUntilUpgrade < remainingTurns)
                    {                        
                        remainingTurns -= turnsUntilUpgrade;
                        nextImpType = (ImprovementTypes)gGlobals.getImprovementInfo(currentImpType).getImprovementUpgrade();
                        extraYield += (PlotYield(gGlobals.getImprovementInfo(nextImpType).getYieldChangeArray()) - PlotYield(gGlobals.getImprovementInfo(currentImpType).getYieldChangeArray()));

                        currentImpType = nextImpType;
                        upgradeTime = gGlobals.getGame().getImprovementUpgradeTime(currentImpType);
                    }
                    else
                    {
                        break;
                    }                    
                }
            }

            return possibleImprovements[workedImprovement].first + extraYield;
        } 
    }

    bool DotMapItem::DotMapPlotData::setWorkedImpIndex(ImprovementTypes currentImp)
    {
        ImprovementTypes baseImp = getBaseImprovement(currentImp);
        for (size_t j = 0, count = possibleImprovements.size(); j < count; ++j)
        {
            // e.g. if have hamlet and possible imp is marked as town - this will match
            if (getBaseImprovement(possibleImprovements[j].second) == baseImp)
            {
                workedImprovement = j;
                return true;
            }
        }
        return false;
    }

    void DotMapItem::DotMapPlotData::debug(std::ostream& os) const
    {
        os << "\nCoords = " << coords;
        for (size_t i = 0, count = possibleImprovements.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << (possibleImprovements[i].second == NO_IMPROVEMENT ? " none" : gGlobals.getImprovementInfo(possibleImprovements[i].second).getType())
               << " = " << possibleImprovements[i].first;
        }
        if (isPinned)
        {
            os << " imp is pinned";
        }
    }

    DotMapItem::DotMapItem(XYCoords coords_, PlotYield cityPlotYield_, bool isFreshWater_, BonusTypes bonusType)
        : coords(coords_), cityPlotYield(cityPlotYield_), isFreshWater(isFreshWater_), numDeadLockedBonuses(0), 
          areaID(-1), subAreaID(-1), selectedPlots(makePlotSet()), projectedTurns(-1)
    {
        if (bonusType != NO_BONUS)
        {
            bonusTypes.insert(bonusType);
        }
    }

    void DotMapItem::calcOutput(const Player& player, int lookAheadTurns, int baseHealthyPop, bool debug)
    {
        selectedPlots.clear();
        selectedPlots.insert(plotDataSet.begin(), plotDataSet.end());
        projectedYield = getUsablePlots(player, selectedPlots, baseHealthyPop, lookAheadTurns, debug);
        projectedTurns = lookAheadTurns;
    }

    int DotMapItem::getFoundValue(const Player& player) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        YieldValueFunctor valueF(makeYieldW(6, 4, 3));
        int value = valueF(projectedYield);

        PlotYield maxYield;
        ImprovementTypes bestImprovement;
        const CvPlot* pCityPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
        boost::tie(maxYield, bestImprovement) = getMaxYield(player.getAnalysis()->getMapAnalysis()->getPlotInfoNode(pCityPlot), player.getPlayerID(), 3);

        BonusTypes cityPlotBonusType = pCityPlot->getBonusType(player.getTeamID());
        bool onFoodSpecial = cityPlotBonusType != NO_BONUS && maxYield[YIELD_FOOD] > 3 && bestImprovement != NO_IMPROVEMENT;
        bool onGoodSpecial = cityPlotBonusType != NO_BONUS && maxYield[YIELD_FOOD] >= foodPerPop && (maxYield[YIELD_PRODUCTION] > 3 || maxYield[YIELD_COMMERCE] > 6);

        if (onFoodSpecial || onGoodSpecial)
        {
#ifdef ALTAI_DEBUG
                os << "\nTreating plot: " << coords << " as good/food special: maxYield = " << maxYield;
#endif
            value *= 4;
            value /= 5;
        }

        if (numDeadLockedBonuses > 0)
        {
#ifdef ALTAI_DEBUG
            os << "\nPlot: " << coords << " has deadlocked bonuses - scaling value by: " << 1 + numDeadLockedBonuses;
#endif
            value /= (1 + numDeadLockedBonuses);
        }

        value = std::max<int>(0, value);

#ifdef ALTAI_DEBUG
        os << "\nPlot: " << coords << " setting found value = " << value;
#endif

        return value;
    }

    std::pair<PlotYield, int> DotMapItem::getOutput() const
    {
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        PlotYield plotYield = cityPlotYield;
        int surplusFood = cityPlotYield[YIELD_FOOD];
        
        for (SortedPlots::const_iterator it(selectedPlots.begin()), itEnd(selectedPlots.end()); it != itEnd; ++it)
        {
            PlotYield thisYield = it->getPlotYield();
            surplusFood += thisYield[YIELD_FOOD] - foodPerPop;
            if (surplusFood < 0)
            {
                break;
            }

            plotYield += thisYield;
        }

        return std::make_pair<PlotYield, int>(plotYield, surplusFood);
    }

    PlotYield DotMapItem::getOutput(XYCoords coords, int improvementIndex) const
    {
        PlotDataConstIter ci = plotDataSet.find(DotMapPlotData(coords));
        if (ci != plotDataSet.end())
        {
            return ci->getPlotYield(improvementIndex);
        }
        else
        {
            return PlotYield();
        }
    }

    std::vector<BonusTypes> DotMapItem::getNewBonuses(const std::set<BonusTypes>& existingBonuses) const
    {
        std::vector<BonusTypes> newBonuses;
        // get a list of our bonusTypes which not in the set of existing ones
        std::set_difference(bonusTypes.begin(), bonusTypes.end(), existingBonuses.begin(), existingBonuses.end(), std::back_inserter(newBonuses));
        return newBonuses;
    }

    int DotMapItem::getBaseFeatureHealth() const
    {
        int baseFeatureHealth = 0;
        for (PlotDataSet::const_iterator ci(plotDataSet.begin()), ciEnd(plotDataSet.end()); ci != ciEnd; ++ci)
        {
            const CvPlot* pPlot = gGlobals.getMap().plot(ci->coords.iX, ci->coords.iY);
            FeatureTypes featureType = pPlot->getFeatureType();
            if (featureType != NO_FEATURE)
            {
                baseFeatureHealth += gGlobals.getFeatureInfo(featureType).getHealthPercent();
            }
        }
        return baseFeatureHealth / 100;
    }

    std::vector<int> DotMapItem::getGrowthRates(const CvPlayer& player, size_t maxPop, int baseHealthyPop, int cultureLevel)
    {
        std::vector<int> growthTurns(1, 1);
        static const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();

        BuildTimesData buildTimesData;
        getBuildTimesData(buildTimesData, player.getID(), cultureLevel, baseHealthyPop);

        size_t pop = 1;
        maxPop = std::min<size_t>(buildTimesData.foodOutputs.size(), maxPop);  // cap max at no. of workable plots (not trying to do any spec logic here)
        
        int foodRate = buildTimesData.foodOutputs[0] + (pop < (int)buildTimesData.foodOutputs.size() ? buildTimesData.foodOutputs[pop] : 0) - foodPerPop, currentFood = 0;
        size_t impIndex = 0;
        int turnsToNextImp = buildTimesData.impBuildTimes.empty() ? MAX_INT : buildTimesData.impBuildTimes[0];
        int turn = 0;

        while (pop <= maxPop && foodRate > 0)
        {
            int growthThreshold = player.getGrowthThreshold(pop);            
            int growthRate = (growthThreshold - currentFood) / foodRate;
            int growthDelta = (growthThreshold - currentFood) % foodRate;            
            int turnsToGrowth = growthRate + (growthDelta ? 1 : 0);

            if (turnsToNextImp < turnsToGrowth)
            {
                turn += turnsToNextImp;
                currentFood += turnsToNextImp * foodRate;

                if (buildTimesData.impIndices[impIndex] <= pop) // recalc food output
                {
                    foodRate += buildTimesData.impOutputs[impIndex] - buildTimesData.foodOutputs[buildTimesData.impIndices[impIndex]];
                }
                buildTimesData.foodOutputs[buildTimesData.impIndices[impIndex]] = buildTimesData.impOutputs[impIndex];
                ++impIndex;
                
                if (impIndex < buildTimesData.impIndices.size())
                {
                    turnsToNextImp = buildTimesData.impBuildTimes[impIndex];
                }
                else
                {
                    turnsToNextImp = MAX_INT;
                }
            }
            else
            {
                turn += turnsToGrowth;
                currentFood += turnsToGrowth * foodRate;
                currentFood -= growthThreshold;

                turnsToNextImp -= turnsToGrowth;
                ++pop;
                foodRate += buildTimesData.foodOutputs[pop];
                foodRate -= foodPerPop * pop;
                
                growthTurns.push_back(turn);
            }
        }

        return growthTurns;
    }

    void DotMapItem::debugOutputs(const Player& player, std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        int surplusFood = cityPlotYield[YIELD_FOOD];
        PlotYield totalYield(cityPlotYield), totalProjectedYield(cityPlotYield), totalConditionalYield(conditionalYield);

        size_t pop = 0, consumedWorkerCount = 0, improvablePlotCount = 0;
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        int currentFood = 0, foodRate = cityPlotYield[YIELD_FOOD], remainingTurns = projectedTurns;

        for (SortedPlots::const_iterator it(selectedPlots.begin()), itEnd(selectedPlots.end()); it != itEnd; ++it)
        {
            PlotYield thisYield = it->getPlotYield();
            surplusFood += thisYield[YIELD_FOOD] - foodPerPop;
            totalYield += thisYield;
            totalConditionalYield += it->conditionalYield;

            foodRate += it->getPlotYield()[YIELD_FOOD];
            foodRate -= foodPerPop;
            ++pop;

            PlotYield projectedYield = it->getProjectedPlotYield(player, remainingTurns);
            totalProjectedYield += projectedYield;

            os << "\n" << it->coords << " = " << thisYield;
            if (projectedYield != thisYield) os << " projected = " << projectedYield << " ";

            os << " (";

            ImprovementTypes workedImprovement = it->getWorkedImprovement();

            if (it->bonusType != NO_BONUS)
            {
                os << gGlobals.getBonusInfo(it->bonusType).getType();
                if (workedImprovement != NO_IMPROVEMENT)
                {
                    // checking this here assumes that only resource based improvements require consumed workers (i.e. workboats)
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(workedImprovement);
                    if (gGlobals.getBuildInfo(buildType).isKill())
                    {
                        ++consumedWorkerCount;
                    }
                }
            }
            
            if (workedImprovement != NO_IMPROVEMENT)
            {
                ++improvablePlotCount;
                os << " " << gGlobals.getImprovementInfo(workedImprovement).getType();
            }

            if (it->featureType != NO_FEATURE)
            {
                os << " " << gGlobals.getFeatureInfo(it->featureType).getType();
            }

            os << " s = " << surplusFood << ") ";

            /*if (it->bonusType != NO_BONUS)
            {
                os << " (possible improvements = ";
                for (size_t i = 0, count = it->possibleImprovements.size(); i < count; ++i)
                {
                    if (it->possibleImprovements[i].second != NO_IMPROVEMENT)
                    {
                        os << gGlobals.getImprovementInfo(it->possibleImprovements[i].second).getType() << ", ";
                    }
                    else
                    {
                        os << "NO_IMPROVEMENT, ";
                    }
                }
                os << ")";
            }*/
            //else if (it->bonusType != NO_BONUS) // debug
            //{
            //    const CvPlot* pPlot = gGlobals.getMap().plot(it->coords.iX, it->coords.iY);
            //    PlotInfo plotInfo(pPlot, pPlot->getOwner());
            //    os << "\n" << plotInfo.getInfo();
            //}

            if (foodRate <= 0)
            {
                break;
            }

            int growthThreshold = player.getCvPlayer()->getGrowthThreshold(pop);
            int growthRate = (growthThreshold - currentFood) / foodRate;
            int growthDelta = (growthThreshold - currentFood) % foodRate;            
            int turnsToGrowth = growthRate + (growthDelta ? 1 : 0);
            if (turnsToGrowth > remainingTurns)
            {
                break;
            }
            remainingTurns -= turnsToGrowth;
            currentFood += turnsToGrowth * foodRate;
            currentFood -= growthThreshold;
        }
        os << "\n\ttotal yield = " << totalYield;
        if (totalYield != totalProjectedYield) os << ", total projected yield = " << totalProjectedYield;
        os << " improvable plots = " << improvablePlotCount;
        if (!requiredAdditionalYieldBuildings.empty())
        {
            os << "\n\trequired buildings: ";
            for (size_t i = 0, count = requiredAdditionalYieldBuildings.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << gGlobals.getBuildingInfo(requiredAdditionalYieldBuildings[i].first).getType()
                    << " => " << totalConditionalYield << " max = " << requiredAdditionalYieldBuildings[i].second;
            }
        }
        if (consumedWorkerCount > 0)
        {
            os << " consumes " << consumedWorkerCount << " worker" << ((consumedWorkerCount > 1) ? "s" : "");
        }
        os << "\n";
#endif
    }

    void DotMapItem::getBuildTimesData(DotMapItem::BuildTimesData& impBuildData, PlayerTypes playerType, int cultureLevel, int baseHealthyPop)
    {
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(playerType);

        YieldPriority yieldPriority;
        yieldPriority.assign(-1);
        yieldPriority[0] = YIELD_FOOD;

        MixedOutputOrderFunctor<PlotYield> mixedF(yieldPriority, makeYieldW(4, 2, 1));

        SortedPlots sortedPlots(mixedF);
        for (PlotDataSet::const_iterator ci(plotDataSet.begin()), ciEnd(plotDataSet.end()); ci != ciEnd; ++ci)
        {
            const CvPlot* pPlot = gGlobals.getMap().plot(ci->coords.iX, ci->coords.iY);
            if (pPlot->getOwner() == playerType || plotDistance(ci->coords.iX, ci->coords.iY, coords.iX, coords.iY) <= cultureLevel)
            {
                sortedPlots.insert(*ci);
            }
        }
        
        getUsablePlots(player, sortedPlots, baseHealthyPop, 3 * player.getAnalysis()->getTimeHorizon());

        impBuildData.foodOutputs = std::vector<int>(1, cityPlotYield[YIELD_FOOD]);  // plot food outputs

        for (SortedPlots::const_iterator plotIter(sortedPlots.begin()), plotEndIter(sortedPlots.end()); plotIter != plotEndIter; ++plotIter)
        {
            impBuildData.foodOutputs.push_back(plotIter->currentYield[YIELD_FOOD]);
            if (plotIter->getPlotYield()[YIELD_FOOD] > plotIter->currentYield[YIELD_FOOD])
            {
                ImprovementTypes impType = plotIter->getWorkedImprovement();
                if (impType != NO_IMPROVEMENT)
                {
                    // get improvement build times - only care about ones which increase plot's food yield
                    // store time to build imp, new food output and which plot was updated
                    // todo - factor in unit workrate here
                    const CvPlot* pPlot = gGlobals.getMap().plot(plotIter->coords.iX, plotIter->coords.iY);
                    impBuildData.impBuildTimes.push_back(pPlot->getBuildTime(GameDataAnalysis::getBuildTypeForImprovementType(getBaseImprovement(impType))) / 100);
                    impBuildData.impOutputs.push_back(plotIter->getPlotYield()[YIELD_FOOD]);
                    impBuildData.impIndices.push_back(impBuildData.foodOutputs.size() - 1);
                }
            }
        }
    }

    PlotYield DotMapItem::getUsablePlots(const Player& player, SortedPlots& sortedPlots, int baseHealthyPop, int lookAheadTurns, bool debug)
    {
        //bool doBreak = coords == XYCoords(48, 14);
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        PlotYield totalYield = cityPlotYield;
        os << "\nusable plots calc for: " << coords << ", cpy = " << cityPlotYield;
#endif
        size_t pop = 0;
        int currentFood = 0;
        int lostFood = 0;
        int foodRate = cityPlotYield[YIELD_FOOD], remainingTurns = lookAheadTurns;
        
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();

        SortedPlots::iterator pi(sortedPlots.begin()), piEnd(sortedPlots.end());
        for (; pi != piEnd; ++pi)
        {
            if (pi->workedByNeighbour || !pi->isSelected)
            {
#ifdef ALTAI_DEBUG
                os << "\n\tskipping plot: " << pi->coords;
#endif
                sortedPlots.erase(pi++);
                continue;
            }
            foodRate += pi->getPlotYield()[YIELD_FOOD];
            foodRate -= foodPerPop;

            ++pop;
            lostFood = std::max<int>(0, pop - baseHealthyPop);

            PlotYield projectedYield = pi->getProjectedPlotYield(player, remainingTurns);
            totalYield += projectedYield;

#ifdef ALTAI_DEBUG            
            os << "\n\tadded plot: " << pi->coords << " = " << pi->getPlotYield();
            if (pi->getPlotYield() != projectedYield) os << ", projected yield = " << projectedYield;
            os << " pop = " << pop << " food rate = " << foodRate << " t_r = " << remainingTurns;
            if (lostFood > 0)
            {
                os << " l_f = " << lostFood;
            }
#endif            
            if (foodRate - lostFood <= 0)
            {
                break;
            }

            int growthThreshold = player.getCvPlayer()->getGrowthThreshold(pop);
            int growthRate = (growthThreshold - currentFood) / (foodRate - lostFood);
            int growthDelta = (growthThreshold - currentFood) % (foodRate - lostFood);            
            int turnsToGrowth = growthRate + (growthDelta ? 1 : 0);
#ifdef ALTAI_DEBUG
            os << " t_g = " << turnsToGrowth;            
#endif
            if (turnsToGrowth > remainingTurns)
            {
                break;
            }
            remainingTurns -= turnsToGrowth;
            currentFood += turnsToGrowth * (foodRate - lostFood);
            currentFood -= growthThreshold;
        }

        // remove any plots we didn't manage to use yet
        if (pi != piEnd)
        {
            sortedPlots.erase(++pi, piEnd);
        }

#ifdef ALTAI_DEBUG
        os << "\n\tfinal pop = " << pop << ", final yield = " << totalYield;
#endif
        return totalYield;
    }
}