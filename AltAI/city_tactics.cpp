#include "AltAI.h"

#include "./city_tactics.h"
#include "./city_unit_tactics.h"
#include "./tactic_selection_data.h"
#include "./building_tactics_visitors.h"
#include "./building_info_visitors.h"
#include "./military_tactics.h"
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
        struct CityBuildSelectionData
        {
            struct BuildingConditions
            {
                BuildingConditions() : noGlobalWonders_(false), noNationalWonders_(false), onlyGlobalWonders_(false),
                    allowNoEconomicDelta_(false), allowZeroDelta_(false)
                {
                }

                BuildingConditions& noGlobalWonders()
                {
                    noGlobalWonders_ = true;
                    return *this;
                }                

                BuildingConditions& noNationalWonders()
                {
                    noNationalWonders_ = true;
                    return *this;
                }

                BuildingConditions& onlyGlobalWonders()
                {
                    onlyGlobalWonders_ = true;
                    return *this;
                }

                BuildingConditions& allowNoEconomicDelta()
                {
                    allowNoEconomicDelta_ = true;
                    return *this;
                }

                BuildingConditions& allowZeroDelta()
                {
                    allowZeroDelta_ = true;
                    return *this;
                }

                bool noGlobalWonders_, noNationalWonders_, onlyGlobalWonders_;
                bool allowNoEconomicDelta_, allowZeroDelta_;
            };

            struct MilitarySelectionConditions
            {
                MilitarySelectionConditions() : minCityDefenceUnits_(1), requiredMilitaryHappinessUnits_(1)
                {
                }

                MilitarySelectionConditions& minCityDefenceUnits(int i)
                {
                    minCityDefenceUnits_ = i;
                    return *this;
                }

                MilitarySelectionConditions& requiredMilitaryHappinessUnits(int i)
                {
                    requiredMilitaryHappinessUnits_ = i;
                    return *this;
                }

                int requiredMilitaryHappinessUnits_, minCityDefenceUnits_;
            };

            struct ScoutConditions
            {
                ScoutConditions() : buildLandScout_(true), buildSeaScout_(true)
                {
                }

                ScoutConditions& buildLandScout(bool flag)
                {
                    buildLandScout_ = flag;
                    return *this;
                }

                ScoutConditions& buildSeaScout(bool flag)
                {
                    buildSeaScout_ = flag;
                    return *this;
                }

                bool buildLandScout_, buildSeaScout_;
            };

            struct SettlerEscortConditions
            {
                SettlerEscortConditions() : buildMilitaryEscort_(true), buildSeaTransport_(false)
                {
                }

                SettlerEscortConditions& buildMilitaryEscort(bool flag)
                {
                    buildMilitaryEscort_ = flag;
                    return *this;
                }

                SettlerEscortConditions& buildSeaTransport(bool flag)
                {
                    buildSeaTransport_ = flag;
                    return *this;
                }

                bool buildMilitaryEscort_, buildSeaTransport_;
            };

            struct WorkerBuildConditions
            {
                WorkerBuildConditions() : onlyConsumedWorkers_(false), checkOutOfAreaBuilds_(false), maxLostOutputPercent_(-1)
                {
                }

                WorkerBuildConditions& onlyConsumedWorkers()
                {
                    onlyConsumedWorkers_ = true;
                    return *this;
                }

                WorkerBuildConditions& checkOutOfAreaBuilds()
                {
                    checkOutOfAreaBuilds_ = true;
                    return *this;
                }

                WorkerBuildConditions& maxLostOutputPercent(int percent)
                {
                    maxLostOutputPercent_ = percent;
                    return *this;
                }

                bool onlyConsumedWorkers_;
                bool checkOutOfAreaBuilds_;
                int maxLostOutputPercent_;
            };

            struct ProcessConditions
            {
                ProcessConditions() : maxResearchRateWithProcesses_(50), maxResearchRate_(30), forceProcess_(false)
                {
                }

                ProcessConditions& maxResearchRateWithProcesses(int i)
                {
                    maxResearchRateWithProcesses_ = i;
                    return *this;
                }

                ProcessConditions& maxResearchRate(int i)
                {
                    maxResearchRate_ = i;
                    return *this;
                }

                ProcessConditions& forceProcess(bool value)
                {
                    forceProcess_ = value;
                    return *this;
                }
                
                int maxResearchRateWithProcesses_, maxResearchRate_;
                bool forceProcess_;
            };

            struct SettlerConditions
            {
                SettlerConditions() : maxExisting_(1), cityThreshold_(140), overseasCityThreshold_(120), minResearchRate_(40),
                    maxSizeOffset_(2), maxLostOutputPercent_(-1)
                {
                }

                SettlerConditions& maxExisting(int i)
                {
                    maxExisting_ = i;
                    return *this;
                }

                SettlerConditions& cityThreshold(int i)
                {
                    cityThreshold_ = i;
                    return *this;
                }

                SettlerConditions& overseasCityThreshold(int i)
                {
                    overseasCityThreshold_ = i;
                    return *this;
                }

                SettlerConditions& minResearchRate(int i)
                {
                    minResearchRate_ = i;
                    return *this;
                }

                SettlerConditions& maxSizeOffset(int i)
                {
                    maxSizeOffset_ = i;
                    return *this;
                }

                SettlerConditions& maxLostOutputPercent(int i)
                {
                    maxLostOutputPercent_ = i;
                    return *this;
                }

                int maxExisting_;
                int cityThreshold_, overseasCityThreshold_;
                int minResearchRate_;
                int maxSizeOffset_;
                int maxLostOutputPercent_;
            };

            struct EconomicBuildsData
            {
                EconomicBuildsData() : usedCityTurns(0) {}
                std::list<const EconomicBuildingValue*> cityBuildItems;
                int usedCityTurns;

                void debug(std::ostream& os) const
                {
                    os << "\n EconomicBuildsData: used turns = " << usedCityTurns << "  ";
                    for (std::list<const EconomicBuildingValue*>::const_iterator ci(cityBuildItems.begin()); ci != cityBuildItems.end(); ++ci)
                    {
                        (*ci)->debug(os);
                        os << " ";
                    }
                }
            };

            CityBuildSelectionData(const PlayerTactics& playerTactics_, const City& city_)
                : playerTactics(playerTactics_), player(playerTactics_.player), city(city_),
                  selection(NO_BUILDING),
                  civLog(CivLog::getLog(*playerTactics_.player.getCvPlayer())->getStream())
            {
                warPlanCount = CvTeamAI::getTeam(player.getTeamID()).getAnyWarPlanCount(true);
                atWarCount = CvTeamAI::getTeam(player.getTeamID()).getAtWarCount(true);
                unownedBorders = false, barbarianLands = false, playerLands = false;

                happyCap = city.getCityData()->getHappyCap();
                existingSettlersCount = 0, existingWorkersCount = 0, workerBuildCount = 0, existingConsumedWorkersCount = 0, consumedWorkerBuildCount = 0;
                unworkedGoodImprovementCount = 0;
                cityBaseOutput = city.getCurrentOutputProjection().getOutput();

                combatUnitCount = attackUnitCount = 0;
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

                calculateCurrentOutput();

                bestEconomicBuilding = bestSmallCultureBuilding = bestLargeCultureBuilding = bestMilitaryBuilding = bestEconomicWonder = bestEconomicNationalWonder = NO_BUILDING;

                bestCollateralUnit = bestCityDefenceUnit = bestScoutUnit = bestCombatUnit = bestSeaDefenceUnit = bestSeaTransportUnit = bestSeaScoutUnit = bestSettlerUnit = NO_UNIT;
                bestDependentTacticUnit = NO_UNIT;

                bestEconomicProcess = bestResearchProcess = bestCultureProcess = NO_PROCESS;
            }

            std::pair<IDInfo, int> getBestBuildTime(UnitTypes unitType) const
            {
                int leastTurns = MAX_INT;
                IDInfo bestCity;
                for (PlayerTactics::UnitTacticsMap::const_iterator ci(playerTactics.unitTacticsMap_.begin()), ciEnd(playerTactics.unitTacticsMap_.end()); ci != ciEnd; ++ci)
                {
                    if (ci->first == unitType && ci->second)
                    {
                        CityIter cityIter(*playerTactics.player.getCvPlayer());
                        while (CvCity* pCity = cityIter())
                        {
                            CityUnitTacticsPtr pCityUnitTactics = ci->second->getCityTactics(pCity->getIDInfo());
                            if (pCityUnitTactics && !pCityUnitTactics->getProjection().units.empty())
                            {
                                const int thisCityTurns = pCityUnitTactics->getProjection().units[0].turns;
                                if (thisCityTurns < leastTurns)
                                {
                                    leastTurns = thisCityTurns;
                                    bestCity = pCity->getIDInfo();
                                }
                            }
                        }
                    }
                }

                return std::make_pair(bestCity, leastTurns);
            }

            void processUnits()
            {
            }

            void processBuildings()
            {
            }

            void processProjects()
            {
            }

            void debug()
            {
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem): Turn = " << gGlobals.getGame().getGameTurn() << ", " << narrow(city.getCvCity()->getName());
                tacticSelectionData.debug(civLog);

                civLog << "\ncurrentCivOutput = " << currentCivOutput << ", currentCityOutput = " << currentCityOutput;

                civLog << "\nWar plan count = " << CvTeamAI::getTeam(player.getTeamID()).getAnyWarPlanCount(true);

                for (std::map<UnitTypes, WorkerUnitValue>::const_iterator ci(tacticSelectionData.workerUnits.begin()), ciEnd(tacticSelectionData.workerUnits.end()); ci != ciEnd; ++ci)
                {
                    ci->second.debug(civLog);
                }

                civLog << "\ncity count = " << cityCount << ", pop count = " << popCount << " happy cap = " << happyCap;
                civLog << "\ncombat unit count = " << combatUnitCount <<", attack unit count = " << attackUnitCount;
                civLog << "\nrank and production = " << rankAndMaxProduction.first << ", " << rankAndMaxProduction.second;

                //civLog << " Possible settler = " << !possibleSettlers.empty(); // << ", possible worker = " << !possibleWorkers.empty() << ", possible consumed worker = " << !consumedPossibleWorkers.empty();
                for (std::map<UnitTypes, SettlerUnitValue>::const_iterator ci(tacticSelectionData.settlerUnits.begin()), ciEnd(tacticSelectionData.settlerUnits.end()); ci != ciEnd; ++ci)
                {
                    ci->second.debug(civLog);
                }
                civLog << "\nBest settler = " << (bestSettlerUnit == NO_UNIT ? "NO_UNIT" : gGlobals.getUnitInfo(bestSettlerUnit).getType());
                civLog << " existing settler count = " << existingSettlersCount;
                civLog << ", unworkedGoodImprovementCount = " << unworkedGoodImprovementCount;

                city.getCityImprovementManager()->logImprovements(civLog);

                /*civLog << ", workerBuildCount = " << workerBuildCount << ", consumedWorkerBuildCount = " << consumedWorkerBuildCount;
                civLog << ", exising worker count = " << existingWorkersCount << ", existing consumed workers count = " << existingConsumedWorkersCount;*/
                    
                civLog << "\n max research rate = " << maxResearchRate << ", with processes = " << maxResearchRateWithProcesses;
                
                if (bestEconomicProcess != NO_PROCESS)
                {
                    civLog << "\nbest economic process = " << gGlobals.getProcessInfo(bestEconomicProcess).getType()
                       << " output = " << tacticSelectionData.processOutputsMap[bestEconomicProcess];
                }
                if (bestResearchProcess != NO_PROCESS)
                {
                    civLog << "\nbest research process = " << gGlobals.getProcessInfo(bestResearchProcess).getType()
                       << " output = " << tacticSelectionData.processOutputsMap[bestResearchProcess];
                }
                if (bestCultureProcess != NO_PROCESS)
                {
                    civLog << "\nbest culture process = " << gGlobals.getProcessInfo(bestCultureProcess).getType()
                       << " output = " << tacticSelectionData.processOutputsMap[bestCultureProcess];
                }

                if (bestSmallCultureBuilding != NO_BUILDING)
                {
                    civLog << "\nbestSmallCultureBuilding = " << gGlobals.getBuildingInfo(bestSmallCultureBuilding).getType();
                }
                if (bestLargeCultureBuilding != NO_BUILDING)
                {
                    civLog << "\nbestLargeCultureBuilding = " << gGlobals.getBuildingInfo(bestLargeCultureBuilding).getType();
                }
                if (bestEconomicBuilding != NO_BUILDING)
                {
                    civLog << "\nbestEconomicBuilding = " << gGlobals.getBuildingInfo(bestEconomicBuilding).getType();
                }

                civLog << "\nCity selection for: " << narrow(city.getCvCity()->getName()) << " ";
                for (std::list<const EconomicBuildingValue*>::const_iterator li(economicBuildsData.cityBuildItems.begin()), liEnd(economicBuildsData.cityBuildItems.end()); li != liEnd; ++li)
                {
                    (*li)->debug(civLog);
                }

                civLog << "\nThis city selection for: " << narrow(city.getCvCity()->getName()) << " ";
                for (std::list<const EconomicBuildingValue*>::const_iterator li(thisCityEconomicBuildsData.cityBuildItems.begin()), liEnd(thisCityEconomicBuildsData.cityBuildItems.end()); li != liEnd; ++li)
                {
                    (*li)->debug(civLog);
                }

                if (bestEconomicWonder != NO_BUILDING)
                {
                    civLog << "\nbestEconomicWonder = " << gGlobals.getBuildingInfo(bestEconomicWonder).getType();
                }
                if (bestEconomicNationalWonder != NO_BUILDING)
                {
                    civLog << "\nbestEconomicNationalWonder = " << gGlobals.getBuildingInfo(bestEconomicNationalWonder).getType();
                }
                switch (bestDependentBuild.first)
                {
                    case BuildingItem:
                        civLog << "\nBest dependent build: " << gGlobals.getBuildingInfo((BuildingTypes)bestDependentBuild.second).getType();
                        break;
                    case UnitItem:
                        civLog << "\nBest dependent build: " << gGlobals.getUnitInfo((UnitTypes)bestDependentBuild.second).getType();
                        break;
                    default:
                        break;
                }
                if (bestMilitaryBuilding != NO_BUILDING)
                {
                    civLog << "\nbestMilitaryBuilding = " << gGlobals.getBuildingInfo(bestMilitaryBuilding).getType();
                }
                if (bestCombatUnit != NO_UNIT)
                {
                    civLog << "\nbestCombatUnit = " << gGlobals.getUnitInfo(bestCombatUnit).getType();
                }
                if (bestCityDefenceUnit != NO_UNIT)
                {
                    civLog << "\nbestCityDefenceUnit = " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType();
                }
                if (bestScoutUnit != NO_UNIT)
                {
                    civLog << "\nbestScoutUnit = " << gGlobals.getUnitInfo(bestScoutUnit).getType();
                }
                if (bestSeaScoutUnit != NO_UNIT)
                {
                    civLog << "\nbestSeaScoutUnit = " << gGlobals.getUnitInfo(bestSeaScoutUnit).getType();
                }
                if (bestCollateralUnit != NO_UNIT)
                {
                    civLog << "\nbestCollateralUnit = " << gGlobals.getUnitInfo(bestCollateralUnit).getType();
                }
                if (bestSeaDefenceUnit != NO_UNIT)
                {
                    civLog << "\nbestSeaDefenceUnit = " << gGlobals.getUnitInfo(bestSeaDefenceUnit).getType();
                }
                if (bestSeaTransportUnit != NO_UNIT)
                {
                    civLog << "\nbestSeaTransportUnit = " << gGlobals.getUnitInfo(bestSeaTransportUnit).getType();
                }
                if (bestDependentTacticUnit != NO_UNIT)
                {
                    civLog << "\nbestDependentTacticUnit = " << gGlobals.getUnitInfo(bestDependentTacticUnit).getType();
                }
                civLog << "\nbest counter units:";
                for (std::map<UnitCombatTypes, UnitTypes>::const_iterator ci(bestUnitCombatTypes.begin()), ciEnd(bestUnitCombatTypes.end()); ci != ciEnd; ++ci)
                {
                    civLog << gGlobals.getUnitCombatInfo(ci->first).getType() << " = " << gGlobals.getUnitInfo(ci->second).getType() << ", ";
                }

                civLog << "\n\nCombat units: ";
                for (size_t i = 0, count = combatUnits.size(); i < count; ++i)
                {
                    civLog << gGlobals.getUnitInfo(combatUnits[i]).getType() << ", ";
                }
                civLog << "\nCity defence units: ";
                for (size_t i = 0, count = cityDefenceUnits.size(); i < count; ++i)
                {
                    civLog << gGlobals.getUnitInfo(cityDefenceUnits[i]).getType() << ", ";
                }
                civLog << "\nCity attack units: ";
                for (size_t i = 0, count = cityAttackUnits.size(); i < count; ++i)
                {
                    civLog << gGlobals.getUnitInfo(cityAttackUnits[i]).getType() << ", ";
                }
                civLog << "\nCollateral units: ";
                for (size_t i = 0, count = collateralUnits.size(); i < count; ++i)
                {
                    civLog << gGlobals.getUnitInfo(collateralUnits[i]).getType() << ", ";
                }
                civLog << "\nSea defence units: ";
                for (size_t i = 0, count = seaDefenceUnits.size(); i < count; ++i)
                {
                    civLog << gGlobals.getUnitInfo(seaDefenceUnits[i]).getType() << ", ";
                }
                for (std::map<UnitCombatTypes, std::vector<UnitTypes> >::const_iterator ci(counterUnits.begin()), ciEnd(counterUnits.end()); ci != ciEnd; ++ci)
                {
                    if (!ci->second.empty())
                    {
                        civLog << "\nBest counter units for: " << gGlobals.getUnitCombatInfo(ci->first).getType() << ": ";
                        for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                        {
                            civLog << gGlobals.getUnitInfo(ci->second[i]).getType() << ", ";
                        }
                    }
                }
#endif
            }

            void calculateCurrentOutput()
            {
                currentCivOutput = TotalOutput();
                currentCityOutput = city.getCityData()->getOutput();

                CityIter cityIter(*player.getCvPlayer());
                while (CvCity* pLoopCity = cityIter())
                {
                    const City& loopCity = player.getCity(pLoopCity);
                    currentCivOutput += loopCity.getCityData()->getOutput();  // use this output fn as this is what is stored in the ProjectionLadders
                }
            }

            void calculateImprovementStats()
            {
                const std::vector<PlotImprovementData>& improvements = city.getCityImprovementManager()->getImprovements();

                for (size_t i = 0, count = improvements.size(); i < count; ++i)
                {
                    if (improvements[i].simulationData.firstTurnWorked != -1)
                    {
                        XYCoords coords(improvements[i].coords);
                        const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                        if (!city.getCvCity()->isWorkingPlot(pPlot))
                        {
                            ++unworkedGoodImprovementCount;
                        }
                    }
                }
            }

            void calculateSmallCultureBuilding()
            {
#ifdef ALTAI_DEBUG
                for (std::multiset<CultureBuildingValue>::const_iterator ci(tacticSelectionData.smallCultureBuildings.begin()), ciEnd(tacticSelectionData.smallCultureBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 20, 1);
                        TotalOutputValueFunctor valueF(weights);

                        civLog << "\n(Small Culture Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType()
                               << " turns = " << ci->nTurns << ", delta = " << ci->output << " value = " << valueF(ci->output) / std::max<int>(1, ci->nTurns);
                    }
                }
