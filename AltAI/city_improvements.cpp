#include "./city_improvements.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./gamedata_analysis.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./city_optimiser.h"
#include "./building_info_visitors.h"
#include "./plot_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./city_simulator.h"
#include "./irrigatable_area.h"
#include "./city_log.h"
#include "./civ_log.h"

#include "CvDLLEngineIFaceBase.h"
#include "CvDLLFAStarIFaceBase.h"
#include "CvGameCoreUtils.h"
#include "FAStarNode.h"

namespace AltAI
{
    namespace
    {
        static const int MAX_IRRIGATION_CHAIN_SEARCH_RADIUS = 5;

        bool plotIsWorkedAndImproved(const CvPlot* pPlot)
        {
            const CvCity* pCity = pPlot->getWorkingCity();
            if (!pCity)
            {
                pCity = pPlot->getWorkingCityOverride();
            }

            if (pCity)
            {
                return pCity->isWorkingPlot(pPlot) && pPlot->getImprovementType() != NO_IMPROVEMENT;
            }
            else
            {
                return false;
            }
        }

        struct PlotDataFinder
        {
            explicit PlotDataFinder(const CityImprovementManager::PlotImprovementData& plotData) : coords(boost::get<0>(plotData)) {}
            explicit PlotDataFinder(XYCoords coords_) : coords(coords_) {}

            bool operator() (const CityImprovementManager::PlotImprovementData& other) const
            {
                return boost::get<0>(other) == coords;
            }

            const XYCoords coords;
        };

        template <typename P>
            struct PlotImprovementDataAdaptor
        {
            typedef typename P Pred;
            PlotImprovementDataAdaptor(P pred_) : pred(pred_) {}

            bool operator () (const CityImprovementManager::PlotImprovementData& p1, const CityImprovementManager::PlotImprovementData& p2) const
            {
                return pred(boost::get<3>(p1), boost::get<3>(p2));
            }
            P pred;
        };
    }

    CityImprovementManager::CityImprovementManager(IDInfo city, bool includeUnclaimedPlots) : city_(city), includeUnclaimedPlots_(includeUnclaimedPlots)
    {
    }

    void CityImprovementManager::calcImprovements(const std::vector<YieldTypes>& yieldTypes, int targetSize, int lookAheadDepth)
    {
        const CvCity* pCity = ::getCity(city_);

        PlayerTypes playerType = pCity->getOwner();

        std::vector<ConditionalPlotYieldEnchancingBuilding> conditionalEnhancements = GameDataAnalysis::getInstance()->getConditionalPlotYieldEnhancingBuildings(playerType, pCity);
        
        DotMapItem dotMapItem(XYCoords(pCity->getX(), pCity->getY()), pCity->plot()->getYield());
        boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(::getCity(city_)->getOwner())->getAnalysis()->getMapAnalysis();
        std::set<XYCoords> pinnedIrrigationPlots;

        CityPlotIter iter(pCity);

        while (IterPlot pLoopPlot = iter())
        {
            if (!pLoopPlot.valid())
            {
                continue;
            }

            PlayerTypes plotOwner = pLoopPlot->getOwner();
            XYCoords plotCoords(pLoopPlot->getX(), pLoopPlot->getY());

            if ((plotOwner == city_.eOwner || plotOwner == NO_PLAYER && includeUnclaimedPlots_) && plotCoords != dotMapItem.coords)
            {
                SharedPlotItemPtr sharedPlotPtr = pMapAnalysis->getSharedPlot(city_, plotCoords);

                if (sharedPlotPtr)
                {
                    if (sharedPlotPtr->assignedImprovement.first.eOwner == NO_PLAYER)
                    {
                        // assign this shared plot to us to manage
                        sharedPlotPtr->assignedImprovement.first = city_;
                    }
                    else if (!(sharedPlotPtr->assignedImprovement.first == city_))
                    {
                        continue;
                    }
                }

                DotMapItem::DotMapPlotData plotData(pLoopPlot, playerType, lookAheadDepth);

                if (!plotData.possibleImprovements.empty())
                {
                    // todo - allow global buildings which change yield to be counted here (e.g. colossus)
                    PlotYield extraYield = getExtraConditionalYield(dotMapItem.coords, XYCoords(pLoopPlot->getX(), pLoopPlot->getY()), conditionalEnhancements);
                    if (!isEmpty(extraYield))
                    {
                        for (size_t i = 0, count = plotData.possibleImprovements.size(); i < count; ++i)
                        {
                            plotData.possibleImprovements[i].first += extraYield;
                        }
                    }

                    for (size_t i = 0, count = improvements_.size(); i < count; ++i)
                    {
                        XYCoords coords = boost::get<0>(improvements_[i]);
                        if (coords == plotCoords)
                        {
                            if (boost::get<6>(improvements_[i]) & IrrigationChainPlot) // mark plot as pinned if it needs to keep irrigation
                            {
                                pinnedIrrigationPlots.insert(coords);
                                for (size_t j = 0, count = plotData.possibleImprovements.size(); j < count; ++j)
                                {
                                    if (plotData.possibleImprovements[j].second == boost::get<2>(improvements_[i]))
                                    {
                                        plotData.workedImprovement = j;
                                        break;
                                    }
                                }
                                plotData.isPinned = true;
#ifdef ALTAI_DEBUG
                                {   // debug
                                    std::ostream& os = CityLog::getLog(pCity)->getStream();
                                    os << "\nMarked plot: " << coords << " as pinned with improvement: "
                                        << (plotData.getWorkedImprovement() == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo((ImprovementTypes)plotData.getWorkedImprovement()).getType());
                                }
#endif
                            }
                            else if (pLoopPlot->getBonusType(::getCity(city_)->getTeam()) == NO_BONUS &&
                                // mark towns and villages as probably want to keep them
                                (improvementIsFinalUpgrade(pLoopPlot->getImprovementType()) || nextImprovementIsFinalUpgrade(pLoopPlot->getImprovementType())))
                            {
                                for (size_t j = 0, count = plotData.possibleImprovements.size(); j < count; ++j)
                                {
                                    if (getBaseImprovement(plotData.possibleImprovements[j].second) == getBaseImprovement(pLoopPlot->getImprovementType()))
                                    {
                                        plotData.workedImprovement = j;
                                        break;
                                    }
                                }
                                plotData.isPinned = true;
#ifdef ALTAI_DEBUG
                                {   // debug
                                    std::ostream& os = CityLog::getLog(pCity)->getStream();
                                    os << "\nMarked plot: " << coords << " as pinned with upgraded improvement: "
                                        << (plotData.getWorkedImprovement() == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo(pLoopPlot->getImprovementType()).getType());
                                }
#endif
                            }
                            break;
                        }
                    }
                    dotMapItem.plotData.insert(plotData);
                }
            }
        }

        //markFeaturesToKeep_(dotMapItem);

        DotMapOptimiser optMixed(dotMapItem, YieldWeights(), YieldWeights());

        //optMixed.optimise(yieldTypes, std::min<int>(dotMapItem.plotData.size(), targetSize));
        optMixed.optimise(yieldTypes, targetSize);
        //optMixed.optimise(yieldTypes, std::min<int>(dotMapItem.plotData.size(), 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel())));

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = CityLog::getLog(pCity)->getStream();
            os << "\nPop = " << pCity->getPopulation() << ", happy = " << pCity->happyLevel() << ", unhappy = " << pCity->unhappyLevel() << " target = " << targetSize;
            for (size_t i = 0, count = yieldTypes.size(); i < count; ++i)
            {
                os << " yieldTypes[" << i << "] = " << yieldTypes[i];
            }
        }
        dotMapItem.debugOutputs(CityLog::getLog(pCity)->getStream());
#endif

