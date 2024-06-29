#include "AltAI.h"

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
#include "./buildings_info.h"
#include "./plot_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./city_simulator.h"
#include "./irrigatable_area.h"
#include "./civ_helper.h"
#include "./city_log.h"
#include "./civ_log.h"
#include "./error_log.h"

#include "../CvGameCoreDLL/CvDLLEngineIFaceBase.h"
#include "../CvGameCoreDLL/CvDLLFAStarIFaceBase.h"
#include "../CvGameCoreDLL/CvGameCoreUtils.h"
#include "../CvGameCoreDLL/FAStarNode.h"

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

        struct PlotImprovementDataFinder
        {
            explicit PlotImprovementDataFinder(const PlotImprovementData& plotData) : coords(plotData.coords) {}
            explicit PlotImprovementDataFinder(XYCoords coords_) : coords(coords_) {}

            bool operator() (const PlotImprovementData& other) const
            {
                return other.coords == coords;
            }

            const XYCoords coords;
        };

        template <typename P>
            struct PlotImprovementDataAdaptor
        {
            typedef typename P Pred;
            PlotImprovementDataAdaptor(P pred_) : pred(pred_) {}

            bool operator () (const PlotImprovementData& p1, const PlotImprovementData& p2) const
            {
                return pred(p1.yield, p2.yield);
            }
            P pred;
        };

        struct PlotImprovementRankOrder
        {
            bool operator() (const PlotImprovementData& p1, const PlotImprovementData& p2) const
            {
                if (p1.simulationData.firstTurnWorked == p2.simulationData.firstTurnWorked)
                {
                    MixedOutputOrderFunctor<PlotYield> mixedF(makeYieldP(YIELD_FOOD), OutputUtils<PlotYield>::getDefaultWeights());
                    return mixedF(p1.yield, p2.yield);
                }
                else
                {
                    if (p1.simulationData.firstTurnWorked == -1)
                    {
                        return false;
                    }
                    else if (p2.simulationData.firstTurnWorked == -1)
                    {
                        return true;
                    }
                    else
                    {
                        return p1.simulationData.firstTurnWorked < p2.simulationData.firstTurnWorked;
                    }
                }
            }
        };
    }

    CityImprovementManager::~CityImprovementManager()
    {
        //ErrorLog::getLog(CvPlayerAI::getPlayer(city_.eOwner))->getStream() << "\ndelete CityImprovementManager at: " << this;
    }

    CityImprovementManager::CityImprovementManager(IDInfo city, bool includeUnclaimedPlots)
        : city_(city), includeUnclaimedPlots_(includeUnclaimedPlots)
    {
    }

    void CityImprovementManager::calcImprovements_(const CityDataPtr& pCityData, const std::vector<YieldTypes>& yieldTypes, int targetSize, int lookAheadDepth)
    {
        const CvCity* pCity = ::getCity(city_);

        PlayerTypes playerType = pCity->getOwner();
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType);

        std::vector<ConditionalPlotYieldEnchancingBuilding> conditionalEnhancements = GameDataAnalysis::getInstance()->getConditionalPlotYieldEnhancingBuildings(playerType, pCity);        
        
        //DotMapItem dotMapItem(XYCoords(pCity->getX(), pCity->getY()), pCity->plot()->getYield());
        boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(playerType)->getAnalysis()->getMapAnalysis();
        const CvMap& theMap = gGlobals.getMap();

        std::set<XYCoords> pinnedIrrigationPlots, sharedCoords = pMapAnalysis->getCitySharedPlots(city_);

        DotMapItem dotMapItem(pCity->plot()->getCoords(), pCityData->getCityPlotData().plotYield, pCity->plot()->isFreshWater());

        for (PlotDataList::const_iterator plotIter(pCityData->getPlotOutputs().begin()), plotEndIter(pCityData->getPlotOutputs().end()); plotIter != plotEndIter; ++plotIter)
        {
            if (plotIter->isActualPlot() && plotIter->ableToWork)
            {
                DotMapItem::DotMapPlotData plotData(*plotIter, playerType, lookAheadDepth);

                if (!plotData.possibleImprovements.empty())
                {
                    // todo - allow global buildings which change yield to be counted here (e.g. colossus)
                    // this seems to include potential buildings - probably should just be actual
                    PlotYield extraYield = getExtraConditionalYield(dotMapItem.coords, plotIter->coords, conditionalEnhancements);
                    if (!isEmpty(extraYield))
                    {
                        for (size_t i = 0, count = plotData.possibleImprovements.size(); i < count; ++i)
                        {
                            plotData.possibleImprovements[i].first += extraYield;
                        }
                    }
                    const bool isSharedPlot = sharedCoords.find(plotIter->coords) != sharedCoords.end();
                    bool isImpOwnedPlot = true;

                    if (isSharedPlot)
                    {
                        IDInfo impCity = pMapAnalysis->getImprovementOwningCity(city_, plotIter->coords);
                        // check for shared plots this city doesn't own the improvement setting for and pin them to their current improvement
                        if (impCity != IDInfo() && impCity != city_)
                        {
                            ImprovementTypes selectedImprovement = plotIter->improvementType;
                            if (selectedImprovement == NO_IMPROVEMENT)
                            {
                                std::pair<bool, PlotImprovementData*> foundImprovement = pPlayer->getCity(impCity.iID).getCityImprovementManager()->findImprovement(plotIter->coords);
                                if (foundImprovement.first)
                                {
                                    selectedImprovement = foundImprovement.second->improvement;
                                }
                            }

                            if (plotData.setWorkedImpIndex(selectedImprovement))
                            {
                                plotData.isPinned = true;
                                isImpOwnedPlot = false;
#ifdef ALTAI_DEBUG
                                {   // debug
                                    std::ostream& os = CityLog::getLog(pCity)->getStream();
                                    os << "\nMarked shared plot: " << plotIter->coords << " as pinned with improvement: "
                                        << (plotData.getWorkedImprovement() == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo((ImprovementTypes)plotData.getWorkedImprovement()).getType());
                                }
#endif
                            }

                        }
                    }

                    if (isImpOwnedPlot)
                    {
                        if (plotIter->bonusType == NO_BONUS && plotIter->improvementType != NO_IMPROVEMENT &&
                                // keep any plot worked long enough to upgrade
                                (getBaseImprovement(plotIter->improvementType) != plotIter->improvementType ||
                                // if base imp, but time imp has been built is less than twice the imp upgrade time
                                (gGlobals.getImprovementInfo(plotIter->improvementType).getImprovementUpgrade() != NO_IMPROVEMENT && 
                                2 * gGlobals.getGame().getImprovementUpgradeTime(plotIter->improvementType) > gGlobals.getMap().plot(plotIter->coords)->getImprovementDuration())))
                                // mark towns and villages as probably want to keep them
                                //(improvementIsFinalUpgrade(plotIter->improvementType) || nextImprovementIsFinalUpgrade(plotIter->improvementType)))
                        {
                            if (plotData.setWorkedImpIndex(plotIter->improvementType))
                            {
                                plotData.isPinned = true;
#ifdef ALTAI_DEBUG
                                {   // debug
                                    std::ostream& os = CityLog::getLog(pCity)->getStream();
                                    os << "\nMarked plot: " << plotIter->coords << " as pinned with upgraded improvement: "
                                        << (plotData.getWorkedImprovement() == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo(plotIter->improvementType).getType());
                                }
#endif
                            }
                        }

                        std::vector<PlotImprovementData>::iterator impIter = std::find_if(improvements_.begin(), improvements_.end(), PlotImprovementDataFinder(plotIter->coords));

                        if (impIter != improvements_.end())
                        {                        
                            if (impIter->flags & PlotImprovementData::IrrigationChainPlot) // mark plot as pinned if it needs to keep irrigation
                            {
                                if (plotData.setWorkedImpIndex(impIter->improvement))
                                {
                                    pinnedIrrigationPlots.insert(plotIter->coords);
                                    plotData.isPinned = true;
#ifdef ALTAI_DEBUG
                                    {   // debug
                                        std::ostream& os = CityLog::getLog(pCity)->getStream();
                                        os << "\nMarked plot: " << impIter->coords << " as pinned with improvement: "
                                            << (plotData.getWorkedImprovement() == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo((ImprovementTypes)plotData.getWorkedImprovement()).getType());
                                    }
#endif
                                }
                            }                            
                        }
                    }
                }
#ifdef ALTAI_DEBUG
                {   // debug
                    std::ostream& os = CityLog::getLog(pCity)->getStream();
                    plotData.debug(os);
                }
#endif
                dotMapItem.plotDataSet.insert(plotData);
            }
        }

        dotMapItem.calcOutput(*pPlayer, 3 * pPlayer->getAnalysis()->getTimeHorizon(), pCity->goodHealth());
        DotMapOptimiser optMixed(dotMapItem, YieldWeights(), YieldWeights());

        //optMixed.optimise(yieldTypes, std::min<int>(dotMapItem.plotData.size(), targetSize));
        optMixed.optimise(yieldTypes, std::min<int>(targetSize, dotMapItem.plotDataSet.size()));
        //optMixed.optimise(yieldTypes, std::min<int>(dotMapItem.plotData.size(), 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel())));

