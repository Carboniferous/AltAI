#include "./city_tactics.h"
#include "./building_tactics.h"
#include "./building_tactics_visitors.h"
#include "./building_info_visitors.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./unit_analysis.h"
#include "./map_analysis.h"
#include "./settler_manager.h"
#include "./city_simulator.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./city.h"
#include "./iters.h"
#include "./civ_log.h"
#include "./civ_helper.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        struct BuildingSelectionHelper
        {
            BuildingSelectionHelper() : constructItem(NO_BUILDING) {}

            explicit BuildingSelectionHelper(ConstructItem constructItem_) : constructItem(constructItem_) {}

            ConstructItem constructItem;

            // flag -> value map
            std::map<int, int> simulationValuesMap;

            typedef std::map<int, TotalOutputWeights> EconomicOutputWeightsMap;
            static EconomicOutputWeightsMap economicOutputWeightsMap;

            void debug(std::ostream& os) const
            {
#ifdef ALTAI_DEBUG
                os << "\nBuildingSelectionHelper: " << constructItem;
                if (!simulationValuesMap.empty())
                {
                    os << "simulationValuesMap: ";
                    for (std::map<int, int>::const_iterator ci(simulationValuesMap.begin()), ciEnd(simulationValuesMap.end()); ci != ciEnd; ++ci)
                    {
                        streamEconomicFlags(os, ci->first);
                        os << " value = " << ci->second;
                    }
                }
#endif
            }
        };

        BuildingSelectionHelper::EconomicOutputWeightsMap BuildingSelectionHelper::economicOutputWeightsMap = boost::assign::map_list_of(EconomicFlags::Output_Food, makeOutputW(5, 3, 3, 3, 1, 1))
            (EconomicFlags::Output_Production, makeOutputW(2, 10, 2, 2, 1, 1))(EconomicFlags::Output_Commerce, makeOutputW(1, 1, 6, 6, 3, 2))
            (EconomicFlags::Output_Happy, makeOutputW(3, 3, 2, 2, 1, 1))(EconomicFlags::Output_Health, makeOutputW(3, 3, 2, 2, 1, 1))
            (EconomicFlags::Output_Research, makeOutputW(1, 2, 2, 10, 1, 1))(EconomicFlags::Output_Gold, makeOutputW(1, 2, 10, 2, 1, 1))
            (EconomicFlags::Output_Culture, makeOutputW(1, 1, 1, 1, 10, 1))(EconomicFlags::Output_Espionage, makeOutputW(1, 1, 1, 1, 1, 10))
            (EconomicFlags::Output_Maintenance_Reduction, makeOutputW(1, 1, 6, 6, 3, 2));

        struct UnitSelectionHelper
        {
            UnitSelectionHelper() : constructItem(NO_UNIT), existingCount(0), inProductionCount(0) {}

            explicit UnitSelectionHelper(ConstructItem constructItem_) : constructItem(constructItem_), existingCount(0), inProductionCount(0) {}

            ConstructItem constructItem;
            int existingCount, inProductionCount;

            int getUnitCount(bool includeInProductionCount) const
            {
                return existingCount + (includeInProductionCount ? inProductionCount : 0);
            }

            void debug(std::ostream& os) const
            {
#ifdef ALTAI_DEBUG
                os << "\nUnitSelectionHelper: " << constructItem << " existing units = " << existingCount << " in production = " << inProductionCount;
#endif
            }
        };

        struct CityBuildSelectionData
        {
            CityBuildSelectionData(const PlayerTactics& playerTactics_, const City& city_)
                : playerTactics(playerTactics_), player(playerTactics_.player), city(city_),
                  improvementManager(playerTactics_.player.getAnalysis()->getMapAnalysis()->getImprovementManager(city_.getCvCity()->getIDInfo())),
                  selection(NO_BUILDING)
            {
                warPlanCount = CvTeamAI::getTeam(player.getTeamID()).getAnyWarPlanCount(true);
                atWarCount = CvTeamAI::getTeam(player.getTeamID()).getAtWarCount(true);
                happyCap = city.getCityData()->happyCap;
                militaryFlagsUnion = 0, unitEconomicFlagsUnion = 0;
                existingSettlersCount = 0, existingWorkersCount = 0, workerBuildCount = 0, existingConsumedWorkersCount = 0, consumedWorkerBuildCount = 0;
                unworkedGoodImprovementCount = 0;

                combatUnitCount = player.getCombatUnitCount(DOMAIN_LAND, true);
                cityCount = player.getCvPlayer()->getNumCities();
                popCount = 0;

                CityIter iter(*player.getCvPlayer());
                while (CvCity* pCity = iter())
                {
                    popCount += pCity->getPopulation();
                }
                
                maxResearchRate = player.getMaxResearchRate();
                maxResearchRateWithProcesses = player.getMaxResearchRateWithProcesses();
                rankAndMaxProduction = player.getCityRank(city.getCvCity()->getIDInfo(), OUTPUT_PRODUCTION);

                bestEconomicBuilding = bestSmallCultureBuilding = bestMilitaryBuilding = NO_BUILDING;

                bestCollateralUnit = bestCityDefenceUnit = bestScoutUnit = bestCombatUnit = bestSeaDefenceUnit = bestSeaTransportUnit = bestSeaScoutUnit = NO_UNIT;

                bestEconomicProcess = bestResearchProcess = bestCultureProcess = NO_PROCESS;
            }

            void processUnits()
            {
                const ConstructList& unitList = playerTactics.selectedUnitTactics_;
                boost::shared_ptr<CivHelper> pCivHelper = player.getCivHelper();

                for (ConstructListConstIter iter(unitList.begin()), iterEnd(unitList.end()); iter != iterEnd; ++iter)
                {
                    if (city.getCvCity()->canTrain(iter->unitType))
                    {
                        UnitSelectionHelper unitSelectionHelper(*iter);
                        unitSelectionHelper.existingCount = player.getUnitCount(iter->unitType);

                        militaryFlagsUnion |= iter->militaryFlags;
                        unitEconomicFlagsUnion |= iter->economicFlags;

                        // remove any builds we can't yet do (or are close to researching)
                        checkWorkerBuilds(unitSelectionHelper.constructItem.possibleBuildTypes);

                        // includes workboats
                        // it is possible to have worker units which can build a mixture of improvements which consume them (e.g. BUILD_FISHING_BOATS) and don't (e.g. BUILD_MINE)
                        // so, a single unit can get added to both sets of worker units 
                        for (std::map<BuildTypes, int>::const_iterator buildsIter(unitSelectionHelper.constructItem.possibleBuildTypes.begin()),
                             endBuildsIter(unitSelectionHelper.constructItem.possibleBuildTypes.end()); buildsIter != endBuildsIter; ++buildsIter)
                        {
                            const CvBuildInfo& buildInfo = gGlobals.getBuildInfo(buildsIter->first);
                            if (buildInfo.isKill())
                            {
                                if (consumedPossibleWorkers.find(iter->unitType) == consumedPossibleWorkers.end())
                                {
                                    consumedPossibleWorkers.insert(iter->unitType);
                                    existingConsumedWorkersCount += unitSelectionHelper.existingCount;
                                    for (std::map<BuildTypes, int>::const_iterator buildsInnerIter(unitSelectionHelper.constructItem.possibleBuildTypes.begin()),
                                        buildsInnerEndIter(unitSelectionHelper.constructItem.possibleBuildTypes.end()); buildsInnerIter != buildsInnerEndIter; ++buildsInnerIter)
                                    {
                                        const CvBuildInfo& buildInfo = gGlobals.getBuildInfo(buildsIter->first);
                                        if (buildInfo.isKill())
                                        {
                                            consumedWorkerBuildCount += buildsInnerIter->second;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                if (possibleWorkers.find(iter->unitType) == possibleWorkers.end())
                                {
                                    // first time we've added this unit, so add its build totals to workerBuildCount
                                    possibleWorkers.insert(iter->unitType);
                                    existingWorkersCount += unitSelectionHelper.existingCount;
                                    for (std::map<BuildTypes, int>::const_iterator buildsInnerIter(unitSelectionHelper.constructItem.possibleBuildTypes.begin()),
                                        buildsInnerEndIter(unitSelectionHelper.constructItem.possibleBuildTypes.end()); buildsInnerIter != buildsInnerEndIter; ++buildsInnerIter)
                                    {
                                        const CvBuildInfo& buildInfo = gGlobals.getBuildInfo(buildsIter->first);
                                        if (!buildInfo.isKill())
                                        {
                                            workerBuildCount += buildsInnerIter->second;
                                        }
                                    }
                                }
                            }
                        }

                        if (iter->economicFlags & EconomicFlags::Output_Settler)
                        {
                            existingSettlersCount += unitSelectionHelper.existingCount;
                            possibleSettlers.insert(iter->unitType);
                        }

                        int currentProductionCount = 0;
                        CityIter cityIter(*player.getCvPlayer());
                        while (CvCity* pLoopCity = cityIter())
                        {
                            if (pLoopCity->getProductionUnit() == iter->unitType)
                            {
                                ++unitSelectionHelper.inProductionCount;
                            }
                        }

                        unitSelectionData.insert(std::make_pair(iter->unitType, unitSelectionHelper));
                    }
                }
            }

            void processBuildings()
            {
#ifdef ALTAI_DEBUG
                // debug
                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
                std::ostream& os = pCivLog->getStream();
                os << "\n(getConstructItem): Turn = " << gGlobals.getGame().getGameTurn();
#endif
                ConstructList buildingsList;
        
                const int nTurns = gGlobals.getGame().getMaxTurns() / 10;

                std::map<IDInfo, ConstructList >::const_iterator ci(playerTactics.selectedCityBuildingTactics_.find(city.getCvCity()->getIDInfo()));
                if (ci != playerTactics.selectedCityBuildingTactics_.end())
                {
                    buildingsList = ci->second;

                    CitySimulator simulator(city.getCvCity());
                    BuildingSimulationResults simulationResults;

                    for (ConstructListConstIter iter(buildingsList.begin()), endIter(buildingsList.end()); iter != endIter; ++iter)
                    {
                        if (iter->processType != NO_PROCESS && player.getCvPlayer()->canMaintain(iter->processType))
                        {
                            BuildingSelectionHelper processSelectionHelper(*iter);

                            TotalOutput maxOutput = city.getMaxOutputs();
                            const CvProcessInfo& processInfo = gGlobals.getProcessInfo(iter->processType);

                            CommerceModifier modifier;
                            Commerce processCommerce; 
                            for (int j = 0; j < NUM_COMMERCE_TYPES; ++j)
                            {
                                modifier[j] = processInfo.getProductionToCommerceModifier(j);
                                processCommerce[j] = modifier[j] * maxOutput[OUTPUT_PRODUCTION] / 100;
                            }
#ifdef ALTAI_DEBUG
                            os << "\nProcessed process: " << processInfo.getType() << " = " << processCommerce;
#endif
                            if (iter->economicFlags & EconomicFlags::Output_Gold)
                            {
                                CommerceValueFunctor valueCF(CommerceWeights(boost::assign::list_of(4)(1)(1)(1)));
                                processSelectionHelper.simulationValuesMap[EconomicFlags::Output_Gold] = nTurns * valueCF(processCommerce);
                            }
                            if (iter->economicFlags & EconomicFlags::Output_Research)
                            {
                                CommerceValueFunctor valueCF(CommerceWeights(boost::assign::list_of(1)(4)(1)(1)));
                                processSelectionHelper.simulationValuesMap[EconomicFlags::Output_Research] = nTurns * valueCF(processCommerce);
                            }
                            if (iter->economicFlags & EconomicFlags::Output_Culture)
                            {
                                CommerceValueFunctor valueCF(CommerceWeights(boost::assign::list_of(1)(1)(2)(1)));
                                processSelectionHelper.simulationValuesMap[EconomicFlags::Output_Culture] = nTurns * valueCF(processCommerce);
                            }
                            if (iter->economicFlags & EconomicFlags::Output_Espionage)
                            {
                                CommerceValueFunctor valueCF(CommerceWeights(boost::assign::list_of(1)(1)(1)(3)));
                                processSelectionHelper.simulationValuesMap[EconomicFlags::Output_Espionage] = nTurns * valueCF(processCommerce);
                            }

                            processSelectionData[iter->processType] = processSelectionHelper;
                        }
                        else if (iter->buildingType != NO_BUILDING && city.getCvCity()->canConstruct(iter->buildingType))
                        {
                            BuildingSelectionHelper buildingSelectionHelper(*iter);
                        
                            bool doBaseline = true;

                            if (iter->economicFlags) // is an economic building
                            {
#ifdef ALTAI_DEBUG
                                os << "\nSimulating building: " << gGlobals.getBuildingInfo(iter->buildingType).getType();
#endif
                                simulator.evaluateBuilding(iter->buildingType, nTurns, simulationResults, doBaseline);
                                doBaseline = false;
                            }
                            else if (iter->militaryFlags)
                            {
                                militaryFlagsUnion |= iter->militaryFlags;
                            }

                            buildingSelectionData[iter->buildingType] = buildingSelectionHelper;
                        }
                    }

#ifdef ALTAI_DEBUG
                    // debug
                    simulationResults.debugResults(CityLog::getLog(city.getCvCity())->getStream());
                    simulationResults.debugResults(os);
#endif

                    for (int i = 0; i < EconomicFlags::Num_Output_FlagTypes; ++i)
                    {
                        const int flag = 1 << i;

                        std::map<BuildingTypes, int> buildingValues = simulator.getBuildingValues(BuildingSelectionHelper::economicOutputWeightsMap[flag], simulationResults);

                        for (ConstructListConstIter iter(buildingsList.begin()), endIter(buildingsList.end()); iter != endIter; ++iter)
                        {
                            if (iter->buildingType != NO_BUILDING && (iter->economicFlags & flag))
                            {
                                economicFlagsUnion |= flag;
                                buildingSelectionData[iter->buildingType].simulationValuesMap[flag] = buildingValues[iter->buildingType];
                            }
                        }
                    }
                }
            }

            void debug()
            {
#ifdef ALTAI_DEBUG
                // debug
                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
                std::ostream& os = pCivLog->getStream();
                os << "\n(getConstructItem): Turn = " << gGlobals.getGame().getGameTurn();

                os << "\nWar plan count = " << CvTeamAI::getTeam(player.getTeamID()).getAnyWarPlanCount(true);

                for (std::map<BuildingTypes, BuildingSelectionHelper>::const_iterator ci(buildingSelectionData.begin()), ciEnd(buildingSelectionData.end()); ci != ciEnd; ++ci)
                {
                    ci->second.debug(os);
                }

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    ci->second.debug(os);
                }

                for (std::map<ProcessTypes, BuildingSelectionHelper>::const_iterator ci(processSelectionData.begin()), ciEnd(processSelectionData.end()); ci != ciEnd; ++ci)
                {
                    ci->second.debug(os);
                }

                os << "\nEconomic flags union:";
                streamEconomicFlags(os, economicFlagsUnion);
                os << "\nUnit Economic flags union:";
                streamEconomicFlags(os, unitEconomicFlagsUnion);
                os << "\nMilitary flags union:";
                streamMilitaryFlags(os, militaryFlagsUnion);

                os << "\ncity count = " << cityCount << ", pop count = " << popCount << " happy cap = " << happyCap;
                os << "\ncombat unit count = " << combatUnitCount;
                os << "\nrank and production = " << rankAndMaxProduction.first << ", " << rankAndMaxProduction.second;

                os << " Possible settler = " << !possibleSettlers.empty() << ", possible worker = " << !possibleWorkers.empty() << ", possible consumed worker = " << !consumedPossibleWorkers.empty();
                os << " existing settler count = " << existingSettlersCount;

                os << ", workerBuildCount = " << workerBuildCount << ", consumedWorkerBuildCount = " << consumedWorkerBuildCount;
                os << ", exising worker count = " << existingWorkersCount << ", existing consumed workers count = " << existingConsumedWorkersCount;
                    
                os << ", max research rate = " << maxResearchRate;

                os << ", unworkedGoodImprovementCount = " << unworkedGoodImprovementCount;

                if (bestSmallCultureBuilding != NO_BUILDING)
                {
                    os << "\nbestSmallCultureBuilding = " << gGlobals.getBuildingInfo(bestSmallCultureBuilding).getType();
                }
                if (bestEconomicBuilding != NO_BUILDING)
                {
                    os << "\nbestEconomicBuilding = " << gGlobals.getBuildingInfo(bestEconomicBuilding).getType();
                }
                if (bestMilitaryBuilding != NO_BUILDING)
                {
                    os << "\nbestMilitaryBuilding = " << gGlobals.getBuildingInfo(bestMilitaryBuilding).getType();
                }
                if (bestCombatUnit != NO_UNIT)
                {
                    os << "\nbestCombatUnit = " << gGlobals.getUnitInfo(bestCombatUnit).getType();
                }
                if (bestCityDefenceUnit != NO_UNIT)
                {
                    os << "\nbestCityDefenceUnit = " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType();
                }
                if (bestScoutUnit != NO_UNIT)
                {
                    os << "\nbestScoutUnit = " << gGlobals.getUnitInfo(bestScoutUnit).getType();
                }
                if (bestSeaScoutUnit != NO_UNIT)
                {
                    os << "\nbestSeaScoutUnit = " << gGlobals.getUnitInfo(bestSeaScoutUnit).getType();
                }
                if (bestCollateralUnit != NO_UNIT)
                {
                    os << "\nbestCollateralUnit = " << gGlobals.getUnitInfo(bestCollateralUnit).getType();
                }
                if (bestSeaDefenceUnit != NO_UNIT)
                {
                    os << "\nbestSeaDefenceUnit = " << gGlobals.getUnitInfo(bestSeaDefenceUnit).getType();
                }
                if (bestSeaTransportUnit != NO_UNIT)
                {
                    os << "\nbestSeaTransportUnit = " << gGlobals.getUnitInfo(bestSeaTransportUnit).getType();
                }
                os << "\nbest counter units:";
                for (std::map<UnitCombatTypes, UnitTypes>::const_iterator ci(bestUnitCombatTypes.begin()), ciEnd(bestUnitCombatTypes.end()); ci != ciEnd; ++ci)
                {
                    os << gGlobals.getUnitCombatInfo(ci->first).getType() << " = " << gGlobals.getUnitInfo(ci->second).getType() << ", ";
                }

                os << "\n\nCombat units: ";
                for (size_t i = 0, count = combatUnits.size(); i < count; ++i)
                {
                    os << gGlobals.getUnitInfo(combatUnits[i]).getType() << ", ";
                }
                os << "\nCity defence units: ";
                for (size_t i = 0, count = cityDefenceUnits.size(); i < count; ++i)
                {
                    os << gGlobals.getUnitInfo(cityDefenceUnits[i]).getType() << ", ";
                }
                os << "\nCity attack units: ";
                for (size_t i = 0, count = cityAttackUnits.size(); i < count; ++i)
                {
                    os << gGlobals.getUnitInfo(cityAttackUnits[i]).getType() << ", ";
                }
                os << "\nCollateral units: ";
                for (size_t i = 0, count = collateralUnits.size(); i < count; ++i)
                {
                    os << gGlobals.getUnitInfo(collateralUnits[i]).getType() << ", ";
                }
                os << "\nSea defence units: ";
                for (size_t i = 0, count = seaDefenceUnits.size(); i < count; ++i)
                {
                    os << gGlobals.getUnitInfo(seaDefenceUnits[i]).getType() << ", ";
                }
                for (std::map<UnitCombatTypes, std::vector<UnitTypes> >::const_iterator ci(counterUnits.begin()), ciEnd(counterUnits.end()); ci != ciEnd; ++ci)
                {
                    if (!ci->second.empty())
                    {
                        os << "\nBest counter units for: " << gGlobals.getUnitCombatInfo(ci->first).getType() << ": ";
                        for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                        {
                            os << gGlobals.getUnitInfo(ci->second[i]).getType() << ", ";
                        }
                    }
                }
#endif
            }

            void calculateImprovementStats()
            {
                const std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();

                for (size_t i = 0, count = improvements.size(); i < count; ++i)
                {
                    if (!isEmpty(boost::get<4>(improvements[i])))
                    {
                        XYCoords coords(boost::get<0>(improvements[i]));
                        const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                        if (!city.getCvCity()->isWorkingPlot(pPlot))
                        {
                            ++unworkedGoodImprovementCount;
                        }
                    }
                }
            }

            void checkWorkerBuilds(std::map<BuildTypes, int>& possibleBuildTypes)
            {
#ifdef ALTAI_DEBUG
                // debug
                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
                std::ostream& os = pCivLog->getStream();
#endif
                boost::shared_ptr<CivHelper> pCivHelper = player.getCivHelper();

                std::map<BuildTypes, int>::iterator iter = possibleBuildTypes.begin(), endIter = possibleBuildTypes.end();

                while (iter != endIter)
                {
                    if (iter->first == NO_BUILD)
                    {
                        possibleBuildTypes.erase(iter++);
                        continue;
                    }
                    TechTypes buildTechType = GameDataAnalysis::getTechTypeForBuildType(iter->first);
                    if (buildTechType == NO_TECH)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nskipped checking for build: " << iter->first << " with tech: " << buildTechType;
#endif
                        possibleBuildTypes.erase(iter++);
                        continue;
                    }
#ifdef ALTAI_DEBUG
                    os << "\nchecking for worker tech: " << gGlobals.getTechInfo(buildTechType).getType()
                       << " has tech = " << pCivHelper->hasTech(buildTechType) << " depth = " << player.getTechResearchDepth(buildTechType);
#endif
                    if (buildTechType == NO_TECH || pCivHelper->hasTech(buildTechType) || player.getTechResearchDepth(buildTechType) < 2 ||
                        player.getCurrentResearchTech().targetTechType == buildTechType)
                    {
                        ++iter;
                    }
                    else
                    {
#ifdef ALTAI_DEBUG
                        os << " erasing build: " << gGlobals.getBuildInfo(iter->first).getType() << ", ";
#endif
                        possibleBuildTypes.erase(iter++);
                    }
                }
            }

            void calculateSmallCultureBuilding()
            {
                if (economicFlagsUnion & EconomicFlags::Output_Culture)
                {
#ifdef ALTAI_DEBUG
                    // debug
                    boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
                    std::ostream& os = pCivLog->getStream();
                    os << "\n(getConstructItem): Turn = " << gGlobals.getGame().getGameTurn();
#endif
                    bestSmallCultureBuilding = NO_BUILDING;
                    int bestValue = MAX_INT;
                    for (std::map<BuildingTypes, BuildingSelectionHelper>::const_iterator ci(buildingSelectionData.begin()), ciEnd(buildingSelectionData.end()); ci != ciEnd; ++ci)
                    {
                        if (ci->second.constructItem.buildingFlags & (BuildingFlags::Building_World_Wonder & BuildingFlags::Building_National_Wonder))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(calculateSmallCultureBuilding) skipping building as is wonder: " << gGlobals.getBuildingInfo(ci->first).getType();
#endif
                            continue;
                        }
#ifdef ALTAI_DEBUG
                        os << "\n(calculateSmallCultureBuilding) checking building: " << gGlobals.getBuildingInfo(ci->first).getType();
#endif
                        std::map<int, int>::const_iterator iter = ci->second.simulationValuesMap.find(EconomicFlags::Output_Culture);
                        if (iter != ci->second.simulationValuesMap.end())
                        {
                            //int thisValue = iter->second;
                            int requiredProduction = city.getCvCity()->getProductionNeeded(ci->first) - city.getCvCity()->getBuildingProduction(ci->first);
                            //thisValue /= requiredProduction;
#ifdef ALTAI_DEBUG
                            os << " required production = " << requiredProduction << " value = " << iter->second;
#endif
                            if (requiredProduction < bestValue)
                            {
                                bestValue = requiredProduction;
                                bestSmallCultureBuilding = ci->second.constructItem.buildingType;
                            }
                        }
                    }
                }
            }

            void calculateBestEconomicBuilding()
            {
                if (economicFlagsUnion)
                {
#ifdef ALTAI_DEBUG
                    // debug
                    boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
                    std::ostream& os = pCivLog->getStream();
                    os << "\n(calculateBestEconomicBuilding): Turn = " << gGlobals.getGame().getGameTurn();
#endif
                    BuildingTypes bestBuilding = NO_BUILDING;
                    int bestValue = 0;
                    for (std::map<BuildingTypes, BuildingSelectionHelper>::const_iterator ci(buildingSelectionData.begin()), ciEnd(buildingSelectionData.end()); ci != ciEnd; ++ci)
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(calculateBestEconomicBuilding) checking building: " << gGlobals.getBuildingInfo(ci->first).getType();
#endif
                        for (std::map<int, int>::const_iterator iter = ci->second.simulationValuesMap.begin(), endIter = ci->second.simulationValuesMap.end(); iter != endIter; ++iter)
                        {
                            int thisValue = iter->second;
                            if (thisValue > bestValue)
                            {
                                bestValue = thisValue;
                                bestEconomicBuilding = ci->second.constructItem.buildingType;
                            }
                        }
                    }
                }
            }

            void calculateBestMilitaryBuilding()
            {
                if (militaryFlagsUnion)
                {
#ifdef ALTAI_DEBUG
                    // debug
                    boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
                    std::ostream& os = pCivLog->getStream();
                    os << "\n(calculateBestMilitaryBuilding): Turn = " << gGlobals.getGame().getGameTurn();
#endif
                    BuildingTypes bestBuilding = NO_BUILDING;
                    int bestValue = 0;
                    for (std::map<BuildingTypes, BuildingSelectionHelper>::const_iterator ci(buildingSelectionData.begin()), ciEnd(buildingSelectionData.end()); ci != ciEnd; ++ci)
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(calculateBestMilitaryBuilding) checking building: " << gGlobals.getBuildingInfo(ci->first).getType();
#endif
                        if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Experience)
                        {
                            bestBuilding = ci->first;
                            break;
                        }
                    }
                    bestMilitaryBuilding = bestBuilding;
                }
            }

            void calculateBestProcesses()
            {
                // TODO - base on research rate and city multipliers
                int bestEconomicProcessValue = 0, bestResearchProcessValue = 0, bestCultureProcessValue = 0;
                for (std::map<ProcessTypes, BuildingSelectionHelper>::const_iterator ci(processSelectionData.begin()), ciEnd(processSelectionData.end()); ci != ciEnd; ++ci)
                {
                    for (std::map<int, int>::const_iterator valuesIter(ci->second.simulationValuesMap.begin()), valuesEndIter(ci->second.simulationValuesMap.end()); valuesIter != valuesEndIter; ++valuesIter)
                    {
                        if (valuesIter->first == EconomicFlags::Output_Gold)
                        {
                            if (valuesIter->second > bestEconomicProcessValue)
                            {
                                bestEconomicProcessValue = valuesIter->second;
                                bestEconomicProcess = ci->second.constructItem.processType;
                            }
                        }
                        else if (valuesIter->first == EconomicFlags::Output_Research)
                        {
                            if (valuesIter->second > bestResearchProcessValue)
                            {
                                bestResearchProcessValue = valuesIter->second;
                                bestResearchProcess = ci->second.constructItem.processType;
                            }
                        }
                        else if (valuesIter->first == EconomicFlags::Output_Culture)
                        {
                            if (valuesIter->second > bestCultureProcessValue)
                            {
                                bestCultureProcessValue = valuesIter->second;
                                bestCultureProcess = ci->second.constructItem.processType;
                            }
                        }
                    }
                }
            }

            void calculateBestCombatUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Combat_Unit && gGlobals.getUnitInfo(ci->second.constructItem.unitType).getDomainType() == DOMAIN_LAND)
                    {
                        ConstructItem::MilitaryFlagValuesMap::const_iterator mapIter = ci->second.constructItem.militaryFlagValuesMap.find(MilitaryFlags::Output_Attack);
                        if (mapIter != ci->second.constructItem.militaryFlagValuesMap.end())
                        {
                            unitValuesMap.insert(std::make_pair(mapIter->second.first, ci->second.constructItem.unitType));
                        }
                    }
                }

                int bestValue = 0;
                combatUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (140 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    combatUnits.push_back(valueIter->second);
                    ++valueIter;
                }

                if (!combatUnits.empty())
                {
                    bestCombatUnit = combatUnits[0];
                }
            }

            void calculateBestCityAttackUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Combat_Unit && gGlobals.getUnitInfo(ci->second.constructItem.unitType).getDomainType() == DOMAIN_LAND)
                    {
                        ConstructItem::MilitaryFlagValuesMap::const_iterator mapIter = ci->second.constructItem.militaryFlagValuesMap.find(MilitaryFlags::Output_City_Attack);
                        if (mapIter != ci->second.constructItem.militaryFlagValuesMap.end())
                        {
                            unitValuesMap.insert(std::make_pair(mapIter->second.first, ci->second.constructItem.unitType));
                        }
                    }
                }

                int bestValue = 0;
                cityAttackUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (140 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    cityAttackUnits.push_back(valueIter->second);
                    ++valueIter;
                }
            }

            void calculateBestScoutUnit()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Explore && gGlobals.getUnitInfo(ci->second.constructItem.unitType).getDomainType() == DOMAIN_LAND)
                    {
                        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(ci->second.constructItem.unitType);
                        int value = unitInfo.getMoves() + unitInfo.getCombat();
                        unitValuesMap.insert(std::make_pair(value, ci->second.constructItem.unitType));
                    }
                }

                if (!unitValuesMap.empty())
                {
                    bestScoutUnit = unitValuesMap.begin()->second;
                }
            }

            struct SeaScoutUnitValueF
            {
                bool operator() (UnitTypes first, UnitTypes second) const
                {
                    const CvUnitInfo& unitInfo1 = gGlobals.getUnitInfo(first), &unitInfo2 = gGlobals.getUnitInfo(second);
                    return unitInfo1.getMoves() == unitInfo2.getMoves() ? unitInfo1.getProductionCost() < unitInfo2.getProductionCost() :
                        unitInfo1.getMoves() > unitInfo2.getMoves();
                }
            };

            void calculateBestSeaScoutUnit()
            {
                typedef std::multiset<UnitTypes, SeaScoutUnitValueF> UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Explore && gGlobals.getUnitInfo(ci->second.constructItem.unitType).getDomainType() == DOMAIN_SEA)
                    {
                        unitValuesMap.insert(ci->second.constructItem.unitType);
                    }
                }

                if (!unitValuesMap.empty())
                {
                    bestSeaScoutUnit = *unitValuesMap.begin();
                }
            }

            void calculateBestCityDefenceUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Combat_Unit)
                    {
                        ConstructItem::MilitaryFlagValuesMap::const_iterator mapIter = ci->second.constructItem.militaryFlagValuesMap.find(MilitaryFlags::Output_City_Defence);
                        if (mapIter != ci->second.constructItem.militaryFlagValuesMap.end())
                        {
                            unitValuesMap.insert(std::make_pair(mapIter->second.first, ci->second.constructItem.unitType));
                        }
                    }
                }

                int bestValue = 0;
                cityDefenceUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (120 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    cityDefenceUnits.push_back(valueIter->second);
                    ++valueIter;
                }

                if (!cityDefenceUnits.empty())
                {
                    bestCityDefenceUnit = cityDefenceUnits[0];
                }
            }

            void calculateBestCollateralUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Collateral)
                    {
                        ConstructItem::MilitaryFlagValuesMap::const_iterator mapIter = ci->second.constructItem.militaryFlagValuesMap.find(MilitaryFlags::Output_Collateral);
                        if (mapIter != ci->second.constructItem.militaryFlagValuesMap.end())
                        {
                            unitValuesMap.insert(std::make_pair(mapIter->second.first, ci->second.constructItem.unitType));
                        }
                    }
                }

                int bestValue = 0;
                collateralUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (150 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    collateralUnits.push_back(valueIter->second);
                    ++valueIter;
                }

                if (!collateralUnits.empty())
                {
                    bestCollateralUnit = collateralUnits[0];
                }
            }

            void calculateBestSeaDefenceUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Combat_Unit && gGlobals.getUnitInfo(ci->second.constructItem.unitType).getDomainType() == DOMAIN_SEA)
                    {
                        ConstructItem::MilitaryFlagValuesMap::const_iterator mapIter = ci->second.constructItem.militaryFlagValuesMap.find(MilitaryFlags::Output_Attack);
                        if (mapIter != ci->second.constructItem.militaryFlagValuesMap.end())
                        {
                            unitValuesMap.insert(std::make_pair(mapIter->second.first, ci->second.constructItem.unitType));
                        }
                    }
                }

                int bestValue = 0;
                seaDefenceUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (120 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    seaDefenceUnits.push_back(valueIter->second);
                    ++valueIter;
                }

                if (!seaDefenceUnits.empty())
                {
                    bestSeaDefenceUnit = seaDefenceUnits[0];
                }
            }

            void calculateBestSeaTransportUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Unit_Transport && gGlobals.getUnitInfo(ci->second.constructItem.unitType).getDomainType() == DOMAIN_SEA)
                    {
                        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(ci->second.constructItem.unitType);
                        int value = unitInfo.getMoves() + unitInfo.getCombat();
                        unitValuesMap.insert(std::make_pair(value, ci->second.constructItem.unitType));
                    }
                }

                int bestValue = 0;
                seaDefenceUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (120 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    seaTransportUnits.push_back(valueIter->second);
                    ++valueIter;
                }

                if (!seaTransportUnits.empty())
                {
                    bestSeaTransportUnit = seaTransportUnits[0];
                }
            }

            void calculateBestUnits()
            {
                for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
                {
                    typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                    UnitValuesMap unitValuesMap;

                    for (UnitSelectionData::const_iterator ci(unitSelectionData.begin()), ciEnd(unitSelectionData.end()); ci != ciEnd; ++ci)
                    {
                        if (ci->second.constructItem.militaryFlags & MilitaryFlags::Output_Combat_Unit)
                        {
                            // TODO - add somthing to select from Output_UnitClass_Counter units too
                            ConstructItem::MilitaryFlagValuesMap::const_iterator mapIter = ci->second.constructItem.militaryFlagValuesMap.find(MilitaryFlags::Output_UnitCombat_Counter);
                            while (mapIter != ci->second.constructItem.militaryFlagValuesMap.end() && mapIter->first == MilitaryFlags::Output_UnitCombat_Counter) 
                            {
                                if ((UnitCombatTypes)mapIter->second.second == (UnitCombatTypes)i)
                                {
                                    unitValuesMap.insert(std::make_pair(mapIter->second.first, ci->second.constructItem.unitType));
                                }
                                ++mapIter;
                            }
                        }
                    }

                    counterUnits[(UnitCombatTypes)i].clear();
                    UnitValuesMap::const_iterator bestValueIter, valueIter;
                    bestValueIter = valueIter = unitValuesMap.begin();
                    while (valueIter != unitValuesMap.end() && (130 * valueIter->first) / 100 >= bestValueIter->first)
                    {
                        counterUnits[(UnitCombatTypes)i].push_back(valueIter->second);
                        ++valueIter;
                    }

                    if (!unitValuesMap.empty())
                    {
                        bestUnitCombatTypes[(UnitCombatTypes)i] = unitValuesMap.begin()->second;
                    }
                }
            }

            bool chooseUnit()
            {
#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
                UnitTypes combatUnit = chooseUnit(combatUnits), collateralUnit = chooseUnit(collateralUnits), attackUnit = chooseUnit(cityAttackUnits);
                int combatUnitCount = player.getUnitCount(combatUnit), collateralUnitCount = player.getUnitCount(collateralUnit),
                    attackUnitCount = player.getUnitCount(attackUnit), cityDefenceUnitCount = player.getUnitCount(bestCityDefenceUnit);
                int totalCollateralUnitCount = player.getCollateralUnitCount(MilitaryFlags::Output_Collateral);
#ifdef ALTAI_DEBUG
                os << "\nUnit counts: ";
                if (combatUnit != NO_UNIT)
                {
                    os << gGlobals.getUnitInfo(combatUnit).getType() << " = " << combatUnitCount;
                }
                if (collateralUnit != NO_UNIT)
                {
                    os << gGlobals.getUnitInfo(collateralUnit).getType() << " = " << collateralUnitCount;
                }
                if (attackUnit != NO_UNIT)
                {
                    os << gGlobals.getUnitInfo(attackUnit).getType() << " = " << attackUnitCount;
                }
                if (bestCityDefenceUnit != NO_UNIT)
                {
                    os << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << " = " << cityDefenceUnitCount;
                }
                os << " totalCollateralUnitCount = " << totalCollateralUnitCount;
#endif
                if (bestCityDefenceUnit != NO_UNIT && 
                    city.getCvCity()->plot()->plotCount(PUF_isUnitType, bestCityDefenceUnit, -1, city.getCvCity()->getOwner()) < 1 + city.getCvCity()->getPopulation() / 10
                    && setConstructItem(bestCityDefenceUnit))
                {
#ifdef ALTAI_DEBUG
                    os << "\n(getConstructItem) Returning city defence unit(1): " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << selection;
#endif
                    return true;
                }
                else if (combatUnitCount < totalCollateralUnitCount / 2 && setConstructItem(combatUnit))
                {
#ifdef ALTAI_DEBUG
                    os << "\n(getConstructItem) Returning combat unit(1): " << gGlobals.getUnitInfo(combatUnit).getType() << selection;
#endif
                    return true;
                }
                else if (attackUnitCount < totalCollateralUnitCount / 2 && setConstructItem(attackUnit))
                {
#ifdef ALTAI_DEBUG
                    os << "\n(getConstructItem) Returning attack unit(1): " << gGlobals.getUnitInfo(attackUnit).getType() << selection;
#endif
                    return true;
                }
                else if (setConstructItem(collateralUnit))
                {
#ifdef ALTAI_DEBUG
                    os << "\n(getConstructItem) Returning collateral unit(1): " << gGlobals.getUnitInfo(collateralUnit).getType() << selection;
#endif
                    return true;
                }
                return false;
            }

            bool chooseLandScoutUnit()
            {
#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
                if (bestScoutUnit != NO_UNIT)
                {
                    CvArea* pArea = city.getCvCity()->area();
                    if (player.getCvPlayer()->AI_totalAreaUnitAIs(pArea, UNITAI_EXPLORE) < ((CvPlayerAI*)player.getCvPlayer())->AI_neededExplorers(pArea))
                    {
                        if (setConstructItem(bestScoutUnit))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning land scout unit: " << gGlobals.getUnitInfo(bestScoutUnit).getType() << selection;
#endif
                            return true;
                        }
                    }
                }
                return false;
            }

            bool chooseSeaScoutUnit()
            {
#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
                if (bestSeaScoutUnit != NO_UNIT)
                {
                    CvArea* pWaterArea = city.getCvCity()->waterArea();
                    if (player.getUnitCount(bestSeaScoutUnit) < 2 + 2 * player.getCvPlayer()->getNumCities() &&
                        player.getCvPlayer()->AI_totalWaterAreaUnitAIs(pWaterArea, UNITAI_EXPLORE) < ((CvPlayerAI*)player.getCvPlayer())->AI_neededExplorers(pWaterArea))
                    {
                        if (setConstructItem(bestSeaScoutUnit))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning water scout unit: " << gGlobals.getUnitInfo(bestSeaScoutUnit).getType() << selection;
#endif
                            return true;
                        }
                    }
                }
                return false;
            }

            UnitTypes getBestWorker(bool isConsumed) const
            {
                int bestBuildCount = 0; 
                UnitTypes bestWorkerUnitType = NO_UNIT;

                for (std::set<UnitTypes>::const_iterator ci((isConsumed ? consumedPossibleWorkers : possibleWorkers).begin()),
                     ciEnd((isConsumed ? consumedPossibleWorkers : possibleWorkers).end()); ci != ciEnd; ++ci)
                {
                    UnitSelectionData::const_iterator iter = unitSelectionData.find(*ci);
                    if (iter != unitSelectionData.end())
                    {
                        int thisBuildCount = 0;
                        for (std::map<BuildTypes, int>::const_iterator buildsIter(iter->second.constructItem.possibleBuildTypes.begin()),
                             endBuildsIter(iter->second.constructItem.possibleBuildTypes.end()); buildsIter != endBuildsIter; ++buildsIter)
                        {
                            const CvBuildInfo& buildInfo = gGlobals.getBuildInfo(buildsIter->first);
                            const bool thisBuildConsumesWorker = buildInfo.isKill();
                            if (isConsumed && thisBuildConsumesWorker || !isConsumed && !thisBuildConsumesWorker)
                            {
                                thisBuildCount += buildsIter->second;
                            }
                        }

                        if (thisBuildCount > bestBuildCount)
                        {
                            bestBuildCount = thisBuildCount; 
                            bestWorkerUnitType = *ci;
                        }
                    }
                }

                return bestWorkerUnitType;
            }

            ConstructItem getSelection()
            {
#ifdef ALTAI_DEBUG
                // debug
                boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
                std::ostream& os = pCivLog->getStream();
#endif
                // No defensive unit, but can build one
                if (city.getCvCity()->getMilitaryHappinessUnits() == 0 && bestCityDefenceUnit != NO_UNIT && setConstructItem(bestCityDefenceUnit))
                {
#ifdef ALTAI_DEBUG
                    os << "\n(getConstructItem) Returning best city defence unit: " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << selection;
#endif
                    return selection;
                }

                // need some culture
                if (bestCultureProcess != NO_PROCESS && setConstructItem(bestCultureProcess))
                {
#ifdef ALTAI_DEBUG
                    os << "\n(getConstructItem) Returning best culture process: " << gGlobals.getProcessInfo(bestCultureProcess).getType() << selection;
#endif
                    return selection;
                }

                if (bestSmallCultureBuilding != NO_BUILDING && setConstructItem(bestSmallCultureBuilding))
                {
#ifdef ALTAI_DEBUG
                    os << "\n(getConstructItem) Returning small culture building: " << gGlobals.getBuildingInfo(bestSmallCultureBuilding).getType() << selection;
#endif
                    return selection;
                }

                // logic is slighly different when have one city
                if (cityCount == 1)
                {
                    if (city.getCvCity()->plot()->plotCount(PUF_isUnitAIType, UNITAI_SETTLE, -1, city.getCvCity()->getOwner()) > 0)
                    {
                        if (bestCityDefenceUnit != NO_UNIT && city.getCvCity()->plot()->plotCount(PUF_isUnitType, bestCityDefenceUnit, -1, city.getCvCity()->getOwner()) < 2)
                        {
                            if (setConstructItem(bestCityDefenceUnit))
                            {
#ifdef ALTAI_DEBUG
                                os << "\n(getConstructItem) Returning best city defence unit for settler escort: " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << selection;
#endif
                                return selection;
                            }
                        }
                    }

                    // no workers, but could build one
                    if (existingWorkersCount == 0 && !possibleWorkers.empty())
                    {
                        UnitTypes bestWorkerUnit = getBestWorker(false);
                        if (bestWorkerUnit != NO_UNIT && setConstructItem(bestWorkerUnit))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning reusable best worker unit: " << gGlobals.getUnitInfo(bestWorkerUnit).getType() << selection;
#endif
                            return selection;
                        }
                    }
                    if (!consumedPossibleWorkers.empty() && consumedWorkerBuildCount - existingConsumedWorkersCount > 0)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nChecking for single-shot worker...";
#endif
                        UnitTypes bestWorkerUnit = getBestWorker(true);
                        if (bestWorkerUnit != NO_UNIT &&  setConstructItem(bestWorkerUnit))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning single-shot best worker unit: " << gGlobals.getUnitInfo(bestWorkerUnit).getType() << selection;
#endif
                            return selection;
                        }
                    }

                    // have improvements we could be working, if we grow, and can grow
                    bool keepGrowing = unworkedGoodImprovementCount > 0 && city.getCvCity()->getPopulation() < happyCap;

                    // want to keep growing, or have less than three combat units, and can build more
                    if ((keepGrowing || combatUnitCount < 1) && bestCombatUnit != NO_UNIT && setConstructItem(bestCombatUnit))
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(getConstructItem) Returning best combat unit: " << gGlobals.getUnitInfo(bestCombatUnit).getType() << selection;
#endif
                        return selection;
                    }

                    if (chooseLandScoutUnit())
                    {
                        return selection;
                    }

                    // no settlers, can build one
                    if (existingSettlersCount == 0 && !possibleSettlers.empty() && setConstructItem(*possibleSettlers.begin()))
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(getConstructItem) Returning settler unit: " << gGlobals.getUnitInfo(*possibleSettlers.begin()).getType() << selection;
#endif
                        return selection;
                    }

                    if (city.getCvCity()->plot()->plotCount(PUF_isUnitAIType, UNITAI_SETTLE, -1, city.getCvCity()->getOwner()) > 0)
                    {
                        int subAreaID = city.getCvCity()->plot()->getSubArea();

                        if (!player.getSettlerManager()->getBestPlot(subAreaID, std::vector<CvPlot*>()))
                        {
                            if (chooseSeaScoutUnit())
                            {
                                return selection;
                            }

                            // todo - set flag on city to indicate overseas destination for settler?
                            if (player.getSettlerManager()->getOverseasCitySitesCount(80, 1, city.getCvCity()->plot()->getSubArea()) > 0)
                            {
                                if (bestSeaTransportUnit != NO_UNIT && player.getUnitCount(bestSeaTransportUnit) < 1 && setConstructItem(bestSeaTransportUnit))
                                {
#ifdef ALTAI_DEBUG
                                    os << "\n(getConstructItem) Returning best sea transport unit for settler escort: " << gGlobals.getUnitInfo(bestSeaTransportUnit).getType() << selection;
#endif  
                                    return selection;
                                }
                            }
                        }
                    }

                    // can build something which has an economic benefit
                    if (bestEconomicBuilding != NO_BUILDING && setConstructItem(bestEconomicBuilding))
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(getConstructItem) Returning best economic building: " << gGlobals.getBuildingInfo(bestEconomicBuilding).getType() << selection;
#endif
                        return selection;
                    }

                    // less than 4 combat units, and can build one
                    if (combatUnitCount < 4 && bestCombatUnit != NO_UNIT && setConstructItem(bestCombatUnit))
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(getConstructItem) Returning best combat unit: " << gGlobals.getUnitInfo(bestCombatUnit).getType() << selection;
#endif
                        return selection;
                    }

                    // can build worker units, and no. of plots to improve greater than 6 * no. workers
                    if (!possibleWorkers.empty() && workerBuildCount - 6 * existingWorkersCount > 0)
                    {
                        // unconsumed workers (hence count of improvements is useful)
                        UnitTypes bestWorkerUnit = getBestWorker(false);
                        if (setConstructItem(bestWorkerUnit))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning reusable best worker unit: " << gGlobals.getUnitInfo(bestWorkerUnit).getType() << selection;
#endif
                            return selection;
                        }
                    }

                    // workboats...
                    if (!consumedPossibleWorkers.empty() && consumedWorkerBuildCount - existingConsumedWorkersCount > 0)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nChecking for single-shot worker...";