#endif
                int bestValue = 0;
                TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 10, 1);
                TotalOutputValueFunctor valueF(weights);

                for (std::multiset<CultureBuildingValue>::const_iterator ci(tacticSelectionData.smallCultureBuildings.begin()), 
                    ciEnd(tacticSelectionData.smallCultureBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        int thisValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                        if (thisValue > bestValue &&
                            tacticSelectionData.exclusions.find(ci->buildingType) == tacticSelectionData.exclusions.end())
                        {
                            bestValue = thisValue;
                            bestSmallCultureBuilding = ci->buildingType;
                        }
                    }
                }
            }

            void calculateLargeCultureBuilding()
            {
#ifdef ALTAI_DEBUG
                for (std::multiset<CultureBuildingValue>::const_iterator ci(tacticSelectionData.largeCultureBuildings.begin()),
                    ciEnd(tacticSelectionData.largeCultureBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 20, 1);
                        TotalOutputValueFunctor valueF(weights);

                        civLog << "\n(Large Culture Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType()
                               << " turns = " << ci->nTurns << ", delta = " << ci->output << " value = " << valueF(ci->output) / std::max<int>(1, ci->nTurns);
                    }
                }
#endif

                int bestValue = 0;
                TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 10, 1);
                TotalOutputValueFunctor valueF(weights);

                for (std::multiset<CultureBuildingValue>::const_iterator ci(tacticSelectionData.largeCultureBuildings.begin()),
                    ciEnd(tacticSelectionData.largeCultureBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        int thisValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                        if (thisValue > bestValue &&
                            tacticSelectionData.exclusions.find(ci->buildingType) == tacticSelectionData.exclusions.end())
                        {
                            bestValue = thisValue;
                            bestLargeCultureBuilding = ci->buildingType;
                        }
                    }
                }
            }

            void calculateBestEconomicBuilding()
            {
#ifdef ALTAI_DEBUG
                {
                    TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 0, 0);
                    TotalOutputValueFunctor valueF(weights);

                    for (std::multiset<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin()), ciEnd(tacticSelectionData.economicBuildings.end()); ci != ciEnd; ++ci)
                    {
                        if (ci->city == city.getCvCity()->getIDInfo())
                        {
                            civLog << "\n(Economic Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType()
                                   << " turns = " << ci->nTurns << ", delta = " << ci->output << " value = " << (valueF(ci->output) / (ci->nTurns == 0 ? 1 : ci->nTurns));
                        }
                    }
                }
#endif
                int bestValue = 0;
                TotalOutputWeights /*weights = makeOutputW(20, 20, 20, 20, 1, 1), */ pureEconomicWeights = makeOutputW(10, 10, 10, 10, 0, 0);
                TotalOutputValueFunctor /*valueF(weights), */ econValueF(pureEconomicWeights);
                
                for (std::multiset<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin()), ciEnd(tacticSelectionData.economicBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        //int thisValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                        int thisValue = econValueF(ci->output) / std::max<int>(1, ci->nTurns);
                        if (thisValue > bestValue && // econValueF(ci->output) > 0 &&
                            tacticSelectionData.exclusions.find(ci->buildingType) == tacticSelectionData.exclusions.end())
                        {
                            bestValue = thisValue;
                            bestEconomicBuilding = ci->buildingType;
                        }
                    }
                }
            }

            void calculateBestEconomicWonder()
            {
                int bestValue = 0, bestValueBuiltTurns = 0;
                BuildingTypes bestWonder = NO_BUILDING;
                TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 1, 1);
                TotalOutputValueFunctor valueF(weights);

                for (std::map<BuildingTypes, EconomicWonderValue>::const_iterator ci(tacticSelectionData.economicWonders.begin()), ciEnd(tacticSelectionData.economicWonders.end());
                    ci != ciEnd; ++ci)
                {
                    int firstBuiltTurn = MAX_INT, thisCityBuiltTurn = MAX_INT;
                    TotalOutput bestDelta, thisDelta;
                    IDInfo firstBuiltCity;
                    
                    for (size_t i = 0, count = ci->second.buildCityValues.size(); i < count; ++i)
                    {
                        const int thisBuiltTurn = ci->second.buildCityValues[i].second.nTurns;
                        if (ci->second.buildCityValues[i].second.buildingType != NO_BUILDING && thisBuiltTurn < firstBuiltTurn)
                        {
                            firstBuiltTurn = ci->second.buildCityValues[i].second.nTurns;
                            firstBuiltCity = ci->second.buildCityValues[i].first;
                            bestDelta = ci->second.buildCityValues[i].second.output;
                        }

                        if (ci->second.buildCityValues[i].first == city.getCvCity()->getIDInfo())
                        {
                            thisCityBuiltTurn = ci->second.buildCityValues[i].second.nTurns;
                            thisDelta = ci->second.buildCityValues[i].second.output;
                        }
                    }

                    if (firstBuiltCity == city.getCvCity()->getIDInfo() || 4 * thisCityBuiltTurn / 5 < firstBuiltTurn)
                    {
                        int thisValue = valueF(thisDelta);
                        if (thisValue > bestValue)
                        {
                            bestValue = thisValue;
                            bestValueBuiltTurns = thisCityBuiltTurn;
                            bestWonder = ci->first;
                        }
                    }

#ifdef ALTAI_DEBUG
                    civLog << "\nWonder: " << gGlobals.getBuildingInfo(ci->first).getType();

                    if (firstBuiltCity.eOwner != NO_PLAYER)
                    {
                        civLog << " best build time = " << firstBuiltTurn << ", in city: " << narrow(getCity(firstBuiltCity)->getName()) << " delta = " << bestDelta
                               << " this city build time = " << thisCityBuiltTurn << ", delta = " << thisDelta;
                    }
                    else
                    {
                        civLog << " not built anywhere";
                    }
#endif
                }

                if (bestWonder != NO_BUILDING)
                {
                    bestEconomicWonder = bestWonder;
                    if (!tacticSelectionData.economicBuildings.empty())
                    {
                        std::multiset<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin());
                        if (valueF(ci->output) / std::max<int>(1, ci->nTurns) < bestValue / bestValueBuiltTurns)
                        {
                            bestEconomicBuilding = bestEconomicWonder;
                        }
                    }
                }
            }

            void calculateBestEconomicNationalWonder()
            {
                int overallBestValue = 0, overallBestValueTurns = MAX_INT;
                BuildingTypes bestNationalWonder = NO_BUILDING;
                TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 1, 1);
                TotalOutputValueFunctor valueF(weights);

                for (std::map<BuildingTypes, EconomicWonderValue>::const_iterator ci(tacticSelectionData.nationalWonders.begin()), ciEnd(tacticSelectionData.nationalWonders.end());
                    ci != ciEnd; ++ci)
                {
                    if (tacticSelectionData.exclusions.find(ci->first) != tacticSelectionData.exclusions.end())
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\nSkipping National Wonder: " << gGlobals.getBuildingInfo(ci->first).getType();
#endif
                        continue;
                    }

                    int bestBuiltTurn = MAX_INT, thisCityBuiltTurn = MAX_INT;
                    TotalOutput bestDelta, thisDelta;
                    IDInfo bestBuiltCity;
                    
                    for (size_t i = 0, count = ci->second.buildCityValues.size(); i < count; ++i)
                    {
                        if (ci->second.buildCityValues[i].second.buildingType != NO_BUILDING)
                        {
                            const TotalOutput delta = ci->second.buildCityValues[i].second.output;
                            if (valueF(delta) > valueF(bestDelta))
                            {
                                bestDelta = delta;
                                bestBuiltTurn = ci->second.buildCityValues[i].second.nTurns;
                                bestBuiltCity = ci->second.buildCityValues[i].first;
                            }

                            if (ci->second.buildCityValues[i].first == city.getCvCity()->getIDInfo())
                            {
                                thisDelta = delta;
                                thisCityBuiltTurn = ci->second.buildCityValues[i].second.nTurns;
                            }
                        }
                    }

                    if (bestBuiltCity == city.getCvCity()->getIDInfo())
                    {
                        int thisValue = valueF(bestDelta);
                        if (thisValue > overallBestValue)
                        {
                            overallBestValue = thisValue;
                            overallBestValueTurns = bestBuiltTurn;
                            bestNationalWonder = ci->first;
                        }
                    }

