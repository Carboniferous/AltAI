#include "./utils.h"
#include "./iters.h"
#include "./city.h"
#include "./city_optimiser.h"
#include "./player.h"
#include "./unit.h"
#include "./events.h"
#include "./game.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./city_simulator.h"
#include "./plot_info_visitors.h"
#include "./plot_info_visitors_streams.h"
#include "./helper_fns.h"
#include "./tactic_streams.h"
#include "./civ_log.h"
#include "./error_log.h"
#include "./save_utils.h"
#include "./city_projections.h"

#include "CvDLLEngineIFaceBase.h"
#include "CvDLLFAStarIFaceBase.h"
#include "CvGameCoreUtils.h"
#include "FAStarNode.h"

#include <sstream>

namespace AltAI
{
    namespace
    {
        CityImprovementManager& getImprovementManager(const CvCity* pCity)
        {
            return gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getMapAnalysis()->getImprovementManager(pCity->getIDInfo());
        }

        const CityImprovementManager& getConstImprovementManager(const CvCity* pCity)
        {
            return gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getMapAnalysis()->getImprovementManager(pCity->getIDInfo());
        }
    }

    City::City(CvCity* pCity)
        : player_(*gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())), pCity_(pCity), constructItem_(NO_BUILDING)
    {
        // defaults (some code may use these before the city calibrates them)
        optWeights_ = makeOutputW(5, 3, 2, 2, 1, 1);
    }

    void City::init()
    {
        pCityData_ = CityDataPtr(new CityData(pCity_));

        boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);

        flags_ |= NeedsBuildingCalcs;
        flags_ |= NeedsImprovementCalcs;
        flags_ |= NeedsBuildSelection;

        calcMaxOutputs_();
        calcImprovements_();

#ifdef ALTAI_DEBUG
        // debug
        {
            pCityLog->getStream() << "\nCity is automated = " << std::boolalpha << pCity_->isCitizensAutomated();
            pCityLog->logCultureData(*pCityData_);
            pCityLog->logUpgradeData(*pCityData_);

            debugGreatPeople_();
        }
#endif

        pCity_->AI_setAssignWorkDirty(true);
    }

    void City::doTurn()
    {
        setFlag(CanReassignSharedPlots);

        if (flags_ & NeedsImprovementCalcs)
        {
            calcImprovements_();
        }
    }

    void City::setFlag(Flags flags)
    {
        flags_ |= flags;
    }

    void City::updateBuildings(BuildingTypes buildingType, int count)
    {
#ifdef ALTAI_DEBUG
        {
            boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
            std::ostream& os = pCityLog->getStream();
            os << "\nTurn = " << gGlobals.getGame().getGameTurn();
            os << " Building " << (count > 0 ? "built: " : "lost: ") << gGlobals.getBuildingInfo(buildingType).getType();
        }
#endif
        BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType).getBuildingClassType();
        const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = isNationalWonderClass(buildingClassType);
        if (!isWorldWonder && !isNationalWonder)
        {
            gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getAnalysis()->getPlayerTactics()->
                updateCityBuildingTactics(pCity_->getIDInfo(), buildingType, count);
        }

        setFlag(NeedsBuildSelection);
        setFlag(NeedsBuildingCalcs);
    }

    void City::updateUnits(UnitTypes unitType)
    {
#ifdef ALTAI_DEBUG
        {
            boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
            std::ostream& os = pCityLog->getStream();
            os << "\nTurn = " << gGlobals.getGame().getGameTurn();
            os << " Unit constructed: " << gGlobals.getUnitInfo(unitType).getType();
        }
#endif
        setFlag(NeedsBuildSelection);
    }

    void City::updateImprovements(const CvPlot* pPlot, ImprovementTypes improvementType)
    {
        if (getImprovementManager(pCity_).updateImprovements(pPlot, improvementType))
        {
            setFlag(NeedsImprovementCalcs);
        }
    }

    void City::assignPlots() // designed to be called from CvCityAI::AI_assignWorkingPlots()
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
        std::ostream& os = pCivLog->getStream();