#endif
                        UnitTypes bestWorkerUnit = getBestWorker(true);
                        if (setConstructItem(bestWorkerUnit))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best single-shot worker unit: " << gGlobals.getUnitInfo(bestWorkerUnit).getType() << selection;
#endif
                            return selection;
                        }
                    }

                    if (chooseSeaScoutUnit())
                    {
                        return selection;
                    }

                    // could use some settlers
                    if (!possibleSettlers.empty() && setConstructItem(*possibleSettlers.begin()))
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(getConstructItem) Returning settler unit: " << gGlobals.getUnitInfo(*possibleSettlers.begin()).getType() << selection;
#endif
                        return selection;
                    }
                }
                else
                {
                    if (!player.getCvPlayer()->isBarbarian() && maxResearchRateWithProcesses < 50 && maxResearchRate < 30 && !processSelectionData.empty())
                    {
                        if (bestEconomicProcess != NO_PROCESS && setConstructItem(bestEconomicProcess))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best economic process as in financial trouble: " << gGlobals.getProcessInfo(bestEconomicProcess).getType() << selection;
#endif
                            return selection;
                        }
                        else if (bestResearchProcess != NO_PROCESS && setConstructItem(bestResearchProcess))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best research process as in financial trouble: " << gGlobals.getProcessInfo(bestResearchProcess).getType() << selection;
#endif
                            return selection;
                        }
                    }

                    // top half of cities for production
                    if (rankAndMaxProduction.first <= cityCount / 2)
                    {
                        if ((atWarCount > 0 || warPlanCount > 0) && chooseUnit())
                        {
                            return selection;
                        }

                        if (city.getCvCity()->plot()->plotCount(PUF_isUnitAIType, UNITAI_SETTLE, -1, city.getCvCity()->getOwner()) > 0)
                        {
                            if (bestCityDefenceUnit != NO_UNIT && city.getCvCity()->plot()->plotCount(PUF_isUnitType, bestCityDefenceUnit, -1, city.getCvCity()->getOwner()) < 2)
                            {
                                if (setConstructItem(bestCityDefenceUnit))
                                {
#ifdef ALTAI_DEBUG
                                    os << "\n(getConstructItem) Returning best city defence unit for settler escort: " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << selection;
#endif
                                    return selection;
                                }
                            }
                        }

                        const bool isDanger = player.getCvPlayer()->AI_getPlotDanger(city.getCvCity()->plot(), 3, false);

                        if (combatUnitCount < (1 + popCount / 3) || isDanger && bestCombatUnit != NO_UNIT)
                        {
                            if (bestMilitaryBuilding != NO_BUILDING && setConstructItem(bestMilitaryBuilding))
                            {
#ifdef ALTAI_DEBUG
                                os << "\n(getConstructItem) Returning best military building(1): " << gGlobals.getBuildingInfo(bestMilitaryBuilding).getType() << selection;
#endif
                                return selection;
                            }
                            else if (city.getCvCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()) && bestSeaDefenceUnit != NO_UNIT && 
                                player.getUnitCount(bestSeaDefenceUnit) < 2 && setConstructItem(bestSeaDefenceUnit))
                            {
#ifdef ALTAI_DEBUG
                                os << "\n(getConstructItem) Returning best sea defence unit(1): " << gGlobals.getUnitInfo(bestSeaDefenceUnit).getType() << selection;
#endif
                                return selection;
                            }

                            if (chooseUnit())
                            {
                                return selection;
                            }
                        }

                        // can build worker units, and no. of plots to improve greater than 4 * no. workers
                        if (!possibleWorkers.empty() && workerBuildCount - 4 * existingWorkersCount > 0)
                        {
                            UnitTypes bestWorkerUnit = getBestWorker(false);
                            if (setConstructItem(bestWorkerUnit))
                            {
#ifdef ALTAI_DEBUG
                                os << "\n(getConstructItem) Returning best reusable worker unit: " << gGlobals.getUnitInfo(bestWorkerUnit).getType() << selection;
#endif
                                return selection;
                            }
                        }

                        // workboats...
                        if (!consumedPossibleWorkers.empty() && consumedWorkerBuildCount - existingConsumedWorkersCount > 0)
                        {
#ifdef ALTAI_DEBUG
                            os << "\nChecking for single-shot worker...";
#endif
                            UnitTypes bestWorkerUnit = getBestWorker(true);
                            if (setConstructItem(bestWorkerUnit))
                            {
#ifdef ALTAI_DEBUG
                                os << "\n(getConstructItem) Returning best single-shot worker unit: " << gGlobals.getUnitInfo(bestWorkerUnit).getType() << selection;
#endif
                                return selection;
                            }
                        }

                        if (!possibleSettlers.empty() && maxResearchRate > 30 && existingSettlersCount < 1 + (player.getSettlerManager()->getBestCitySites(140, 4).size() / 2) && setConstructItem(*possibleSettlers.begin()))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning settler unit: " << gGlobals.getUnitInfo(*possibleSettlers.begin()).getType() << selection;