#ifdef ALTAI_DEBUG
        {
            std::ostream& os = CityLog::getLog(pCity)->getStream();
            os << "\nPop = " << pCity->getPopulation() << ", happy = " << pCity->happyLevel() << ", unhappy = " << pCity->unhappyLevel() << " target = " << std::min<int>(targetSize, dotMapItem.plotDataSet.size());
            /*for (size_t i = 0, count = yieldTypes.size(); i < count; ++i)
            {
                os << " yieldTypes[" << i << "] = " << yieldTypes[i];
            }*/
        }
        dotMapItem.debugOutputs(*gGlobals.getGame().getAltAI()->getPlayer(city_.eOwner), CityLog::getLog(pCity)->getStream());
#endif

        improvements_.clear();
        improvementsDelta_ = TotalOutput();

        for (DotMapItem::PlotDataConstIter ci(dotMapItem.plotDataSet.begin()), ciEnd(dotMapItem.plotDataSet.end()); ci != ciEnd; ++ci)
        {
            ImprovementTypes improvementType = ci->getWorkedImprovement();

            if (improvementType != NO_IMPROVEMENT)
            {
                ImprovementTypes baseImprovement = getBaseImprovement(improvementType);
                bool buildRemovesFeature = GameDataAnalysis::doesBuildTypeRemoveFeature(
                    GameDataAnalysis::getBuildTypeForImprovementType(baseImprovement), ci->featureType);

                const CvPlot* pPlot = gGlobals.getMap().plot(ci->coords.iX, ci->coords.iY);

                if (baseImprovement == getBaseImprovement(pPlot->getImprovementType()))  // already built and selected
                {
                    improvements_.push_back(PlotImprovementData(ci->coords, pPlot->getFeatureType(), improvementType, pPlot->getYield(), 
                        PlotImprovementData::Built, PlotImprovementData::None));
                }
                else
                {
                    PlotYield plotYield = baseImprovement == improvementType ?
                        ci->getPlotYield() : getYield(pMapAnalysis->getPlotInfoNode(pPlot), city_.eOwner, baseImprovement, 
                        buildRemovesFeature ? NO_FEATURE : ci->featureType, pPlot->getRouteType());
#ifdef ALTAI_DEBUG
                    /*if (isEmpty(plotYield))
                    {
                        std::ostream& os = CityLog::getLog(pCity)->getStream();
                        os << "\nempty plot yield for imp at: " << ci->coords;
                    }*/
#endif
                    improvements_.push_back(PlotImprovementData(ci->coords, 
                        buildRemovesFeature ? ci->featureType : NO_FEATURE, baseImprovement, 
                        plotYield, PlotImprovementData::Not_Built, PlotImprovementData::None));
                }
                
                //ImprovementTypes currentImprovement = pPlot->getImprovementType();

                /*PlotImprovementData::ImprovementState improvementState = 
                    getBaseImprovement(currentImprovement) == getBaseImprovement(improvementType) ? PlotImprovementData::Built : PlotImprovementData::Not_Built;*/

                if (ci->improvementMakesBonusValid)
                {
                    improvements_.rbegin()->flags |= PlotImprovementData::ImprovementMakesBonusValid;
                }

                if (!ci->isSelected && !ci->improvementMakesBonusValid)  // always mark bonuses as selected if we can build the appropriate improvement
                {
                    improvements_.rbegin()->state = PlotImprovementData::Not_Selected;
                    if (sharedCoords.find(ci->coords) != sharedCoords.end())
                    {
#ifdef ALTAI_DEBUG
                        {
                            std::ostream& os = CityLog::getLog(pCity)->getStream();
                            os << "\n unused shared coord: " << ci->coords;
                        }
#endif
                    }
                }
                /*else
                {
                    improvements_.rbegin()->state = improvementState;
                }*/

                if (pinnedIrrigationPlots.find(ci->coords) != pinnedIrrigationPlots.end())
                {
                    improvements_.rbegin()->flags |= PlotImprovementData::IrrigationChainPlot;
                }
            }
        }

        // no longer distinguish if simulation
        markPlotsWhichNeedIrrigation_();
        markPlotsWhichNeedRoute_();
        markPlotsWhichNeedTransport_();
        markPlotsWhichNeedFeatureRemoved_();
    }

    void CityImprovementManager::markPlotsWhichNeedIrrigation_()
    {
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            //if (improvements_[i].state == PlotImprovementData::Built)
            //{
                const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(improvements_[i].improvement);
                const PlotYield irrigatedYieldChange = const_cast<CvImprovementInfo&>(improvementInfo).getIrrigatedYieldChangeArray();
                if (!isEmpty(irrigatedYieldChange))
                {
                    const CvPlot* pPlot = gGlobals.getMap().plot(improvements_[i].coords.iX, improvements_[i].coords.iY);
                    if (!pPlot->isIrrigated() && !pPlot->isFreshWater())
                    {
                        const int irrigatableAreaID = pPlot->getIrrigatableArea();
                        if (irrigatableAreaID != FFreeList::INVALID_INDEX)
                        {
                            boost::shared_ptr<IrrigatableArea> pIrrigatableArea = gGlobals.getMap().getIrrigatableArea(irrigatableAreaID);
                            if (pIrrigatableArea->hasFreshWaterAccess())
                            {
                                improvements_[i].flags |= PlotImprovementData::NeedsIrrigation;
                            }
                        }
                    }
                }
            //}
        }
    }

    void CityImprovementManager::markPlotsWhichNeedRoute_()
    {
        const CvCity* pCity = ::getCity(city_);
        const int citySubArea = pCity->plot()->getSubArea();

        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (improvements_[i].flags & PlotImprovementData::ImprovementMakesBonusValid)
            {
                XYCoords coords = improvements_[i].coords;
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                if (pPlot->getOwner() == NO_PLAYER)
                {
                    continue;
                }

                // todo - deal with plots on other subareas which need routes (needs consideration of forts and check if there are other cities in that sub area)
                //check for plot group as it appears we can be called on city capture before plot groups are properly setup?
                if (citySubArea == pPlot->getSubArea() && !pPlot->isConnectedTo(pCity))
                {
                    improvements_[i].flags |= PlotImprovementData::NeedsRoute;
                }
            }
        }
    }

    void CityImprovementManager::markPlotsWhichNeedFeatureRemoved_()
    {
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (improvements_[i].state == PlotImprovementData::Built)
            {
                XYCoords coords = improvements_[i].coords;
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                FeatureTypes featureType = pPlot->getFeatureType();
                if (featureType != NO_FEATURE)
                {
                    if (GameDataAnalysis::isBadFeature(featureType))
                    {
                        BuildTypes buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureType);
                        if (buildType != NO_BUILD)
                        {
                            improvements_[i].flags |= PlotImprovementData::RemoveFeature;
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
            if (improvements_[i].state == PlotImprovementData::Not_Built)
            {
                XYCoords coords = improvements_[i].coords;
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

                // todo - deal with plots on other subareas which need routes (needs consideration of forts and check if there are other cities in that sub area)
                if (!pPlot->isWater() && citySubArea != pPlot->getSubArea())
                {
                    improvements_[i].flags |= PlotImprovementData::WorkerNeedsTransport;
                }
            }
        }
    }

    boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> CityImprovementManager::getBestImprovementNotBuilt(bool whichMakesBonusValid, bool selectedOnly, const std::vector<PlotCondPtr >& conditions) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CityLog::getLog(::getCity(city_))->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << __FUNCTION__;
        //logImprovements();
        for (size_t i = 0, count = conditions.size(); i < count; ++i)
        {
            os << "\n\twith condition: ";
            conditions[i]->log(os);
        }
#endif

        int bestImprovementIndex = NO_IMPROVEMENT;
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(city_.eOwner);

        // improvements are in order of selection
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (improvements_[i].improvement != NO_IMPROVEMENT)
            {
                const XYCoords coords = improvements_[i].coords;
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                BonusTypes bonusType = pPlot->getBonusType(::getCity(city_)->getTeam());
                ImprovementTypes currentImprovementType = pPlot->getImprovementType(), plannedImprovementType = improvements_[i].improvement;
                bool hasFort = currentImprovementType != NO_IMPROVEMENT && gGlobals.getImprovementInfo(currentImprovementType).isActsAsCity();
                bool plotNeedsBonusImprovement = bonusType != NO_BONUS && (getBaseImprovement(currentImprovementType) != getBaseImprovement(plannedImprovementType) || hasFort);

                if (plotNeedsBonusImprovement && !(improvements_[i].flags & PlotImprovementData::ImprovementMakesBonusValid))
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
                os << "\nChecking plot: " << coords << " for planned improvement: " << i << " " << gGlobals.getImprovementInfo(plannedImprovementType).getType();
                logImprovement(os, improvements_[i]);
#endif

                bool valid = improvements_[i].isSelectedAndNotBuilt() ||
                    // build improvements which setup bonus resources (maybe add flag to override this if want to prevent connecting a resource)
                    (plotNeedsBonusImprovement && improvements_[i].state == PlotImprovementData::Not_Built && improvements_[i].flags & PlotImprovementData::ImprovementMakesBonusValid);
                if (valid)
                {
                    for (size_t j = 0, count = conditions.size(); j < count; ++j)
                    {
                        valid = valid && (*conditions[j])(pPlot);
                    }
                }

                if (valid)
                {
#ifdef ALTAI_DEBUG
                    os << " (valid) ";
#endif
                    if (!selectedOnly || (selectedOnly && improvements_[i].simulationData.numTurnsWorked > 0))
                    {
#ifdef ALTAI_DEBUG
                        os << " selected imp as best ";
#endif
                        bestImprovementIndex = i;
                    }                    
                    break;
                }
            }
        }
        
        if (bestImprovementIndex != NO_IMPROVEMENT)
        {
            return boost::make_tuple(improvements_[bestImprovementIndex].coords, 
                improvements_[bestImprovementIndex].removedFeature, 
                getBaseImprovement(improvements_[bestImprovementIndex].improvement),
                improvements_[bestImprovementIndex].flags);
        }
        else
        {
            return boost::make_tuple(XYCoords(-1, -1), NO_FEATURE, NO_IMPROVEMENT, 0);
        }
    }

    ImprovementTypes CityImprovementManager::getBestImprovementNotBuilt(XYCoords coords) const
    {
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (improvements_[i].coords == coords)
            {
                const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                BonusTypes bonusType = pPlot->getBonusType(::getCity(city_)->getTeam());
                ImprovementTypes currentImprovementType = pPlot->getImprovementType();

                bool hasFort = currentImprovementType != NO_IMPROVEMENT && gGlobals.getImprovementInfo(currentImprovementType).isActsAsCity();

                bool plotNeedsBonusImprovement = bonusType != NO_BONUS && (currentImprovementType != getBaseImprovement(improvements_[i].improvement) || hasFort);

                if (!plotNeedsBonusImprovement)
                {
                    if (plotIsWorkedAndImproved(pPlot) || improvementIsUpgradeOf(currentImprovementType, getBaseImprovement(improvements_[i].improvement)))
                    {
                        break;
                    }
                }
                else
                {
#ifdef ALTAI_DEBUG
                    std::ostream& os = CityLog::getLog(::getCity(city_))->getStream();
                    os << "\nFound plot at: " << coords << " which needs bonus improvement: "
                       << gGlobals.getImprovementInfo(getBaseImprovement(improvements_[i].improvement)).getType();
                    if (hasFort)
                    {
                        os << " (currently has fort)";
                    }
#endif
                }
                return getBaseImprovement(improvements_[i].improvement);
            }
        }
        return NO_IMPROVEMENT;
    }

    int CityImprovementManager::getRank(XYCoords coords) const
    {        
        std::vector<PlotImprovementData>::const_iterator ci = std::find_if(improvements_.begin(), improvements_.end(), PlotImprovementDataFinder(coords));        
        return std::distance(improvements_.begin(), ci);
    }

    int CityImprovementManager::getNumImprovementsNotBuilt() const
    {
        int improvementCount = 0;
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (improvements_[i].state != PlotImprovementData::Built && improvements_[i].state != PlotImprovementData::Not_Selected)
            {
                ++improvementCount;
            }
        }
        return improvementCount;
    }

    int CityImprovementManager::getNumImprovementsBuilt() const
    {
        int improvementCount = 0;
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            if (improvements_[i].state == PlotImprovementData::Built)
            {
                ++improvementCount;
            }
        }
        return improvementCount;
    } 

    void CityImprovementManager::simulateImprovements(const CityDataPtr& pCityData, int lookAheadDepth, const std::string& logLabel)
    {
        const CvCity* pCity = ::getCity(city_);
        const PlayerPtr& player = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());        

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player->getCvPlayer())->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " label = " << logLabel;
        std::ostream& cityLog = CityLog::getLog(pCityData->getCity())->getStream();
        cityLog << "\nTurn = " << gGlobals.getGame().getGameTurn() << " label = " << logLabel;