        improvements_.clear();
        for (DotMapItem::PlotDataConstIter ci(dotMapItem.plotData.begin()), ciEnd(dotMapItem.plotData.end()); ci != ciEnd; ++ci)
        {
            ImprovementTypes improvementType = ci->getWorkedImprovement();

            if (improvementType != NO_IMPROVEMENT)
            {
                FeatureTypes featureRemovedByImprovement = GameDataAnalysis::doesBuildTypeRemoveFeature(
                    GameDataAnalysis::getBuildTypeForImprovementType(improvementType), ci->featureType) ? ci->featureType : NO_FEATURE;

                improvements_.push_back(boost::make_tuple(ci->coords, featureRemovedByImprovement, getBaseImprovement(improvementType), ci->getPlotYield(), TotalOutput(), Not_Built, None));

                const CvPlot* pPlot = gGlobals.getMap().plot(ci->coords.iX, ci->coords.iY);
                ImprovementTypes currentImprovement = pPlot->getImprovementType();

                ImprovementState improvementState = getBaseImprovement(currentImprovement) == getBaseImprovement(improvementType) ? Built : Not_Built;

                if (ci->improvementMakesBonusValid)
                {
                    boost::get<6>(*improvements_.rbegin()) |= ImprovementMakesBonusValid;
                }

                if (!ci->isSelected && !ci->improvementMakesBonusValid)  // always mark bonuses as selected if we can build the appropriate improvement
                {
                    boost::get<5>(*improvements_.rbegin()) = Not_Selected;
                }
                else
                {
                    boost::get<5>(*improvements_.rbegin()) = improvementState;
                }

                if (pinnedIrrigationPlots.find(ci->coords) != pinnedIrrigationPlots.end())
                {
                    boost::get<6>(*improvements_.rbegin()) |= IrrigationChainPlot;
                }
            }
        }

        // don't care about this as this flag indicates a simulation
        if (!includeUnclaimedPlots_)
        {
            markPlotsWhichNeedIrrigation_();
            markPlotsWhichNeedRoute_();
            markPlotsWhichNeedTransport_();
            markPlotsWhichNeedFeatureRemoved_();
        }
    }

    void CityImprovementManager::markPlotsWhichNeedIrrigation_()
    {
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (boost::get<5>(improvements_[i]) == Built)
            {
                XYCoords coords = boost::get<0>(improvements_[i]);
                ImprovementTypes improvementType = boost::get<2>(improvements_[i]);
                const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(improvementType);
                const PlotYield irrigatedYieldChange = const_cast<CvImprovementInfo&>(improvementInfo).getIrrigatedYieldChangeArray();
                if (!isEmpty(irrigatedYieldChange))
                {
                    const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                    if (!pPlot->isIrrigated())
                    {
                        const int irrigatableAreaID = pPlot->getIrrigatableArea();
                        if (irrigatableAreaID != FFreeList::INVALID_INDEX)
                        {
                            boost::shared_ptr<IrrigatableArea> pIrrigatableArea = gGlobals.getMap().getIrrigatableArea(irrigatableAreaID);
                            if (pIrrigatableArea->hasFreshWaterAccess())
                            {
                                boost::get<6>(improvements_[i]) |= NeedsIrrigation;
                            }
                        }
                    }
                }
            }
        }
    }

    void CityImprovementManager::markPlotsWhichNeedRoute_()
    {
        const CvCity* pCity = ::getCity(city_);
        const int citySubArea = pCity->plot()->getSubArea();

        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (boost::get<5>(improvements_[i]) == Built && (boost::get<6>(improvements_[i]) & ImprovementMakesBonusValid))
            {
                XYCoords coords = boost::get<0>(improvements_[i]);
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

                // todo - deal with plots on other subareas which need routes (needs consideration of forts and check if there are other cities in that sub area)
                if (citySubArea == pPlot->getSubArea() && !pPlot->isConnectedTo(pCity))
                {
                    boost::get<6>(improvements_[i]) |= NeedsRoute;
                }
            }
        }
    }

    void CityImprovementManager::markPlotsWhichNeedFeatureRemoved_()
    {
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (boost::get<5>(improvements_[i]) == Built)
            {
                XYCoords coords = boost::get<0>(improvements_[i]);
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                FeatureTypes featureType = pPlot->getFeatureType();
                if (featureType != NO_FEATURE)
                {
                    if (GameDataAnalysis::isBadFeature(featureType))
                    {
                        BuildTypes buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureType);
                        if (buildType != NO_BUILD)
                        {
                            boost::get<6>(improvements_[i]) |= RemoveFeature;
                        }
                    }
                }
            }
        }
    }

    void CityImprovementManager::markPlotsWhichNeedTransport_()
    {
        const CvCity* pCity = ::getCity(city_);
        const int citySubArea = pCity->plot()->getSubArea();

        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (boost::get<5>(improvements_[i]) == Not_Built)
            {
                XYCoords coords = boost::get<0>(improvements_[i]);
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

                // todo - deal with plots on other subareas which need routes (needs consideration of forts and check if there are other cities in that sub area)
                if (citySubArea != pPlot->getSubArea())
                {
                    boost::get<6>(improvements_[i]) |= WorkerNeedsTransport;
                }
            }
        }
    }

    boost::tuple<XYCoords, FeatureTypes, ImprovementTypes> CityImprovementManager::getBestImprovementNotBuilt(TotalOutputWeights outputWeights, bool whichMakesBonusValid, 
        bool simulatedOnly, const std::vector<boost::shared_ptr<PlotCond> >& conditions) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CityLog::getLog(::getCity(city_))->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn();
        logImprovements();