#endif
                            return selection;
                        }

                        if (existingSettlersCount > 0 && player.getSettlerManager()->getOverseasCitySitesCount(140, 1, city.getCvCity()->plot()->getSubArea()) > 0)
                        {
                            if (chooseSeaScoutUnit())
                            {
                                return selection;
                            }

                            if (bestSeaTransportUnit != NO_UNIT && player.getUnitCount(bestSeaTransportUnit) < cityCount / 2 && setConstructItem(bestSeaTransportUnit))
                            {
#ifdef ALTAI_DEBUG
                                os << "\n(getConstructItem) Returning best sea transport unit for settler escort: " << gGlobals.getUnitInfo(bestSeaTransportUnit).getType() << selection;
#endif
                                return selection;
                            }
                        }

                        if (chooseLandScoutUnit())
                        {
                            return selection;
                        }

                        // top rank for production, and can build something which improves the economy
                        if (rankAndMaxProduction.first == 1 && bestEconomicBuilding != NO_BUILDING &&
                            !(findConstructItem(bestEconomicBuilding).buildingFlags & BuildingFlags::Building_World_Wonder) && setConstructItem(bestEconomicBuilding))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best economic building as best production city: " << gGlobals.getBuildingInfo(bestEconomicBuilding).getType() << selection;
