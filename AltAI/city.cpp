#include "AltAI.h"

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
#include "./spec_info_visitors.h"
#include "./helper_fns.h"
#include "./tactic_streams.h"
#include "./civ_log.h"
#include "./error_log.h"
#include "./save_utils.h"
#include "./city_projections.h"
#include "./city_improvement_projections.h"
#include "./modifiers_helper.h"

#include "../CvGameCoreDLL/CvDLLEngineIFaceBase.h"
#include "../CvGameCoreDLL/CvDLLFAStarIFaceBase.h"
#include "../CvGameCoreDLL/CvGameCoreUtils.h"
#include "../CvGameCoreDLL/FAStarNode.h"

#include <sstream>

namespace AltAI
{
    City::City(CvCity* pCity)
        : player_(*gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())), pCity_(pCity), 
          pCityImprovementManager_(CityImprovementManagerPtr(new CityImprovementManager(pCity->getIDInfo()))),
          constructItem_(NO_BUILDING), flags_(0)
    {        
    }

    // called when found a new city only
    void City::init()
    {
        flags_ |= NeedsCityDataCalc;
        flags_ |= NeedsImprovementCalcs;
        flags_ |= NeedsBuildSelection;
        pCity_->AI_setAssignWorkDirty(true);

        updateProjections_();

        // set flag to update other cities as maintenance costs will have changed
        CityIter iter(CvPlayerAI::getPlayer(pCity_->getOwner()));
        while (CvCity* pCity = iter())
        {
            if (pCity->getIDInfo() == pCity_->getIDInfo())
            {
                continue;
            }
            player_.getCity(pCity).setFlag(NeedsProjectionCalcs | NeedsCityDataCalc);
        }
    }

    void City::doTurn()
    {
        if (!pCityImprovementManager_)
        {
            pCityImprovementManager_ = CityImprovementManagerPtr(new CityImprovementManager(pCity_->getIDInfo()));
            //ErrorLog::getLog(*player_.getCvPlayer())->getStream() << "\nnew CityImprovementManager at: " << pCityImprovementManager_.get();
        }

        calcMaxOutputs_();
        logImprovements();

        setFlag(CanReassignSharedPlots);

        if (flags_ & NeedsImprovementCalcs)
        {
            calcImprovements();
        }
    }

    void City::setFlag(int flags)
    {
        flags_ |= flags;
    }

    int City::getFlags() const
    {
        return flags_;
    }

    void City::updateBuildings(BuildingTypes buildingType, int count)
    {
#ifdef ALTAI_DEBUG
        {
            boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
            std::ostream& os = pCityLog->getStream();
            os << "\nTurn = " << gGlobals.getGame().getGameTurn() << __FUNCTION__;
            os << " Building " << (count > 0 ? "built: " : "lost: ") << gGlobals.getBuildingInfo(buildingType).getType();
        }
#endif
        BuildingClassTypes buildingClassType = getBuildingClass(buildingType);
        const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = isNationalWonderClass(buildingClassType);
        if (!isWorldWonder && !isNationalWonder)
        {
            gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getAnalysis()->getPlayerTactics()->
                updateCityBuildingTactics(pCity_->getIDInfo(), buildingType, count);
        }

        setFlag(NeedsBuildSelection);
    }

    void City::updateUnits(UnitTypes unitType)
    {
#ifdef ALTAI_DEBUG
        {
            boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
            std::ostream& os = pCityLog->getStream();
            os << "\nTurn = " << gGlobals.getGame().getGameTurn() << __FUNCTION__;
            os << " Unit constructed: " << gGlobals.getUnitInfo(unitType).getType();
        }
#endif
        setFlag(NeedsBuildSelection);
    }

    void City::updateImprovements(const CvPlot* pPlot, ImprovementTypes improvementType)
    {
        // can be called when a city is acquired before we have called init()
        if (pCityImprovementManager_ && pCityImprovementManager_->updateImprovements(pPlot, improvementType, pPlot->getFeatureType(), pPlot->getRouteType()))
        {
            setFlag(NeedsImprovementCalcs);
        }
    }

    void City::updateRoutes(const CvPlot* pPlot, RouteTypes routeType)
    {
        // can be called when a city is acquired before we have called init()
        if (pCityImprovementManager_)
        {
            // update plot even if we didn't find it (could be building a road on a plot to link a bonus up, but that plot has no imp selected)
            pCityImprovementManager_->updateImprovements(pPlot, pPlot->getImprovementType(), pPlot->getFeatureType(), routeType);
            setFlag(NeedsImprovementCalcs);
        }
    }

    void City::optimisePlots(const CityDataPtr& pCityData, const ConstructItem& constructItem, bool debug) const
    {
#ifdef ALTAI_DEBUG
        //boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
        boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
        std::ostream& os = pCityLog->getStream();
#endif

        CityOptimiser opt(pCityData, std::make_pair(maxOutputs_, optWeights_));
        bool isFoodProduction = constructItem.unitType != NO_UNIT && pCity_->isFoodProduction(constructItem.unitType);
        const int warPlanCount = CvTeamAI::getTeam(pCity_->getTeam()).getAnyWarPlanCount(true);
        const int atWarCount = CvTeamAI::getTeam(pCity_->getTeam()).getAtWarCount(true);

        if (!isFoodProduction)
        {            
            PlotAssignmentSettings plotAssignmentSettings = makePlotAssignmentSettings(pCityData, pCity_, constructItem);
#ifdef ALTAI_DEBUG
            if (debug)
            {
                os << "\n" << narrow(pCity_->getName()) << " plot assign settings: ";
                plotAssignmentSettings.debug(os);
                os << "\nmax outputs: " << maxOutputs_;
            }
#endif
            if (constructItem.processType != NO_PROCESS)
            {
                const CvProcessInfo& processInfo = gGlobals.getProcessInfo(constructItem.processType);

                CommerceModifier modifier;
                for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
                {
                    modifier[i] = processInfo.getProductionToCommerceModifier(i);
                }

                opt.optimise<ProcessValueAdaptorFunctor<MixedWeightedTotalOutputOrderFunctor>, MixedWeightedTotalOutputOrderFunctor>
                    (plotAssignmentSettings.outputPriorities, plotAssignmentSettings.outputWeights, plotAssignmentSettings.growthType, modifier, false);
            }
            else
            {
                //if (!player_.getCvPlayer()->isHuman())
                {
                    std::vector<TotalOutputPriority> outputPriorities;
                    std::vector<OutputTypes> outputTypes;
                    std::vector<SpecialistTypes> mixedSpecialistTypes;

                    if (pCityData->getPopulation() > 3)
                    {
                        if (constructItem_.buildingType != NO_BUILDING && isWorldWonderClass(getBuildingClass(constructItem_.buildingType)))
                        {
                            outputTypes.clear();
                            outputTypes.push_back(OUTPUT_PRODUCTION);
                            outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));
                        }
                        else if (constructItem_.unitType != NO_UNIT && warPlanCount > 0 || atWarCount > 0)
                        {
                            outputTypes.clear();
                            outputTypes.push_back(OUTPUT_PRODUCTION);
                            outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));
                        }
                        else
                        {
                            outputTypes.clear();
                            outputTypes.push_back(OUTPUT_PRODUCTION);                
                            outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));

                            outputTypes.clear();
                            outputTypes.push_back(OUTPUT_FOOD);
                            outputTypes.push_back(OUTPUT_RESEARCH);
                            outputTypes.push_back(OUTPUT_GOLD);
                            outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));

                            int requiredYield = 100 * (pCityData->getPopulation() - std::max<int>(0, pCityData->angryPopulation() - pCityData->happyPopulation())) * gGlobals.getFOOD_CONSUMPTION_PER_POPULATION() + pCityData->getLostFood();
                            if (maxOutputs_[OUTPUT_FOOD] > requiredYield + 3 * gGlobals.getFOOD_CONSUMPTION_PER_POPULATION() * 100)
                            {
                                if (pCityData->getSpecialistSlotCount() > 0 && player_.getCvPlayer()->getNumCities() > 2)
                                {
                                    mixedSpecialistTypes = pCityData->getBestMixedSpecialistTypes();
#ifdef ALTAI_DEBUG
                                    if (debug)
                                    {
                                        os << " setting GPP flag, best mixed specialists: ";
                                        for (size_t specIndex = 0; specIndex < mixedSpecialistTypes.size(); ++specIndex)
                                        {
                                            os << (mixedSpecialistTypes[specIndex] == NO_SPECIALIST ? " none" : gGlobals.getSpecialistInfo(mixedSpecialistTypes[specIndex]).getType()) << " ";
                                        }
                                    }
#endif
                                }
                            }
                        }
                    }
                    else
                    {
                        outputTypes.clear();
                        outputTypes.push_back(OUTPUT_FOOD);
                        outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));

                        outputTypes.clear();
                        outputTypes.push_back(OUTPUT_PRODUCTION);
                        outputTypes.push_back(OUTPUT_GOLD);
                        outputTypes.push_back(OUTPUT_FOOD);
                        outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));
                    }

                    opt.optimise(outputPriorities, mixedSpecialistTypes, false);
                }
                /*else  // isHuman - allow emphasize buttons to do something
                {
                    std::vector<OutputTypes> emphasizedOutputTypes;
                    std::vector<TotalOutputPriority> outputPriorities;
                    std::vector<SpecialistTypes> mixedSpecialistTypes;

                    for (int i = 0; i < NUM_YIELD_TYPES - 1; ++i)
                    {
                        if (((CvCityAI*)pCity_)->AI_isEmphasizeYield((YieldTypes)i))
                        {
                            emphasizedOutputTypes.push_back((OutputTypes)i);
                        }
                    }

                    for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
                    {
                        if (((CvCityAI*)pCity_)->AI_isEmphasizeCommerce((CommerceTypes)i))
                        {
                            emphasizedOutputTypes.push_back((OutputTypes)(i + NUM_YIELD_TYPES - 1));
                        }
                    }

                    if (((CvCityAI*)pCity_)->AI_isEmphasizeYield(YIELD_COMMERCE))
                    {
                        if (!((CvCityAI*)pCity_)->AI_isEmphasizeCommerce(COMMERCE_RESEARCH))
                        {
                            emphasizedOutputTypes.push_back(OUTPUT_RESEARCH);
                        }
                        if (!((CvCityAI*)pCity_)->AI_isEmphasizeCommerce(COMMERCE_GOLD))
                        {
                            emphasizedOutputTypes.push_back(OUTPUT_GOLD);
                        }
                    }

                    if (emphasizedOutputTypes.empty())
                    {
                        std::vector<OutputTypes> outputTypes;

                        if (pCity_->getPopulation() > 3)
                        {
                            outputTypes.push_back(OUTPUT_PRODUCTION);                
                            outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));

                            outputTypes.clear();
                            outputTypes.push_back(OUTPUT_FOOD);
                            outputTypes.push_back(OUTPUT_RESEARCH);
                            outputTypes.push_back(OUTPUT_GOLD);
                            outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));
                        }
                        else
                        {
                            outputTypes.push_back(OUTPUT_FOOD);
                            outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));

                            outputTypes.clear();
                            outputTypes.push_back(OUTPUT_PRODUCTION);
                            outputTypes.push_back(OUTPUT_GOLD);
                            outputTypes.push_back(OUTPUT_FOOD);
                            outputPriorities.push_back(makeTotalOutputPriorities(outputTypes));
                        }
                    }
                    else
                    {
                        outputPriorities.push_back(makeTotalOutputPriorities(emphasizedOutputTypes));
                    }

                    if (((CvCityAI*)pCity_)->AI_isEmphasizeGreatPeople())
                    {
                        std::vector<OutputTypes> outputTypes;

                        if (emphasizedOutputTypes.empty())
                        {
                            outputTypes.push_back(OUTPUT_PRODUCTION);
                            outputTypes.push_back(OUTPUT_RESEARCH);
                            outputTypes.push_back(OUTPUT_GOLD);
                        }
                        else
                        {
                            outputTypes = emphasizedOutputTypes;
                        }

                        TotalOutputWeights outputWeights = makeOutputW(1, 1, 1, 1, 1, 1);
                        YieldModifier yieldModifier = pCityData->getModifiersHelper()->getTotalYieldModifier(*pCityData);
                        CommerceModifier commerceModifier = makeCommerce(100, 100, 100, 100);

                        mixedSpecialistTypes = AltAI::getBestSpecialists(player_, yieldModifier, commerceModifier, 4, MixedWeightedOutputOrderFunctor<TotalOutput>(makeTotalOutputPriorities(outputTypes), outputWeights));
#ifdef ALTAI_DEBUG
                        if (debug)
                        {
                            os << " setting GPP flag, best mixed specialists: ";
                            for (size_t specIndex = 0; specIndex < mixedSpecialistTypes.size(); ++specIndex)
                            {
                                os << (mixedSpecialistTypes[specIndex] == NO_SPECIALIST ? " none" : gGlobals.getSpecialistInfo(mixedSpecialistTypes[specIndex]).getType()) << " ";
                            }
                        }
#endif
                    }

                    opt.optimise(outputPriorities, mixedSpecialistTypes, false);
                }*/
            }
            //else if (plotAssignmentSettings_.targetFoodYield != Range<>())
            //{
            //    opt.optimise<MixedWeightedTotalOutputOrderFunctor>(plotAssignmentSettings_.outputPriorities, plotAssignmentSettings_.outputWeights, plotAssignmentSettings_.targetFoodYield, true);
            //}
            //else
            //{
            //    opt.optimise<MixedWeightedTotalOutputOrderFunctor>(plotAssignmentSettings_.outputPriorities, plotAssignmentSettings_.outputWeights, plotAssignmentSettings_.growthType, true);
            //    //opt.optimise<MixedTotalOutputOrderFunctor>(plotAssignmentSettings_.outputPriorities, plotAssignmentSettings_.outputWeights, plotAssignmentSettings_.growthType, false);
            //}
        }
        else
        {
            opt.optimiseFoodProduction(constructItem.unitType, false);
#ifdef ALTAI_DEBUG
            if (debug)
            {
                pCityData->debugBasicData(os);
            }
#endif
        }
    }

    void City::assignPlots() // designed to be called from CvCityAI::AI_assignWorkingPlots()
    {
#ifdef ALTAI_DEBUG
        //boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
        boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
        std::ostream& os = pCityLog->getStream();
#endif
        calcMaxOutputs_();

        plotAssignmentSettings_.growthType = CityOptimiser::Not_Set;
        
        CityDataPtr pCityData = getCityData();

        checkConstructItem_();

        optimisePlots(pCityData, constructItem_, true);

        std::map<SpecialistTypes, int> specCounts;

        // city plot is not in plot data list
        pCity_->setWorkingPlot(CITY_HOME_PLOT, pCity_->getPopulation() > 0 && pCity_->canWork(pCity_->getCityIndexPlot(CITY_HOME_PLOT)));

        for (PlotDataListConstIter iter(pCityData->getPlotOutputs().begin()), endIter(pCityData->getPlotOutputs().end()); iter != endIter; ++iter)
        {
            if (iter->isWorked)
            {
                if (iter->isActualPlot())
                {
                    CvPlot* pPlot = gGlobals.getMap().plot(iter->coords.iX, iter->coords.iY);

                    // error check
                    if (pPlot->getWorkingCity() != NULL && pPlot->getWorkingCity() != pCity_)
                    {
                        std::ostream& os = ErrorLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
                        os << "\nTurn: " << gGlobals.getGame().getGameTurn() << " plot: " << iter->coords << " has conflicting working city settings: plot's = " 
                           << safeGetCityName(pPlot->getWorkingCity()) << " and city is: " << safeGetCityName(pCity_) << ")";
                    }

                    pCity_->setWorkingPlot(pPlot, true);  // all plots were set to not worked in AI_assignWorkingPlots before this call
                }
                else
                {
                    SpecialistTypes specialistType = (SpecialistTypes)iter->coords.iY;
                    ++specCounts[specialistType];
                }
            }
        }

        for (PlotDataListConstIter iter(pCityData->getFreeSpecOutputs().begin()), endIter(pCityData->getFreeSpecOutputs().end()); iter != endIter; ++iter)
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
            //gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->getAnalysis()->getMapAnalysis()->reassignUnworkedSharedPlots(pCity_->getIDInfo());
            flags_ &= ~CanReassignSharedPlots;
        }

        // update projection
        {
            const PlayerPtr& player = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner());