#endif
        std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);
        const int targetSize = std::min<int>(20, 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel()));
        calcImprovements_(pCityData, yieldTypes, targetSize, lookAheadDepth);

        {
            std::vector<TechTypes> techs;
            if (lookAheadDepth > 0)
            {
                techs = player->getAnalysis()->getTechsWithDepth(lookAheadDepth);
                for (size_t i = 0, count = techs.size(); i < count; ++i)
                {
                    player->getCivHelper()->addTech(techs[i]);
                }
            }

            CityDataPtr pSimulationCityData = CityDataPtr(pCityData->clone());
            std::vector<IProjectionEventPtr> events;

            ConstructItem constructItem;
            const int numSimTurns = player->getAnalysis()->getNumSimTurns();
            ProjectionLadder base = getProjectedOutput(*gGlobals.getGame().getAltAI()->getPlayer(pCityData->getOwner()), pSimulationCityData, numSimTurns, events, constructItem, __FUNCTION__, false, false);

            // add all imps
            {
                pSimulationCityData = CityDataPtr(pCityData->clone());

                for (size_t i = 0, count = improvements_.size(); i < count; ++i)
                {
                    XYCoords coords = improvements_[i].coords;
                    const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                    if (!improvementIsUpgradeOf(pPlot->getImprovementType(), improvements_[i].improvement) && improvements_[i].state != PlotImprovementData::Built)
                    {
                        PlotDataListIter simulatedPlotIter = pSimulationCityData->findPlot(coords);
                        if (simulatedPlotIter->controlled)
                        {
                            updateCityOutputData(*pSimulationCityData, *simulatedPlotIter, improvements_[i].removedFeature, simulatedPlotIter->routeType, getBaseImprovement(improvements_[i].improvement));
                        }
                    }
                }

                events.clear();
                ProjectionLadder ladder = getProjectedOutput(*gGlobals.getGame().getAltAI()->getPlayer(pCityData->getOwner()), pSimulationCityData, numSimTurns, events, constructItem, __FUNCTION__, false, false);

                {
#ifdef ALTAI_DEBUG
                    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCityData->getOwner()))->getStream();
                    os << "\nWorked plots data:";
#endif
                    for (size_t i = 0, count = improvements_.size(); i < count; ++i)
                    {
                        std::pair<int, int> workedData = ladder.getWorkedTurns(improvements_[i].coords);
                        improvements_[i].simulationData.firstTurnWorked = workedData.first;
                        improvements_[i].simulationData.numTurnsWorked = workedData.second;
#ifdef ALTAI_DEBUG
                        os << "\n\tplot: " << improvements_[i].coords;
                        if (workedData.first != -1)
                        {
                            os << " imp = " << gGlobals.getImprovementInfo(improvements_[i].improvement).getType();
                            os << " first worked turn: " << workedData.first << " for: " << workedData.second << " turns";                            
                        }
                        else
                        {
                            os << " not worked";
                        }
#endif
                    }
                    improvementsDelta_ = ladder.getOutput() - base.getOutput();
#ifdef ALTAI_DEBUG
                    os << "\nTotal delta: " << improvementsDelta_;
#endif
                }
            }

            for (size_t i = 0, count = techs.size(); i < count; ++i)
            {
                player->getCivHelper()->removeTech(techs[i]);
            }
        }

        std::sort(improvements_.begin(), improvements_.end(), PlotImprovementRankOrder());