#endif
                            return selection;
                        }

                        // less than two settlers
                        if (!possibleSettlers.empty() && maxResearchRate > 40 && existingSettlersCount < 1 + (player.getSettlerManager()->getBestCitySites(100, 4).size() / 2) && setConstructItem(*possibleSettlers.begin()))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning settler unit: " << gGlobals.getUnitInfo(*possibleSettlers.begin()).getType() << selection;
#endif
                            return selection;
                        }

                        if (rankAndMaxProduction.first == 1 && bestEconomicBuilding != NO_BUILDING && setConstructItem(bestEconomicBuilding))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best economic building as best production city: " << gGlobals.getBuildingInfo(bestEconomicBuilding).getType() << selection;
#endif
                            return selection;
                        }

                        if (combatUnitCount < (5 * cityCount) / 2 || combatUnitCount < (1 + 2 * popCount / 5))
                        {
                            if (bestMilitaryBuilding != NO_BUILDING && setConstructItem(bestMilitaryBuilding))
                            {
#ifdef ALTAI_DEBUG
                                os << "\n(getConstructItem) Returning best military building(2): " << gGlobals.getBuildingInfo(bestMilitaryBuilding).getType() << selection;
#endif
                                return selection;
                            }

                            if (city.getCvCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()) && bestSeaDefenceUnit != NO_UNIT && 
                                player.getUnitCount(bestSeaDefenceUnit) < 2 && setConstructItem(bestSeaDefenceUnit))
                            {
#ifdef ALTAI_DEBUG
                                os << "\n(getConstructItem) Returning best sea defence unit(2): " << gGlobals.getUnitInfo(bestSeaDefenceUnit).getType() << selection;
#endif
                                return selection;
                            }

                            if (chooseUnit())
                            {
                                return selection;
                            }
                        }
                    }

                    // workboats...
                    if (!consumedPossibleWorkers.empty() && consumedWorkerBuildCount - existingConsumedWorkersCount > 0)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nChecking for single-shot worker...";