#endif
        pCityData_ = CityDataPtr(new CityData(pCity_));
        calcMaxOutputs_();

        plotAssignmentSettings_.growthType = CityOptimiser::Not_Set;
        bool isFoodProduction = pCity_->isFoodProduction();

        CityOptimiser opt(pCityData_, std::make_pair(maxOutputs_, optWeights_));

        if (!isFoodProduction)
        {
            plotAssignmentSettings_ = makePlotAssignmentSettings(pCityData_, pCity_, constructItem_);
            if (pCity_->isProductionProcess())
            {
                const CvProcessInfo& processInfo = gGlobals.getProcessInfo(pCity_->getProductionProcess());

                CommerceModifier modifier;
                for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
                {
                    modifier[i] = processInfo.getProductionToCommerceModifier(i);
                }

                opt.optimise<ProcessValueAdaptorFunctor<MixedWeightedTotalOutputOrderFunctor>, MixedWeightedTotalOutputOrderFunctor>
                    (plotAssignmentSettings_.outputPriorities, plotAssignmentSettings_.outputWeights, plotAssignmentSettings_.growthType, modifier, true);
            }
            else if (plotAssignmentSettings_.targetFoodYield != Range<>())
            {
                opt.optimise<MixedWeightedTotalOutputOrderFunctor>(plotAssignmentSettings_.outputPriorities, plotAssignmentSettings_.outputWeights, plotAssignmentSettings_.targetFoodYield, true);
            }
            else
            {
                opt.optimise<MixedWeightedTotalOutputOrderFunctor>(plotAssignmentSettings_.outputPriorities, plotAssignmentSettings_.outputWeights, plotAssignmentSettings_.growthType, true);
                //opt.optimise<MixedTotalOutputOrderFunctor>(plotAssignmentSettings_.outputPriorities, plotAssignmentSettings_.outputWeights, plotAssignmentSettings_.growthType, true);
            }
        }
        else
        {
            opt.optimiseFoodProduction(pCity_->getProductionUnit(), true);
#ifdef ALTAI_DEBUG
            pCityData_->debugBasicData(os);
#endif
        }

        std::map<SpecialistTypes, int> specCounts;

        // city plot is not in plot data list
        pCity_->setWorkingPlot(CITY_HOME_PLOT, pCity_->getPopulation() > 0 && pCity_->canWork(pCity_->getCityIndexPlot(CITY_HOME_PLOT)));

        for (PlotDataListConstIter iter(pCityData_->getPlotOutputs().begin()), endIter(pCityData_->getPlotOutputs().end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                if (iter->isActualPlot())
                {
                    CvPlot* pPlot = gGlobals.getMap().plot(iter->coords.iX, iter->coords.iY);

                    // error check
                    /*if (pPlot->getWorkingCity() != pCity_)
                    {
                        std::ostream& os = ErrorLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
                        os << "\nPlot: " << iter->coords << " has conflicting working city settings: plot's = " 
                            << narrow(pPlot->getWorkingCity()->getName()) << " and city is: " << narrow(pCity_->getName()) << ")";
                    }*/

                    pCity_->setWorkingPlot(pPlot, true);
                }
                else
                {
                    SpecialistTypes specialistType = (SpecialistTypes)iter->coords.iY;
                    ++specCounts[specialistType];
                }
            }
        }

        for (PlotDataListConstIter iter(pCityData_->getFreeSpecOutputs().begin()), endIter(pCityData_->getFreeSpecOutputs().end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                ++specCounts[(SpecialistTypes)iter->coords.iY];
            }
        }

        for (int i = 0, count = gGlobals.getNumSpecialistInfos(); i < count; ++i)
        {
            std::map<SpecialistTypes, int>::const_iterator ci(specCounts.find((SpecialistTypes)i));
            if (ci != specCounts.end() || pCity_->getSpecialistCount((SpecialistTypes)i) > 0)
            {
                pCity_->setSpecialistCount((SpecialistTypes)i, ci != specCounts.end() ? ci->second : 0);
            }
        }

        if (flags_ & CanReassignSharedPlots)
        {
            gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getAnalysis()->getMapAnalysis()->reassignUnworkedSharedPlots(pCity_->getIDInfo());
            flags_ &= ~CanReassignSharedPlots;
        }

        // update projection
        {
            const boost::shared_ptr<Player>& player = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner());

            CityDataPtr pCityData = pCityData_->clone();
            std::vector<IProjectionEventPtr> events;
            events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent(pCityData)));

            currentOutputProjection_ = getProjectedOutput(*player, pCityData, 50, events);
        }

#ifdef ALTAI_DEBUG
        {
            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
            std::ostream& os = pCivLog->getStream();
            os << "\nCompleted assign plots for city: " << narrow(pCity_->getName()) << " angry = " << pCity_->angryPopulation();
        }
#endif
    }

    PlotAssignmentSettings City::getPlotAssignmentSettings() const
    {
        return plotAssignmentSettings_;
    }

    void City::calcMaxOutputs_()
    {
        CityOptimiser opt(pCityData_);
        opt.optimise(NO_OUTPUT, CityOptimiser::Not_Set, false);
        maxOutputs_[OUTPUT_FOOD] = opt.getMaxFood();
        TotalOutputWeights outputWeights = makeOutputW(1, 4, 3, 3, 1, 1);
        for (int i = 1; i < NUM_OUTPUT_TYPES; ++i)
        {
            TotalOutputPriority priorities(makeTotalOutputSinglePriority((OutputTypes)i));
            opt.optimise<MixedWeightedTotalOutputOrderFunctor>(priorities, outputWeights, opt.getGrowthType(), false);
            maxOutputs_[i] = pCityData_->getOutput()[i];
        }
        optWeights_ = makeOutputW(3, 4, 3, 3, 1, 1);//opt.getMaxOutputWeights();
    }

    void City::calcImprovements_()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
        os << "\nCity::calcImprovements_: " << narrow(pCity_->getName()) << " turn = " << gGlobals.getGame().getGameTurn();
#endif

        CityImprovementManager& improvementManager = getImprovementManager(pCity_);
        improvementManager.simulateImprovements(optWeights_);

        improvementManager.logImprovements();

        flags_ &= ~NeedsImprovementCalcs;
    }

    void City::calcBuildings_()
    {
        const boost::shared_ptr<Player>& player = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner());
        player->getAnalysis()->getPlayerTactics()->selectBuildingTactics(*this);

        flags_ &= ~NeedsBuildingCalcs;
    }

    int City::getID() const
    {
        return pCity_->getID();
    }

    XYCoords City::getCoords() const
    {
        const CvPlot* pPlot = pCity_->getCityIndexPlot(0);
        return XYCoords(pPlot->getX(), pPlot->getY());
    }

    const CvCity* City::getCvCity() const
    {
        return pCity_;
    }

    const ConstructItem& City::getConstructItem() const
    {
        return constructItem_;
    }

    boost::tuple<UnitTypes, BuildingTypes, ProcessTypes, ProjectTypes> City::getBuild()
    {
        const boost::shared_ptr<Player>& player = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner());
        player->getAnalysis()->getPlayerTactics()->updateCityBuildingTactics(pCity_->getIDInfo());

        if (constructItem_.projectType != NO_PROJECT)
        {
            if (pCity_->getProjectProduction(constructItem_.projectType) > 0 && pCity_->getProductionNeeded(constructItem_.projectType) > 0)
            {
#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
                os << "\n" << narrow(pCity_->getName()) << " keeping build: " << constructItem_ << " production so far = "
                   << pCity_->getProjectProduction(constructItem_.projectType) << ", needed = " << pCity_->getProductionNeeded(constructItem_.projectType);
#endif
                return boost::make_tuple(NO_UNIT, NO_BUILDING, NO_PROCESS, constructItem_.projectType);
            }
        }

        if (flags_ & NeedsBuildingCalcs)
        {
            
            calcBuildings_();
        }

#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
        std::ostream& os = pCivLog->getStream();