#ifdef ALTAI_DEBUG
            {
                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                std::ostream& os = pCivLog->getStream();

                os << "\nFound " << player->getNumWorkersTargetingCity(pCity_) << " workers targeting city " << narrow(pCity_->getName());

                /*std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > > missions = player->getWorkerMissionsForCity(pCity_);
                for (size_t i = 0, count = missions.size(); i < count; ++i)
                {
                    os << "\nUnit: " << gGlobals.getUnitInfo(missions[i].first).getType();
                    for (size_t j = 0, count = missions[i].second.size(); j < count; ++j)
                    {
                        missions[i].second[j].debug(os);
                    }
                }*/
            }
#endif            
            //std::vector<IUnitEventGeneratorPtr> unitEventGenerators = player->getCityUnitEvents(pCity_->getIDInfo());
            //for (size_t unitEventIndex = 0, unitEventCount = unitEventGenerators.size(); unitEventIndex < unitEventCount; ++unitEventIndex)
            //{
            //    events.push_back(unitEventGenerators[unitEventIndex]->getProjectionEvent(pCityData));
            //}

            updateProjections_();
        }

#ifdef ALTAI_DEBUG
        {
            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
            std::ostream& os = pCivLog->getStream();
            os << "\nTurn = " << gGlobals.getGame().getGameTurn() << " Completed assign plots for city: " << narrow(pCity_->getName()) << " angry = " << pCity_->angryPopulation() << " ";
            pCityData->debugBasicData(os);
            plotAssignmentSettings_.debug(os);
            os << "\nPlots: ";
            pCityData->debugSummary(os);
            os << "\nConstructing: ";
            constructItem_.debug(os);
            os << "\nCurrent output projection: ";
            currentOutputProjection_.debug(os);
        }