#endif
                        UnitTypes bestWorkerUnit = getBestWorker(true);
                        if (setConstructItem(bestWorkerUnit))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best single-shot worker unit: " << gGlobals.getUnitInfo(bestWorkerUnit).getType() << selection;
#endif
                            return selection;
                        }
                    }

                    if ((combatUnitCount < (7 * cityCount) / 2 || combatUnitCount < (1 + 2 * popCount / 3)) && (atWarCount > 0 || warPlanCount > 0))
                    {
                        if (bestMilitaryBuilding != NO_BUILDING && setConstructItem(bestMilitaryBuilding))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best military building(3): " << gGlobals.getBuildingInfo(bestMilitaryBuilding).getType() << selection;
#endif
                            return selection;
                        }

                        if (chooseUnit())
                        {
                            return selection;
                        }
                    }

                    if (existingSettlersCount > 0 && player.getSettlerManager()->getOverseasCitySitesCount(100, 1, city.getCvCity()->plot()->getSubArea()) > 0)
                    {
                        if (bestSeaTransportUnit != NO_UNIT && player.getUnitCount(bestSeaTransportUnit) < 2 && setConstructItem(bestSeaTransportUnit))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best sea transport unit for settler escort: " << gGlobals.getUnitInfo(bestSeaTransportUnit).getType() << selection;
#endif
                            return selection;
                        }
                    }

                    // build something economic
                    if (bestEconomicBuilding != NO_BUILDING && setConstructItem(bestEconomicBuilding))
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(getConstructItem) Returning best economic building: " << gGlobals.getBuildingInfo(bestEconomicBuilding).getType() << selection;
                        //os << "\n(getConstructItem) Returning best economic building: " << bestEconomicBuilding << " " << selection;