#endif
        if (flags_ & NeedsBuildSelection)
        {
            constructItem_ = player->getAnalysis()->getPlayerTactics()->getBuildItem(*this);
#ifdef ALTAI_DEBUG
            os << "\n" << narrow(pCity_->getName()) << " calculated build: " << constructItem_;
#endif
        }

        if (constructItem_.buildingType != NO_BUILDING)
        {
            /*CityDataPtr pCityData = pCityData_->clone();
            std::vector<IProjectionEventPtr> events;
            pCityData->pushBuilding(constructItem_.buildingType);
            events.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pCityData, player->getAnalysis()->getBuildingInfo(constructItem_.buildingType))));
            events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent(pCityData)));

            ProjectionLadder buildingLadder = getProjectedOutput(*player, pCityData, 50, events);
#ifdef ALTAI_DEBUG
            os << "\n" << narrow(pCity_->getName()) << " projection2: ";
            buildingLadder.debug(os);
            os << ", delta = " << buildingLadder.getOutput() - currentOutputProjection_.getOutput();
#endif*/
            return boost::make_tuple(NO_UNIT, constructItem_.buildingType, NO_PROCESS, NO_PROJECT);
        }
        else if (constructItem_.unitType != NO_UNIT)
        {
            return boost::make_tuple(constructItem_.unitType, NO_BUILDING, NO_PROCESS, NO_PROJECT);
        }
        else if (constructItem_.processType != NO_PROCESS)
        {
            return boost::make_tuple(NO_UNIT, NO_BUILDING, constructItem_.processType, NO_PROJECT);
        }
        else if (constructItem_.projectType != NO_PROJECT)
        {
            return boost::make_tuple(NO_UNIT, NO_BUILDING, NO_PROCESS, constructItem_.projectType);
        }
        return boost::make_tuple(NO_UNIT, NO_BUILDING, NO_PROCESS, NO_PROJECT);
    }

    std::pair<BuildTypes, int> City::getBestImprovement(XYCoords coords, const std::string& sourceFunc)
    {
        // don't return a different coordinate's build order here (so no irrigation chaining)
        const CityImprovementManager& improvementManager = getImprovementManager(pCity_);

        ImprovementTypes improvementType = improvementManager.getBestImprovementNotBuilt(coords);
        if (improvementType != NO_IMPROVEMENT)
        {
            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(improvementType);
            BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(improvementType);
            ImprovementTypes currentImprovement = gGlobals.getMap().plot(coords.iX, coords.iY)->getImprovementType();

            if (!improvementInfo.isRequiresIrrigation() && getBaseImprovement(improvementType) != getBaseImprovement(currentImprovement) && 
                !gGlobals.getMap().plot(coords.iX, coords.iY)->canBuild(buildType, pCity_->getOwner()))
            {
#ifdef ALTAI_DEBUG
                {   // debug
                    boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                    std::ostream& os = pCivLog->getStream();
                    os << "\n" << narrow(pCity_->getName()) << " getBestImprovement(XYCoords): Set improvement can't be built: " << improvementInfo.getType() << " at: " << coords
                        << " current improvement = " << (currentImprovement == NO_IMPROVEMENT ? " none " : gGlobals.getImprovementInfo(currentImprovement).getType());
                }
#endif
                return std::make_pair(NO_BUILD, 0);
            }
            else
            {
                int rank = improvementManager.getRank(coords, optWeights_);

#ifdef ALTAI_DEBUG
                {   // debug
                    boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                    std::ostream& os = pCivLog->getStream();
                    os << "\n(" << sourceFunc << ") " << narrow(pCity_->getName()) << " getBestImprovement(XYCoords): Returned: " << gGlobals.getBuildInfo(buildType).getType()
                        << " with value: " << std::max<int>(1000, 10000 - rank * rank * 500) << " for: " << coords;
                }
#endif
                return std::make_pair(buildType, std::max<int>(1000, 10000 - rank * rank * 500));
            }
        }

#ifdef ALTAI_DEBUG
        //{   // debug
        //        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
        //        std::ostream& os = pCivLog->getStream();
        //        os << "\n(" << sourceFunc << ") getBestImprovement(XYCoords): " << narrow(pCity_->getName()) << " Returned no build";
        //}
#endif
        return std::make_pair(NO_BUILD, 0);
    }

    bool City::selectImprovement(CvUnit* pUnit, bool simulatedOnly)
    {
        std::pair<XYCoords, BuildTypes> coordsAndBuildType = getBestImprovement("selectImprovement", simulatedOnly);
        if (coordsAndBuildType.second != NO_BUILD)
        {
            CvPlot* pPlot = gGlobals.getMap().plot(coordsAndBuildType.first.iX, coordsAndBuildType.first.iY);
            
            if (pPlot->getSubArea() == pUnit->plot()->getSubArea())
            {
                CvSelectionGroup* pGroup = pUnit->getGroup();
                if (!pUnit->atPlot(pPlot))
                {
                    pGroup->pushMission(MISSION_MOVE_TO, pPlot->getX(), pPlot->getY(), 0, false, false, MISSIONAI_BUILD, pPlot);
                }

                std::vector<BuildTypes> additionalBuilds = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->addAdditionalPlotBuilds(pPlot, coordsAndBuildType.second);
                for (size_t i = 0, count = additionalBuilds.size(); i < count; ++i)
                {
                    pGroup->pushMission(MISSION_BUILD, additionalBuilds[i], -1, 0, (pGroup->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
                }

                return pGroup->getLengthMissionQueue() > 0;
            }
        }
        return false;
    }

    std::pair<XYCoords, BuildTypes> City::getBestImprovement(const std::string& sourceFunc, bool simulatedOnly)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
#endif
        const CityImprovementManager& improvementManager = getImprovementManager(pCity_);

        std::vector<boost::shared_ptr<PlotCond> > conditions;
        conditions.push_back(boost::shared_ptr<PlotCond>(new IsLand()));

        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes> bestImprovement;
        ImprovementTypes improvementType = NO_IMPROVEMENT;
        bool targetPlotHasWorkers = true;

        while (targetPlotHasWorkers)
        {
            bestImprovement = improvementManager.getBestImprovementNotBuilt(optWeights_, false, simulatedOnly, conditions);
            improvementType = boost::get<2>(bestImprovement);
            if (improvementType == NO_IMPROVEMENT)
            {
                break;
            }

            XYCoords coords = boost::get<0>(bestImprovement);
            const CvPlot* targetPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
            if (getNumWorkersAtPlot(targetPlot) < 1)
            {
                targetPlotHasWorkers = false;
            }
            else
            {
                conditions.push_back(boost::shared_ptr<PlotCond>(new IgnorePlot(targetPlot)));
            }
        }

        if (improvementType != NO_IMPROVEMENT && !targetPlotHasWorkers)
        {
            XYCoords coords = boost::get<0>(bestImprovement);
            CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

            std::pair<XYCoords, BuildTypes> buildOrder = getImprovementBuildOrder_(boost::get<0>(bestImprovement), improvementType);

#ifdef ALTAI_DEBUG
            {   // debug
                os << "\n(" << sourceFunc << ") " << narrow(pCity_->getName()) << " getBestImprovement(): Returned build of: "
                   << (buildOrder.second == NO_BUILD ? "(NO_BUILD)" : gGlobals.getBuildInfo(buildOrder.second).getType())
                   << " for: " << boost::get<0>(bestImprovement)
                   << " improvement = " << gGlobals.getImprovementInfo(improvementType).getType()
                   << " rank = " << improvementManager.getRank(boost::get<0>(bestImprovement), optWeights_);
                if (buildOrder.second == NO_BUILD)
                {
                    os << " plot danger = " << CvPlayerAI::getPlayer(pCity_->getOwner()).AI_getPlotDanger(pPlot, 2);
                }
            }
#endif
            return buildOrder;
        }

        std::pair<XYCoords, RouteTypes> coordsAndRouteType = improvementManager.getBestRoute();
        if (coordsAndRouteType.second != NO_ROUTE)
        {
#ifdef ALTAI_DEBUG
            os << "\n(" << sourceFunc << ") getBestImprovement(): " << narrow(pCity_->getName()) << " returning build route at: "
               << coordsAndRouteType.first << " for route type: " << gGlobals.getRouteInfo(coordsAndRouteType.second).getType();
#endif
            return std::make_pair(coordsAndRouteType.first, GameDataAnalysis::getBuildTypeForRouteType(coordsAndRouteType.second));
        }

#ifdef ALTAI_DEBUG
        {   // debug
            os << "\n(" << sourceFunc << ") getBestImprovement(): " << narrow(pCity_->getName()) << " Returned no build";
        }
#endif
        return std::make_pair(XYCoords(), NO_BUILD);
    }

    std::pair<XYCoords, BuildTypes> City::getBestBonusImprovement(bool isWater)
    {
        if (flags_ & NeedsImprovementCalcs)
        {
            calcImprovements_();
        }

        const CityImprovementManager& improvementManager = getImprovementManager(pCity_);

        std::vector<boost::shared_ptr<PlotCond> > conditions;
        if (isWater)
        {
            conditions.push_back(boost::shared_ptr<PlotCond>(new IsWater()));
        }
        else
        {
            conditions.push_back(boost::shared_ptr<PlotCond>(new IsLand()));
        }
        conditions.push_back(boost::shared_ptr<PlotCond>(new HasBonus(pCity_->getTeam())));

        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes> bestImprovement;
        ImprovementTypes improvementType = NO_IMPROVEMENT;
        bool targetPlotHasWorkers = true;

        while (targetPlotHasWorkers)
        {
            bestImprovement = improvementManager.getBestImprovementNotBuilt(optWeights_, true, false, conditions);
            improvementType = boost::get<2>(bestImprovement);
            if (improvementType == NO_IMPROVEMENT)
            {
                break;
            }

            XYCoords coords = boost::get<0>(bestImprovement);
            const CvPlot* targetPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
            if (getNumWorkersAtPlot(targetPlot) < 1)
            {
                targetPlotHasWorkers = false;
            }
            else
            {
                conditions.push_back(boost::shared_ptr<PlotCond>(new IgnorePlot(targetPlot)));
            }
        }
        
        if (improvementType != NO_IMPROVEMENT && !targetPlotHasWorkers)
        {
            std::pair<XYCoords, BuildTypes> buildOrder = getImprovementBuildOrder_(boost::get<0>(bestImprovement), improvementType);

#ifdef ALTAI_DEBUG
            {   // debug
                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                std::ostream& os = pCivLog->getStream();
                os << "\ngetBestBonusImprovement(): " << narrow(pCity_->getName()) << " Returned build of: " << (buildOrder.second == NO_BUILD ? "(NO_BUILD)" : gGlobals.getBuildInfo(buildOrder.second).getType())
                   << " for: " << boost::get<0>(bestImprovement);
                if (buildOrder.second == NO_BUILD)
                {
                    XYCoords coords = boost::get<0>(bestImprovement);
                    CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                    os << " plot danger = " << CvPlayerAI::getPlayer(pCity_->getOwner()).AI_getPlotDanger(pPlot, 2);
                }
            }
#endif

            return buildOrder;
        }
        else
        {

#ifdef ALTAI_DEBUG
            //{   // debug
            //    boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
            //    std::ostream& os = pCivLog->getStream();
            //    os << "\ngetBestBonusImprovement(): " << narrow(pCity_->getName()) << " Returned no build";
            //}
#endif

            return std::make_pair(XYCoords(), NO_BUILD);
        }
    }

    std::pair<XYCoords, BuildTypes> City::getImprovementBuildOrder_(XYCoords coords, ImprovementTypes improvementType, bool wantIrrigationForBonus) const
    {
        CityImprovementManager& improvementManager = getImprovementManager(pCity_);

        CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
        BonusTypes bonusType = pPlot->getBonusType(pCity_->getTeam());

        const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(improvementType);

        if (!pPlot->canHaveImprovement(improvementType, pCity_->getTeam(), true))  // true to pass irrigation check
        {
            const PlotInfo::PlotInfoNode& plotNode = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getAnalysis()->getMapAnalysis()->getPlotInfoNode(pPlot);

            PlotInfo comparePlotInfo(pPlot, pCity_->getOwner());

            {   // log error
                boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                std::ostream& os = pErrorLog->getStream();
                os << "\nCity: " << narrow(pCity_->getName()) << " can't build improvement: " << improvementInfo.getType()
                   << " at: " << coords << " (Plot node = " << plotNode << "\n"
                   << "compare node key = " << comparePlotInfo.getKey() << "\n" << comparePlotInfo.getInfo();
            }

            return std::make_pair(XYCoords(), NO_BUILD);
        }

        // need to calc irrigation chain (todo - maybe not if have biology)
        bool markPlotAsPartOfIrrigationChain = false;
        if (improvementInfo.isRequiresIrrigation())
        {
            const bool isFreshWater = pPlot->isFreshWater();
            const bool isIrrigationAvailable = pPlot->isIrrigationAvailable(false);
            if (isIrrigationAvailable && !isFreshWater)
            {
                // if we return a different plot as part of a chain, that will be marked
                // we also want to mark the final plot (this one) if it's part of the chain - do that from here
                markPlotAsPartOfIrrigationChain = true;  
            }

            if (!isIrrigationAvailable || wantIrrigationForBonus)
            {
                // want irrigation, but it's not essential to build the improvement
                const bool hasBonusAndWantsIrrigation = bonusType != NO_BONUS && improvementInfo.isImprovementBonusMakesValid(bonusType) && !wantIrrigationForBonus;

                if (CvTeamAI::getTeam(pCity_->getTeam()).isIrrigation())  // can chain irrigation
                {
                    XYCoords buildCoords = improvementManager.getIrrigationChainPlot(coords, improvementType);
#ifdef ALTAI_DEBUG
                    {   // debug
                        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                        std::ostream& os = pCivLog->getStream();
                        os << "\n" << narrow(pCity_->getName()) << " Checking for irrigation path from: " << coords << " to: " << buildCoords;
                    }
#endif
                    if (buildCoords != XYCoords(-1, -1))  // found an irrigation source
                    {
                        pPlot = gGlobals.getMap().plot(buildCoords.iX, buildCoords.iY);
                        if (!CvPlayerAI::getPlayer(pCity_->getOwner()).AI_getPlotDanger(pPlot, 2))
                        {
#ifdef ALTAI_DEBUG
                            {   // debug
                                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                                std::ostream& os = pCivLog->getStream();
                                os << "\n" << narrow(pCity_->getName()) << " Found plot and requested build for irrigation path from: " << coords << " to: " << buildCoords;
                            }
#endif
                            //// if connecting farm on a bonus to irrigation, mark the bonus's farm as part of the chain (won't happen otherwise, as we built the bonus's farm earlier)
                            //// todo - consider whether to add the case of non-bonus farm being (re)-connected?
                            //if (wantIrrigationForBonus)
                            //{
                            //    // is build plot adjacent to plot we are trying to irrigate?
                            //    if (plotDistance(buildCoords.iX, buildCoords.iY, coords.iX, coords.iY) == 1)
                            //    {
                            //        std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();
                            //        for (size_t i = 0, count = improvements.size(); i < count; ++i)
                            //        {
                            //            if (boost::get<0>(improvements[i]) == coords)
                            //            {
                            //                boost::get<6>(improvements[i]) |= CityImprovementManager::IrrigationChainPlot;
                            //                break;
                            //            }
                            //        }
                            //    }
                            //}
                            return std::make_pair(buildCoords, GameDataAnalysis::getBuildTypeForImprovementType(improvementType));
                        }
                        else if (!hasBonusAndWantsIrrigation)
                        {
                            return std::make_pair(coords, NO_BUILD);
                        }
                    }
                    else if (!hasBonusAndWantsIrrigation)
                    {
                        {
                            boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                            std::ostream& os = pErrorLog->getStream();
                            os << "\n" << narrow(pCity_->getName()) << " Failed to find plot for irrigation path from: " << coords;
                        }
                        return std::make_pair(coords, GameDataAnalysis::getBuildTypeForImprovementType(improvementManager.getSubstituteImprovement(coords)));
                    }
                }
                // todo - why does canBuild not appear to work for unirrigated farms on bonuses?
                else if (!hasBonusAndWantsIrrigation && !pPlot->canBuild(GameDataAnalysis::getBuildTypeForImprovementType(improvementType)))
                {
#ifdef ALTAI_DEBUG
                    {
                        boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                        std::ostream& os = pErrorLog->getStream();
                        os << "\n" << narrow(pCity_->getName()) << " Requested build requiring chained irrigation at plot: " << coords << " without ability to chain irrigation";
                    }
#endif
                    return std::make_pair(XYCoords(), NO_BUILD);
                }
            }
        }

        if (CvPlayerAI::getPlayer(pCity_->getOwner()).AI_getPlotDanger(pPlot, 2))
        {
            return std::make_pair(XYCoords(), NO_BUILD);
        }

#ifdef ALTAI_DEBUG
        {   // debug
            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
            std::ostream& os = pCivLog->getStream();
            os << "\n" << narrow(pCity_->getName()) << " Requested build of: " << improvementInfo.getType() << " at: " << coords;
        }
#endif
        //// mark plot - as we got here, and didn't send worker to another plot to build part of the chain
        //if (markPlotAsPartOfIrrigationChain)
        //{
        //    std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();
        //    for (size_t i = 0, count = improvements.size(); i < count; ++i)
        //    {
        //        if (boost::get<0>(improvements[i]) == coords)
        //        {
        //            boost::get<6>(improvements[i]) |= CityImprovementManager::IrrigationChainPlot;
        //            break;
        //        }
        //    }
        //}
        return std::make_pair(coords, GameDataAnalysis::getBuildTypeForImprovementType(improvementType));
    }

    bool City::checkResourceConnections(CvUnitAI* pUnit) const
    {
        const CityImprovementManager& improvementManager = getConstImprovementManager(pCity_);
        const std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();
        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            if (boost::get<6>(improvements[i]) & CityImprovementManager::NeedsRoute)
            {
                XYCoords coords = boost::get<0>(improvements[i]);
                CvPlot* pImprovementPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

                if (getNumWorkersAtPlot(pImprovementPlot) > 0)
                {
                    continue;
                }

                if (pImprovementPlot->getSubArea() == pUnit->plot()->getSubArea() && !pImprovementPlot->isConnectedTo(pCity_))  // already checked plot is in same sub-area as city when improvement was flagged
                {
                    if (pUnit->generatePath(pImprovementPlot, MOVE_SAFE_TERRITORY, true))
					{
						if (pUnit->atPlot(pImprovementPlot))
						{
							pUnit->getGroup()->pushMission(MISSION_ROUTE_TO, pCity_->getX(), pCity_->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pImprovementPlot);
						}
						else
						{
							pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pImprovementPlot->getX(), pImprovementPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pImprovementPlot);
							pUnit->getGroup()->pushMission(MISSION_ROUTE_TO, pCity_->getX(), pCity_->getY(), MOVE_SAFE_TERRITORY, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pImprovementPlot);
						}

#ifdef ALTAI_DEBUG
                        {   // debug
                            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                            std::ostream& os = pCivLog->getStream();
                            BonusTypes bonusType = pImprovementPlot->getBonusType(pCity_->getTeam());
                            os << "\n" << narrow(pCity_->getName()) << " Requested connection of resource: " << (bonusType == NO_BONUS ? " none? " : gGlobals.getBonusInfo(bonusType).getType()) << " at: " << coords;
                        }
#endif
    					return true;
                    }
                }
            }
        }
        return false;
    }

    bool City::connectCities(CvUnitAI* pUnit) const
    {
        RouteTypes routeType = CvPlayerAI::getPlayer(pCity_->getOwner()).getBestRoute();
#ifdef ALTAI_DEBUG
        std::ostream& os = CityLog::getLog(pCity_)->getStream();
        os << "\nRoutes for city: " << narrow(pCity_->getName());
#endif
        FAStar* pSubAreaStepFinder = gDLL->getFAStarIFace()->create();
        CvMap& theMap = gGlobals.getMap();
        gDLL->getFAStarIFace()->Initialize(pSubAreaStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), subAreaStepDestValid, stepHeuristic, stepCost, subAreaStepValid, stepAdd, NULL, NULL);
        const int stepFinderInfo = MAKEWORD((short)pCity_->getOwner(), (short)SubAreaStepFlags::Team_Territory);

        FAStar& routeFinder = gGlobals.getRouteFinder();
        gDLL->getFAStarIFace()->ForceReset(&routeFinder);

        const int subAreaID = pCity_->plot()->getSubArea();
        XYCoords cityCoords(pCity_->plot()->getX(), pCity_->plot()->getY());        

        std::multimap<int, IDInfo> routeMap;
        CityIter iter(CvPlayerAI::getPlayer(pCity_->getOwner()));
        while (CvCity* pDestCity = iter())
        {
            if (pDestCity->getIDInfo() == pCity_->getIDInfo())
            {
                continue;
            }

            XYCoords destCityCoords(pDestCity->plot()->getX(), pDestCity->plot()->getY());
            if (pDestCity->plot()->getSubArea() == subAreaID)
            {
                bool haveRoute = false;
                int pathLength = 0, routeLength = 0;
                if (gDLL->getFAStarIFace()->GeneratePath(&routeFinder, cityCoords.iX, cityCoords.iY, destCityCoords.iX, destCityCoords.iY, false, pCity_->getOwner(), true))
                {
                    FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(&routeFinder);

                    // don't count starting city plot
                    while (pNode->m_pParent)
                    {
                        ++routeLength;
                        pNode = pNode->m_pParent;
                    }
                    haveRoute = true;
                }

                if (gDLL->getFAStarIFace()->GeneratePath(pSubAreaStepFinder, cityCoords.iX, cityCoords.iY, destCityCoords.iX, destCityCoords.iY, false, stepFinderInfo, true))
                {
                    FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pSubAreaStepFinder);
                    pathLength = pNode->m_iData1;
                }

                int minDistance = stepDistance(cityCoords.iX, cityCoords.iY, destCityCoords.iX, destCityCoords.iY);
#ifdef ALTAI_DEBUG
                os << "\nCity: " << narrow(pDestCity->getName()) << " route length = " << routeLength << ", path length = " << pathLength << ", step distance = " << minDistance << " route = " << haveRoute;
#endif
                //  select if no route, or route could be shorter (by at least 3)
                if (pathLength != 0 && (routeLength == 0 || pathLength + 2 < routeLength))
                {
                    int routeValue = routeLength == 0 ? pathLength : routeLength - pathLength;
                    routeMap.insert(std::make_pair(routeValue, pDestCity->getIDInfo()));
                }
            }
        }