#ifdef ALTAI_DEBUG
                    civLog << "\nNational Wonder: " << gGlobals.getBuildingInfo(ci->first).getType();

                    if (bestBuiltCity.eOwner != NO_PLAYER)
                    {
                        civLog << " best build time = " << bestBuiltTurn << ", in city: " << narrow(getCity(bestBuiltCity)->getName()) << " delta = " << bestDelta
                               << " this city build time = " << thisCityBuiltTurn << ", delta = " << thisDelta;
                    }
                    else
                    {
                        civLog << " not built anywhere or no value";
                    }
#endif
                }

                if (bestNationalWonder != NO_BUILDING)
                {
                    bestEconomicNationalWonder = bestNationalWonder;
                    TotalOutput bestEconomicOutput = tacticSelectionData.getEconomicBuildingOutput(bestEconomicBuilding, city.getCvCity()->getIDInfo());
                    if (valueF(bestEconomicOutput) > overallBestValue)
                    {
                        bestEconomicBuilding = bestEconomicNationalWonder;
                    }
#ifdef ALTAI_DEBUG
                    civLog << "\nBest National Wonder: " << gGlobals.getBuildingInfo(bestNationalWonder).getType()
                           << " value = " << overallBestValue << ", build time = " << overallBestValueTurns;
#endif
                }
            }

            void calculateBestEconomicDependentTactic()
            {
                TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 0, 0);
                TotalOutputValueFunctor valueF(weights);
                int bestValue = 0;
                IDInfo bestCity;
                BuildingTypes bestBuildingType = NO_BUILDING;
                std::pair<BuildQueueTypes, int> buildItem(NoItem, -1);

                for (std::map<BuildingTypes, std::vector<BuildingTypes> >::const_iterator ci(tacticSelectionData.buildingsCityCanAssistWith.begin()),
                    ciEnd(tacticSelectionData.buildingsCityCanAssistWith.end()); ci != ciEnd; ++ci)
                {
                    PlayerTactics::LimitedBuildingsTacticsMap::const_iterator li = playerTactics.globalBuildingsTacticsMap_.find(ci->first);
                    if (li != playerTactics.globalBuildingsTacticsMap_.end())
                    {
                        /*for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                        {
                            PlayerTactics::CityBuildingTacticsList::const_iterator li = ti->second.find(ci->second[i]);
                            if (li != ti->second.end())
                            {
                                const ProjectionLadder& ladder = li->second->getProjection();
                                if (!ladder.buildings.empty())
                                {
                                    int nTurns = ladder.buildings[0].first;
                                    int thisValue = 0;
                                    if (!ladder.comparisons.empty())
                                    {
                                        thisValue = valueF(ladder.getOutput() - ladder.comparisons[0].getOutput()) / std::max<int>(1, nTurns);
                                    }
                                    if (thisValue > bestValue)
                                    {
                                        bestValue = thisValue;
                                        bestBuildingType = li->second->getBuildingType();
                                        bestCity = ci->first;

                                        const std::vector<IDependentTacticPtr>& dependentTactics = li->second->getDependencies();
                                        if (!dependentTactics.empty())
                                        {
                                            buildItem = dependentTactics[0]->getBuildItem();
                                        }
                                    }
                                }
                            }
                        }*/
                    }
                }

                bestDependentBuild = buildItem;

#ifdef ALTAI_DEBUG
                if (bestBuildingType != NO_BUILDING)
                {
                    civLog << "\nBest city to help: " << narrow(getCity(bestCity)->getName()) << " with building: "
                           << gGlobals.getBuildingInfo(bestBuildingType).getType() << " with value = " << bestValue;
                    if (buildItem.first == BuildingItem)
                    {
                        civLog << " build item = " << gGlobals.getBuildingInfo((BuildingTypes)buildItem.second).getType();
                    }
                    else if (buildItem.first == UnitItem)
                    {
                        civLog << " build item = " << gGlobals.getUnitInfo((UnitTypes)buildItem.second).getType();
                    }

                    if (!tacticSelectionData.economicBuildings.empty())
                    {
                        TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 0, 0);
                        TotalOutputValueFunctor valueF(weights);

                        std::multiset<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin());
                        int ownValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                        civLog << ", best own building value = " << ownValue << " for: " << gGlobals.getBuildingInfo(ci->buildingType).getType();
                    }
                }
#endif
            }

            void calculateBestLocalEconomicDependentTactic()
            {
                TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 0, 0);
                TotalOutputValueFunctor valueF(weights);
                int bestValue = 0;
                IDInfo bestCity;
                BuildingTypes bestBuildingType = NO_BUILDING;
                std::pair<BuildQueueTypes, int> buildItem(NoItem, -1);

                TotalOutput base = city.getCurrentOutputProjection().getOutput();
                PlayerTactics::CityBuildingTacticsMap::const_iterator ci = playerTactics.cityBuildingTacticsMap_.find(city.getCvCity()->getIDInfo());

                for (std::map<BuildingTypes, std::vector<BuildingTypes> >::const_iterator di(tacticSelectionData.dependentBuildings.begin()),
                    diEnd(tacticSelectionData.dependentBuildings.end()); di != diEnd; ++di)
                {
                    for (size_t i = 0, count = di->second.size(); i < count; ++i)
                    {
                        PlayerTactics::CityBuildingTacticsList::const_iterator li = ci->second.find(di->second[i]);
                        if (li != ci->second.end())
                        {
                            const ProjectionLadder& ladder = li->second->getProjection();
                            if (!ladder.buildings.empty())
                            {
                                int nTurns = ladder.buildings[0].first;
                                int thisValue = valueF(ladder.getOutput() - base) / std::max<int>(1, nTurns);
                                if (thisValue > bestValue)
                                {
                                    bestValue = thisValue;
                                    bestBuildingType = li->second->getBuildingType();
                                    bestCity = ci->first;

                                    const std::vector<IDependentTacticPtr>& dependentTactics = li->second->getDependencies();
                                    if (!dependentTactics.empty())
                                    {
                                        buildItem = dependentTactics[0]->getBuildItem();
                                    }
                                }
                            }
                        }
                    }
                }

                bestDependentBuild = buildItem;

#ifdef ALTAI_DEBUG
                if (bestBuildingType != NO_BUILDING)
                {
                    civLog << "\nBest build for this city to help itself: " << narrow(getCity(bestCity)->getName()) << " with building: "
                           << gGlobals.getBuildingInfo(bestBuildingType).getType() << " with value = " << bestValue;
                    if (buildItem.first == BuildingItem)
                    {
                        civLog << " build item = " << gGlobals.getBuildingInfo((BuildingTypes)buildItem.second).getType();
                    }
                    else if (buildItem.first == UnitItem)
                    {
                        civLog << " build item = " << gGlobals.getUnitInfo((UnitTypes)buildItem.second).getType();
                    }
                }