#endif
                        return selection;
                    }

                    // choose a process
                    if (!player.getCvPlayer()->isBarbarian() && !processSelectionData.empty())
                    {
                        if (bestEconomicProcess != NO_PROCESS && setConstructItem(bestEconomicProcess))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best economic process: " << gGlobals.getProcessInfo(bestEconomicProcess).getType() << selection;
#endif
                            return selection;
                        }
                        else if (bestResearchProcess != NO_PROCESS && setConstructItem(bestResearchProcess))
                        {
#ifdef ALTAI_DEBUG
                            os << "\n(getConstructItem) Returning best research process: " << gGlobals.getProcessInfo(bestResearchProcess).getType() << selection;
#endif
                            return selection;
                        }
                    }
                }

                return selection;
            }

            UnitTypes chooseUnit(const std::vector<UnitTypes>& units)
            {
                if (units.empty())
                {
                    return NO_UNIT;
                }

                const size_t unitCount = units.size();
                if (unitCount == 1)
                {
                    return units[0];
                }

                std::vector<int> counts(unitCount);
                for (size_t i = 0; i < unitCount; ++i)
                {
                    counts[i] = player.getUnitCount(units[i]);
                }

                std::vector<int>::const_iterator ci = std::min_element(counts.begin(), counts.end());
                return units[ci - counts.begin()];
            }

            bool setConstructItem(UnitTypes unitType)
            {
                UnitSelectionData::const_iterator iter = unitSelectionData.find(unitType);
                if (iter != unitSelectionData.end())
                {
                    selection = iter->second.constructItem;
                    return true;
                }
                return false;
            }

            bool setConstructItem(BuildingTypes buildingType)
            {
                std::map<BuildingTypes, BuildingSelectionHelper>::const_iterator iter = buildingSelectionData.find(buildingType);
                if (iter != buildingSelectionData.end())
                {
                    selection = iter->second.constructItem;
                    return true;
                }
                return false;
            }

            bool setConstructItem(ProcessTypes processType)
            {
                std::map<ProcessTypes, BuildingSelectionHelper>::const_iterator iter = processSelectionData.find(processType);
                if (iter != processSelectionData.end())
                {
                    selection = iter->second.constructItem;
                    return true;
                }
                return false;
            }

            ConstructItem findConstructItem(BuildingTypes buildingType) const
            {
                std::map<BuildingTypes, BuildingSelectionHelper>::const_iterator ci(buildingSelectionData.find(buildingType));
                if (ci != buildingSelectionData.end())
                {
                    return ci->second.constructItem;
                }
                else
                {
                    return ConstructItem(buildingType);
                }
            }

            std::map<BuildingTypes, BuildingSelectionHelper> buildingSelectionData;
            std::map<ProcessTypes, BuildingSelectionHelper> processSelectionData;
            typedef std::multimap<UnitTypes, UnitSelectionHelper> UnitSelectionData;
            UnitSelectionData unitSelectionData;

            const PlayerTactics& playerTactics;
            const Player& player;
            const City& city;
            const CityImprovementManager& improvementManager;

            int warPlanCount, atWarCount;
            int happyCap;

            int militaryFlagsUnion, unitEconomicFlagsUnion, economicFlagsUnion;
            std::set<UnitTypes> possibleWorkers, consumedPossibleWorkers, possibleSettlers;
            int existingSettlersCount, existingWorkersCount, workerBuildCount, existingConsumedWorkersCount, consumedWorkerBuildCount;

            int combatUnitCount;
            int cityCount, popCount;
            int maxResearchRate, maxResearchRateWithProcesses;
            std::pair<int, int> rankAndMaxProduction;
            int unworkedGoodImprovementCount;

            BuildingTypes bestSmallCultureBuilding, bestEconomicBuilding, bestMilitaryBuilding;

            // todo - move unit data into separate unit manager
            UnitTypes bestCombatUnit, bestScoutUnit, bestCityDefenceUnit, bestCollateralUnit, bestSeaDefenceUnit, bestSeaTransportUnit, bestSeaScoutUnit;
            std::vector<UnitTypes> combatUnits, cityAttackUnits, cityDefenceUnits, seaDefenceUnits, seaTransportUnits, collateralUnits;
            std::map<UnitCombatTypes, std::vector<UnitTypes> > counterUnits;
            std::map<UnitCombatTypes, UnitTypes> bestUnitCombatTypes;
            ProcessTypes bestEconomicProcess, bestResearchProcess, bestCultureProcess;

            ConstructItem selection;
        };
    }

    ConstructItem getConstructItem(const PlayerTactics& playerTactics, const City& city)
    {
        CityBuildSelectionData selectionData(playerTactics, city);

        selectionData.calculateImprovementStats();

        selectionData.processUnits();
        selectionData.processBuildings();

        selectionData.calculateSmallCultureBuilding();
        selectionData.calculateBestEconomicBuilding();
        selectionData.calculateBestMilitaryBuilding();
        selectionData.calculateBestProcesses();

        selectionData.calculateBestCombatUnits();
        selectionData.calculateBestCityAttackUnits();
        selectionData.calculateBestCityDefenceUnits();
        selectionData.calculateBestCollateralUnits();

        if (city.getCvCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
        {
            selectionData.calculateBestSeaScoutUnit();
            selectionData.calculateBestSeaDefenceUnits();
            selectionData.calculateBestSeaTransportUnits();
        }

        selectionData.calculateBestScoutUnit();
        selectionData.calculateBestUnits();

        selectionData.debug();

        return selectionData.getSelection();
    }
}