#endif
    }

    PlotAssignmentSettings City::getPlotAssignmentSettings() const
    {
        return plotAssignmentSettings_;
    }

    void City::updateProjections_()
    {
        const PlayerPtr& player = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner());
        const int numSimTurns = player->getAnalysis()->getNumSimTurns();
        std::vector<IProjectionEventPtr> events;

        CityDataPtr pCityData = getCityData();

        events.push_back(IProjectionEventPtr(new WorkerBuildEvent(player->getWorkerMissionsForCity(pCity_))));
        pProjectionCityData_ = pCityData->clone();
        currentOutputProjection_ = getProjectedOutput(*player, pProjectionCityData_, numSimTurns, events, constructItem_, __FUNCTION__, false, true);

        events.clear();
        pBaseProjectionCityData_ = pCityData->clone();
        baseOutputProjection_ = getProjectedOutput(*player, pBaseProjectionCityData_, numSimTurns, events, ConstructItem(), __FUNCTION__, false, true);

        flags_ &= ~NeedsProjectionCalcs;
    }

    void City::calcMaxOutputs_()
    {
        CityDataPtr pCityData = getCityData();
        CityOptimiser opt(pCityData);
        opt.optimise(NO_OUTPUT, CityOptimiser::Not_Set, true);
        maxOutputs_[OUTPUT_FOOD] = opt.getMaxFood();
        TotalOutputWeights outputWeights = makeOutputW(1, 4, 3, 3, 1, 1);
        for (int i = 1; i < NUM_OUTPUT_TYPES; ++i)
        {
            TotalOutputPriority priorities(makeTotalOutputSinglePriority((OutputTypes)i));
            opt.optimise<MixedWeightedTotalOutputOrderFunctor>(priorities, outputWeights, opt.getGrowthType(), false);
            maxOutputs_[i] = pCityData->getOutput()[i];
        }
        optWeights_ = makeOutputW(3, 4, 3, 3, 1, 1);//opt.getMaxOutputWeights();
    }

    void City::calcImprovements()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
        std::ostream& cityStream = CityLog::getLog(pCity_)->getStream();
        os << "\nCity::calcImprovements: " << narrow(pCity_->getName()) << " turn = " << gGlobals.getGame().getGameTurn();
        cityStream << "\nCity::calcImprovements" << " turn = " << gGlobals.getGame().getGameTurn();