#endif
            }

            std::pair<int, BuildingTypes> getBestMilitaryBuilding(UnitTypes unitType, const BuildingConditions& buildingConditions = BuildingConditions())
            {
                std::multimap<int, const MilitaryBuildingValue*> militaryBuildingValuesMapForUnit;

                for (std::set<MilitaryBuildingValue>::const_iterator ci(tacticSelectionData.militaryBuildings.begin()), ciEnd(tacticSelectionData.militaryBuildings.end()); ci != ciEnd; ++ci)
                {
                    // check we built this - could be a wonder built elsewhere - as only keep best cities for those
                    if (bestMilitaryBuilding != NO_BUILDING && ci->city == city.getCvCity()->getIDInfo())
                    {
                        bestMilitaryBuilding = ci->buildingType;
                    }
                    int defenceValue = 0, attackValue = 0, collateralValue = 0;
                    for (std::set<UnitTacticValue>::const_iterator unitIter(ci->cityDefenceUnits.begin()), unitEndIter(ci->cityDefenceUnits.end()); unitIter != unitEndIter; ++unitIter)
                    {
                        if (unitIter->unitType == unitType)
                        {
                            std::map<UnitTypes, int>::const_iterator baseUnitValueIter = unitDefenceValues.find(unitType);
                            if (baseUnitValueIter != unitDefenceValues.end())
                            {
                                defenceValue += (std::max<int>(0, (unitIter->unitAnalysisValue - baseUnitValueIter->second)) * 100) / baseUnitValueIter->second;
                            }
                            break;
                        }
                    }                    
                    for (std::set<UnitTacticValue>::const_iterator unitIter(ci->cityAttackUnits.begin()), unitEndIter(ci->cityAttackUnits.end()); unitIter != unitEndIter; ++unitIter)
                    {
                        if (unitIter->unitType == unitType)
                        {
                            std::map<UnitTypes, int>::const_iterator baseUnitValueIter = unitAttackValues.find(unitType);
                            if (baseUnitValueIter != unitAttackValues.end())
                            {
                                attackValue += (std::max<int>(0, (unitIter->unitAnalysisValue - baseUnitValueIter->second)) * 100) / baseUnitValueIter->second;
                            }
                            break;
                        }
                    }
                    for (std::set<UnitTacticValue>::const_iterator unitIter(ci->collateralUnits.begin()), unitEndIter(ci->collateralUnits.end()); unitIter != unitEndIter; ++unitIter)
                    {
                        if (unitIter->unitType == unitType)
                        {
                            std::map<UnitTypes, int>::const_iterator baseUnitValueIter = unitCollateralValues.find(unitType);
                            if (baseUnitValueIter != unitCollateralValues.end())
                            {
                                collateralValue += (std::max<int>(0, (unitIter->unitAnalysisValue - baseUnitValueIter->second)) * 100) / baseUnitValueIter->second;
                            }
                            break;
                        }
                    }
                    militaryBuildingValuesMapForUnit.insert(std::make_pair(defenceValue + attackValue + collateralValue, &*ci));
                }

                std::multimap<int, const MilitaryBuildingValue*>::const_reverse_iterator vi = militaryBuildingValuesMapForUnit.rbegin();
#ifdef ALTAI_DEBUG
                civLog << "\nBest military building for unit: " << gGlobals.getUnitInfo(unitType).getType() << " : "
                       << (militaryBuildingValuesMapForUnit.empty() ? "none" : gGlobals.getBuildingInfo(vi->second->buildingType).getType())
                       << " value = "
                       << (militaryBuildingValuesMapForUnit.empty() ? 0 : vi->first);
#endif
                return (militaryBuildingValuesMapForUnit.empty() ? std::make_pair(0, NO_BUILDING) : std::make_pair(vi->first, vi->second->buildingType));
            }

            std::pair<int, BuildingTypes> getBestDefensiveMilitaryBuilding(const BuildingConditions& buildingConditions = BuildingConditions())
            {
                std::multimap<int, const MilitaryBuildingValue*> thisCityMilitaryBuildingValuesMapForUnit;

                for (std::set<MilitaryBuildingValue>::const_iterator ci(tacticSelectionData.militaryBuildings.begin()), ciEnd(tacticSelectionData.militaryBuildings.end()); ci != ciEnd; ++ci)
                {
                    int thisCityDefenceValue = 0;
                    for (std::set<UnitTacticValue>::const_iterator unitIter(ci->thisCityDefenceUnits.begin()), unitEndIter(ci->thisCityDefenceUnits.end()); unitIter != unitEndIter; ++unitIter)
                    {
                        // count all units for city defence case
                        std::map<UnitTypes, int>::const_iterator baseUnitValueIter = thisCityUnitDefenceValues.find(unitIter->unitType);
                        if (baseUnitValueIter != thisCityUnitDefenceValues.end())
                        {
                            thisCityDefenceValue += (std::max<int>(0, (unitIter->unitAnalysisValue - baseUnitValueIter->second)) * 100) / baseUnitValueIter->second;
                        }
                    }
                    thisCityMilitaryBuildingValuesMapForUnit.insert(std::make_pair(thisCityDefenceValue, &*ci));
                }

                std::multimap<int, const MilitaryBuildingValue*>::const_reverse_iterator vi = thisCityMilitaryBuildingValuesMapForUnit.rbegin();
#ifdef ALTAI_DEBUG
                civLog << "\nBest defence building: "
                       << (thisCityMilitaryBuildingValuesMapForUnit.empty() ? "none" : gGlobals.getBuildingInfo(vi->second->buildingType).getType())
                       << " value = "
                       << (thisCityMilitaryBuildingValuesMapForUnit.empty() ? 0 : vi->first);
#endif
                return (thisCityMilitaryBuildingValuesMapForUnit.empty() ? std::make_pair(0, NO_BUILDING) : std::make_pair(vi->first, vi->second->buildingType));
            }

            void calculateUnitValues()
            {
                for (std::set<UnitTacticValue>::const_iterator unitIter(tacticSelectionData.cityDefenceUnits.begin()), unitEndIter(tacticSelectionData.cityDefenceUnits.end()); unitIter != unitEndIter; ++unitIter)
                {
                    unitDefenceValues[unitIter->unitType] = unitIter->unitAnalysisValue;
                }
                for (std::set<UnitTacticValue>::const_iterator unitIter(tacticSelectionData.thisCityDefenceUnits.begin()), unitEndIter(tacticSelectionData.thisCityDefenceUnits.end()); unitIter != unitEndIter; ++unitIter)
                {
                    thisCityUnitDefenceValues[unitIter->unitType] = unitIter->unitAnalysisValue;
                }
                for (std::set<UnitTacticValue>::const_iterator unitIter(tacticSelectionData.cityAttackUnits.begin()), unitEndIter(tacticSelectionData.cityAttackUnits.end()); unitIter != unitEndIter; ++unitIter)
                {
                    unitAttackValues[unitIter->unitType] = unitIter->unitAnalysisValue;
                }
                for (std::set<UnitTacticValue>::const_iterator unitIter(tacticSelectionData.collateralUnits.begin()), unitEndIter(tacticSelectionData.collateralUnits.end()); unitIter != unitEndIter; ++unitIter)
                {
                    unitCollateralValues[unitIter->unitType] = unitIter->unitAnalysisValue;
                }
            }

            void calculateBestMilitaryBuilding()
            {
                for (std::set<MilitaryBuildingValue>::const_iterator ci(tacticSelectionData.militaryBuildings.begin()), ciEnd(tacticSelectionData.militaryBuildings.end()); ci != ciEnd; ++ci)
                {
                    // check we built this - could be a wonder built elsewhere - as only keep best cities for those
                    if (bestMilitaryBuilding != NO_BUILDING && ci->city == city.getCvCity()->getIDInfo())
                    {
                        bestMilitaryBuilding = ci->buildingType;
                    }
#ifdef ALTAI_DEBUG
                    civLog << "\n(Military Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType() << " turns = " << ci->nTurns;
#endif
                    int defenceValue = 0, thisCityDefenceValue = 0, attackValue = 0, collateralValue = 0;
                    for (std::set<UnitTacticValue>::const_iterator unitIter(ci->cityDefenceUnits.begin()), unitEndIter(ci->cityDefenceUnits.end()); unitIter != unitEndIter; ++unitIter)
                    {
                        std::map<UnitTypes, int>::const_iterator baseUnitValueIter = unitDefenceValues.find(unitIter->unitType);
                        if (baseUnitValueIter != unitDefenceValues.end())
                        {
                            defenceValue += (std::max<int>(0, (unitIter->unitAnalysisValue - baseUnitValueIter->second)) * 100) / baseUnitValueIter->second;
                        }
                    }
                    for (std::set<UnitTacticValue>::const_iterator unitIter(ci->thisCityDefenceUnits.begin()), unitEndIter(ci->thisCityDefenceUnits.end()); unitIter != unitEndIter; ++unitIter)
                    {
                        std::map<UnitTypes, int>::const_iterator baseUnitValueIter = thisCityUnitDefenceValues.find(unitIter->unitType);
                        if (baseUnitValueIter != thisCityUnitDefenceValues.end())
                        {
                            thisCityDefenceValue += (std::max<int>(0, (unitIter->unitAnalysisValue - baseUnitValueIter->second)) * 100) / baseUnitValueIter->second;
                        }
                    }
                    for (std::set<UnitTacticValue>::const_iterator unitIter(ci->cityAttackUnits.begin()), unitEndIter(ci->cityAttackUnits.end()); unitIter != unitEndIter; ++unitIter)
                    {
                        std::map<UnitTypes, int>::const_iterator baseUnitValueIter = unitAttackValues.find(unitIter->unitType);
                        if (baseUnitValueIter != unitAttackValues.end())
                        {
                            attackValue += (std::max<int>(0, (unitIter->unitAnalysisValue - baseUnitValueIter->second)) * 100) / baseUnitValueIter->second;
                        }
                    }
                    for (std::set<UnitTacticValue>::const_iterator unitIter(ci->collateralUnits.begin()), unitEndIter(ci->collateralUnits.end()); unitIter != unitEndIter; ++unitIter)
                    {
                        std::map<UnitTypes, int>::const_iterator baseUnitValueIter = unitCollateralValues.find(unitIter->unitType);
                        if (baseUnitValueIter != unitCollateralValues.end())
                        {
                            collateralValue += (std::max<int>(0, (unitIter->unitAnalysisValue - baseUnitValueIter->second)) * 100) / baseUnitValueIter->second;
                        }
                    }

                    militaryBuildingValuesMap.insert(std::make_pair(defenceValue + attackValue + collateralValue, &*ci));
                    thisCityMilitaryBuildingValuesMap.insert(std::make_pair(thisCityDefenceValue, &*ci));
#ifdef ALTAI_DEBUG
                    civLog << "\t% increases, defence: " << defenceValue << ", this city: " << thisCityDefenceValue
                           << ", attack: " << attackValue << ", collateral: " << collateralValue;
#endif
                }
            }

            void calculateBestProcesses()
            {
                // for now: treat gold equal to research (if we can then build gold, it allows a higher research rate, but we may have better multipliers for research)
                TotalOutputWeights economicWeights = makeOutputW(0, 0, 1, 1, 0, 0), researchWeights = makeOutputW(0, 0, 0, 1, 0, 0), cultureWeights = makeOutputW(0, 0, 0, 0, 1, 0);
                TotalOutputValueFunctor econValueF(economicWeights), researchValueF(researchWeights), cultureValueF(cultureWeights);
                int bestEconValue = 0, bestResearchValue = 0, bestCultureValue = 0;

                for (std::map<ProcessTypes, TotalOutput>::const_iterator ci(tacticSelectionData.processOutputsMap.begin()), ciEnd(tacticSelectionData.processOutputsMap.end());
                    ci != ciEnd; ++ci)
                {
                    int thisValue = econValueF(ci->second);
                    if (thisValue > bestEconValue)
                    {
                        bestEconValue = thisValue;
                        bestEconomicProcess = ci->first;
                    }

                    thisValue = researchValueF(ci->second);
                    if (thisValue > bestResearchValue)
                    {
                        bestResearchValue = thisValue;
                        bestResearchProcess = ci->first;
                    }

                    thisValue = cultureValueF(ci->second);
                    if (thisValue > bestCultureValue)
                    {
                        bestCultureValue = thisValue;
                        bestCultureProcess = ci->first;
                    }
                }
            }

            void countCombatUnits()
            {
                for (size_t i = 0, count = combatUnits.size(); i < count; ++i)
                {
                    const int thisUnitCount = player.getUnitCount(combatUnits[i]);
                    if (combatUnits[i] != bestCityDefenceUnit)
                    {
                        attackUnitCount += thisUnitCount;
                    }
                    combatUnitCount += thisUnitCount;
                }
            }

            void calculateBestCombatUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.cityAttackUnits.begin()), ciEnd(tacticSelectionData.cityAttackUnits.end()); ci != ciEnd; ++ci)
                {
                    unitValuesMap.insert(std::make_pair(ci->unitAnalysisValue, ci->unitType));
#ifdef ALTAI_DEBUG
                    civLog << "\n(Combat Unit): " << gGlobals.getUnitInfo(ci->unitType).getType()
                           << " turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue;
#endif
                }

                int bestValue = 0;
                combatUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (167 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    if (gGlobals.getUnitInfo(valueIter->second).getCombatLimit() == 100)
                    {
                        combatUnits.push_back(valueIter->second);
                    }
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

                for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.cityAttackUnits.begin()), ciEnd(tacticSelectionData.cityAttackUnits.end()); ci != ciEnd; ++ci)
                {
                    unitValuesMap.insert(std::make_pair(ci->unitAnalysisValue, ci->unitType));
#ifdef ALTAI_DEBUG
                    civLog << "\n(City Attack Unit): " << gGlobals.getUnitInfo(ci->unitType).getType()
                           << " turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue;
#endif
                }

                int bestValue = 0;
                cityAttackUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (120 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    cityAttackUnits.push_back(valueIter->second);
                    ++valueIter;
                }
            }

            void calculateBestScoutUnit()
            {
                int bestValue = 0;
                for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.scoutUnits.begin()), ciEnd(tacticSelectionData.scoutUnits.end()); ci != ciEnd; ++ci)
                {
                    if (ci->unitAnalysisValue > bestValue)
                    {
#ifdef ALTAI_DEBUG
                    civLog << "\n(Scout Unit): " << gGlobals.getUnitInfo(ci->unitType).getType()
                           << " turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue;
#endif
                        bestValue = ci->unitAnalysisValue;
                        bestScoutUnit = ci->unitType;
                    }
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
            }

            void calculateBestCityDefenceUnits()
            {
                cityDefenceUnits.clear();
                for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.cityDefenceUnits.begin()), ciEnd(tacticSelectionData.cityDefenceUnits.end()); ci != ciEnd; ++ci)
                {
                    cityDefenceUnits.push_back(ci->unitType);
#ifdef ALTAI_DEBUG
                    civLog << "\n(City Defence Unit): " << gGlobals.getUnitInfo(ci->unitType).getType()
                           << " turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue;
#endif
                }

                if (!tacticSelectionData.cityDefenceUnits.empty())
                {
                    bestCityDefenceUnit = tacticSelectionData.cityDefenceUnits.begin()->unitType;
                }
            }

            void calculateBestCollateralUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.collateralUnits.begin()), ciEnd(tacticSelectionData.collateralUnits.end()); ci != ciEnd; ++ci)
                {
                    unitValuesMap.insert(std::make_pair(ci->unitAnalysisValue, ci->unitType));
#ifdef ALTAI_DEBUG
                    civLog << "\n(Collateral Unit): " << gGlobals.getUnitInfo(ci->unitType).getType()
                           << " turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue;
#endif
                }

                int bestValue = 0;
                collateralUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end() && (200 * valueIter->first) / 100 >= bestValueIter->first)
                {
                    collateralUnits.push_back(valueIter->second);
                    ++valueIter;
                }
            }

            void calculateBestSeaDefenceUnits()
            {
                typedef std::multimap<int, UnitTypes, std::greater<int> > UnitValuesMap;
                UnitValuesMap unitValuesMap;

                for (std::set<UnitTacticValue>::const_iterator ci(tacticSelectionData.seaCombatUnits.begin()), ciEnd(tacticSelectionData.seaCombatUnits.end()); ci != ciEnd; ++ci)
                {
                    unitValuesMap.insert(std::make_pair(ci->unitAnalysisValue, ci->unitType));
#ifdef ALTAI_DEBUG
                    civLog << "\n(Sea Defence Unit): " << gGlobals.getUnitInfo(ci->unitType).getType()
                           << " turns = " << ci->nTurns << ", value = " << ci->unitAnalysisValue;
#endif
                }

                int bestValue = 0;
                seaDefenceUnits.clear();
                UnitValuesMap::const_iterator bestValueIter, valueIter;
                bestValueIter = valueIter = unitValuesMap.begin();
                while (valueIter != unitValuesMap.end())
                {
                    seaDefenceUnits.push_back(valueIter->second);
                    ++valueIter;
                }
            }

            void calculateBestSeaTransportUnits()
            {
            }

            void calculateBestUnits()
            {
            }

            void calcEconomicBuildItems()
            {
                calcPossibleCityBuildItems(currentCityOutput, thisCityEconomicBuildsData);
                calcPossibleCityBuildItems(currentCivOutput, economicBuildsData);
            }

            void calcPossibleCityBuildItems(TotalOutput refOutput, EconomicBuildsData& buildsData)
            {
                const int turnsAvailable = 30;

                std::vector<OutputTypes> outputTypes;
                outputTypes.push_back(OUTPUT_FOOD);
                outputTypes.push_back(OUTPUT_PRODUCTION);
                outputTypes.push_back(OUTPUT_RESEARCH);
                outputTypes.push_back(OUTPUT_GOLD);

                for (std::multiset<EconomicBuildingValue>::const_iterator baseIter(tacticSelectionData.economicBuildings.begin()), baseEndIter(tacticSelectionData.economicBuildings.end());
                    baseIter != baseEndIter; ++baseIter)
                {
                    if (baseIter->city.iID != city.getID())
                    {
                        continue;
                    }
                    if (buildsData.usedCityTurns <= turnsAvailable && TacticSelectionData::isSignificantTacticItem(*baseIter, refOutput, outputTypes))
                    {
                        buildsData.cityBuildItems.push_back(&*baseIter);
                        buildsData.usedCityTurns += baseIter->nTurns;
                    }
                }
            }

            bool chooseCityDefence(const MilitarySelectionConditions& militarySelectionConditions = MilitarySelectionConditions())
            {
                // No defensive unit, but can build one
                if (bestCityDefenceUnit != NO_UNIT)
                {
                    if (city.getCvCity()->getMilitaryHappinessUnits() < militarySelectionConditions.minCityDefenceUnits_ && 
                        setConstructItem(bestCityDefenceUnit))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning best city defence unit: " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << selection;
#endif
                        return true;
                    }
                    /*else
                    {
                        int totalCityCombatUnits = 0;
                        for (size_t i = 0, count = combatUnits.size(); i < count; ++i)
                        {
                            totalCityCombatUnits += city.getCvCity()->plot()->plotCount(PUF_isUnitType, combatUnits[i], -1, city.getCvCity()->getOwner());
                        }
                        if (std::find(combatUnits.begin(), combatUnits.end(), bestCityDefenceUnit) == combatUnits.end())
                        {
                            totalCityCombatUnits += city.getCvCity()->plot()->plotCount(PUF_isUnitType, bestCityDefenceUnit, -1, city.getCvCity()->getOwner());
                        }

                        if (totalCityCombatUnits < militarySelectionConditions.minCityDefenceUnits_ && setConstructItem(bestCityDefenceUnit))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning city defence unit(1): " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << selection
                                << " total city unit count = " << totalCityCombatUnits;
#endif
                            return true;
                        }
                    }*/
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseCityDefence - false";
#endif
                return false;
            }

            bool chooseUnit(UnitTypes requestedUnit)
            {
                UnitTypes combatUnit = chooseUnit(combatUnits), collateralUnit = chooseUnit(collateralUnits), attackUnit = chooseUnit(cityAttackUnits);
                const int thisCombatUnitCount = player.getUnitCount(combatUnit), collateralUnitCount = player.getUnitCount(collateralUnit),
                          thisAttackUnitCount = player.getUnitCount(attackUnit), cityDefenceUnitCount = player.getUnitCount(bestCityDefenceUnit);

                UnitTacticValue combatUnitValue = tacticSelectionData.getUnitValue(tacticSelectionData.cityAttackUnits, combatUnit);
                UnitTacticValue collateralUnitValue = tacticSelectionData.getUnitValue(tacticSelectionData.collateralUnits, collateralUnit);
                UnitTacticValue attackUnitValue = tacticSelectionData.getUnitValue(tacticSelectionData.cityAttackUnits, attackUnit);
                UnitTacticValue cityDefenceUnitValue = tacticSelectionData.getUnitValue(tacticSelectionData.cityDefenceUnits, combatUnit);

                std::set<UnitTypes> units;
                units.insert(combatUnit);
                units.insert(collateralUnit);
                units.insert(attackUnit);

                int totalUnitCount = 0;
                for (size_t i = 0, count = combatUnits.size(); i < count; ++i)
                {
                    totalUnitCount += player.getUnitCount(combatUnits[i]);
                }
                for (size_t i = 0, count = collateralUnits.size(); i < count; ++i)
                {
                    totalUnitCount += player.getUnitCount(collateralUnits[i]);
                }

                int iFreeUnits, iFreeMilitaryUnits, iPaidUnits, iPaidMilitaryUnits, iMilitaryCost, iBaseUnitCost, iExtraCost;
                const int supportCost = player.getCvPlayer()->calculateUnitCost(iFreeUnits, iFreeMilitaryUnits, iPaidUnits, iPaidMilitaryUnits, iMilitaryCost, iBaseUnitCost, iExtraCost);

                int maxUnitCount = 0;
                const int maxResearchRate = player.getMaxResearchRate();

                if (maxResearchRate < 30)
                {
                    maxUnitCount = 1 + cityCount + (popCount / 5);
                }
                else if (maxResearchRate < 60)
                {
                    maxUnitCount = 2 + cityCount + (popCount / 3);
                }
                else
                {
                    maxUnitCount = 1 + 2 * cityCount + (popCount / 2);
                }

#ifdef ALTAI_DEBUG
                civLog << "\nUnit counts: (max unit count = " << maxUnitCount << ", free = "<< iFreeMilitaryUnits << ") ";
                if (combatUnit != NO_UNIT)
                {
                    civLog << "(combatUnit) " << gGlobals.getUnitInfo(combatUnit).getType() << " = " << thisCombatUnitCount << " ";
                }
                if (collateralUnit != NO_UNIT)
                {
                    civLog << "(collateralUnit) " << gGlobals.getUnitInfo(collateralUnit).getType() << " = " << collateralUnitCount << " ";
                }
                if (attackUnit != NO_UNIT)
                {
                    civLog << "(attackUnit) " << gGlobals.getUnitInfo(attackUnit).getType() << " = " << thisAttackUnitCount << " ";
                }
                if (bestCityDefenceUnit != NO_UNIT)
                {
                    civLog << "(bestCityDefenceUnit) " << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << " = " << cityDefenceUnitCount << " ";
                }
                civLog << "total combat unit count = " << totalUnitCount << " max unit count = " << maxUnitCount;
#endif
                //{
                //    // todo - add condition for cities which border other civs
                //    size_t minDefensiveUnits = 1 + (city.getCityData()->getPopulation() / 5);
                //    MilitarySelectionConditions militarySelectionConditions;
                //    militarySelectionConditions.minCityDefenceUnits(minDefensiveUnits);

                //    if (chooseCityDefence(militarySelectionConditions))
                //    {
                //        return true;
                //    }
                //}

                if (player.getCvPlayer()->getNumMilitaryUnits() > maxUnitCount)
                {
#ifdef ALTAI_DEBUG
                    civLog << " player unit count = " << player.getCvPlayer()->getNumMilitaryUnits() << ", ret...";
#endif
                    return false;
                }

                /*{
                    MilitarySelectionConditions militarySelectionConditions;
                    militarySelectionConditions.minCityDefenceUnits(1 + city.getCvCity()->getPopulation() / 10);

                    if (chooseCityDefence(militarySelectionConditions))
                    {
                        return true;
                    }
                }*/

                if (requestedUnit == NO_UNIT && attackUnit == combatUnit && combatUnit == bestCityDefenceUnit)
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) chooseUnit() Returning false";
#endif
                    return false;
                }
                
                BuildingConditions buildConditions;
                buildConditions.noGlobalWonders().noNationalWonders();

                if (requestedUnit != NO_UNIT)
                {
                    UnitTacticValue combatUnitValue = tacticSelectionData.getUnitValue(tacticSelectionData.fieldAttackUnits, requestedUnit);
                    std::pair<int, BuildingTypes> buildingAndValue = getBestMilitaryBuilding(requestedUnit, buildConditions);

                    MilitaryBuildingValue buildingValue = tacticSelectionData.getBuildingValue(tacticSelectionData.militaryBuildings, buildingAndValue.second);
                    if (buildingAndValue.first > 20 && buildingValue.nTurns < combatUnitValue.nTurns * 2 && setConstructItem(buildingAndValue.second))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning mil building for requested unit: " << gGlobals.getUnitInfo(requestedUnit).getType() << selection
                            << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType())
                            << " unit value: ";
                        combatUnitValue.debug(civLog);