#ifdef ALTAI_DEBUG
        logImprovements(os);
#endif
    }

    bool CityImprovementManager::updateImprovements(const CvPlot* pPlot, ImprovementTypes improvementType, FeatureTypes featureType, RouteTypes routeType, bool simulated)
    {
        const CvCity* pCity = ::getCity(city_);
        const PlayerPtr& player = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        boost::shared_ptr<MapAnalysis> pMapAnalysis = player->getAnalysis()->getMapAnalysis();
        std::set<XYCoords> sharedCoords = pMapAnalysis->getCitySharedPlots(city_);

        XYCoords coords(pPlot->getCoords());
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
        const bool isSharedPlot = sharedCoords.find(coords) != sharedCoords.end();

        std::pair<bool, PlotImprovementData*> foundImprovement = findImprovement(coords);

        if (foundImprovement.first)  // found an entry for these coordinates
        {
            foundCorrectImprovement = getBaseImprovement(improvementType) == getBaseImprovement(foundImprovement.second->improvement);
            if (foundCorrectImprovement)
            {
                foundImprovement.second->state = PlotImprovementData::Built;
                if (markPlotAsPartOfIrrigationChain)
                {
                    foundImprovement.second->flags |= PlotImprovementData::IrrigationChainPlot;
                }
#ifdef ALTAI_DEBUG
                {
                    boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity);
                    std::ostream& os = pCityLog->getStream();
                    os << "\nFound built improvement: " << (improvementType == NO_IMPROVEMENT ? "(none) " : gGlobals.getImprovementInfo(improvementType).getType())
                        << " at: " << pPlot->getCoords() << " with yield = " << foundImprovement.second->yield;
                }
#endif
                if (isSharedPlot && !simulated)
                {
                    IDInfo impCity = pMapAnalysis->getImprovementOwningCity(city_, coords);
                    if (impCity == IDInfo())
                    {
                        pMapAnalysis->setImprovementOwningCity(city_, coords);
#ifdef ALTAI_DEBUG
                        {
                            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player->getCvPlayer());
                            //boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity);
                            std::ostream& os = pCivLog->getStream();
                            os << "\nSetting city: " << safeGetCityName(pCity) << " as shared improvement owner: " << (improvementType == NO_IMPROVEMENT ? "(none) " : gGlobals.getImprovementInfo(improvementType).getType())
                                << " at: " << pPlot->getCoords() << " with yield = " << foundImprovement.second->yield;
                        }
#endif
                    }
                }
            }
            else if (improvementType != NO_IMPROVEMENT)
            {
#ifdef ALTAI_DEBUG                    
                {
                    boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity);
                    std::ostream& os = pCityLog->getStream();
                    os << "\nFailed to match built improvement: " << (improvementType == NO_IMPROVEMENT ? "(none) " : gGlobals.getImprovementInfo(improvementType).getType())
                        << " at: " << pPlot->getCoords() << " to: " << 
                        (foundImprovement.second->improvement== NO_IMPROVEMENT ? "(none) " : gGlobals.getImprovementInfo(foundImprovement.second->improvement).getType());
                }