#endif

        int bestImprovementValue = 0, bestSimulatedImprovementValue = 0;
        int bestImprovementIndex = NO_IMPROVEMENT, bestSimulatedImprovementIndex = NO_IMPROVEMENT;
        YieldValueFunctor valueF(makeYieldW(4, 2, 1));
        TotalOutputValueFunctor simValueF(outputWeights);

        boost::shared_ptr<Player> pPlayer = gGlobals.getGame().getAltAI()->getPlayer(city_.eOwner);

        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (boost::get<2>(improvements_[i]) != NO_IMPROVEMENT)
            {
                const XYCoords coords = boost::get<0>(improvements_[i]);
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                BonusTypes bonusType = pPlot->getBonusType(::getCity(city_)->getTeam());
                ImprovementTypes currentImprovementType = pPlot->getImprovementType(), plannedImprovementType = boost::get<2>(improvements_[i]);
                bool hasFort = currentImprovementType != NO_IMPROVEMENT && gGlobals.getImprovementInfo(currentImprovementType).isActsAsCity();
                bool plotNeedsBonusImprovement = bonusType != NO_BONUS && (getBaseImprovement(currentImprovementType) != getBaseImprovement(plannedImprovementType) || hasFort);

                if (plotNeedsBonusImprovement && !(boost::get<6>(improvements_[i]) & ImprovementMakesBonusValid))
                {
                    if (whichMakesBonusValid)
                    {
                        continue;
                    }

                    TechTypes currentResearch = pPlayer->getCvPlayer()->getCurrentResearch();
                    if (currentResearch != NO_TECH)
                    {
                        std::vector<BonusTypes> bonusTypesFromTech = getWorkableBonuses(pPlayer->getAnalysis()->getTechInfo(currentResearch));
                        if (std::find(bonusTypesFromTech.begin(), bonusTypesFromTech.end(), bonusType) != bonusTypesFromTech.end())
                        {
#ifdef ALTAI_DEBUG
                            os << "\nSkipping improvement: " << gGlobals.getImprovementInfo(plannedImprovementType).getType() << " at: " << coords
                               << " as researching tech: " << gGlobals.getTechInfo(currentResearch).getType() << " which can make bonus: "
                               << gGlobals.getBonusInfo(bonusType).getType() << " valid.";
#endif
                            // researching the tech which would make this bonus valid - not worth building a different improvement (probably!)
                            continue;
                        }
                    }
                }

                if (!plotNeedsBonusImprovement)
                {
                    // todo - check this logic
                    if (plotIsWorkedAndImproved(pPlot) || improvementIsUpgradeOf(currentImprovementType, getBaseImprovement(plannedImprovementType)))
                    {
                        continue;
                    }
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << "\nFound plot at: " << coords << " which needs bonus improvement: " << gGlobals.getImprovementInfo(getBaseImprovement(plannedImprovementType)).getType();
                    if (hasFort)
                    {
                        os << " (currently has fort)";
                    }
#endif
                }

#ifdef ALTAI_DEBUG
                os << "\nChecking plot: " << coords << " for planned improvement: " << gGlobals.getImprovementInfo(plannedImprovementType).getType();
                logImprovement(os, improvements_[i]);
#endif

                bool valid = boost::get<5>(improvements_[i]) == Not_Built || boost::get<5>(improvements_[i]) == Not_Selected;
                if (valid)
                {
                    for (size_t j = 0, count = conditions.size(); j < count; ++j)
                    {
                        valid = valid && (*conditions[j])(pPlot);
                    }
                }

                if (valid)
                {
                    if (boost::get<4>(improvements_[i]) != TotalOutput() && simValueF(boost::get<4>(improvements_[i])) > bestSimulatedImprovementValue)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nSelected simulated improvement: " << i << " at: " << coords << " value = " << simValueF(boost::get<4>(improvements_[i]));
#endif
                        bestSimulatedImprovementIndex = i;
                        bestSimulatedImprovementValue = simValueF(boost::get<4>(improvements_[i]));
                    }

                    if (!simulatedOnly && valueF(boost::get<3>(improvements_[i])) > bestImprovementValue)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nSelected improvement: " << i << " at: " << coords << " value = " << valueF(boost::get<3>(improvements_[i]));
#endif
                        bestImprovementIndex = i;
                        bestImprovementValue = valueF(boost::get<3>(improvements_[i]));
                    }
                }
                else
                {
#ifdef ALTAI_DEBUG
                    os << " - validity check failed ";
#endif
                }
            }
        }
        
        if (bestSimulatedImprovementIndex != NO_IMPROVEMENT)
        {
            return boost::make_tuple(boost::get<0>(improvements_[bestSimulatedImprovementIndex]), boost::get<1>(improvements_[bestSimulatedImprovementIndex]), getBaseImprovement(boost::get<2>(improvements_[bestSimulatedImprovementIndex])));
        }
        else if (bestImprovementIndex != NO_IMPROVEMENT)
        {
            return boost::make_tuple(boost::get<0>(improvements_[bestImprovementIndex]), boost::get<1>(improvements_[bestImprovementIndex]), getBaseImprovement(boost::get<2>(improvements_[bestImprovementIndex])));
        }
        else
        {
            return boost::make_tuple(XYCoords(), NO_FEATURE, NO_IMPROVEMENT);
        }
    }

    ImprovementTypes CityImprovementManager::getBestImprovementNotBuilt(XYCoords coords) const
    {
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (boost::get<0>(improvements_[i]) == coords)
            {
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                BonusTypes bonusType = pPlot->getBonusType(::getCity(city_)->getTeam());
                ImprovementTypes currentImprovementType = pPlot->getImprovementType();

                bool hasFort = currentImprovementType != NO_IMPROVEMENT && gGlobals.getImprovementInfo(currentImprovementType).isActsAsCity();

                bool plotNeedsBonusImprovement = bonusType != NO_BONUS && (currentImprovementType != getBaseImprovement(boost::get<2>(improvements_[i])) || hasFort);

                if (!plotNeedsBonusImprovement)
                {
                    if (plotIsWorkedAndImproved(pPlot) || improvementIsUpgradeOf(currentImprovementType, getBaseImprovement(boost::get<2>(improvements_[i]))))
                    {
                        break;
                    }
                }
                else
                {
#ifdef ALTAI_DEBUG
                    std::ostream& os = CityLog::getLog(::getCity(city_))->getStream();
                    os << "\nFound plot at: " << coords << " which needs bonus improvement: " << gGlobals.getImprovementInfo(getBaseImprovement(boost::get<2>(improvements_[i]))).getType();
                    if (hasFort)
                    {
                        os << " (currently has fort)";
                    }
#endif
                }
                return getBaseImprovement(boost::get<2>(improvements_[i]));
            }
        }
        return NO_IMPROVEMENT;
    }

    int CityImprovementManager::getRank(XYCoords coords, TotalOutputWeights outputWeights) const
    {
        TotalOutputValueFunctor simValueF(outputWeights);
        YieldValueFunctor valueF(makeYieldW(4, 2, 1));
        std::multimap<int, XYCoords, std::greater<int> > simulatedImprovementsMap, improvementsMap;

        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (boost::get<4>(improvements_[i]) != TotalOutput())
            {
                simulatedImprovementsMap.insert(std::make_pair(simValueF(boost::get<4>(improvements_[i])), boost::get<0>(improvements_[i])));
            }
            else
            {
                improvementsMap.insert(std::make_pair(valueF(boost::get<3>(improvements_[i])), boost::get<0>(improvements_[i])));
            }
        }

        int rank = 0;
        for (std::multimap<int, XYCoords, std::greater<int> >::const_iterator ci(simulatedImprovementsMap.begin()), ciEnd(simulatedImprovementsMap.end()); ci != ciEnd; ++ci)
        {
            if (ci->second == coords)
            {
                return rank;
            }
            else
            {
                ++rank;
            }
        }

        for (std::multimap<int, XYCoords, std::greater<int> >::const_iterator ci(improvementsMap.begin()), ciEnd(improvementsMap.end()); ci != ciEnd; ++ci)
        {
            if (ci->second == coords)
            {
                return rank;
            }
            else
            {
                ++rank;
            }
        }

        return improvements_.size();
    }

    int CityImprovementManager::getNumImprovementsNotBuilt() const
    {
        int improvementCount = 0;
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (boost::get<5>(improvements_[i]) != Built && boost::get<5>(improvements_[i]) != Not_Selected)
            {
                ++improvementCount;
            }
        }
        return improvementCount;
    }

    PlotYield CityImprovementManager::getProjectedYield(int citySize, YieldPriority yieldP, YieldWeights yieldW) const
    {
        MixedOutputOrderFunctor<PlotYield> mixedF(yieldP, yieldW);
        std::vector<PlotImprovementData> improvements(improvements_);

        std::sort(improvements.begin(), improvements.end(), PlotImprovementDataAdaptor<MixedOutputOrderFunctor<PlotYield> >(mixedF));
        
        const CvCity* pCity = ::getCity(city_);
        PlotYield cityPlotYield = pCity->plot()->getYield();

        PlotYield plotYield = cityPlotYield;
        int surplusFood = cityPlotYield[YIELD_FOOD];
        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();

#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(city_.eOwner));
        {
            std::ostream& os = pCivLog->getStream();
            os << "\nyield = " << plotYield;
        }
#endif

        std::vector<PlotImprovementData>::const_iterator ci(improvements.begin()), ciEnd(improvements.end());
        int count = 0;
        while (ci != ciEnd && count < citySize)
        {
            ++count;
            PlotYield thisYield = boost::get<3>(*ci++);
            surplusFood += thisYield[YIELD_FOOD] - foodPerPop;
            if (surplusFood < 0)
            {
                break;
            }
            else
            {
#ifdef ALTAI_DEBUG
                {
                    std::ostream& os = pCivLog->getStream();
                    os << "\nyield = " << thisYield;
                }
#endif
                plotYield += thisYield;
            }
        }
        return plotYield;
    }

    TotalOutput CityImprovementManager::simulateImprovements(TotalOutputWeights outputWeights, const std::string& logLabel)
    {
        const CvCity* pCity = ::getCity(city_);
        CitySimulator simulator(pCity);

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = CityLog::getLog(pCity)->getStream();
            os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " label = " << logLabel;
        }