#endif
                        return true;
                    }
                    else if (setConstructItem(requestedUnit))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning requested unit: " << gGlobals.getUnitInfo(requestedUnit).getType() << selection
                            << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());
#endif
                        return true;
                    }
                }

                if (attackUnit != combatUnit && attackUnit != NO_UNIT)
                {
                    if (thisAttackUnitCount <= thisCombatUnitCount && thisAttackUnitCount < 1 + maxUnitCount / 3)
                    {
                        UnitTacticValue thisUnitValue = (attackUnitValue.unitType == attackUnit ? attackUnitValue :
                            (collateralUnitValue.unitType == attackUnit ? collateralUnitValue : combatUnitValue));

                        std::pair<int, BuildingTypes> buildingAndValue = getBestMilitaryBuilding(attackUnit, buildConditions);

                        MilitaryBuildingValue buildingValue = tacticSelectionData.getBuildingValue(tacticSelectionData.militaryBuildings, buildingAndValue.second);
                        if (buildingAndValue.first > 20 && buildingValue.nTurns < attackUnitValue.nTurns * 2 && setConstructItem(buildingAndValue.second))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning mil building for attack unit: " << gGlobals.getUnitInfo(attackUnit).getType() << selection
                                << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType())
                                << " unit value: ";
                            thisUnitValue.debug(civLog);
#endif
                            return true;
                        }
                        else if (setConstructItem(attackUnit))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning attack unit: " << gGlobals.getUnitInfo(attackUnit).getType() << selection
                                << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());
#endif
                            return true;
                        }
                    }
                    else if (combatUnit != NO_UNIT && thisCombatUnitCount < 1 + maxUnitCount / 3)
                    {
                        UnitTacticValue thisUnitValue = (combatUnitValue.unitType == combatUnit ? combatUnitValue :
                            (collateralUnitValue.unitType == combatUnit ? collateralUnitValue : attackUnitValue));

                        std::pair<int, BuildingTypes> buildingAndValue = getBestMilitaryBuilding(combatUnit, buildConditions);
                        MilitaryBuildingValue buildingValue = tacticSelectionData.getBuildingValue(tacticSelectionData.militaryBuildings, buildingAndValue.second);

                        if (buildingAndValue.first > 20 && buildingValue.nTurns < combatUnitValue.nTurns * 2 && setConstructItem(buildingAndValue.second))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning mil building for combat unit: " << gGlobals.getUnitInfo(combatUnit).getType() << selection
                                << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType())
                                << " unit value: ";
                            thisUnitValue.debug(civLog);
#endif
                            return true;
                        }
                        else if (setConstructItem(combatUnit))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning combat unit: " << gGlobals.getUnitInfo(combatUnit).getType() << selection
                                << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());                            
#endif
                            return true;
                        }
                    }
                }
                else if (combatUnit != NO_UNIT && thisCombatUnitCount < 1 + maxUnitCount / 3)
                {
                    UnitTacticValue thisUnitValue = (combatUnitValue.unitType == combatUnit ? combatUnitValue :
                        (collateralUnitValue.unitType == combatUnit ? collateralUnitValue : attackUnitValue));

                    std::pair<int, BuildingTypes> buildingAndValue = getBestMilitaryBuilding(combatUnit, buildConditions);
                    MilitaryBuildingValue buildingValue = tacticSelectionData.getBuildingValue(tacticSelectionData.militaryBuildings, buildingAndValue.second);

                    if (buildingAndValue.first > 20 && buildingValue.nTurns < combatUnitValue.nTurns * 2 && setConstructItem(buildingAndValue.second))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning mil building for combat (2) unit: " << gGlobals.getUnitInfo(combatUnit).getType() << selection
                            << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType())
                            << " unit value: ";
                            thisUnitValue.debug(civLog);
#endif
                        return true;
                    }
                    else if (setConstructItem(combatUnit))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning combat (2) unit: " << gGlobals.getUnitInfo(combatUnit).getType() << selection
                            << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());                        
#endif
                        return true;
                    }
                }
#ifdef ALTAI_DEBUG
                civLog << "\n (getConstructItem) chooseUnit() Returning false";