#ifdef ALTAI_DEBUG
        for (std::multimap<int, IDInfo>::const_iterator ci(routeMap.begin()), ciEnd(routeMap.end()); ci != ciEnd; ++ci)
        {
            os << "\nCity: " << narrow(getCity(ci->second)->getName()) << " value = " << ci->first;
        }
#endif
        if (!routeMap.empty() && pUnit)
        {
            std::multimap<int, IDInfo>::const_iterator ci = routeMap.begin();

            while (ci != routeMap.end())
            {
                CvPlot* pTargetCityPlot = getCity(ci->second)->plot();

                if (pUnit->generatePath(pTargetCityPlot, MOVE_SAFE_TERRITORY, true))
			    {
                    // generate path from target, and find first plot which doesn't have the route type we want, and has no workers already on it
                    gDLL->getFAStarIFace()->GeneratePath(pSubAreaStepFinder, pTargetCityPlot->getX(), pTargetCityPlot->getY(), cityCoords.iX, cityCoords.iY, false, stepFinderInfo, true);
                    FAStarNode* pNode = gDLL->getFAStarIFace()->GetLastNode(pSubAreaStepFinder);
                    std::vector<CvPlot*> pTargetPlots;
                    while (pNode)
                    {
                        CvPlot* pPlot = gGlobals.getMap().plot(pNode->m_iX, pNode->m_iY);
                        if (pPlot->getRouteType() != routeType)
                        {
                            if (getNumWorkersAtPlot(pPlot) == 0)
                            {
                                pTargetPlots.push_back(pPlot);
                            }
                        }
                        pNode = pNode->m_pParent;
                    }

                    if (!pTargetPlots.empty())
                    {
                        for (size_t i = 0, count = pTargetPlots.size(); i < count; ++i)
                        {
                            if (i == 0)
                            {
                                if (!pUnit->atPlot(pTargetPlots[i]))
                                {
                                    pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pTargetPlots[i]->getX(), pTargetPlots[i]->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pTargetPlots[i]);
                                }
                            }

		    				pUnit->getGroup()->pushMission(MISSION_ROUTE_TO, pTargetPlots[i]->getX(), pTargetPlots[i]->getY(), MOVE_SAFE_TERRITORY, pUnit->getGroup()->getLengthMissionQueue() > 0, false, MISSIONAI_BUILD, pTargetPlots[i]);
#ifdef ALTAI_DEBUG
                            {   // debug
                                RouteTypes currentRouteType = pTargetPlots[i]->getRouteType();
                                os << "\n" << narrow(pCity_->getName()) << " Requested connection to city: " << narrow(getCity(ci->second)->getName())
                                   << " at plot: " << XYCoords(pTargetPlots[i]->getX(), pTargetPlots[i]->getY()) << " mission queue = " << pUnit->getGroup()->getLengthMissionQueue()
                                   << " for route type: " << gGlobals.getRouteInfo(routeType).getType()
                                   << " current route type: " << (currentRouteType == NO_ROUTE ? " NO_ROUTE" : gGlobals.getRouteInfo(currentRouteType).getType());
                            }
#endif
                        }
                        gDLL->getFAStarIFace()->destroy(pSubAreaStepFinder);
                        return true;
                    }
                }
                ++ci;
            }
        }

        gDLL->getFAStarIFace()->destroy(pSubAreaStepFinder);
        return false;
    }

    bool City::checkBadImprovementFeatures(CvUnitAI* pUnit) const
    {
        const CityImprovementManager& improvementManager = getConstImprovementManager(pCity_);
        const std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();
        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            if (boost::get<6>(improvements[i]) & CityImprovementManager::RemoveFeature)
            {
                const XYCoords coords = boost::get<0>(improvements[i]);
                CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

                FeatureTypes featureType = pPlot->getFeatureType();
                if (featureType == NO_FEATURE)
                {
                    continue;
                }

                if (getNumWorkersAtPlot(pPlot) > 0)
                {
                    continue;
                }

                BuildTypes buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureType);
                if (pPlot->canBuild(buildType, pCity_->getOwner()))
                {
					if (pUnit->atPlot(pPlot))
					{
                        pUnit->getGroup()->pushMission(MISSION_BUILD, buildType, -1, 0, false, false, MISSIONAI_BUILD, pPlot);
					}
					else
					{
						pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pPlot->getX(), pPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
						pUnit->getGroup()->pushMission(MISSION_BUILD, buildType, -1, 0, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
					}

#ifdef ALTAI_DEBUG
                    {   // debug
                        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                        std::ostream& os = pCivLog->getStream();
                        BonusTypes bonusType = pPlot->getBonusType(pCity_->getTeam());
                        os << "\n" << narrow(pCity_->getName()) << " Requested removal of feature: " << gGlobals.getFeatureInfo(featureType).getType() << " at: " << coords;
                        if (bonusType != NO_BONUS)
                        {
                            os << " bonus = " << gGlobals.getBonusInfo(bonusType).getType();
                        }
                    }
#endif
    				return true;
                }
            }
        }
        return false;
    }

    bool City::checkIrrigation(CvUnitAI* pUnit, bool onlyForResources) const
    {
        //boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
        //std::ostream& os = pCivLog->getStream();
        //os << "\nChecking city: " << narrow(pCity_->getName()) << " for missing irrigation.";

        const CityImprovementManager& improvementManager = getConstImprovementManager(pCity_);
        const std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();
        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            if (boost::get<6>(improvements[i]) & CityImprovementManager::NeedsIrrigation)
            {
                const XYCoords coords = boost::get<0>(improvements[i]);
                CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

                BonusTypes bonusType = pPlot->getBonusType(pCity_->getTeam());
                if (bonusType == NO_BONUS && onlyForResources)
                {
                    continue;
                }

                ImprovementTypes improvementType = boost::get<2>(improvements[i]);

                std::pair<XYCoords, BuildTypes> coordsAndBuildType = getImprovementBuildOrder_(coords, improvementType, bonusType == NO_BONUS ? true : false);

                if (coordsAndBuildType.first != XYCoords(-1, -1))
                {
                    CvPlot* pBuildPlot = gGlobals.getMap().plot(coordsAndBuildType.first.iX, coordsAndBuildType.first.iY);
                    if (getNumWorkersAtPlot(pBuildPlot) > 1)  // irrigation chains can take some work - so allow up to two workers
                    {
                        continue;
                    }

                    if (pBuildPlot->canBuild(coordsAndBuildType.second, pCity_->getOwner()))
                    {
					    if (pUnit->atPlot(pBuildPlot))
					    {
                            pUnit->getGroup()->pushMission(MISSION_BUILD, coordsAndBuildType.second, -1, 0, false, false, MISSIONAI_BUILD, pBuildPlot);
					    }
    					else
	    				{
		    				pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBuildPlot->getX(), pBuildPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pBuildPlot);
			    			pUnit->getGroup()->pushMission(MISSION_BUILD, coordsAndBuildType.second, -1, 0, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBuildPlot);
				    	}

#ifdef ALTAI_DEBUG
                        {   // debug
                            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                            std::ostream& os = pCivLog->getStream();
                            os << "\n" << narrow(pCity_->getName()) << " Requested irrigation connection at: " << coordsAndBuildType.first << " for plot: " << coords;
                            if (bonusType != NO_BONUS)
                            {
                                os << ", bonus = " << gGlobals.getBonusInfo(bonusType).getType();
                            }
                        }
#endif
    				    return true;
                    }
                }
            }
        }
        return false;
    }

    int City::getNumReqdWorkers() const
    {
#ifdef ALTAI_DEBUG
        { // debug
            boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
            std::ostream& os = pCityLog->getStream();
            os << "\nImprovements not built count = " << getImprovementManager(pCity_).getNumImprovementsNotBuilt()
               << " Worker build count = " << getNumWorkers();
        }
#endif
        return std::max<int>(0, std::min<int>(3, getImprovementManager(pCity_).getNumImprovementsNotBuilt()) - getNumWorkers());
    }

    int City::getNumWorkers() const
    {
        int count = 0;
        const CityImprovementManager& improvementManager = getConstImprovementManager(pCity_);

        SelectionGroupIter iter(CvPlayerAI::getPlayer(pCity_->getOwner()));
        CvSelectionGroup* pGroup = NULL;

        while (pGroup = iter())
        {
            bool potentialWorkerGroup = false;
            bool idleWorkers = false;
            const CvPlot* pPlot = NULL;
            if (pGroup->AI_getMissionAIType() == NO_MISSIONAI)
            {
                pPlot = pGroup->plot();
                if (pPlot && pPlot->getWorkingCity() == pCity_)
                {
                    potentialWorkerGroup = true;
                    idleWorkers = true;
                }
            }
            else
            {
                pPlot = pGroup->AI_getMissionAIPlot();
                if (pPlot && pGroup->AI_getMissionAIType() == MISSIONAI_BUILD)
                {
                    // means we are managing improvements for this plot (although won't count irrigation chain plots which may originate elsewhere anyway)
                    if (improvementManager.getBestImprovementNotBuilt(XYCoords(pPlot->getX(), pPlot->getY())) != NO_IMPROVEMENT)
                    {
                        potentialWorkerGroup = true;
                    }
                }
            }
            if (potentialWorkerGroup)
            {
                int thisCount = 0;
                UnitGroupIter unitIter(pGroup);
                const CvUnit* pUnit = NULL;
                while (pUnit = unitIter())
                {
                    if (pUnit->AI_getUnitAIType() == UNITAI_WORKER)
                    {
                        ++thisCount;
                    }
                }
                count += thisCount;

#ifdef ALTAI_DEBUG
                // debug
                if (thisCount > 0)
                {
                    boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
                    std::ostream& os = pCityLog->getStream();
                    os << "\nFound: " << thisCount << (idleWorkers ? " idle " : "") << " workers at: " << XYCoords(pPlot->getX(), pPlot->getY());
                }
#endif
            }
        }
        return count;
    }

    int City::getNumWorkersAtPlot(const CvPlot* pTargetPlot) const
    {
        int count = 0;
        const CityImprovementManager& improvementManager = getConstImprovementManager(pCity_);

        SelectionGroupIter iter(CvPlayerAI::getPlayer(pCity_->getOwner()));
        CvSelectionGroup* pGroup = NULL;

        while (pGroup = iter())
        {
            bool potentialWorkerGroup = false;
            bool idleWorkers = false;
            const CvPlot* pPlot = NULL;
            if (pGroup->AI_getMissionAIType() == NO_MISSIONAI)
            {
                pPlot = pGroup->plot();
                if (pPlot && pTargetPlot == pPlot)
                {
                    potentialWorkerGroup = true;
                    idleWorkers = true;
                }
            }
            else
            {
                pPlot = pGroup->AI_getMissionAIPlot();
                if (pPlot && pTargetPlot == pPlot && pGroup->AI_getMissionAIType() == MISSIONAI_BUILD)
                {
                    // means we are managing improvements for this plot (although won't count irrigation chain plots which may originate elsewhere anyway)
                    if (improvementManager.getBestImprovementNotBuilt(XYCoords(pPlot->getX(), pPlot->getY())) != NO_IMPROVEMENT)
                    {
                        potentialWorkerGroup = true;
                    }
                }
            }
            if (potentialWorkerGroup)
            {
                int thisCount = 0;
                UnitGroupIter unitIter(pGroup);
                const CvUnit* pUnit = NULL;
                while (pUnit = unitIter())
                {
                    if (pUnit->AI_getUnitAIType() == UNITAI_WORKER)
                    {
                        ++thisCount;
                    }
                }
                count += thisCount;

#ifdef ALTAI_DEBUG
                // debug
                if (thisCount > 0)
                {
                    boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
                    std::ostream& os = pCityLog->getStream();
                    os << "\nFound: " << thisCount << (idleWorkers ? " idle " : "") << " workers at: " << XYCoords(pPlot->getX(), pPlot->getY());
                }
#endif
            }
        }
        return count;
    }

    TotalOutput City::getMaxOutputs() const
    {
        return maxOutputs_;
    }

    TotalOutputWeights City::getMaxOutputWeights() const
    {
        return optWeights_;
    }

    const CityDataPtr& City::getCityData() const
    {
        return pCityData_;
    }

    const ProjectionLadder& City::getCurrentOutputProjection() const
    {
        return currentOutputProjection_;
    }

    void City::write(FDataStreamBase* pStream) const
    {
        constructItem_.write(pStream);
        maxOutputs_.write(pStream);
        writeArray(pStream, optWeights_);
    }

    void City::read(FDataStreamBase* pStream)
    {
        constructItem_.read(pStream);
        maxOutputs_.read(pStream);
        readArray(pStream, optWeights_);
    }

    void City::debugGreatPeople_()
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
        CityOptimiser opt(pCityData_);

        std::set<UnitTypes> unitTypes = pCityData_->getAvailableGPTypes();
        for (std::set<UnitTypes>::const_iterator ci(unitTypes.begin()), ciEnd(unitTypes.end()); ci != ciEnd; ++ci)
        {
            opt.optimise(*ci, CityOptimiser::FlatGrowth);
            pCityLog->getStream() << "\nOpt output for: " << gGlobals.getUnitInfo(*ci).getType();
            pCityLog->logPlots(opt);
        }
#endif
    }

    void City::logImprovements() const
    {
#ifdef ALTAI_DEBUG
        getImprovementManager(pCity_).logImprovements();
#endif
    }
}