#endif
        const boost::shared_ptr<Player>& player = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        boost::shared_ptr<MapAnalysis> pMapAnalysis = player->getAnalysis()->getMapAnalysis();

        std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);
        const int targetSize = 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel());
        calcImprovements(yieldTypes, targetSize);
        if (!includeUnclaimedPlots_)
        {
            pMapAnalysis->assignSharedPlotImprovements(city_);
        }

        PlotsAndImprovements plotsAndImprovements;

        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            XYCoords coords = boost::get<0>(improvements_[i]);
            const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
            if (!improvementIsUpgradeOf(pPlot->getImprovementType(), boost::get<2>(improvements_[i])))
            {
#ifdef ALTAI_DEBUG
                {
                    std::ostream& os = CityLog::getLog(pCity)->getStream();
                    os << "\nAdding improvement to simulation list: ";
                    logImprovement(os, improvements_[i]);
                }
#endif
                plotsAndImprovements.push_back(std::make_pair(coords, 
                    std::vector<std::pair<FeatureTypes, ImprovementTypes> >(1, std::make_pair(boost::get<1>(improvements_[i]), boost::get<2>(improvements_[i])))));
            }
        }

#ifdef ALTAI_DEBUG
        {   // debug
            std::ostream& os = CityLog::getLog(pCity)->getStream();
            os << "\nImprovements to simulate:";

            for (size_t i = 0, count = plotsAndImprovements.size(); i < count; ++i)
            {
                os << "\n" << plotsAndImprovements[i].first;
                for (size_t j = 0, count = plotsAndImprovements[i].second.size(); j < count; ++j)
                {
                    os << gGlobals.getImprovementInfo(plotsAndImprovements[i].second[j].second).getType();
                }
            }
        }