#endif                    

                PlotYield thisYield = getYield(pMapAnalysis->getPlotInfoNode(pPlot), pPlot->getOwner(), improvementType, featureType, routeType);
#ifdef ALTAI_DEBUG
                {
                    boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity);
                    std::ostream& os = pCityLog->getStream();
                    os <<  " current yield = " << thisYield;
                }
#endif
                *foundImprovement.second = PlotImprovementData(coords, featureType, improvementType, 
                    thisYield, PlotImprovementData::Built, markPlotAsPartOfIrrigationChain ? PlotImprovementData::IrrigationChainPlot : 0);                    
            }
            else // improvement was pillaged or otherwise destroyed
            {
                foundImprovement.second->state = PlotImprovementData::Not_Built;
            }
        }
        else  // add a new entry for this improvement
        {
            PlotYield thisYield = getYield(pMapAnalysis->getPlotInfoNode(pPlot), pPlot->getOwner(), improvementType, featureType, routeType);
            improvements_.push_back(PlotImprovementData(coords, featureType, improvementType, 
                thisYield, PlotImprovementData::Built, markPlotAsPartOfIrrigationChain ? PlotImprovementData::IrrigationChainPlot : 0));
#ifdef ALTAI_DEBUG                    
            {
                boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity);
                std::ostream& os = pCityLog->getStream();
                os << "\nAdding unexpected improvement: " << (improvementType == NO_IMPROVEMENT ? "(none) " : gGlobals.getImprovementInfo(improvementType).getType())
                    << " at: " << pPlot->getCoords();
            }