#endif
                return false;
            }

            bool chooseProject()
            {
                return false;
            }

            bool chooseScoutUnit(const ScoutConditions& scoutConditions = ScoutConditions())
            {
                if (scoutConditions.buildLandScout_)
                {
                    if (bestScoutUnit != NO_UNIT)
                    {
                        CvArea* pArea = city.getCvCity()->area();
                        if (player.getCvPlayer()->AI_totalAreaUnitAIs(pArea, UNITAI_EXPLORE) < ((CvPlayerAI*)player.getCvPlayer())->AI_neededExplorers(pArea))
                        {
                            if (setConstructItem(bestScoutUnit))
                            {
    #ifdef ALTAI_DEBUG
                                civLog << "\n(getConstructItem) Returning land scout unit: " << gGlobals.getUnitInfo(bestScoutUnit).getType() << selection;
    #endif
                                return true;
                            }
                        }
                    }
                }

                // todo split this logic - between building scouts for city sites and generally exploring
                if (scoutConditions.buildSeaScout_)
                {
                    const int subAreaID = city.getCvCity()->plot()->getSubArea();

                    // no good city plots in this sub area?
                    if (!player.getSettlerManager()->getBestPlot(subAreaID, std::vector<CvPlot*>()))
                    {
                        if (bestSeaScoutUnit != NO_UNIT)
                        {
                            CvArea* pWaterArea = city.getCvCity()->waterArea();
                            if (player.getUnitCount(bestSeaScoutUnit, true) < 2 + 2 * player.getCvPlayer()->getNumCities() &&
                                player.getCvPlayer()->AI_totalWaterAreaUnitAIs(pWaterArea, UNITAI_EXPLORE) < ((CvPlayerAI*)player.getCvPlayer())->AI_neededExplorers(pWaterArea))
                            {
                                if (setConstructItem(bestSeaScoutUnit))
                                {
#ifdef ALTAI_DEBUG
                                    civLog << "\n(getConstructItem) Returning water scout unit: " << gGlobals.getUnitInfo(bestSeaScoutUnit).getType() << selection;
#endif
                                    return true;
                                }
                            }
                        }
                    }
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseScoutUnit - false";
#endif

                return false;
            }

            void calculateBestSettlerUnits()
            {
                int bestBuildTime = MAX_INT;
                for (std::map<UnitTypes, SettlerUnitValue>::const_iterator ci(tacticSelectionData.settlerUnits.begin()), ciEnd(tacticSelectionData.settlerUnits.end()); ci != ciEnd; ++ci)
                {
                    const int bestOverallBuildTime = getBestBuildTime(ci->first).second;

                    if (ci->second.nTurns < bestBuildTime)
                    {
                        if (ci->second.nTurns < 2 * bestOverallBuildTime)
                        {
                            bestBuildTime = ci->second.nTurns;
                            bestSettlerUnit = ci->first;                            
                        }
                        existingSettlersCount = player.getUnitCount(ci->first, true);
                        if (city.getConstructItem().unitType == ci->first)
                        {
                            --existingSettlersCount;
                        }
                    }
#ifdef ALTAI_DEBUG
                    civLog << "\n(Settler Unit): " << gGlobals.getUnitInfo(ci->first).getType();
                    civLog << "\n\t turns = " << ci->second.nTurns << " best overall build time = " << bestOverallBuildTime;
#endif
                }
            }

            bool buildSettlerEscort(const SettlerEscortConditions& settlerEscortConditions = SettlerEscortConditions())
            {
                if (city.getCvCity()->plot()->plotCount(PUF_isUnitAIType, UNITAI_SETTLE, -1, city.getCvCity()->getOwner()) > 0)
                {
                    if (settlerEscortConditions.buildMilitaryEscort_)
                    {
                        // todo - check selectiongroup of settler?
                        if (bestCityDefenceUnit != NO_UNIT && 
                            city.getCvCity()->plot()->plotCount(PUF_isUnitType, bestCityDefenceUnit, -1, city.getCvCity()->getOwner()) < 2)
                        {
                            if (setConstructItem(bestCityDefenceUnit))
                            {
    #ifdef ALTAI_DEBUG
                                civLog << "\n(getConstructItem) Returning best city defence unit for settler escort: "
                                    << gGlobals.getUnitInfo(bestCityDefenceUnit).getType() << selection;
    #endif
                                return true;
                            }
                        }
                    }

                    if (settlerEscortConditions.buildSeaTransport_)
                    {
                        const int subAreaID = city.getCvCity()->plot()->getSubArea();

                        // todo - set flag on city to indicate overseas destination for settler?
                        if (player.getSettlerManager()->getOverseasCitySitesCount(80, 1, subAreaID) > 0)
                        {
                            if (bestSeaTransportUnit != NO_UNIT && 
                                city.getCvCity()->plot()->plotCount(PUF_isUnitType, bestSeaTransportUnit, -1, city.getCvCity()->getOwner()) < 1 &&
                                setConstructItem(bestSeaTransportUnit))
                            {
#ifdef ALTAI_DEBUG
                                civLog << "\n(getConstructItem) Returning best sea transport unit for settler escort: " << gGlobals.getUnitInfo(bestSeaTransportUnit).getType() << selection;
#endif  
                                return true;
                            }
                        }
                    }
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) buildSettlerEscort - false";
#endif
                return false;
            }

            bool chooseWorker(const WorkerBuildConditions& workerBuildConditions = WorkerBuildConditions())
            {
                const int cityCount = player.getCvPlayer()->getNumCities();
				const int pendingCityCount = player.getUnitCount(UNITAI_SETTLE);  // bit basic - should check missions

                TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));

                for (std::map<UnitTypes, WorkerUnitValue>::const_iterator ci(tacticSelectionData.workerUnits.begin()),
                    ciEnd(tacticSelectionData.workerUnits.end()); ci != ciEnd; ++ci)
                {
                    const bool reusable = ci->second.isReusable();
                    const bool consumed = ci->second.isConsumed();
                    const int existingUnitCount = player.getUnitCount(ci->second.unitType);
                    const int inactiveCount = existingUnitCount - player.getNumActiveWorkers(ci->second.unitType);

                    if (workerBuildConditions.maxLostOutputPercent_ >= 0 && valueF(ci->second.lostOutput) * 100 / valueF(cityBaseOutput) > workerBuildConditions.maxLostOutputPercent_)
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n\tskipping worker: " << gGlobals.getUnitInfo(ci->second.unitType).getType() << " as lost output: "
                            << (valueF(ci->second.lostOutput) * 100 / valueF(cityBaseOutput)) << " exceeded threshold: " << workerBuildConditions.maxLostOutputPercent_;
#endif
                        continue;
                    }

                    // workboats...
                    if (consumed && ci->second.getHighestConsumedBuildValue() > 0)
                    {
                        if (existingUnitCount < ci->second.getBuildsCount())
                        {
                            if (setConstructItem(ci->second.unitType))
                            {
                                selection.pUnitEventGenerator = ci->second.getUnitEventGenerator();
#ifdef ALTAI_DEBUG
                                civLog << "\n(getConstructItem) Returning best consumed worker unit: "
                                        << gGlobals.getUnitInfo(ci->second.unitType).getType() << selection;
#endif
                                return true;
                            }
                        }
                    }
                    // workers...
                    else if (!workerBuildConditions.onlyConsumedWorkers_)
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n\texisting worker count = " << existingUnitCount << ", built value = " << ci->second.getBuildValue() << ", count = " << ci->second.getBuildsCount()
                            << " inactive count = " << inactiveCount << " city count = " << cityCount << ", pop = " << popCount;
#endif

                        if (cityCount == 1)
                        {
							// was just 0 - now takes into cities we're about to build
                            if (reusable && existingUnitCount <= 0 + pendingCityCount && inactiveCount <= 0)
                            {
                                if (setConstructItem(ci->second.unitType))
                                {
                                    selection.pUnitEventGenerator = ci->second.getUnitEventGenerator();
#ifdef ALTAI_DEBUG
                                    civLog << "\n(getConstructItem) Returning reusable worker unit: "
                                        << gGlobals.getUnitInfo(ci->second.unitType).getType() << selection;
#endif
                                    return true;
                                }
                            }
                        }
                        else if (ci->second.getBuildValue() > 0 && 
                                 3 * existingUnitCount < 2 * ci->second.getBuildsCount() &&
                                 existingUnitCount < 3 * std::max<int>(cityCount, popCount / 5)
                                 && inactiveCount <= 1)
                        {
                            if (setConstructItem(ci->second.unitType))
                            {
                                selection.pUnitEventGenerator = ci->second.getUnitEventGenerator();
#ifdef ALTAI_DEBUG
                                civLog << "\n(getConstructItem) Returning reusable worker unit: "
                                       << gGlobals.getUnitInfo(ci->second.unitType).getType() << selection;
#endif
                                return true;
                            }
                        }
                    }

                    // only track out of area builds in tactics data for consumed builds
                    // (effectively resources which need workboats but are not worked by a city)
                    if (consumed && workerBuildConditions.checkOutOfAreaBuilds_)
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\nChecking out of area builds...";
#endif
                        for (WorkerUnitValue::BuildsMap::const_iterator buildsIter(ci->second.nonCityBuildsMap.begin()), buildsEndIter(ci->second.nonCityBuildsMap.end());
                            buildsIter != buildsEndIter; ++buildsIter)
                        {
                            for (size_t buildIndex = 0, buildCount = buildsIter->second.size(); buildIndex < buildCount; ++buildIndex)
                            {
                                XYCoords buildTarget = boost::get<0>(buildsIter->second[buildIndex]);
                                if (player.getCitiesTargetingPlot(ci->second.unitType, buildTarget).empty())
                                {
                                    if (setConstructItem(ci->second.unitType, buildTarget))
                                    {
                                        selection.pUnitEventGenerator = ci->second.getUnitEventGenerator();
#ifdef ALTAI_DEBUG
                                        civLog << "\n(getConstructItem) Returning reusable worker unit: "
                                               << gGlobals.getUnitInfo(ci->second.unitType).getType() << selection;
#endif
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseWorker - false";
#endif
                return false;
            }

            bool chooseEconomicBuilding(const EconomicBuildsData& buildsData, const BuildingConditions& buildingConditions = BuildingConditions())
            {
                for (std::list<const EconomicBuildingValue*>::const_iterator li(buildsData.cityBuildItems.begin()), liEnd(buildsData.cityBuildItems.end()); li != liEnd; ++li)
                {
                    if (buildingConditions.noGlobalWonders_ && isWorldWonderClass(getBuildingClass((*li)->buildingType)))
                    {
                        continue;
                    }

                    if (buildingConditions.noNationalWonders_ && isNationalWonderClass(getBuildingClass((*li)->buildingType)))
                    {
                        continue;
                    }

                    if (buildingConditions.onlyGlobalWonders_ && !isWorldWonderClass(getBuildingClass((*li)->buildingType)))
                    {
                        continue;
                    }

                    if (setConstructItem((*li)->buildingType))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning best economic building: " << gGlobals.getBuildingInfo((*li)->buildingType).getType() << selection;
#endif
                        return true;
                    }
                }

#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseEconomicBuilding - false";
#endif
                return false;
            }

            bool chooseMilitaryBuilding(const BuildingConditions& buildingConditions = BuildingConditions())
            {
                std::pair<int, BuildingTypes> buildingAndValue;
                if (bestCombatUnit != NO_UNIT)
                {
                    buildingAndValue = getBestMilitaryBuilding(bestCombatUnit, buildingConditions);
                    if (playerLands && buildingAndValue.first > 20 && setConstructItem(buildingAndValue.second))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning def mil building for city: " << selection
                               << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());
#endif
                        return true;
                    
                    }
                }

                buildingAndValue = getBestDefensiveMilitaryBuilding(buildingConditions);

                if (playerLands && buildingAndValue.first > 20 && setConstructItem(buildingAndValue.second))
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) Returning mil building for city: " << selection
                           << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());