#endif

        CityDataPtr pCityData(new CityData(pCity, includeUnclaimedPlots_));
        PlotImprovementSimulationResults simulatorResults(simulator.evaluateImprovements(plotsAndImprovements, pCityData, 20, false));

        std::vector<boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, TotalOutput> > bestSimulatedImprovements = simulator.getBestImprovements(outputWeights, simulatorResults);

#ifdef ALTAI_DEBUG
		{   // debug
            std::ostream& os = CityLog::getLog(pCity)->getStream();
            os << "\nBest improvements:";

            for (size_t i = 0, count = bestSimulatedImprovements.size(); i < count; ++i)
            {
                ImprovementTypes improvementType = boost::get<2>(bestSimulatedImprovements[i]);
                os << "\n" << boost::get<0>(bestSimulatedImprovements[i]) << 
                    (improvementType != NO_IMPROVEMENT ? gGlobals.getImprovementInfo(boost::get<2>(bestSimulatedImprovements[i])).getType() : " (none) "); 
            }
        }
#endif

		TotalOutput baseOutput;
        for (size_t i = 0, count = bestSimulatedImprovements.size(); i < count; ++i)
        {
            XYCoords coords = boost::get<0>(bestSimulatedImprovements[i]);
			if (coords == XYCoords(pCity->getX(), pCity->getY()))
			{
				baseOutput = boost::get<3>(bestSimulatedImprovements[i]);
			}
			else
			{
				for (size_t j = 0, count = improvements_.size(); j < count; ++j)
				{
					if (boost::get<0>(improvements_[j]) == coords)
					{
						// update TotalOutput
						boost::get<4>(improvements_[j]) = boost::get<3>(bestSimulatedImprovements[i]);
						break;
					}
				}
			}
        }

		return baseOutput;
    }

    bool CityImprovementManager::updateImprovements(const CvPlot* pPlot, ImprovementTypes improvementType)
    {
        const CvCity* pCity = ::getCity(city_);
        XYCoords coords(pPlot->getX(), pPlot->getY());
        bool foundPlot = false, foundCorrectImprovement = false;

        bool markPlotAsPartOfIrrigationChain = false;
        const bool isFreshWater = pPlot->isFreshWater();
        const bool isIrrigated = pPlot->isIrrigated();
        if (isIrrigated && !isFreshWater)
        {
            // if we return a different plot as part of a chain, that will be marked
            // we also want to mark the final plot (this one) if it's part of the chain - do that from here
            markPlotAsPartOfIrrigationChain = true;  
        }

        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (coords == boost::get<0>(improvements_[i]))
            {
                foundPlot = true;
                foundCorrectImprovement = getBaseImprovement(improvementType) == getBaseImprovement(boost::get<2>(improvements_[i]));
                if (foundCorrectImprovement)
                {
                    boost::get<5>(improvements_[i]) = Built;
                    if (markPlotAsPartOfIrrigationChain)
                    {
                        boost::get<6>(improvements_[i]) |= IrrigationChainPlot;
                    }
#ifdef ALTAI_DEBUG
                    {
                        boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity);
                        std::ostream& os = pCityLog->getStream();
                        os << "\nFound built improvement: " << (improvementType == NO_IMPROVEMENT ? "(none) " : gGlobals.getImprovementInfo(improvementType).getType())
                           << " at: " << pPlot->getX() << ", " << pPlot->getY();
                    }
#endif
                }
                else
                {
#ifdef ALTAI_DEBUG
                    {
                        boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity);
                        std::ostream& os = pCityLog->getStream();
                        os << "\nFailed to match built improvement: " << (improvementType == NO_IMPROVEMENT ? "(none) " : gGlobals.getImprovementInfo(improvementType).getType())
                           << " at: " << pPlot->getX() << ", " << pPlot->getY();
                    }
#endif
                    boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getMapAnalysis();

                    PlotYield thisYield = getYield(pMapAnalysis->getPlotInfoNode(pPlot), pPlot->getOwner(), improvementType, pPlot->getFeatureType(), pPlot->getRouteType());
                    improvements_[i] = boost::make_tuple(coords, pPlot->getFeatureType(), improvementType, thisYield, TotalOutput(), Built, markPlotAsPartOfIrrigationChain ? IrrigationChainPlot : 0);
                    
                }