#endif      
        }

        if (markPlotAsPartOfIrrigationChain && !simulated)
        {
            pMapAnalysis->markIrrigationChainSourcePlot(coords, improvementType);

#ifdef ALTAI_DEBUG       
            {   // debug
                std::ostream& os = CityLog::getLog(pCity)->getStream();
                os << "\nAdded plot: " << coords << " as part of irrigation chain with improvement: "
                   << (improvementType == NO_IMPROVEMENT ? " (none) " : gGlobals.getImprovementInfo(improvementType).getType());
            }
#endif           
        }

        return foundCorrectImprovement;
    }

    const std::vector<PlotImprovementData>& CityImprovementManager::getImprovements() const
    {
        return improvements_;
    }

    std::vector<PlotImprovementData>& CityImprovementManager::getImprovements()
    {
        return improvements_;
    }

    TotalOutput CityImprovementManager::getImprovementsDelta() const
    {
        return improvementsDelta_;
    }

    std::pair<bool, PlotImprovementData*> CityImprovementManager::findImprovement(XYCoords coords)
    {
        std::vector<PlotImprovementData>::iterator iter = std::find_if(improvements_.begin(), improvements_.end(), PlotImprovementDataFinder(coords));
        return std::make_pair(iter != improvements_.end(), &*iter);
    }

    void CityImprovementManager::markFeaturesToKeep_(DotMapItem& dotMapItem) const
    {
        std::multimap<int, DotMapItem::PlotDataIter> forestMap;
        YieldValueFunctor valueF(makeYieldW(4, 2, 1));

        for (DotMapItem::PlotDataIter plotIter(dotMapItem.plotDataSet.begin()), endIter(dotMapItem.plotDataSet.end()); plotIter != endIter; ++plotIter)
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

    XYCoords CityImprovementManager::getIrrigationChainPlot(XYCoords destination)
    {
        // we presume that this plot has freshwater access since we checked when the plot's PlotInfo was built using IrrigatableArea (but we may not control it)
        const CvPlot* pPlot = gGlobals.getMap().plot(destination.iX, destination.iY);

#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pPlot->getOwner()));
        std::ostream& os = pCivLog->getStream();
#endif
        TeamTypes teamType = ::getCity(city_)->getTeam();
        int subAreaID = pPlot->getSubArea();
        int irrigatableAreaID = pPlot->getIrrigatableArea();

        FAStar* pIrrigationPathFinder = gDLL->getFAStarIFace()->create();
        CvMap& theMap = gGlobals.getMap();
        gDLL->getFAStarIFace()->Initialize(pIrrigationPathFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(),
            NULL, stepHeuristic, irrigationStepCost, irrigationStepValid, irrigationStepAdd, NULL, NULL);

        std::map<int, XYCoords> pathCostMap;
        int lowestCost = MAX_INT, shortestLength = MAX_INT;
        XYCoords bestCoords(-1, -1);

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
                        gDLL->getFAStarIFace()->ForceReset(pIrrigationPathFinder);
                        int cost = MAX_INT, length = MAX_INT;
                        XYCoords buildCoords;
#ifdef ALTAI_DEBUG
                        os << "\nChecking potential irrigation source plot: " << XYCoords(pLoopPlot->getX(), pLoopPlot->getY());
#endif
                        boost::tie(cost, length, buildCoords) = getPathAndCost_(pIrrigationPathFinder, pLoopPlot->getCoords(), destination, pPlot->getOwner());

                        if (cost == lowestCost)
                        {
                            if (cost != MAX_INT && length < shortestLength)
                            {
                                shortestLength = length;
                                bestCoords = buildCoords;
#ifdef ALTAI_DEBUG
                                os << " c=" << cost << " l=" << length << " " << bestCoords;
#endif
                            }
                        }
                        else if (cost < lowestCost)
                        {
                            lowestCost = cost;
                            shortestLength = length;
                            bestCoords = buildCoords;
#ifdef ALTAI_DEBUG
                            os << " c=" << cost << " l=" << length << " " << bestCoords;
#endif
                        }
                    }
                }
            }
        }
        gDLL->getFAStarIFace()->destroy(pIrrigationPathFinder);

        return bestCoords;
    }

    ImprovementTypes CityImprovementManager::getSubstituteImprovement(const CityDataPtr& pCityData, XYCoords coords)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(city_.eOwner))->getStream();