#endif
                    return true;
                }

                for (std::set<MilitaryBuildingValue>::const_iterator ci(tacticSelectionData.militaryBuildings.begin()),
                    ciEnd(tacticSelectionData.militaryBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (buildingConditions.noGlobalWonders_ && isWorldWonderClass(getBuildingClass(ci->buildingType)))
                    {
                        continue;
                    }

                    if (buildingConditions.noNationalWonders_ && isNationalWonderClass(getBuildingClass(ci->buildingType)))
                    {
                        continue;
                    }

                    if (setConstructItem(ci->buildingType))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(Military Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType() << " turns = " << ci->nTurns;
#endif
                        return true;
                    }
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseMilitaryBuilding - false";
#endif
                return false;
            }

            bool chooseDependentBuild()
            {
                switch (bestDependentBuild.first)
                {
                    case BuildingItem:
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Attempting best dependent build: " << gGlobals.getBuildingInfo((BuildingTypes)bestDependentBuild.second).getType() << selection;
#endif
                        if (setConstructItem((BuildingTypes)bestDependentBuild.second))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning best dependent build: " << gGlobals.getBuildingInfo((BuildingTypes)bestDependentBuild.second).getType() << selection;
#endif
                            return true;
                        }
                        break;
                    case UnitItem:
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Attempting best dependent build: " << gGlobals.getUnitInfo((UnitTypes)bestDependentBuild.second).getType() << selection;
#endif
                        if (setConstructItem((UnitTypes)bestDependentBuild.second))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning best dependent build: " << gGlobals.getUnitInfo((UnitTypes)bestDependentBuild.second).getType() << selection;
#endif
                            return true;
                        }
                        break;
                    default:
                        break;
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseDependentBuild - false";
#endif
                return false;
            }

            bool chooseCulture()
            {
                const CvCity* pCity = city.getCvCity();
                const bool needCulture = pCity->getCultureLevel() == 1 && pCity->getCommerceRate(COMMERCE_CULTURE) == 0;

                if (needCulture && bestCultureProcess != NO_PROCESS && setConstructItem(bestCultureProcess))
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) Returning best culture process: " << gGlobals.getProcessInfo(bestCultureProcess).getType() << selection;
#endif
                    return true;
                }
                else if (needCulture && bestSmallCultureBuilding != NO_BUILDING && setConstructItem(bestSmallCultureBuilding))
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) Returning small culture building: " << gGlobals.getBuildingInfo(bestSmallCultureBuilding).getType() << selection;
#endif
                    return true;
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseCulture - false";
#endif
                return false;
            }

            bool chooseProcess(const ProcessConditions& processConditions = ProcessConditions())
            {
                // e.g. processes improve research rate and max research rate < 30% (or whatever the value of processConditions.maxResearchRate_)
                if (processConditions.forceProcess_ || (maxResearchRate < processConditions.maxResearchRate_ && maxResearchRateWithProcesses > maxResearchRate))
                {
                    if (bestEconomicProcess != NO_PROCESS && setConstructItem(bestEconomicProcess))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning best economic process: " << gGlobals.getProcessInfo(bestEconomicProcess).getType() << selection;
#endif
                        return true;
                    }
                    else if (bestResearchProcess != NO_PROCESS && setConstructItem(bestResearchProcess))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) Returning best research process: " << gGlobals.getProcessInfo(bestResearchProcess).getType() << selection;
#endif
                        return true;
                    }
                }
                
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseProcess - false";
#endif
                return false;
            }

            bool chooseCombatUnit()
            {
                if (bestCombatUnit != NO_UNIT && setConstructItem(bestCombatUnit))
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) Returning best combat unit: " << gGlobals.getUnitInfo(bestCombatUnit).getType() << selection;
#endif
                    return true;
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseCombatUnit - false";
#endif
                return false;
            }

            bool chooseSettler(const SettlerConditions& settlerConditions = SettlerConditions())
            {
                if (bestSettlerUnit != NO_UNIT)
                {
                    if (existingSettlersCount < settlerConditions.maxExisting_ &&
                        maxResearchRate > settlerConditions.minResearchRate_)
                    {
                        TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));

                        std::map<UnitTypes, SettlerUnitValue>::const_iterator ci = tacticSelectionData.settlerUnits.find(bestSettlerUnit);

                        if (settlerConditions.maxLostOutputPercent_ >= 0 && valueF(ci->second.lostOutput) * 100 / valueF(cityBaseOutput) > settlerConditions.maxLostOutputPercent_)
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n\tskipping settler: " << gGlobals.getUnitInfo(ci->second.unitType).getType() << " as lost output: "
                                << (valueF(ci->second.lostOutput) * 100 / valueF(cityBaseOutput)) << " exceeded threshold: " << settlerConditions.maxLostOutputPercent_;
#endif
                            return false;
                        }
                        else if (setConstructItem(bestSettlerUnit))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning settler unit: " << gGlobals.getUnitInfo(bestSettlerUnit).getType() << selection
                                << ", settlerConditions.maxExisting_ = " << settlerConditions.maxExisting_;
#endif
                            return true;
                        }
                    }
                }
#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseSettler - false";
#endif
                return false;
            }
            
            ConstructItem getSingleCitySelection(TotalOutput currentOutput)
            {
                TotalOutput totalProjectedOutput = city.getCurrentOutputProjection().getOutput();

                // have improvements we could be working if we grow, and can grow (allow a little wiggle room)
                const bool keepGrowing = (unworkedGoodImprovementCount > 0 || city.getCityData()->getPopulation() < 4) && 
                    happyCap > 0 && city.getCurrentOutputProjection().getPopChange() > 1;

#ifdef ALTAI_DEBUG
                civLog << "\nProjected pop change: " << city.getCurrentOutputProjection().getPopChange() << ", projected city output = " << totalProjectedOutput
                    << " keep growing: " << keepGrowing;
#endif
                if (chooseCityDefence())
                {
                    return selection;
                }

                SettlerEscortConditions settlerEscortConditions;
                if (buildSettlerEscort(settlerEscortConditions))
                {
                    return selection;
                }

                // should choose reusable worker if available, otherwise consumed workers if required (workboats)
                {
                    WorkerBuildConditions workerBuildConditions;
                    workerBuildConditions.maxLostOutputPercent(20);
                    if (happyCap > 0 && chooseWorker(workerBuildConditions))
                    {
                        return selection;
                    }
                }

                {
                    ScoutConditions scoutConditions;
                    scoutConditions.buildLandScout(true);
                    if (chooseScoutUnit())
                    {
                        return selection;
                    }
                }

                // want to keep growing and have less than three combat units, and can build more
                if (keepGrowing && combatUnitCount < 2 && chooseCombatUnit())
                {
                    return selection;
                }

                {
                    // will only choose consumed workers
                    WorkerBuildConditions workerBuildConditions;
                    workerBuildConditions.onlyConsumedWorkers();
                    if (happyCap > 0 && chooseWorker(workerBuildConditions))
                    {
                        return selection;
                    }
                }

                {
                    BuildingConditions buildingConditions;
                    buildingConditions.noGlobalWonders().noNationalWonders();

                    if (keepGrowing && chooseEconomicBuilding(economicBuildsData, buildingConditions))
                    {
                        return selection;
                    }
                
                    if (keepGrowing && chooseMilitaryBuilding(buildingConditions))
                    {
                        return selection;
                    }
                }

                {
                    // no settlers, can build one
                    SettlerConditions settlerConditions;
                    settlerConditions.maxLostOutputPercent(20);
                    //if (!keepGrowing && chooseSettler(settlerConditions))
                    if (chooseSettler(settlerConditions))
                    {
                        return selection;
                    }
                }

                // does a settler need transport?
                settlerEscortConditions.buildSeaTransport(true);
                if (buildSettlerEscort(settlerEscortConditions))
                {
                    return selection;
                }

                {
                    BuildingConditions buildingConditions;
                    buildingConditions.allowNoEconomicDelta();

                    if (chooseEconomicBuilding(economicBuildsData))
                    {
                        return selection;
                    }
                }

                if (keepGrowing && combatUnitCount < 4 && chooseCombatUnit())
                {
                    return selection;
                }

                {
                    // allow wonders
                    BuildingConditions buildingConditions;
                    buildingConditions.allowNoEconomicDelta();

                    if (chooseEconomicBuilding(economicBuildsData, buildingConditions))
                    {
                        return selection;
                    }

                    if (chooseMilitaryBuilding(buildingConditions))
                    {
                        return selection;
                    }
                }

                if (bestLargeCultureBuilding != NO_BUILDING)
                {
                    if (setConstructItem(bestLargeCultureBuilding))
                    {
                        return selection;
                    }
                }

                return selection;
            }

            ConstructItem getBarbarianSelection()
            {
                if (chooseCityDefence())
                {
                    return selection;
                }

                if (chooseCulture())
                {
                    return selection;
                }

                if (chooseWorker())
                {
                    return selection;
                }

                if (combatUnitCount < player.getCvPlayer()->getNumCities() &&
                    combatUnitCount < city.getCvCity()->getPopulation() &&
                    chooseCombatUnit())
                {
                    return selection;
                }

                if (chooseEconomicBuilding(thisCityEconomicBuildsData))
                {
                    return selection;
                }

                return selection;
            }

            ConstructItem getSelection()
            {
                const CvCity* pCity = city.getCvCity();

                if (player.getCvPlayer()->isBarbarian())
                {
                    return getBarbarianSelection();
                }

                UnitTypes requestedUnitType = playerTactics.player.getAnalysis()->getMilitaryAnalysis()->getUnitRequestBuild(pCity, tacticSelectionData);

                if (cityCount == 1)
                {
                    return getSingleCitySelection(currentCivOutput);
                }

                if (chooseCityDefence())
                {
                    return selection;
                }

                int numKnownCivs = playerTactics.player.getNumKnownPlayers();
                bool possiblyIsolated = false;
                if (numKnownCivs == 0)
                {
                    boost::shared_ptr<MapAnalysis> pMapAnalysis = playerTactics.player.getAnalysis()->getMapAnalysis();
                    std::map<int /* sub area id */, std::vector<IDInfo> > subAreaCityMap = pMapAnalysis->getSubAreaCityMap();
                    possiblyIsolated = true;
                    for (std::map<int, std::vector<IDInfo> >::const_iterator ci(subAreaCityMap.begin()), ciEnd(subAreaCityMap.end()); ci != ciEnd; ++ci)
                    {
                        // still got more exploring to do?
                        if (!pMapAnalysis->isSubAreaComplete(ci->first))
                        {
                            possiblyIsolated = false;
                            break;
                        }
                    }
                }
#ifdef ALTAI_DEBUG
                if (possiblyIsolated)
                {
                    civLog << "\nPossibly isolated!";
                }
#endif

                // todo - more sophisticated danger check - i.e. what type of danger, more than one threat, etc...
                const bool isDanger = player.getCvPlayer()->AI_getPlotDanger(pCity->plot(), 3, false);
                const TeamTypes ourTeam = player.getTeamID();

                NeighbourPlotIter neighbourPlotIter(pCity->plot(), 4, 4);
                while (IterPlot pNeighbourPlot = neighbourPlotIter())
                {
                    if (!pNeighbourPlot.valid() || pNeighbourPlot->isImpassable())
                    {
                        continue;
                    }

                    if (pNeighbourPlot->isVisible(ourTeam, false))
                    {
                        if (pNeighbourPlot->getTeam() == ourTeam)
                        {
                            continue;
                        }

                        PlayerTypes owner = pNeighbourPlot->getOwner();
                        if (owner == NO_PLAYER)
                        {
                            if (!pNeighbourPlot->isWater())
                            {
                                unownedBorders = true;
                            }
                        }
                        else if (owner == BARBARIAN_PLAYER)
                        {
                            barbarianLands = true;
                        }
                        else
                        {
                            neighbours.insert(owner);
                        }
                    }
                }

                playerLands = !neighbours.empty();

                if (chooseCulture())
                {
                    return selection;
                }

                if (chooseProcess())
                {
                    return selection;
                }

                // top half of cities for production
                if (rankAndMaxProduction.first <= (1 + cityCount) / 2)
                {
                    if (chooseUnit(requestedUnitType))
                    {
                        return selection;
                    }

                    if ((atWarCount > 0 || warPlanCount > 0) && chooseUnit(bestCombatUnit))
                    {
                        return selection;
                    }

                    if (buildSettlerEscort())
                    {
                        return selection;
                    }

                    // can build worker units, and no. of plots to improve greater than 3 * no. workers
                    if (chooseWorker())
                    {
                        return selection;
                    }
                    
                    // not growing too rapidly (don't want to stifle growth too much, particularly for smaller cities)
                    if (!isDanger && city.getCurrentOutputProjection().getPopChange() < 2)
                    {
                        SettlerConditions settlerConditions;
                        settlerConditions.maxExisting(1 + (player.getSettlerManager()->getBestCitySites(140, 4).size() / 2));
                        if (chooseSettler(settlerConditions))
                        {
                            return selection;
                        }
                    }

                    if (bestCityDefenceUnit != bestCombatUnit || (isDanger && bestCombatUnit != NO_UNIT))
                    {
                        if (chooseUnit(requestedUnitType))
                        {
                            return selection;
                        }

                        if (bestMilitaryBuilding != NO_BUILDING && setConstructItem(bestMilitaryBuilding))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning best military building(1): " << gGlobals.getBuildingInfo(bestMilitaryBuilding).getType() << selection;
#endif
                            return selection;
                        }
                        else if (pCity->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()) && bestSeaDefenceUnit != NO_UNIT && 
                            player.getUnitCount(bestSeaDefenceUnit) < 2 && setConstructItem(bestSeaDefenceUnit))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning best sea defence unit(1): " << gGlobals.getUnitInfo(bestSeaDefenceUnit).getType() << selection;
#endif
                            return selection;
                        }                        
                    }

                    if (chooseScoutUnit())
                    {
                        return selection;
                    }

                    {
                        BuildingConditions buildingConditions;
                        buildingConditions.onlyGlobalWonders();

                        if (rankAndMaxProduction.first == 1 && chooseEconomicBuilding(economicBuildsData, buildingConditions))
                        {
                            return selection;
                        }
                    }

                    // top rank for production, and can build something which improves the economy
                    if (rankAndMaxProduction.first == 1 && chooseEconomicBuilding(economicBuildsData))
                    {
                        return selection;
                    }

                    if (city.getCurrentOutputProjection().getPopChange() < 2)
                    {
                        SettlerConditions settlerConditions;
                        settlerConditions.maxExisting(1 + std::min<int>(cityCount / 3, (player.getSettlerManager()->getBestCitySites(100, 4).size() / 2)));
                        if (chooseSettler(settlerConditions))
                        {
                            return selection;
                        }
                    }

                    {
                        WorkerBuildConditions workerBuildConditions;
                        workerBuildConditions.onlyConsumedWorkers().checkOutOfAreaBuilds();
                        
                        if (chooseWorker(workerBuildConditions))
                        {
                            return selection;
                        }
                    }

                    if (chooseProject())
                    {
                        return selection;
                    }

                    if (bestCityDefenceUnit != bestCombatUnit)
                    {
                        if (bestMilitaryBuilding != NO_BUILDING && setConstructItem(bestMilitaryBuilding))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning best military building(2): " << gGlobals.getBuildingInfo(bestMilitaryBuilding).getType() << selection;
#endif
                            return selection;
                        }

                        if (city.getCvCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()) && bestSeaDefenceUnit != NO_UNIT && 
                            player.getUnitCount(bestSeaDefenceUnit) < 2 && setConstructItem(bestSeaDefenceUnit))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning best sea defence unit(2): " << gGlobals.getUnitInfo(bestSeaDefenceUnit).getType() << selection;