#ifdef ALTAI_DEBUG
                if (markPlotAsPartOfIrrigationChain)
                {   // debug
                    std::ostream& os = CityLog::getLog(pCity)->getStream();
                    os << "\nMarked plot: " << coords << " as part of irrigation chain with improvement: "
                        << (improvementType == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo(improvementType).getType());
                }
#endif
                break;
            }
        }

        if (!foundPlot)
        {
            boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getMapAnalysis();

            PlotYield thisYield = getYield(pMapAnalysis->getPlotInfoNode(pPlot), pPlot->getOwner(), improvementType, pPlot->getFeatureType(), pPlot->getRouteType());
            improvements_.push_back(
                boost::make_tuple(coords, pPlot->getFeatureType(), improvementType, thisYield, TotalOutput(), CityImprovementManager::Built, markPlotAsPartOfIrrigationChain ? IrrigationChainPlot : 0));

#ifdef ALTAI_DEBUG
            if (markPlotAsPartOfIrrigationChain)
            {   // debug
                std::ostream& os = CityLog::getLog(pCity)->getStream();
                os << "\nAdded plot: " << coords << " as part of irrigation chain with improvement: "
                    << (improvementType == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo(improvementType).getType());
            }
#endif
        }

        if (markPlotAsPartOfIrrigationChain)
        {
            boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(::getCity(city_)->getOwner())->getAnalysis()->getMapAnalysis();
            pMapAnalysis->markIrrigationChainSourcePlot(coords, improvementType);
        }
        return foundCorrectImprovement;
    }

    const std::vector<CityImprovementManager::PlotImprovementData>& CityImprovementManager::getImprovements() const
    {
        return improvements_;
    }

    std::vector<CityImprovementManager::PlotImprovementData>& CityImprovementManager::getImprovements()
    {
        return improvements_;
    }

    void CityImprovementManager::markFeaturesToKeep_(DotMapItem& dotMapItem) const
    {
        std::multimap<int, DotMapItem::PlotDataIter> forestMap;
        YieldValueFunctor valueF(makeYieldW(4, 2, 1));

        for (DotMapItem::PlotDataIter plotIter(dotMapItem.plotData.begin()), endIter(dotMapItem.plotData.end()); plotIter != endIter; ++plotIter)
        {
            // TODO - detect bonuses on good features which don't require removal of the feature (e.g. furs and deer)
            if (plotIter->featureType != NO_FEATURE && plotIter->bonusType == NO_BONUS && gGlobals.getFeatureInfo(plotIter->featureType).getHealthPercent() > 0)
            {
                forestMap.insert(std::make_pair(valueF(plotIter->getPlotYield()), plotIter));
            }
        }
#ifdef ALTAI_DEBUG
        {
            std::ostream& os = CityLog::getLog(::getCity(city_))->getStream();
            os << "\n";
            for (std::multimap<int, DotMapItem::PlotDataIter>::const_iterator ci(forestMap.begin()), ciEnd(forestMap.end()); ci != ciEnd; ++ci)
            {
                os << ci->first << ", " << ci->second->coords << "; ";
            }
        }
#endif
        std::multimap<int, DotMapItem::PlotDataIter>::const_iterator ci(forestMap.begin());
        for (size_t i = 0, count = std::min<size_t>(forestMap.size(), 2); i < count; ++i)
        {
#ifdef ALTAI_DEBUG
            {  // debug
                CityLog::getLog(::getCity(city_))->getStream() << "\nMarking plot: " << ci->second->coords << " to keep feature";
            }
#endif
            std::vector<std::pair<PlotYield, ImprovementTypes> > newPossibleImprovements;
            for (size_t j = 0, count = ci->second->possibleImprovements.size(); j < count; ++j)
            {
                bool keepsFeature = ci->second->possibleImprovements[j].second == NO_IMPROVEMENT ||
                    !GameDataAnalysis::doesBuildTypeRemoveFeature(
                        GameDataAnalysis::getBuildTypeForImprovementType(getBaseImprovement(ci->second->possibleImprovements[j].second)), ci->second->featureType);
                if (keepsFeature)
                {
                    newPossibleImprovements.push_back(ci->second->possibleImprovements[j]);
                }
            }
            ci->second->possibleImprovements = newPossibleImprovements;
            ++ci;
        }
    }

    XYCoords CityImprovementManager::getIrrigationChainPlot(XYCoords destination, ImprovementTypes improvementType)
    {
        //std::ostream& os = CityLog::getLog(::getCity(city_))->getStream();
        // we presume that this plot has freshwater access since we checked when the plot's PlotInfo was built using IrrigatableArea (but we may not control it)
        const CvPlot* pPlot = gGlobals.getMap().plot(destination.iX, destination.iY);

        TeamTypes teamType = ::getCity(city_)->getTeam();
        int subAreaID = pPlot->getSubArea();
        int irrigatableAreaID = pPlot->getIrrigatableArea();

        FAStar* pIrrigationPathFinder = gDLL->getFAStarIFace()->create();
        CvMap& theMap = gGlobals.getMap();
        gDLL->getFAStarIFace()->Initialize(pIrrigationPathFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(),
            NULL, stepHeuristic, irrigationStepCost, irrigationStepValid, NULL, NULL, NULL);

        std::map<int, XYCoords> pathCostMap;

        for (int i = 1; i <= MAX_IRRIGATION_CHAIN_SEARCH_RADIUS; ++i)
        {
            CultureRangePlotIter iter(pPlot, (CultureLevelTypes)i);
        
            while (IterPlot pLoopPlot = iter())
            {
                // check we or our team owns the plot? (don't think you can build improvements unless you own the plot - except roads)
                if (pLoopPlot.valid() && pLoopPlot->isRevealed(teamType, false) && pLoopPlot->getSubArea() == subAreaID && pLoopPlot->getIrrigatableArea() == irrigatableAreaID && pLoopPlot->getOwner() == city_.eOwner)
                {
                    if ((pLoopPlot->isFreshWater() && pLoopPlot->getNonObsoleteBonusType(teamType) == NO_BONUS) || pLoopPlot->isIrrigated())
                    {
                        int cost = MAX_INT;
                        XYCoords buildCoords;
#ifdef ALTAI_DEBUG
                        //os << "\nChecking plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY());
#endif
                        boost::tie(cost, buildCoords) = getPathAndCost_(pIrrigationPathFinder, XYCoords(pLoopPlot->getX(), pLoopPlot->getY()), destination, pPlot->getOwner());

                        if (cost != MAX_INT)
                        {
                            // duplicates will be ignored in favour of the first entry with that cost (TODO - rethink this, or at least detect duplicates and log them?)
                            pathCostMap.insert(std::make_pair(cost, buildCoords));
                        }
                    }
                }
            }
        }
        gDLL->getFAStarIFace()->destroy(pIrrigationPathFinder);

        return pathCostMap.empty() ? XYCoords(-1, -1) : pathCostMap.begin()->second;
    }

    ImprovementTypes CityImprovementManager::getSubstituteImprovement(XYCoords coords)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(city_.eOwner))->getStream();