#endif
        const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

        PlotDataListConstIter plotIter = pCityData->findPlot(coords);
        DotMapItem::DotMapPlotData plotData(*plotIter, city_.eOwner, 0);

        std::vector<PlotImprovementData>::iterator iter = std::find_if(improvements_.begin(), improvements_.end(), PlotImprovementDataFinder(coords));
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

            if (!hasIrrigation && iter->flags & PlotImprovementData::IrrigationChainPlot)
            {
                iter->flags &= ~PlotImprovementData::IrrigationChainPlot;
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
            XYCoords plotCoords(pLoopPlot->getCoords());

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
                            bestCoords = pLoopPlot->getCoords();
                        }
                    }
                }
            }
        }

        return std::make_pair(bestCoords, bestRouteType);
    }

    boost::tuple<int, int, XYCoords> CityImprovementManager::getPathAndCost_(FAStar* pIrrigationPathFinder, XYCoords start, XYCoords destination, PlayerTypes playerType) const
    {
        if (gDLL->getFAStarIFace()->GeneratePath(pIrrigationPathFinder, start.iX, start.iY, destination.iX, destination.iY, false, playerType))
        {
            int totalCost = 0, totalLength = 0;
#ifdef ALTAI_DEBUG
            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(playerType));
            std::ostream& os = pCivLog->getStream();
            os << "\nFound path: ";
#endif
            FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pIrrigationPathFinder);
            XYCoords buildCoords;
            while (pNode)
            {
                totalCost += pNode->m_iData1;
                ++totalLength;
#ifdef ALTAI_DEBUG
                os << " (" << pNode->m_iX << ", " << pNode->m_iY << ")" << " cost = " << pNode->m_iData1;
#endif
                const CvPlot* pBuildPlot = gGlobals.getMap().plot(pNode->m_iX, pNode->m_iY);
                if (pBuildPlot->isIrrigationAvailable(true))
                {
                    return boost::make_tuple(totalCost, totalLength, XYCoords(pNode->m_iX, pNode->m_iY));
                }
                
                pNode = pNode->m_pParent;
            }
        }
        return boost::make_tuple(MAX_INT, MAX_INT, XYCoords(-1, -1));
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

    void CityImprovementManager::writeImprovements(FDataStreamBase* pStream, const std::vector<PlotImprovementData>& improvements)
    {
        size_t count = improvements.size();
        pStream->Write(count);
        for (size_t i = 0; i < count; ++i)
        {
            improvements[i].write(pStream);
        }
    }

    void CityImprovementManager::readImprovements(FDataStreamBase* pStream, std::vector<PlotImprovementData>& improvements)
    {
        size_t count = 0;
        pStream->Read(&count);
        improvements.clear();

        for (size_t i = 0; i < count; ++i)
        {
            PlotImprovementData improvementData;
            improvementData.read(pStream);
            improvements.push_back(improvementData);
        }
    }

    void CityImprovementManager::logImprovements() const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CityLog::getLog(::getCity(city_))->getStream();
        os << "\nTurn = " << gGlobals.getGame().getGameTurn()  << __FUNCTION__ << " : ";

        logImprovements(os);