#endif
                            return selection;
                        }

                        if (chooseUnit(requestedUnitType))
                        {
                            return selection;
                        }
                    }
                }

                if (bestCityDefenceUnit != bestCombatUnit && (atWarCount > 0 || warPlanCount > 0))
                {
                    if (chooseUnit(requestedUnitType))
                    {
                        return selection;
                    }
                }

                {
                    BuildingConditions buildingConditions;
                    buildingConditions.allowNoEconomicDelta();

                    if (chooseEconomicBuilding(thisCityEconomicBuildsData, buildingConditions))
                    {
                        return selection;
                    }
                }

                if (chooseMilitaryBuilding())
                {
                    return selection;
                }

                {
                    ProcessConditions processConditions;
                    processConditions.maxResearchRate(80);
                    if (chooseProcess(processConditions))
                    {
                        return selection;
                    }
                }

                if (chooseDependentBuild())
                {
                    return selection;
                }

                /*{
                    BuildingConditions buildingConditions;
                    buildingConditions.allowNoEconomicDelta();
                    buildingConditions.allowZeroDelta();

                    if (chooseEconomicBuilding(thisCityEconomicBuildsData, buildingConditions))
                    {
                        return selection;
                    }
                }*/

                if (bestMilitaryBuilding != NO_BUILDING && setConstructItem(bestMilitaryBuilding))
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) Returning best military building(2): " << gGlobals.getBuildingInfo(bestMilitaryBuilding).getType() << selection;
#endif
                    return selection;
                }

                {
                    ProcessConditions processConditions;
                    processConditions.forceProcess(true);
                    if (chooseProcess(processConditions))
                    {
                        return selection;
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

            bool setConstructItem(UnitTypes unitType, XYCoords buildTarget = XYCoords())
            {
                if (city.getCvCity()->canTrain(unitType, city.getCvCity()->getProductionUnit() == unitType))
                {
                    selection = ConstructItem(unitType);
                    selection.buildTarget = buildTarget;
                    return true;
                }
                else
                {
                    return false;
                }
            }

            bool setConstructItem(BuildingTypes buildingType)
            {
                if (city.getCvCity()->canConstruct(buildingType, city.getCvCity()->getProductionBuilding() == buildingType))
                {
                    selection = ConstructItem(buildingType);
                    return true;
                }
                else
                {
                    return false;
                }
            }

            bool setConstructItem(ProcessTypes processType)
            {
                selection = ConstructItem(processType);
                return true;
            }

            bool setConstructItem(ProjectTypes projectType)
            {
                return false;
            }

            const PlayerTactics& playerTactics;
            const Player& player;
            const City& city;

            TotalOutput cityBaseOutput;

            int warPlanCount, atWarCount;
            int happyCap;

            bool unownedBorders, barbarianLands, playerLands;
            std::set<PlayerTypes> neighbours;

            //std::set<UnitTypes> possibleWorkers, consumedPossibleWorkers, possibleSettlers;
            int existingSettlersCount, existingWorkersCount, workerBuildCount, existingConsumedWorkersCount, consumedWorkerBuildCount;

            int combatUnitCount, attackUnitCount;
            int cityCount, popCount;
            int maxResearchRate, maxResearchRateWithProcesses;
            std::pair<int, int> rankAndMaxProduction;
            int unworkedGoodImprovementCount;

            TotalOutput currentCivOutput, currentCityOutput;

            BuildingTypes bestSmallCultureBuilding, bestLargeCultureBuilding, bestEconomicBuilding, bestMilitaryBuilding, bestEconomicWonder, bestEconomicNationalWonder;

            std::pair<BuildQueueTypes, int> bestDependentBuild;

            // todo - move unit data into separate unit manager
            UnitTypes bestCombatUnit, bestScoutUnit, bestCityDefenceUnit, bestCollateralUnit, bestSeaDefenceUnit, bestSeaTransportUnit, bestSeaScoutUnit, bestSettlerUnit;
            UnitTypes bestDependentTacticUnit;
            std::vector<UnitTypes> combatUnits, cityAttackUnits, cityDefenceUnits, seaDefenceUnits, seaTransportUnits, collateralUnits;
            std::map<UnitCombatTypes, std::vector<UnitTypes> > counterUnits;
            std::map<UnitCombatTypes, UnitTypes> bestUnitCombatTypes;
            ProcessTypes bestEconomicProcess, bestResearchProcess, bestCultureProcess;            
            EconomicBuildsData economicBuildsData, thisCityEconomicBuildsData;

            std::map<UnitTypes, int> unitDefenceValues, thisCityUnitDefenceValues, unitAttackValues, unitCollateralValues;
            std::multimap<int, const MilitaryBuildingValue*> militaryBuildingValuesMap, thisCityMilitaryBuildingValuesMap;

            TacticSelectionData tacticSelectionData;

            ConstructItem selection;

            std::ostream& civLog;
        };
    }
    
    ConstructItem getConstructItem(const PlayerTactics& playerTactics, const City& city)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream();
#endif
        CityBuildSelectionData selectionData(playerTactics, city);
        IDInfo cityID = city.getCvCity()->getIDInfo();

        // city buildings
        PlayerTactics::CityBuildingTacticsMap::const_iterator ci = playerTactics.cityBuildingTacticsMap_.find(cityID);
        if (ci != playerTactics.cityBuildingTacticsMap_.end())
        {
            for (PlayerTactics::CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
            {
                if (li->second->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                {
                    li->second->update(playerTactics.player, city.getCityData());
                    li->second->apply(selectionData.tacticSelectionData);
                }
            }

            selectionData.tacticSelectionData.buildingsCityCanAssistWith = playerTactics.getBuildingsCityCanAssistWith(cityID);
            selectionData.tacticSelectionData.dependentBuildings = playerTactics.getPossibleDependentBuildings(cityID);
        }

        // city units
        for (PlayerTactics::UnitTacticsMap::const_iterator iter(playerTactics.unitTacticsMap_.begin()),
            endIter(playerTactics.unitTacticsMap_.end()); iter != endIter; ++iter)
        {
            if (iter->second && iter->second->areDependenciesSatisfied(playerTactics.player, IDependentTactic::Ignore_None))
            {
                CityUnitTacticsPtr pCityUnitTacticsPtr = iter->second->getCityTactics(cityID);
                if (pCityUnitTacticsPtr)
                {
                    pCityUnitTacticsPtr->update(playerTactics.player, city.getCityData());
                    pCityUnitTacticsPtr->apply(selectionData.tacticSelectionData, IDependentTactic::Ignore_None);
                }
            }
        }

        // wonders
        for (PlayerTactics::LimitedBuildingsTacticsMap::const_iterator iter(playerTactics.globalBuildingsTacticsMap_.begin()),
            endIter(playerTactics.globalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            ICityBuildingTacticsPtr pGlobalCityBuildingTactics = iter->second->getCityTactics(cityID);
            if (iter->second->areDependenciesSatisfied(cityID, IDependentTactic::Ignore_None))
            {
                iter->second->update(playerTactics.player, city.getCityData());
                iter->second->apply(selectionData.tacticSelectionData);
            }
        }

        // national wonders
        for (PlayerTactics::LimitedBuildingsTacticsMap::const_iterator iter(playerTactics.nationalBuildingsTacticsMap_.begin()),
            endIter(playerTactics.nationalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            ICityBuildingTacticsPtr pNationalCityBuildingTactics = iter->second->getCityTactics(cityID);

            if (iter->second->areDependenciesSatisfied(cityID, IDependentTactic::Ignore_None))
            {
                iter->second->update(playerTactics.player, city.getCityData());
                iter->second->apply(selectionData.tacticSelectionData);
            }
        }

        // processes
        for (PlayerTactics::ProcessTacticsMap::const_iterator ci(playerTactics.processTacticsMap_.begin()), ciEnd(playerTactics.processTacticsMap_.end());
            ci != ciEnd; ++ci)
        {
            if (ci->second->areDependenciesSatisfied(playerTactics.player, IDependentTactic::Ignore_None))
            {
                ProjectionLadder projection = ci->second->getProjection(cityID);
                selectionData.tacticSelectionData.processOutputsMap[ci->second->getProcessType()] = projection.getProcessOutput();
            }
        }

        UnitTypes requestedUnitType = playerTactics.player.getAnalysis()->getMilitaryAnalysis()->getUnitRequestBuild(city.getCvCity(), selectionData.tacticSelectionData);

        selectionData.calculateImprovementStats();

        selectionData.processUnits();
        selectionData.processBuildings();
        selectionData.processProjects();

        selectionData.calculateSmallCultureBuilding();
        selectionData.calculateLargeCultureBuilding();
        selectionData.calculateBestEconomicBuilding();
        selectionData.calculateBestEconomicWonder();
        selectionData.calculateBestEconomicNationalWonder();
        selectionData.calculateBestEconomicDependentTactic();
        selectionData.calculateBestLocalEconomicDependentTactic();
        selectionData.calcEconomicBuildItems();

        selectionData.calculateUnitValues();
        selectionData.calculateBestMilitaryBuilding();
        selectionData.calculateBestProcesses();

        selectionData.calculateBestSettlerUnits();

        selectionData.calculateBestCombatUnits();
        selectionData.calculateBestCityAttackUnits();
        selectionData.calculateBestCityDefenceUnits();
        selectionData.calculateBestCollateralUnits();
        selectionData.countCombatUnits();

        if (city.getCvCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
        {
            selectionData.calculateBestSeaScoutUnit();
            selectionData.calculateBestSeaDefenceUnits();
            selectionData.calculateBestSeaTransportUnits();
        }

        selectionData.calculateBestScoutUnit();
        selectionData.calculateBestUnits();

        selectionData.debug();

        ConstructItem selection = selectionData.getSelection();

#ifdef ALTAI_DEBUG
        CityDataPtr pCityData = city.getCityData()->clone();
        std::vector<IProjectionEventPtr> events;

        if (selection.buildingType != NO_BUILDING)
        {            
            pCityData->pushBuilding(selection.buildingType);
            events.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pCityData->getCity(), playerTactics.player.getAnalysis()->getBuildingInfo(selection.buildingType))));
        }
        else if (selection.unitType != NO_UNIT)
        {
            pCityData->pushUnit(selection.unitType);
            events.push_back(IProjectionEventPtr(new ProjectionUnitEvent(pCityData->getCity(), playerTactics.player.getAnalysis()->getUnitInfo(selection.unitType))));
        }

        ProjectionLadder projection = getProjectedOutput(playerTactics.player, pCityData, 30, events, selection, __FUNCTION__, selection.buildingType != NO_BUILDING, true);

        os << "\n(getConstructItem) projection for " << narrow(city.getCvCity()->getName()) << ": ";
        if (selection.buildingType != NO_BUILDING)
        {
            os << gGlobals.getBuildingInfo(selection.buildingType).getType();
        }
        else if(selection.unitType != NO_UNIT)
        {
            os << gGlobals.getUnitInfo(selection.unitType).getType();
        }
        else if (selection.projectType != NO_PROJECT)
        {
            os << gGlobals.getProjectInfo(selection.projectType).getType();
        }
        else if (selection.processType != NO_PROCESS)
        {
            os << gGlobals.getProcessInfo(selection.processType).getType();
        }
        projection.debug(os);    
#endif

        return selection;
    }
}