#endif
        const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

        DotMapItem::DotMapPlotData plotData(pPlot, city_.eOwner, 0);

        std::vector<PlotImprovementData>::iterator iter = std::find_if(improvements_.begin(), improvements_.end(), PlotDataFinder(coords));
        if (iter != improvements_.end())
        {
            bool hasIrrigation = false;
            ImprovementTypes currentImprovement = pPlot->getImprovementType();
            if (currentImprovement != NO_IMPROVEMENT)
            {
                const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(currentImprovement);
                if (improvementInfo.isCarriesIrrigation())
                {
                    hasIrrigation = true;
                }
            }

            if (!hasIrrigation && boost::get<6>(*iter) & IrrigationChainPlot)
            {
                boost::get<6>(*iter) &= ~IrrigationChainPlot;
            }
        }

        ImprovementTypes bestImprovement = NO_IMPROVEMENT;
        int bestValue = 0;
        YieldValueFunctor valueF(makeYieldW(2, 3, 1));

        for (size_t i = 0, count = plotData.possibleImprovements.size(); i < count; ++i)
        {
            if (plotData.possibleImprovements[i].second == NO_IMPROVEMENT)
            {
                continue;
            }

            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(plotData.possibleImprovements[i].second);
            if (improvementInfo.isRequiresIrrigation())
            {
                continue;
            }
            int thisValue = valueF(plotData.getPlotYield(i));

#ifdef ALTAI_DEBUG
            os << "\nChecking possible substitute improvement: " << improvementInfo.getType() << " value = " << thisValue;
#endif

            if (thisValue > bestValue)
            {
                bestValue = thisValue;
                bestImprovement = plotData.possibleImprovements[i].second;
            }
        }

        return getBaseImprovement(bestImprovement);
    }

    std::pair<XYCoords, RouteTypes> CityImprovementManager::getBestRoute() const
    {
        const CvCity* pCity = ::getCity(city_);
        CityPlotIter iter(pCity);
        const CvPlayerAI& player = CvPlayerAI::getPlayer(city_.eOwner);
        boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getMapAnalysis();

        RouteTypes bestRouteType = NO_ROUTE;
        int bestRouteValue = 0;
        XYCoords bestCoords;
        YieldValueFunctor yieldF(makeYieldW(2, 1, 1));

        while (IterPlot pLoopPlot = iter())
        {
            if (!pLoopPlot.valid())
            {
                continue;
            }

            PlayerTypes plotOwner = pLoopPlot->getOwner();
            XYCoords plotCoords(pLoopPlot->getX(), pLoopPlot->getY());

            if (plotOwner == city_.eOwner)
            {
                const PlotInfo::PlotInfoNode& node = pMapAnalysis->getPlotInfoNode(pLoopPlot);

                std::vector<std::pair<RouteTypes, PlotYield> > routeYieldChanges = 
                    getRouteYieldChanges(node, city_.eOwner, pLoopPlot->getImprovementType(), pLoopPlot->getFeatureType());
                
                for (size_t i = 0, count = routeYieldChanges.size(); i < count; ++i)
                {
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForRouteType(routeYieldChanges[i].first);
                    if (player.canBuild(pLoopPlot, buildType))
                    {
                        int thisRoutesValue = yieldF(routeYieldChanges[i].second);
                        if (thisRoutesValue > bestRouteValue)
                        {
                            bestRouteValue = thisRoutesValue;
                            bestRouteType = routeYieldChanges[i].first;
                            bestCoords = XYCoords(pLoopPlot->getX(), pLoopPlot->getY());
                        }
                    }
                }
            }
        }

        return std::make_pair(bestCoords, bestRouteType);
    }

    std::pair<int, XYCoords> CityImprovementManager::getPathAndCost_(FAStar* pIrrigationPathFinder, XYCoords start, XYCoords destination, PlayerTypes playerType) const
    {
        if (gDLL->getFAStarIFace()->GeneratePath(pIrrigationPathFinder, start.iX, start.iY, destination.iX, destination.iY, false, playerType))
        {
#ifdef ALTAI_DEBUG
            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(playerType));
            std::ostream& os = pCivLog->getStream();
            os << "\nFound path: ";
#endif
            FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pIrrigationPathFinder);
            XYCoords buildCoords;
            while (pNode)
            {
#ifdef ALTAI_DEBUG
                os << " (" << pNode->m_iX << ", " << pNode->m_iY << ")";
#endif
                const CvPlot* pBuildPlot = gGlobals.getMap().plot(pNode->m_iX, pNode->m_iY);
                if (pBuildPlot->isIrrigationAvailable(true))
                {
                    return std::make_pair(pNode->m_iData2, XYCoords(pNode->m_iX, pNode->m_iY));
                }
                
                pNode = pNode->m_pParent;
            }
        }
        return std::make_pair(MAX_INT, XYCoords());
    }

    void CityImprovementManager::write(FDataStreamBase* pStream) const
    {
        city_.write(pStream);
        writeImprovements(pStream, improvements_);
    }

    void CityImprovementManager::read(FDataStreamBase* pStream)
    {
        city_.read(pStream);
        readImprovements(pStream, improvements_);
    }

    void CityImprovementManager::writeImprovements(FDataStreamBase* pStream, const std::vector<CityImprovementManager::PlotImprovementData>& improvements)
    {
        size_t count = improvements.size();
        pStream->Write(count);
        for (size_t i = 0; i < count; ++i)
        {
            XYCoords coords = boost::get<0>(improvements[i]);
            pStream->Write(coords.iX);
            pStream->Write(coords.iY);
            pStream->Write(boost::get<1>(improvements[i]));
            pStream->Write(boost::get<2>(improvements[i]));
            boost::get<3>(improvements[i]).write(pStream);
            boost::get<4>(improvements[i]).write(pStream);
            pStream->Write(boost::get<5>(improvements[i]));
            pStream->Write(boost::get<6>(improvements[i]));
        }
    }

    void CityImprovementManager::readImprovements(FDataStreamBase* pStream, std::vector<CityImprovementManager::PlotImprovementData>& improvements)
    {
        size_t count = 0;
        pStream->Read(&count);
        improvements.clear();

        for (size_t i = 0; i < count; ++i)
        {
            XYCoords coords;
            pStream->Read(&coords.iX);
            pStream->Read(&coords.iY);

            FeatureTypes featureType = NO_FEATURE;
            pStream->Read((int*)&featureType);

            ImprovementTypes improvementType = NO_IMPROVEMENT;
            pStream->Read((int*)&improvementType);

            PlotYield plotYield;
            plotYield.read(pStream);

            TotalOutput totalOutput;
            totalOutput.read(pStream);

            ImprovementState improvementState;
            pStream->Read((int*)&improvementState);

            int improvementFlags = 0;
            pStream->Read(&improvementFlags);

            improvements.push_back(boost::make_tuple(coords, featureType, improvementType, plotYield, totalOutput, improvementState, improvementFlags));
        }
    }

    void CityImprovementManager::logImprovements() const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CityLog::getLog(::getCity(city_))->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " CityImprovementManager::logImprovements: ";

        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            logImprovement(os, improvements_[i]);
        }