#endif
    }

    void CityImprovementManager::logImprovements(std::ostream& os) const
    {
        for (size_t i = 0, count = improvements_.size(); i < count; ++i)
        {
            logImprovement(os, improvements_[i]);
            os << " rank = " << getRank(improvements_[i].coords);
        }
    }

    // static
    void CityImprovementManager::logImprovement(std::ostream& os, const PlotImprovementData& improvement)
    {
#ifdef ALTAI_DEBUG
        os << "\n" << improvement.coords;
        FeatureTypes featureType = improvement.removedFeature;
        ImprovementTypes improvementType = improvement.improvement;

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

        os << " yield = " << improvement.yield;

        os << " first turn worked: " << improvement.simulationData.firstTurnWorked;
        if (improvement.simulationData.firstTurnWorked >= 0)
        {
            os << " for: " << improvement.simulationData.numTurnsWorked << " turns ";
        }

        PlotImprovementData::ImprovementState state = improvement.state;
        switch (state)
        {
        case PlotImprovementData::Not_Set:
            os << " state = unknown";
            break;
        case PlotImprovementData::Not_Built:
            os << " state = not built";
            break;
        case PlotImprovementData::Being_Built:
            os << " state = being built";
            break;
        case PlotImprovementData::Built:
            os << " state = built";
            break;
        case PlotImprovementData::Not_Selected:
            os << " state = not selected";
            break;
        default:
            os << " bad state?";
        }

        int flags = improvement.flags;

        if (flags)
        {
            os << " flags = ";
        }
        
        if (flags & PlotImprovementData::IrrigationChainPlot)
        {
            os << " irrigation chain, ";
        }
        if (flags & PlotImprovementData::NeedsIrrigation)
        {
            os << " needs irrigation, ";
        }
        if (flags & PlotImprovementData::NeedsRoute)
        {
            os << " needs route, ";
        }
        if (flags & PlotImprovementData::KeepFeature)
        {
            os << " keep feature, ";
        }
        if (flags & PlotImprovementData::RemoveFeature)
        {
            os << " remove feature, ";
        }
        if (flags & PlotImprovementData::KeepExistingImprovement)
        {
            os << " keep existing, ";
        }
        if (flags & PlotImprovementData::ImprovementMakesBonusValid)
        {
            os << " makes bonus valid, ";
        }
        if (flags & PlotImprovementData::WorkerNeedsTransport)
        {
            os << " worker needs transport, ";
        }
#endif
    }

    std::vector<PlotImprovementData> 
            findNewImprovements(const std::vector<PlotImprovementData>& baseImprovements, const std::vector<PlotImprovementData>& newImprovements)
    {
        std::vector<PlotImprovementData> delta;

        for (size_t i = 0, count = newImprovements.size(); i < count; ++i)
        {
            if (newImprovements[i].improvement != NO_IMPROVEMENT)
            {
                std::vector<PlotImprovementData>::const_iterator ci = std::find_if(baseImprovements.begin(), baseImprovements.end(), PlotImprovementDataFinder(newImprovements[i]));
                if (ci != baseImprovements.end())
                {
                    if (ci->improvement != newImprovements[i].improvement) // new type of improvement
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