#endif
        // careful - as can update imps before call doTurn() - which would recreate city data - happens when borders expand, incl post conquest
        CityDataPtr pCityData = getCityData();
        //std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);
        //const int targetSize = 3 + std::max<int>(pCity_->getPopulation(), pCity_->getPopulation() + pCity_->happyLevel() - pCity_->unhappyLevel());

        //pCityImprovementManager_->calcImprovements(pCityData, yieldTypes, targetSize);
        pCityImprovementManager_->simulateImprovements(pCityData);

#ifdef ALTAI_DEBUG
        pCityImprovementManager_->logImprovements(os);
        pCityImprovementManager_->logImprovements(cityStream);
#endif

        flags_ &= ~NeedsImprovementCalcs;
    }

    void City::checkConstructItem_()
    {
        if (pCity_->isProductionBuilding() && pCity_->getProductionBuilding() != constructItem_.buildingType)
        {
            constructItem_.buildingType = pCity_->getProductionBuilding();
        }
        else if(pCity_->isProductionUnit() && pCity_->getProductionUnit() != constructItem_.unitType)
        {
            constructItem_.unitType = pCity_->getProductionUnit();
        }
        else if(pCity_->isProductionProcess() && pCity_->getProductionProcess() != constructItem_.processType)
        {
            constructItem_.processType = pCity_->getProductionProcess();
        }
    }

    int City::getID() const
    {
        return pCity_->getID();
    }

    XYCoords City::getCoords() const
    {
        return pCity_->plot()->getCoords();
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
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
        std::ostream& os = pCivLog->getStream();
#endif
        const PlayerPtr& player = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner());
        player->getAnalysis()->getPlayerTactics()->updateCityBuildingTactics(pCity_->getIDInfo());

        if (constructItem_.projectType != NO_PROJECT)
        {
            if (pCity_->getProjectProduction(constructItem_.projectType) > 0 && pCity_->getProductionNeeded(constructItem_.projectType) > 0)
            {
#ifdef ALTAI_DEBUG
                os << "\n" << narrow(pCity_->getName()) << " keeping build: " << constructItem_ << " production so far = "
                   << pCity_->getProjectProduction(constructItem_.projectType) << ", needed = " << pCity_->getProductionNeeded(constructItem_.projectType);
#endif
                return boost::make_tuple(NO_UNIT, NO_BUILDING, NO_PROCESS, constructItem_.projectType);
            }
        }

        if (constructItem_.processType != NO_PROCESS)
        {
            setFlag(NeedsBuildSelection);
        }        
        else if (constructItem_.buildingType != NO_BUILDING && !pCity_->canConstruct(constructItem_.buildingType))
        {
            setFlag(NeedsBuildSelection);
        }
        else if (constructItem_.unitType != NO_UNIT && !pCity_->canTrain(constructItem_.unitType))
        {
            setFlag(NeedsBuildSelection);
        }

        if (flags_ & NeedsBuildSelection)
        {
            constructItem_ = player->getAnalysis()->getPlayerTactics()->getBuildItem(*this);
#ifdef ALTAI_DEBUG
            os << "\n" << narrow(pCity_->getName()) << " calculated build: " << constructItem_ << ", turn = " << gGlobals.getGame().getGameTurn();
            if (constructItem_.pUnitEventGenerator)
            {
                os << " with projection event: ";
                constructItem_.pUnitEventGenerator->getProjectionEvent(getCityData())->debug(os);
            }
#endif
            flags_ &= ~NeedsBuildSelection;
        }

        return constructItem_.getConstructItem();
    }

    std::pair<BuildTypes, int> City::getBestImprovement(XYCoords coords, const std::string& sourceFunc)
    {
        // don't return a different coordinate's build order here (so no irrigation chaining)
        ImprovementTypes improvementType = pCityImprovementManager_->getBestImprovementNotBuilt(coords);
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
                int rank = pCityImprovementManager_->getRank(coords);

#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
                {   // debug
                    os << "\n(" << sourceFunc << ") " << narrow(pCity_->getName()) << " getBestImprovement(XYCoords): Returned: " << gGlobals.getBuildInfo(buildType).getType()
                        << " with value: " << std::max<int>(1000, 10000 - rank * rank * 500) << " for: " << coords;
                }
                if (sourceFunc != "AI_updateBestBuild")
                {
                    player_.debugWorkerMissions(os);
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

    bool City::selectImprovement(CvUnitAI* pUnit, bool simulatedOnly)
    {
        std::pair<XYCoords, BuildTypes> coordsAndBuildType = getBestImprovement("selectImprovement", pUnit, simulatedOnly);
        if (coordsAndBuildType.second != NO_BUILD)
        {
            CvPlot* pPlot = gGlobals.getMap().plot(coordsAndBuildType.first.iX, coordsAndBuildType.first.iY);
            
            CvSelectionGroup* pGroup = pUnit->getGroup();
            if (!pUnit->atPlot(pPlot))
            {
                //pGroup->pushMission(MISSION_MOVE_TO, pPlot->getX(), pPlot->getY(), 0, false, false, MISSIONAI_BUILD, pPlot);
                player_.pushWorkerMission(pUnit, pCity_, pPlot, MISSION_MOVE_TO, NO_BUILD, 0, "City::selectImprovement");
            }

            std::vector<BuildTypes> additionalBuilds = gGlobals.getGame().getAltAI()->getPlayer(pCity_->getOwner())->addAdditionalPlotBuilds(pPlot, coordsAndBuildType.second);
            for (size_t i = 0, count = additionalBuilds.size(); i < count; ++i)
            {
                //pGroup->pushMission(MISSION_BUILD, additionalBuilds[i], -1, 0, (pGroup->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
                player_.pushWorkerMission(pUnit, pCity_, pPlot, MISSION_BUILD, additionalBuilds[i], 0, "City::selectImprovement");
            }

            return pGroup->getLengthMissionQueue() > 0;
        }

        return false;
    }

    std::pair<XYCoords, BuildTypes> City::getBestImprovement(const std::string& sourceFunc, CvUnitAI* pUnit, bool simulatedOnly)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
#endif
        std::vector<PlotCondPtr > conditions;
        conditions.push_back(PlotCondPtr(new IsLand()));

        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> bestImprovement;
        ImprovementTypes improvementType = NO_IMPROVEMENT;
        bool targetPlotHasWorkers = true;
        const CvPlot* pTargetPlot;

        while (targetPlotHasWorkers)
        {
            bestImprovement = pCityImprovementManager_->getBestImprovementNotBuilt(false, simulatedOnly, conditions);
            improvementType = boost::get<2>(bestImprovement);
            if (improvementType == NO_IMPROVEMENT)
            {
                break;
            }

            XYCoords coords = boost::get<0>(bestImprovement);
            pTargetPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

#ifdef ALTAI_DEBUG
            os << "\n(getBestImprovement) - Checking plot: " << coords << " for worker missions: " << getNumWorkersTargetingPlot(coords);
#endif

            if (pTargetPlot->getSubArea() == pUnit->plot()->getSubArea() && getNumWorkersTargetingPlot(coords) < 1)
            {
                targetPlotHasWorkers = false;
            }
            else
            {
                conditions.push_back(PlotCondPtr(new IgnorePlot(pTargetPlot)));
            }
        }

        if (improvementType != NO_IMPROVEMENT && !targetPlotHasWorkers)
        {
            XYCoords coords = boost::get<0>(bestImprovement);

            std::pair<XYCoords, BuildTypes> buildOrder = getImprovementBuildOrder_(coords, improvementType);

#ifdef ALTAI_DEBUG
            {   // debug
                os << "\n(" << sourceFunc << ") " << narrow(pCity_->getName()) << " getBestImprovement(): Returned build of: "
                   << (buildOrder.second == NO_BUILD ? "(NO_BUILD)" : gGlobals.getBuildInfo(buildOrder.second).getType())
                   << " for: " << boost::get<0>(bestImprovement)
                   << " improvement = " << gGlobals.getImprovementInfo(improvementType).getType()
                   << " rank = " << pCityImprovementManager_->getRank(boost::get<0>(bestImprovement));
                if (buildOrder.second == NO_BUILD)
                {
                    os << " plot danger = " << CvPlayerAI::getPlayer(pCity_->getOwner()).AI_getPlotDanger((CvPlot*)pTargetPlot, 2);
                }

                {
                    player_.debugWorkerMissions(os);
                }
            }
#endif
            return buildOrder;
        }

        std::pair<XYCoords, RouteTypes> coordsAndRouteType = pCityImprovementManager_->getBestRoute();
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
            calcImprovements();
        }

        //const CityImprovementManager& improvementManager = getImprovementManager(pCity_);

        std::vector<PlotCondPtr > conditions;
        if (isWater)
        {
            conditions.push_back(PlotCondPtr(new IsWater()));
        }
        else
        {
            conditions.push_back(PlotCondPtr(new IsLand()));
        }
        conditions.push_back(PlotCondPtr(new HasBonus(pCity_->getTeam())));

        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> bestImprovement;
        ImprovementTypes improvementType = NO_IMPROVEMENT;
        bool targetPlotHasWorkers = true;

        while (targetPlotHasWorkers)
        {
            bestImprovement = pCityImprovementManager_->getBestImprovementNotBuilt(true, false, conditions);
            improvementType = boost::get<2>(bestImprovement);
            if (improvementType == NO_IMPROVEMENT)
            {
                break;
            }

            XYCoords coords = boost::get<0>(bestImprovement);
            const CvPlot* targetPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
            os << "\n(getBestBonusImprovement) - Checking plot: " << coords << " for worker missions: " << getNumWorkersTargetingPlot(coords);
#endif
            if (getNumWorkersTargetingPlot(coords) < 1)
            {
                targetPlotHasWorkers = false;
            }
            else
            {
                conditions.push_back(PlotCondPtr(new IgnorePlot(targetPlot)));
            }
        }
        
        if (improvementType != NO_IMPROVEMENT && !targetPlotHasWorkers)
        {
            std::pair<XYCoords, BuildTypes> buildOrder = getImprovementBuildOrder_(boost::get<0>(bestImprovement), improvementType);

#ifdef ALTAI_DEBUG
            {   // debug
                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                std::ostream& os = pCivLog->getStream();
                os << "\ngetBestBonusImprovement(): " << narrow(pCity_->getName()) << " Returned build of: "
                   << (buildOrder.second == NO_BUILD ? "(NO_BUILD)" : gGlobals.getBuildInfo(buildOrder.second).getType())
                   << " for: " << boost::get<0>(bestImprovement);
                if (buildOrder.second == NO_BUILD)
                {
                    XYCoords coords = boost::get<0>(bestImprovement);
                    CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                    os << " plot danger = " << CvPlayerAI::getPlayer(pCity_->getOwner()).AI_getPlotDanger(pPlot, 2);
                }

                {
                    player_.debugWorkerMissions(os);
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

    std::pair<XYCoords, BuildTypes> City::getImprovementBuildOrder_(XYCoords coords, ImprovementTypes improvementType)
    {
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

            if (!isIrrigationAvailable)
            {
                // want irrigation, but it's not essential to build the improvement
                const bool hasBonusAndWantsIrrigation = bonusType != NO_BONUS && improvementInfo.isImprovementBonusMakesValid(bonusType);

                if (CvTeamAI::getTeam(pCity_->getTeam()).isIrrigation())  // can chain irrigation
                {
                    XYCoords buildCoords = pCityImprovementManager_->getIrrigationChainPlot(coords);
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
                            //        std::vector<PlotImprovementData>& improvements = improvementManager.getImprovements();
                            //        for (size_t i = 0, count = improvements.size(); i < count; ++i)
                            //        {
                            //            if (improvements[i].coords == coords)
                            //            {
                            //                improvements[i].flags |= CityImprovementManager::IrrigationChainPlot;
                            //                break;
                            //            }
                            //        }
                            //    }
                            //}
                            return std::make_pair(buildCoords, GameDataAnalysis::getBuildTypeForImprovementType(improvementType));
                        }
                        else // if (!hasBonusAndWantsIrrigation)
                        {
                            return std::make_pair(XYCoords(), NO_BUILD);
                        }
                    }
                    else if (!hasBonusAndWantsIrrigation)
                    {
                        {
                            boost::shared_ptr<ErrorLog> pErrorLog = ErrorLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
                            std::ostream& os = pErrorLog->getStream();
                            os << "\n" << gGlobals.getGame().getGameTurn() << " " << narrow(pCity_->getName()) << " - failed to find plot for irrigation path from: " << coords;
                        }
                        // see if can find a substitute improvement, since we can't seem to build an irrigated one and it's not a farmed bonus
                        return std::make_pair(coords, GameDataAnalysis::getBuildTypeForImprovementType(pCityImprovementManager_->getSubstituteImprovement(getCityData(), coords)));
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
        //    std::vector<PlotImprovementData>& improvements = improvementManager.getImprovements();
        //    for (size_t i = 0, count = improvements.size(); i < count; ++i)
        //    {
        //        if (improvements[i].coords == coords)
        //        {
        //            improvements[i].flags |= CityImprovementManager::IrrigationChainPlot;
        //            break;
        //        }
        //    }
        //}
        return std::make_pair(coords, GameDataAnalysis::getBuildTypeForImprovementType(improvementType));
    }

    bool City::checkResourceConnections(CvUnitAI* pUnit) const
    {
        const std::vector<PlotImprovementData>& improvements = pCityImprovementManager_->getImprovements();
        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            if (improvements[i].flags & PlotImprovementData::NeedsRoute)
            {
                XYCoords coords = improvements[i].coords;
                CvPlot* pImprovementPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
                os << "\n(checkResourceConnections) - Checking plot: " << coords << " for worker missions: " << getNumWorkersTargetingPlot(coords);
#endif
                if (getNumWorkersTargetingPlot(coords) > 0)
                {
                    continue;
                }

                BuildTypes buildType = NO_BUILD;
                RouteTypes routeType = pUnit->getGroup()->getBestBuildRoute(pImprovementPlot, &buildType);

#ifdef ALTAI_DEBUG
                os << "\nPlot: " << coords << " has " << (pImprovementPlot->isConnectedTo(pCity_) ? "" : "no") 
                   << " connection to: " << narrow(pCity_->getName()) << ", routetype = "
                   << (routeType == NO_ROUTE ? "none" : gGlobals.getRouteInfo(routeType).getType());
#endif

                if (routeType != NO_ROUTE &&
                    pImprovementPlot->getSubArea() == pUnit->plot()->getSubArea()
                    && !pImprovementPlot->isConnectedTo(pCity_))  // already checked plot is in same sub-area as city when improvement was flagged
                {
                    if (pUnit->generatePath(pImprovementPlot, MOVE_SAFE_TERRITORY, true))
                    {
                        if (!pUnit->atPlot(pImprovementPlot))
                        {
                            //pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pImprovementPlot->getX(), pImprovementPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pImprovementPlot);
                            player_.pushWorkerMission(pUnit, pCity_, pImprovementPlot, MISSION_MOVE_TO, NO_BUILD, MOVE_SAFE_TERRITORY, "City::checkResourceConnections");
                        }

                        //pUnit->getGroup()->pushMission(MISSION_ROUTE_TO, pCity_->getX(), pCity_->getY(), MOVE_SAFE_TERRITORY, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pImprovementPlot);
                        player_.pushWorkerMission(pUnit, pCity_, pCity_->plot(), MISSION_ROUTE_TO, buildType, MOVE_SAFE_TERRITORY, "City::checkResourceConnections");

#ifdef ALTAI_DEBUG
                        {   // debug
                            BonusTypes bonusType = pImprovementPlot->getBonusType(pCity_->getTeam());
                            os << "\n" << narrow(pCity_->getName()) << " Requested connection of resource: " << (bonusType == NO_BONUS ? " none? " : gGlobals.getBonusInfo(bonusType).getType()) << " at: " << coords;
                        }
                        {
                            player_.debugWorkerMissions(os);
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
        if (routeType == NO_ROUTE)
        {
            return false;
        }

#ifdef ALTAI_DEBUG
        std::ostream& os = CityLog::getLog(pCity_)->getStream();
        os << "\nRoutes for city: " << narrow(pCity_->getName());
#endif
        FAStar* pSubAreaStepFinder = gDLL->getFAStarIFace()->create();
        CvMap& theMap = gGlobals.getMap();
        gDLL->getFAStarIFace()->Initialize(pSubAreaStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), subAreaStepDestValid, stepHeuristic, stepCost, subAreaStepValid, stepAdd, NULL, NULL);
        const int stepFinderInfo = MAKEWORD((short)pCity_->getOwner(), (short)(SubAreaStepFlags::Team_Territory | SubAreaStepFlags::Unowned_Territory));

        FAStar& routeFinder = gGlobals.getRouteFinder();
        gDLL->getFAStarIFace()->ForceReset(&routeFinder);

        const int subAreaID = pCity_->plot()->getSubArea();
        const XYCoords cityCoords(pCity_->plot()->getCoords());

        std::multimap<int, IDInfo> routeMap;
        CityIter iter(CvPlayerAI::getPlayer(pCity_->getOwner()));
        while (CvCity* pDestCity = iter())
        {
#ifdef ALTAI_DEBUG
            os << "\nChecking city: " << narrow(pDestCity->getName());
#endif

            if (pDestCity->getIDInfo() == pCity_->getIDInfo())
            {
                continue;
            }

            XYCoords destCityCoords(pDestCity->plot()->getCoords());
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
#ifdef ALTAI_DEBUG
                            os << "\n(connectCities) - Checking plot: " << pPlot->getCoords() << " for worker missions: " << getNumWorkersTargetingPlot(pPlot->getCoords());
#endif
                            if (getNumWorkersTargetingPlot(pPlot->getCoords()) == 0)
                            {
                                pTargetPlots.push_back(pPlot);
                            }
                        }
                        pNode = pNode->m_pParent;
                    }

                    if (!pTargetPlots.empty())
                    {
                        if (!pUnit->atPlot(pTargetPlots[0]))
                        {
                            //pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pTargetPlots[i]->getX(), pTargetPlots[0]->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pTargetPlots[0]);
                            for (size_t plotIndex = 0, plotCount = pTargetPlots.size(); plotIndex < plotCount; ++plotIndex)
                            {                            
                                player_.pushWorkerMission(pUnit, getCity(ci->second), pTargetPlots[plotCount - plotIndex - 1], MISSION_MOVE_TO, NO_BUILD, MOVE_NO_ENEMY_TERRITORY, "City::connectCities");
                            }
                        }

                        //pUnit->getGroup()->pushMission(MISSION_ROUTE_TO, pTargetPlots[0]->getX(), pTargetPlots[0]->getY(), MOVE_SAFE_TERRITORY, false, MISSIONAI_BUILD, pTargetPlots[0]);
                        BuildTypes buildType = GameDataAnalysis::getBuildTypeForRouteType(pUnit->getGroup()->getBestBuildRoute(pTargetPlots[0]));
                        for (size_t plotIndex = 1, plotCount = pTargetPlots.size(); plotIndex < plotCount; ++plotIndex)
                        {
                            //player_.pushWorkerMission(pUnit, getCity(ci->second), pTargetCityPlot, MISSION_ROUTE_TO, buildType, MOVE_NO_ENEMY_TERRITORY, pUnit->getGroup()->getLengthMissionQueue() > 0, "City::connectCities");
                            player_.pushWorkerMission(pUnit, getCity(ci->second), pTargetPlots[plotIndex], MISSION_ROUTE_TO, buildType, MOVE_NO_ENEMY_TERRITORY, "City::connectCities");
                        }
                        player_.pushWorkerMission(pUnit, getCity(ci->second), pTargetCityPlot, MISSION_ROUTE_TO, buildType, MOVE_NO_ENEMY_TERRITORY, "City::connectCities");
#ifdef ALTAI_DEBUG
                        {   // debug
                            RouteTypes currentRouteType = pTargetPlots[0]->getRouteType();
                            os << "\n" << narrow(pCity_->getName()) << " Requested connection to city: " << narrow(getCity(ci->second)->getName())
                                << " at plot: " << XYCoords(pTargetPlots[0]->getX(), pTargetPlots[0]->getY()) << " mission queue = " << pUnit->getGroup()->getLengthMissionQueue()
                                << " for route type: " << gGlobals.getRouteInfo(routeType).getType()
                                << " current route type: " << (currentRouteType == NO_ROUTE ? " NO_ROUTE" : gGlobals.getRouteInfo(currentRouteType).getType());
                        }
                        {
                            player_.debugWorkerMissions(os);
                        }
#endif
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

    bool City::connectCity(CvUnitAI* pUnit, CvCity* pDestCity) const
    {
        RouteTypes routeType = CvPlayerAI::getPlayer(pCity_->getOwner()).getBestRoute();
        if (routeType == NO_ROUTE)
        {
            return false;
        }

#ifdef ALTAI_DEBUG
        std::ostream& os = CityLog::getLog(pCity_)->getStream();
        os << "\nRoutes for city: " << narrow(pCity_->getName()) << " to: " << narrow(pDestCity->getName());
#endif
        FAStar* pSubAreaStepFinder = gDLL->getFAStarIFace()->create();
        CvMap& theMap = gGlobals.getMap();
        gDLL->getFAStarIFace()->Initialize(pSubAreaStepFinder, theMap.getGridWidth(), theMap.getGridHeight(), theMap.isWrapX(), theMap.isWrapY(), subAreaStepDestValid, stepHeuristic, stepCost, subAreaStepValid, stepAdd, NULL, NULL);
        const int stepFinderInfo = MAKEWORD((short)pCity_->getOwner(), (short)(SubAreaStepFlags::Team_Territory | SubAreaStepFlags::Unowned_Territory));

        FAStar& routeFinder = gGlobals.getRouteFinder();
        gDLL->getFAStarIFace()->ForceReset(&routeFinder);

        const int subAreaID = pCity_->plot()->getSubArea();
        const XYCoords cityCoords(pCity_->plot()->getCoords());

        if (pDestCity->getIDInfo() == pCity_->getIDInfo() || pDestCity->plot()->getSubArea() != subAreaID)
        {
            return false;
        }

        XYCoords destCityCoords(pDestCity->plot()->getCoords());

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
        int routeValue = 0;
        bool selected = pathLength != 0 && (routeLength == 0 || pathLength + 2 < routeLength);
        if (selected)
        {
            routeValue = routeLength == 0 ? pathLength : routeLength - pathLength;
        }

#ifdef ALTAI_DEBUG
        os << "\nTarget City: " << narrow(pDestCity->getName()) << " value = " << routeValue;
#endif
        if (selected && pUnit)
        {
            CvPlot* pTargetCityPlot = pDestCity->plot();

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
#ifdef ALTAI_DEBUG
                        os << "\n(connectCity) - Checking plot: " << pPlot->getCoords() << " for worker missions: " << getNumWorkersTargetingPlot(pPlot->getCoords());
#endif
                        if (getNumWorkersTargetingPlot(pPlot->getCoords()) == 0)
                        {
                            pTargetPlots.push_back(pPlot);
                        }
                    }
                    pNode = pNode->m_pParent;
                }

                if (!pTargetPlots.empty())
                {
                    if (!pUnit->atPlot(pTargetPlots[0]))
                    {
                        //pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pTargetPlots[i]->getX(), pTargetPlots[0]->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pTargetPlots[0]);
                        player_.pushWorkerMission(pUnit, pDestCity, pTargetPlots[0], MISSION_MOVE_TO, NO_BUILD, MOVE_SAFE_TERRITORY, "City::connectCities");
                    }

                    //pUnit->getGroup()->pushMission(MISSION_ROUTE_TO, pTargetPlots[0]->getX(), pTargetPlots[0]->getY(), MOVE_SAFE_TERRITORY, pUnit->getGroup()->getLengthMissionQueue() > 0, false, MISSIONAI_BUILD, pTargetPlots[0]);
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForRouteType(pUnit->getGroup()->getBestBuildRoute(pTargetPlots[0]));
                    player_.pushWorkerMission(pUnit, pDestCity, pTargetCityPlot, MISSION_ROUTE_TO, buildType, MOVE_SAFE_TERRITORY, "City::connectCities");
#ifdef ALTAI_DEBUG
                    {   // debug
                        RouteTypes currentRouteType = pTargetPlots[0]->getRouteType();
                        os << "\n" << narrow(pCity_->getName()) << " Requested connection to city: " << narrow(pDestCity->getName())
                            << " at plot: " << XYCoords(pTargetPlots[0]->getX(), pTargetPlots[0]->getY()) << " mission queue = " << pUnit->getGroup()->getLengthMissionQueue()
                            << " for route type: " << gGlobals.getRouteInfo(routeType).getType()
                            << " current route type: " << (currentRouteType == NO_ROUTE ? " NO_ROUTE" : gGlobals.getRouteInfo(currentRouteType).getType());
                    }
                    {
                        player_.debugWorkerMissions(os);
                    }
#endif
                    gDLL->getFAStarIFace()->destroy(pSubAreaStepFinder);
                    return true;
                }
            }
        }

        gDLL->getFAStarIFace()->destroy(pSubAreaStepFinder);
        return false;
    }

    bool City::checkBadImprovementFeatures(CvUnitAI* pUnit) const
    {
        const std::vector<PlotImprovementData>& improvements = pCityImprovementManager_->getImprovements();
        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            if (improvements[i].flags & PlotImprovementData::RemoveFeature)
            {
                const XYCoords coords = improvements[i].coords;
                CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

                FeatureTypes featureType = pPlot->getFeatureType();
                if (featureType == NO_FEATURE)
                {
                    continue;
                }

#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
                os << "\n(checkBadImprovementFeatures) - Checking plot: " << pPlot->getCoords() << " for worker missions: " << getNumWorkersTargetingPlot(coords);
#endif
                if (getNumWorkersTargetingPlot(coords) > 0)
                {
                    continue;
                }

                BuildTypes buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureType);
                if (pPlot->canBuild(buildType, pCity_->getOwner()))
                {
                    if (pUnit->atPlot(pPlot))
                    {
                        //pUnit->getGroup()->pushMission(MISSION_BUILD, buildType, -1, 0, false, false, MISSIONAI_BUILD, pPlot);
                        player_.pushWorkerMission(pUnit, pCity_, pPlot, MISSION_BUILD, buildType, 0, "City::checkBadImprovementFeatures");
                    }
                    else
                    {
                        //pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pPlot->getX(), pPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pPlot);
                        //pUnit->getGroup()->pushMission(MISSION_BUILD, buildType, -1, 0, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pPlot);
                        player_.pushWorkerMission(pUnit, pCity_, pPlot, MISSION_MOVE_TO, NO_BUILD, 0, "City::checkBadImprovementFeatures");
                        player_.pushWorkerMission(pUnit, pCity_, pPlot, MISSION_BUILD, buildType, 0, "City::checkBadImprovementFeatures");
                    }

#ifdef ALTAI_DEBUG
                    {   // debug
                        BonusTypes bonusType = pPlot->getBonusType(pCity_->getTeam());
                        os << "\n" << narrow(pCity_->getName()) << " Requested removal of feature: " << gGlobals.getFeatureInfo(featureType).getType() << " at: " << coords;
                        if (bonusType != NO_BONUS)
                        {
                            os << " bonus = " << gGlobals.getBonusInfo(bonusType).getType();
                        }
                    }
                    {
                        player_.debugWorkerMissions(os);
                    }
#endif
                    return true;
                }
            }
        }
        return false;
    }

    bool City::checkIrrigation(CvUnitAI* pUnit, bool onlyForResources)
    {
        //boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()));
        //std::ostream& os = pCivLog->getStream();
        //os << "\nChecking city: " << narrow(pCity_->getName()) << " for missing irrigation.";

        //const CityImprovementManager& improvementManager = getConstImprovementManager(pCity_);
        const std::vector<PlotImprovementData>& improvements = pCityImprovementManager_->getImprovements();
        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            if (improvements[i].flags & PlotImprovementData::NeedsIrrigation)
            {
                const XYCoords coords = improvements[i].coords;
                CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);

                BonusTypes bonusType = pPlot->getBonusType(pCity_->getTeam());
                if (bonusType == NO_BONUS && onlyForResources)
                {
                    continue;
                }

                std::pair<XYCoords, BuildTypes> coordsAndBuildType = getImprovementBuildOrder_(coords, improvements[i].improvement);

                if (coordsAndBuildType.first != XYCoords(-1, -1))
                {
                    CvPlot* pBuildPlot = gGlobals.getMap().plot(coordsAndBuildType.first.iX, coordsAndBuildType.first.iY);
#ifdef ALTAI_DEBUG
                    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pCity_->getOwner()))->getStream();
                    os << "\n(checkIrrigation) - Checking plot: " << coordsAndBuildType.first << " for worker missions: " << getNumWorkersTargetingPlot(coordsAndBuildType.first);
#endif
                    if (getNumWorkersTargetingPlot(coordsAndBuildType.first) > 1)  // irrigation chains can take some work - so allow up to two workers
                    {
                        continue;
                    }

                    if (pBuildPlot->canBuild(coordsAndBuildType.second, pCity_->getOwner()))
                    {
                        if (pUnit->atPlot(pBuildPlot))
                        {
                            //pUnit->getGroup()->pushMission(MISSION_BUILD, coordsAndBuildType.second, -1, 0, false, false, MISSIONAI_BUILD, pBuildPlot);
                            player_.pushWorkerMission(pUnit, pCity_, pBuildPlot, MISSION_BUILD, coordsAndBuildType.second, 0, "City::checkIrrigation");
                        }
                        else
                        {
                            //pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBuildPlot->getX(), pBuildPlot->getY(), MOVE_SAFE_TERRITORY, false, false, MISSIONAI_BUILD, pBuildPlot);
                            //pUnit->getGroup()->pushMission(MISSION_BUILD, coordsAndBuildType.second, -1, 0, (pUnit->getGroup()->getLengthMissionQueue() > 0), false, MISSIONAI_BUILD, pBuildPlot);
                            player_.pushWorkerMission(pUnit, pCity_, pBuildPlot, MISSION_MOVE_TO, NO_BUILD, MOVE_SAFE_TERRITORY, "City::checkIrrigation");
                            player_.pushWorkerMission(pUnit, pCity_, pBuildPlot, MISSION_BUILD, coordsAndBuildType.second, 0, "City::checkIrrigation");
                        }

#ifdef ALTAI_DEBUG
                        {   // debug
                            os << "\n" << narrow(pCity_->getName()) << " Requested irrigation connection at: " << coordsAndBuildType.first << " for plot: " << coords;
                            if (bonusType != NO_BONUS)
                            {
                                os << ", bonus = " << gGlobals.getBonusInfo(bonusType).getType();
                            }
                        }
                        {
                            player_.debugWorkerMissions(os);
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
            boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player_.getCvPlayer());
            std::ostream& os = pCivLog->getStream();
            os << "\nImprovements not built count = " << pCityImprovementManager_->getNumImprovementsNotBuilt()
               << " Worker build count = " << getNumWorkers();
        }
#endif
        return std::max<int>(0, std::min<int>(3, pCityImprovementManager_->getNumImprovementsNotBuilt()) - getNumWorkers());
    }

    int City::getNumWorkers() const
    {
        return player_.getNumWorkersTargetingCity(pCity_);
    }

    int City::getNumWorkersTargetingPlot(XYCoords targetCoords) const
    {
        return player_.getNumWorkersTargetingPlot(targetCoords);
    }

    TotalOutput City::getMaxOutputs() const
    {
        return maxOutputs_;
    }

    TotalOutputWeights City::getMaxOutputWeights() const
    {
        return optWeights_;
    }

    const CityDataPtr& City::getCityData()
    {
        if (flags_ & NeedsCityDataCalc)
        {
            pCityData_ = CityDataPtr(new CityData(pCity_));
            flags_ &= ~NeedsCityDataCalc;
        }
        return pCityData_;
    }

    const CityImprovementManagerPtr City::getCityImprovementManager() const
    {
        return pCityImprovementManager_;
    }

    const ProjectionLadder& City::getCurrentOutputProjection()
    {
        if (flags_ & NeedsProjectionCalcs)
        {
            updateProjections_();
        }
        return currentOutputProjection_;
    }

    const ProjectionLadder& City::getBaseOutputProjection()
    {
        if (flags_ & NeedsProjectionCalcs)
        {
            updateProjections_();
        }
        return baseOutputProjection_;
    }

    const CityDataPtr& City::getProjectionCityData()
    {
        if (flags_ & NeedsProjectionCalcs)
        {
            updateProjections_();
        }
        return pProjectionCityData_;
    }

    const CityDataPtr& City::getBaseProjectionCityData()
    {
        if (flags_ & NeedsProjectionCalcs)
        {
            updateProjections_();
        }
        return pBaseProjectionCityData_;
    }

    void City::write(FDataStreamBase* pStream) const
    {
        constructItem_.write(pStream);
        maxOutputs_.write(pStream);
        writeArray(pStream, optWeights_);
        pStream->Write(flags_);
        pCityImprovementManager_->write(pStream);
    }

    void City::read(FDataStreamBase* pStream)
    {
        constructItem_.read(pStream);
        maxOutputs_.read(pStream);
        readArray(pStream, optWeights_);
        pStream->Read(&flags_);
        pCityImprovementManager_->read(pStream);
    }

    void City::debugGreatPeople_()
    {
#ifdef ALTAI_DEBUG
        boost::shared_ptr<CityLog> pCityLog = CityLog::getLog(pCity_);
        CityOptimiser opt(getCityData());

        std::set<UnitTypes> unitTypes = getCityData()->getAvailableGPTypes();
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
        pCityImprovementManager_->logImprovements();
#endif
    }
}