#endif
    }

    // static
    void CityImprovementManager::logImprovement(std::ostream& os, const PlotImprovementData& improvement)
    {
#ifdef ALTAI_DEBUG
        os << "\n" << boost::get<0>(improvement);
        FeatureTypes featureType = boost::get<1>(improvement);
        ImprovementTypes improvementType = boost::get<2>(improvement);

        if (improvementType == NO_IMPROVEMENT)
        {
            os << " (no improvement)";
        }
        else
        {
            os << " " << gGlobals.getImprovementInfo(improvementType).getType();
        }

        if (featureType != NO_FEATURE)
        {
            os << " (" << gGlobals.getFeatureInfo(featureType).getType() << ")";
        }

        os << " yield = " << boost::get<3>(improvement);

        TotalOutput totalOutput = boost::get<4>(improvement);
        if (totalOutput != TotalOutput())
        {
            os << " simulated output = " << totalOutput;
        }

        ImprovementState state = boost::get<5>(improvement);
        switch (state)
        {
        case Not_Set:
            os << " state = unknown";
            break;
        case Not_Built:
            os << " state = not built";
            break;
        case Being_Built:
            os << " state = being built";
            break;
        case Built:
            os << " state = built";
            break;
        case Not_Selected:
            os << " state = not selected";
            break;
        default:
            os << " bad state?";
        }

        int flags = boost::get<6>(improvement);

        if (flags)
        {
            os << " flags = ";
        }
        
        if (flags & IrrigationChainPlot)
        {
            os << " irrigation chain, ";
        }
        if (flags & NeedsIrrigation)
        {
            os << " needs irrigation, ";
        }
        if (flags & NeedsRoute)
        {
            os << " needs route, ";
        }
        if (flags & KeepFeature)
        {
            os << " keep feature, ";
        }
        if (flags & RemoveFeature)
        {
            os << " remove feature, ";
        }
        if (flags & KeepExistingImprovement)
        {
            os << " keep existing, ";
        }
        if (flags & ImprovementMakesBonusValid)
        {
            os << " makes bonus valid, ";
        }
        if (flags & WorkerNeedsTransport)
        {
            os << " worker needs transport, ";
        }

        //os << " rank = " << getRank(boost::get<0>(improvement), gGlobals.getGame().getAltAI()->getPlayer(city_.eOwner)->getCity(city_.iID).getPlotAssignmentSettings().outputWeights);
#endif
    }

    std::vector<CityImprovementManager::PlotImprovementData> 
            findNewImprovements(const std::vector<CityImprovementManager::PlotImprovementData>& baseImprovements, const std::vector<CityImprovementManager::PlotImprovementData>& newImprovements)
    {
        std::vector<CityImprovementManager::PlotImprovementData> delta;

        for (size_t i = 0, count = newImprovements.size(); i < count; ++i)
        {
            if (boost::get<2>(newImprovements[i]) != NO_IMPROVEMENT)
            {
                std::vector<CityImprovementManager::PlotImprovementData>::const_iterator ci = std::find_if(baseImprovements.begin(), baseImprovements.end(), PlotDataFinder(newImprovements[i]));
                if (ci != baseImprovements.end())
                {
                    if (boost::get<2>(*ci) != boost::get<2>(newImprovements[i])) // new type of improvement
                    {
                        delta.push_back(newImprovements[i]);
                    }
                }
                else
                {
                    delta.push_back(newImprovements[i]); // new improvement where we had none before
                }
            }
        }
        return delta;
    }
}