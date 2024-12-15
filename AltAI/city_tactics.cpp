#include "AltAI.h"

#include "./city_tactics.h"
#include "./city_unit_tactics.h"
#include "./building_tactics_deps.h"
#include "./religion_tactics.h"
#include "./tactic_selection_data.h"
#include "./building_tactics_visitors.h"
#include "./tech_info_visitors.h"
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
                    onlyGlobalWonders_ = false;
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
                    noGlobalWonders_ = false;
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

                void debug(std::ostream& os) const
                {
                    if (noGlobalWonders_) os << " no global wonders ";
                    if (noNationalWonders_) os << " no national wonders ";
                    if (onlyGlobalWonders_) os << " only global wonders ";
                    if (allowNoEconomicDelta_) os << " allow no economic delta ";
                    if (allowZeroDelta_) os << " allow zero delta ";
                }

                // mutually exclusive
                bool noGlobalWonders_, onlyGlobalWonders_;
                bool noNationalWonders_;
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
                ScoutConditions() : buildLandScout_(true), buildSeaScout_(false), maxExisting_(1)
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

                ScoutConditions& maxExisting(int i)
                {
                    maxExisting_ = i;
                    return *this;
                }

                bool buildLandScout_, buildSeaScout_;
                int maxExisting_;
            };

            struct SeaScoutUnitValueF
            {
                bool operator() (UnitTypes first, UnitTypes second) const
                {
                    const CvUnitInfo& unitInfo1 = gGlobals.getUnitInfo(first), &unitInfo2 = gGlobals.getUnitInfo(second);
                    return unitInfo1.getMoves() == unitInfo2.getMoves() ? unitInfo1.getProductionCost() < unitInfo2.getProductionCost() :
                        unitInfo1.getMoves() > unitInfo2.getMoves();
                }
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

            CityBuildSelectionData(PlayerTactics& playerTactics_, City& city_)
                : playerTactics(playerTactics_), player(playerTactics_.player), city(city_),
                  numSimTurns(playerTactics.player.getAnalysis()->getNumSimTurns()),
                  timeHorizon(playerTactics.player.getAnalysis()->getTimeHorizon()),
                  selection(NO_BUILDING),
                  tacticSelectionData(playerTactics_.getCityTacticSelectionData(city.getCvCity()->getIDInfo())),
                  civLog(CivLog::getLog(*playerTactics_.player.getCvPlayer())->getStream())
            {
                possiblyIsolated = false;
                int numKnownCivs = playerTactics.player.getNumKnownPlayers();
                if (numKnownCivs == 0)
                {
                    possiblyIsolated = true;
                    boost::shared_ptr<MapAnalysis> pMapAnalysis = playerTactics.player.getAnalysis()->getMapAnalysis();
                    std::map<int /* sub area id */, std::vector<IDInfo> > subAreaCityMap = pMapAnalysis->getSubAreaCityMap();
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

                warPlanCount = CvTeamAI::getTeam(player.getTeamID()).getAnyWarPlanCount(true);
                atWarCount = CvTeamAI::getTeam(player.getTeamID()).getAtWarCount(true);
                unownedBorders = false, barbarianLands = false, playerLands = false;

                happyCap = city.getCityData()->getHappyCap();
                existingSettlersCount = 0, existingWorkersCount = 0, workerBuildCount = 0, existingConsumedWorkersCount = 0, consumedWorkerBuildCount = 0;
                unworkedGoodImprovementCount = 0;
                cityBaseOutput = city.getCurrentOutputProjection().getOutput();

                combatUnitCount = attackUnitCount = 0;
                cityCount = player.getCvPlayer()->getNumCities();
                popCount = player.getCvPlayer()->getTotalPopulation();
                
                maxResearchRate = player.getMaxResearchPercent();
                maxResearchRateWithProcesses = player.getMaxResearchPercentWithProcesses();
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
                civLog << "\n City tactic selection data: ";
                tacticSelectionData.debug(civLog);

                TacticSelectionDataMap& globalSelectionData = playerTactics.tacticSelectionDataMap;
                civLog << "\n Global tactic selection data: ";
                for (TacticSelectionDataMap::const_iterator tMapIter(globalSelectionData.begin()), tMapEndIter(globalSelectionData.end()); tMapIter != tMapEndIter; ++tMapIter)
                {
                    civLog << "\nDep items: ";
                    debugDepItemSet(tMapIter->first, civLog);
                    tMapIter->second.debug(civLog);
                }

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
                currentCivOutput = player.getCurrentOutput();
                currentCityOutput = city.getCityData()->getOutput();
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
                for (std::set<CultureBuildingValue>::const_iterator ci(tacticSelectionData.smallCultureBuildings.begin()), ciEnd(tacticSelectionData.smallCultureBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 10, 1);
                        TotalOutputValueFunctor valueF(weights);
                        // don't use timeHorizon here - evaluate for a shorter period as want quick culture
                        // todo - just calc first building to culture level 1 with no or least negative impacts
                        int scaledValue = TacticSelectionData::getScaledEconomicValue(numSimTurns, ci->nTurns, numSimTurns, ci->output, weights);

                        civLog << "\n(Small Culture Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType()
                               << " turns = " << ci->nTurns << ", delta = " << ci->output << " value = " << valueF(ci->output) / std::max<int>(1, ci->nTurns)
                               << " scaled value: " << scaledValue;
                    }
                }
#endif
                int bestValue = 0;
                TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 10, 1);
                TotalOutputValueFunctor valueF(weights);

                for (std::set<CultureBuildingValue>::const_iterator ci(tacticSelectionData.smallCultureBuildings.begin()), 
                    ciEnd(tacticSelectionData.smallCultureBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        //int thisValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                        int scaledValue = TacticSelectionData::getScaledEconomicValue(numSimTurns, ci->nTurns, numSimTurns, ci->output, weights);
                        if (scaledValue > bestValue &&
                            tacticSelectionData.exclusions.find(ci->buildingType) == tacticSelectionData.exclusions.end())
                        {
                            bestValue = scaledValue;
                            bestSmallCultureBuilding = ci->buildingType;
                        }
                    }
                }
            }

            void calculateLargeCultureBuilding()
            {
#ifdef ALTAI_DEBUG
                for (std::set<CultureBuildingValue>::const_iterator ci(tacticSelectionData.largeCultureBuildings.begin()),
                    ciEnd(tacticSelectionData.largeCultureBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 20, 1);
                        TotalOutputValueFunctor valueF(weights);
                        int scaledValue = TacticSelectionData::getScaledEconomicValue(numSimTurns, ci->nTurns, timeHorizon, ci->output, weights);

                        civLog << "\n(Large Culture Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType()
                               << " turns = " << ci->nTurns << ", delta = " << ci->output << " value = " << valueF(ci->output) / std::max<int>(1, ci->nTurns)
                               << " scaled value: " << scaledValue;
                    }
                }
#endif

                int bestValue = 0;
                TotalOutputWeights weights = makeOutputW(1, 1, 1, 1, 10, 1);
                TotalOutputValueFunctor valueF(weights);

                for (std::set<CultureBuildingValue>::const_iterator ci(tacticSelectionData.largeCultureBuildings.begin()),
                    ciEnd(tacticSelectionData.largeCultureBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        //int thisValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                        int scaledValue = TacticSelectionData::getScaledEconomicValue(numSimTurns, ci->nTurns, timeHorizon, ci->output, weights);
                        if (scaledValue > bestValue &&
                            tacticSelectionData.exclusions.find(ci->buildingType) == tacticSelectionData.exclusions.end())
                        {
                            bestValue = scaledValue;
                            bestLargeCultureBuilding = ci->buildingType;
                        }
                    }
                }
            }

            void calculateBestDependentBuilding()
            {
                TacticSelectionDataMap& globalSelectionData = playerTactics.tacticSelectionDataMap;
                TacticSelectionData& baseGlobalSelectionData = playerTactics.getBaseTacticSelectionData();
                TotalOutput baseCityOutput = city.getBaseOutputProjection().getOutput();
                TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 0, 0);
                TotalOutputValueFunctor valueF(weights);

                for (TacticSelectionDataMap::const_iterator tacticMapIter(globalSelectionData.begin()), tacticMapEndIter(globalSelectionData.end()); tacticMapIter != tacticMapEndIter; ++tacticMapIter)
                {
                    if (tacticMapIter->first.size() == 1 && tacticMapIter->first.begin()->first == ReligiousDependency::ID)
                    {
                        ReligionTypes depReligion = (ReligionTypes)tacticMapIter->first.begin()->second;
                        // city has this religion - see if we can build requested unit
                        if (city.getCvCity()->isHasReligion(depReligion) && !tacticMapIter->second.dependentBuilds.empty())
                        {
                            BuildQueueItem buildItem = *tacticMapIter->second.dependentBuilds.begin();
                            // can we build it?
                            if (buildItem.first == UnitItem)
                            {
                                PlayerTactics::UnitTacticsMap::const_iterator unitsIter = playerTactics.unitTacticsMap_.find((UnitTypes)buildItem.second);
                                if (unitsIter != playerTactics.unitTacticsMap_.end())
                                {
                                    CityUnitTacticsPtr pCityUnitTacticsPtr = unitsIter->second->getCityTactics(city.getCvCity()->getIDInfo());
                                    if (pCityUnitTacticsPtr)
                                    {
                                        if (pCityUnitTacticsPtr->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                                        {
#ifdef ALTAI_DEBUG
                                            civLog << "\n\t city can build dep item unit: " << gGlobals.getUnitInfo((UnitTypes)buildItem.second).getType();
#endif
                                        }
                                        else
                                        {
#ifdef ALTAI_DEBUG
                                            civLog << "\n\t city can't build dep item unit: " << gGlobals.getUnitInfo((UnitTypes)buildItem.second).getType();
                                            PlayerTactics::CityBuildingTacticsMap::const_iterator cityBuildingTactics = playerTactics.cityBuildingTacticsMap_.find(city.getCvCity()->getIDInfo());
                                            std::vector<DependencyItem> depItems = pCityUnitTacticsPtr->getDepItems(IDependentTactic::City_Buildings_Dep);
                                            for (size_t depIndex = 0, depCount = depItems.size(); depIndex < depCount; ++depIndex)
                                            {
                                                civLog << "\n\t unit deps: ";
                                                debugDepItem(depItems[depIndex], civLog);
                                                if (depItems[depIndex].first == CityBuildingDependency::ID)
                                                {
                                                    PlayerTactics::CityBuildingTacticsList::const_iterator bIter = cityBuildingTactics->second.find((BuildingTypes)depItems[depIndex].second);
                                                    if (bIter != cityBuildingTactics->second.end())
                                                    {
                                                        if (bIter->second->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                                                        {
                                                            civLog << " city can build dep item unit building: " << gGlobals.getBuildingInfo((BuildingTypes)depItems[depIndex].second).getType();
                                                        }
                                                        else
                                                        {
                                                            civLog << " city can't build dep item unit building: " << gGlobals.getBuildingInfo((BuildingTypes)depItems[depIndex].second).getType();
                                                        }
                                                    }
                                                }
                                            }
#endif
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            std::map<ReligionTypes, std::map<IDInfo, std::pair<TotalOutput, TotalOutput> > >::const_iterator baseReligionDiffIter = 
                                baseGlobalSelectionData.potentialReligionOutputDeltas.find(depReligion);

                            if (baseReligionDiffIter == tacticMapIter->second.potentialReligionOutputDeltas.end())
                            {
#ifdef ALTAI_DEBUG
                                civLog << "\n Unable to find base outputs for religion: " << gGlobals.getReligionInfo(depReligion).getType();
#endif
                                continue;
                            }

                            TotalOutput thisCityWithReligionBase, accumulatedDelta;

                            std::map<IDInfo, std::pair<TotalOutput, TotalOutput> >::const_iterator baseReligionDiffCityIter = baseReligionDiffIter->second.find(city.getCvCity()->getIDInfo());
                            if (baseReligionDiffCityIter != baseReligionDiffIter->second.end())
                            {
                                thisCityWithReligionBase = baseReligionDiffCityIter->second.first;  // output of city if it just had this religion spread to it
                                accumulatedDelta = thisCityWithReligionBase - baseReligionDiffCityIter->second.second;  // diff from base output of just this religion (only want to count this once)
#ifdef ALTAI_DEBUG
                                civLog << "\n Base city output with just religion: " << thisCityWithReligionBase 
                                    << ", diff from main base: " << thisCityWithReligionBase - baseReligionDiffCityIter->second.second
                                    << " value = " << valueF(thisCityWithReligionBase - baseReligionDiffCityIter->second.second);
#endif
                            }

                            for (std::set<EconomicBuildingValue>::const_iterator ci(tacticMapIter->second.economicBuildings.begin()), ciEnd(tacticMapIter->second.economicBuildings.end()); ci != ciEnd; ++ci)
                            {
                                if (ci->city != city.getCvCity()->getIDInfo())  // this is a civ wide set of religion dependent buildings
                                {
                                    continue;
                                }
#ifdef ALTAI_DEBUG
                                civLog << "\n\t adding delta for building: " << gGlobals.getBuildingInfo(ci->buildingType).getType() << ": " << ci->output + baseCityOutput - thisCityWithReligionBase
                                    << " value = " << valueF(ci->output + baseCityOutput - thisCityWithReligionBase) / (ci->nTurns == 0 ? 1 : ci->nTurns);
#endif
                                accumulatedDelta += ci->output + baseCityOutput - thisCityWithReligionBase;  // add in delta from each building dep on this religion
                            }
#ifdef ALTAI_DEBUG
                            civLog << "\n\t dependent economic building total possible delta: ";
                            debugDepItem(*tacticMapIter->first.begin(), civLog);
                            TotalOutput base = city.getBaseOutputProjection().getOutput();
                            civLog << " total delta: " << accumulatedDelta << ", base: " << base << " %age change for city: " << asPercentageOf(accumulatedDelta, base);
#endif
                        }
                    }
                }
            }

            void calculateBestEconomicBuilding()
            {
#ifdef ALTAI_DEBUG
                {
                    TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 0, 0), unitWeights = makeOutputW(1, 1, 1, 1, 0, 0);
                    TotalOutputValueFunctor valueF(weights);
                    TotalOutput base = city.getBaseOutputProjection().getOutput();
                    civLog << "\n " << __FUNCTION__ << " base output: " << base;

                    for (std::set<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin()), ciEnd(tacticSelectionData.economicBuildings.end()); ci != ciEnd; ++ci)
                    {
                        if (ci->city == city.getCvCity()->getIDInfo())
                        {
                            TotalOutput extraOutput;
                            int scaledValue = 0;
                            if (tacticSelectionData.possibleFreeTechs.find(ci->buildingType) != tacticSelectionData.possibleFreeTechs.end())
                            {
                                const int techCost = calculateTechResearchCost(tacticSelectionData.possibleFreeTechs[ci->buildingType], player.getPlayerID());
                                extraOutput += makeOutput(0, 0, 0, techCost * 100, 0, 0);
                                scaledValue += TacticSelectionData::getScaledEconomicValue(numSimTurns, 1, timeHorizon, extraOutput, unitWeights);
                                civLog << "\n(Economic Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType() << " adding free tech: " <<
                                    gGlobals.getTechInfo(tacticSelectionData.possibleFreeTechs[ci->buildingType]).getType() << " as extra output: " << extraOutput << " scaled value: " << scaledValue;
                            }
                            scaledValue += ci->getScaledEconomicValue(numSimTurns, timeHorizon, unitWeights);
                            civLog << "\n(Economic Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType()
                                   << " turns = " << ci->nTurns << ", delta = " << ci->output << " value = " << (valueF(ci->output + extraOutput) / (ci->nTurns == 0 ? 1 : ci->nTurns))
                                   << " scaled value: " << scaledValue
                                   << " %age of base: " << asPercentageOf(ci->output + extraOutput, base);
                        }
                    }
                }
#endif
                int bestValue = 0;
                TotalOutputWeights /*weights = makeOutputW(20, 20, 20, 20, 1, 1), */ pureEconomicWeights = makeOutputW(10, 10, 10, 10, 0, 0), unitWeights = makeOutputW(1, 1, 1, 1, 0, 0);
                TotalOutputValueFunctor /*valueF(weights), */ econValueF(pureEconomicWeights);
                
                for (std::set<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin()), ciEnd(tacticSelectionData.economicBuildings.end()); ci != ciEnd; ++ci)
                {
                    if (ci->city == city.getCvCity()->getIDInfo())
                    {
                        TotalOutput extraOutput;
                        int scaledValue = 0;
                        if (tacticSelectionData.possibleFreeTechs.find(ci->buildingType) != tacticSelectionData.possibleFreeTechs.end())
                        {
                            const int techCost = calculateTechResearchCost(tacticSelectionData.possibleFreeTechs[ci->buildingType], player.getPlayerID());
                            extraOutput += makeOutput(0, 0, 0, techCost * 100, 0, 0);
                            scaledValue += TacticSelectionData::getScaledEconomicValue(numSimTurns, 1, timeHorizon, extraOutput, unitWeights);
                        }
                        //int thisValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                        scaledValue += ci->getScaledEconomicValue(numSimTurns, timeHorizon, unitWeights);
                        //int thisValue = econValueF(ci->output + extraOutput) / std::max<int>(1, ci->nTurns);
                        if (scaledValue > bestValue && // econValueF(ci->output) > 0 &&
                            tacticSelectionData.exclusions.find(ci->buildingType) == tacticSelectionData.exclusions.end())
                        {
                            bestValue = scaledValue;
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
                        //int thisValue = valueF(thisDelta);
                        int scaledValue = TacticSelectionData::getScaledEconomicValue(numSimTurns, thisCityBuiltTurn, timeHorizon, thisDelta, weights);

                        TotalOutput extraOutput;
                        if (tacticSelectionData.possibleFreeTechs.find(ci->first) != tacticSelectionData.possibleFreeTechs.end())
                        {
                            const int techCost = calculateTechResearchCost(tacticSelectionData.possibleFreeTechs[ci->first], player.getPlayerID());
                            scaledValue += TacticSelectionData::getScaledEconomicValue(numSimTurns, 1, timeHorizon, extraOutput, weights);
                            //thisValue += valueF(makeOutput(0, 0, 0, techCost * 100, 0, 0));
                            civLog << "\n(Economic Building): " << gGlobals.getBuildingInfo(ci->first).getType() << " adding free tech: " <<
                                gGlobals.getTechInfo(tacticSelectionData.possibleFreeTechs[ci->first]).getType() << " as extra output: " << valueF(makeOutput(0, 0, 0, techCost * 100, 0, 0))
                                << " scaled value: " << scaledValue;
                        }

                        if (scaledValue > bestValue)
                        {
                            bestValue = scaledValue;
                            bestValueBuiltTurns = thisCityBuiltTurn;
                            bestWonder = ci->first;
                        }
                    }

#ifdef ALTAI_DEBUG
                    civLog << "\nWonder: " << gGlobals.getBuildingInfo(ci->first).getType();

                    if (firstBuiltCity.eOwner != NO_PLAYER)
                    {
                        civLog << " best build time = " << firstBuiltTurn << ", in city: " << narrow(getCity(firstBuiltCity)->getName()) << " delta = " << bestDelta
                               << " this city build time = " << thisCityBuiltTurn << ", delta = " << thisDelta << " scaled value: " << bestValue;
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
                    bool bestValueBeatsSimpleBuildings = true;
                    for (std::set<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin()), ciEnd(tacticSelectionData.economicBuildings.end());
                        ci != ciEnd; ++ci)
                    {
                        FAssertMsg(ci->buildingType != bestWonder, "Unexpected wonder in simple building list?");

                        int buildingScaledValue = ci->getScaledEconomicValue(numSimTurns, timeHorizon, weights);
                        if (buildingScaledValue > bestValue)
                        {
                            bestValueBeatsSimpleBuildings = false;
                        }
                    }

                    if (!tacticSelectionData.economicBuildings.empty() && bestValueBeatsSimpleBuildings)
                    {
                        bestEconomicBuilding = bestEconomicWonder;
                        //std::set<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin());
                        /*if (valueF(ci->output) / std::max<int>(1, ci->nTurns) < bestValue / bestValueBuiltTurns)
                        {
                            bestEconomicBuilding = bestEconomicWonder;
                        }*/
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
                BuildQueueItem buildItem(NoItem, -1);

                for (std::map<BuildingTypes, std::set<BuildingTypes> >::const_iterator ci(tacticSelectionData.buildingsCityCanAssistWith.begin()),
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

                        std::set<EconomicBuildingValue>::const_iterator ci(tacticSelectionData.economicBuildings.begin());
                        int ownValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                        civLog << ", best own building value = " << ownValue << " for: " << gGlobals.getBuildingInfo(ci->buildingType).getType();
                    }
                }
#endif
            }

            void calculateBestLocalEconomicDependentTactic()
            {
                TotalOutputWeights weights = makeOutputW(10, 10, 10, 10, 0, 0), unitWeights = makeOutputW(1, 1, 1, 1, 0, 0);
                TotalOutputValueFunctor valueF(weights);
                int bestValue = 0;
                IDInfo bestCity;
                BuildingTypes bestBuildingType = NO_BUILDING;
                BuildQueueItem buildItem(NoItem, -1);

                TotalOutput base = city.getCurrentOutputProjection().getOutput();
                PlayerTactics::CityBuildingTacticsMap::const_iterator ci = playerTactics.cityBuildingTacticsMap_.find(city.getCvCity()->getIDInfo());

                for (std::map<BuildingTypes, std::set<BuildingTypes> >::const_iterator di(tacticSelectionData.dependentBuildings.begin()),
                    diEnd(tacticSelectionData.dependentBuildings.end()); di != diEnd; ++di)
                {
                    for (std::set<BuildingTypes>::const_iterator bIter(di->second.begin()), bEndIter(di->second.end()); bIter != bEndIter; ++bIter)
                    {
                        PlayerTactics::CityBuildingTacticsList::const_iterator li = ci->second.find(*bIter);
                        if (li != ci->second.end())
                        {
                            const ProjectionLadder& ladder = li->second->getProjection();
                            if (!ladder.buildings.empty())
                            {
                                int nTurns = ladder.buildings[0].first;
                                //int thisValue = valueF(ladder.getOutput() - base) / std::max<int>(1, nTurns);
                                int scaledValue = TacticSelectionData::getScaledEconomicValue(numSimTurns, nTurns, timeHorizon, ladder.getOutput() - base, unitWeights);
                                if (scaledValue > bestValue)
                                {
                                    bestValue = scaledValue; //thisValue;
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
                int bestCityDefenceUnitValue = 0;
                for (std::set<UnitTacticValue>::const_iterator unitIter(tacticSelectionData.thisCityDefenceUnits.begin()), unitEndIter(tacticSelectionData.thisCityDefenceUnits.end()); unitIter != unitEndIter; ++unitIter)
                {
                    cityDefenceUnits.push_back(unitIter->unitType);
                    thisCityUnitDefenceValues[unitIter->unitType] = unitIter->unitAnalysisValue;
                    if (unitIter->unitAnalysisValue > bestCityDefenceUnitValue)
                    {
                        bestCityDefenceUnitValue = unitIter->unitAnalysisValue;
                        bestCityDefenceUnit = unitIter->unitType;
                    }
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

            void calculateBestSeaScoutUnit()
            {
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

            void calcEconomicBuildItems()
            {
                calcPossibleCityBuildItems(currentCityOutput, thisCityEconomicBuildsData);
                calcPossibleCityBuildItems(currentCivOutput, economicBuildsData);
            }

            void calcPossibleCityBuildItems(TotalOutput refOutput, EconomicBuildsData& buildsData)
            {
                const int turnsAvailable = player.getAnalysis()->getNumSimTurns();
                TotalOutputWeights unitWeights(makeOutputW(1, 1, 1, 1, 0, 0));

                std::vector<OutputTypes> outputTypes;
                outputTypes.push_back(OUTPUT_FOOD);
                outputTypes.push_back(OUTPUT_PRODUCTION);
                outputTypes.push_back(OUTPUT_RESEARCH);
                outputTypes.push_back(OUTPUT_GOLD);

                for (std::set<EconomicBuildingValue>::const_iterator baseIter(tacticSelectionData.economicBuildings.begin()), baseEndIter(tacticSelectionData.economicBuildings.end());
                    baseIter != baseEndIter; ++baseIter)
                {
                    if (baseIter->city.iID != city.getID())
                    {
                        continue;
                    }
                    if (baseIter->hasPositiveEconomicValue(numSimTurns, unitWeights))
                    {
                        buildsData.cityBuildItems.push_back(&*baseIter);
                    }
                    /*if (buildsData.usedCityTurns <= turnsAvailable && TacticSelectionData::isSignificantTacticItem(*baseIter, refOutput, turnsAvailable, outputTypes))
                    {
                        buildsData.cityBuildItems.push_back(&*baseIter);
                        buildsData.usedCityTurns += baseIter->nTurns;
                    }*/
                }

                EconomicBuildingTacticValueComp comp(numSimTurns, timeHorizon, unitWeights);
                buildsData.cityBuildItems.sort(comp);
#ifdef ALTAI_DEBUG
                civLog << "\n" << __FUNCTION__;
                buildsData.debug(civLog);
#endif
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

            bool chooseUnitOrAssociatedBuilding(UnitTypes unitType, const UnitTacticValue& unitValue, const std::string& debugString)
            {
                BuildingConditions buildConditions;
                buildConditions.noGlobalWonders().noNationalWonders();
                std::pair<int, BuildingTypes> buildingAndValue = getBestMilitaryBuilding(unitType, buildConditions);

                MilitaryBuildingValue buildingValue = tacticSelectionData.getMilitaryBuildingValue(buildingAndValue.second);
                if (buildingAndValue.first > 20 && buildingValue.nTurns < unitValue.nTurns * 2 && setConstructItem(buildingAndValue.second))
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) Returning mil building for : " << debugString << " unit: " << gGlobals.getUnitInfo(unitType).getType() << selection
                        << " value, mil building: " << buildingAndValue.first << ", "
                        << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType())
                        << " unit value: ";
                    unitValue.debug(civLog);
#endif
                    return true;
                }
                else if (setConstructItem(unitType))
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) Returning " << debugString << " unit: " << gGlobals.getUnitInfo(unitType).getType() << selection
                        << " value, mil building: " << buildingAndValue.first << ", "
                        << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());
#endif
                    return true;
                }

                return false;
            }

            bool chooseUnit(UnitTypes requestedUnit)
            {
                UnitTypes combatUnit = chooseUnit(combatUnits), collateralUnit = chooseUnit(collateralUnits), attackUnit = chooseUnit(cityAttackUnits);
                const int thisCombatUnitCount = player.getUnitCount(combatUnit), collateralUnitCount = player.getUnitCount(collateralUnit),
                          thisAttackUnitCount = player.getUnitCount(attackUnit), cityDefenceUnitCount = player.getUnitCount(bestCityDefenceUnit);

                UnitTacticValue combatUnitValue = TacticSelectionData::getUnitValue(tacticSelectionData.cityAttackUnits, combatUnit);
                UnitTacticValue collateralUnitValue = TacticSelectionData::getUnitValue(tacticSelectionData.collateralUnits, collateralUnit);
                UnitTacticValue attackUnitValue = TacticSelectionData::getUnitValue(tacticSelectionData.cityAttackUnits, attackUnit);
                UnitTacticValue cityDefenceUnitValue = TacticSelectionData::getUnitValue(tacticSelectionData.cityDefenceUnits, combatUnit);

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

                bool isRequestedUnit = requestedUnitData.unitType == requestedUnit;  // request from a mission
                int requestedUnitCount = player.getUnitCount(requestedUnit);
                int underConstructionCount = player.getUnitCount(requestedUnit, true) - requestedUnitCount;

                int maxGoldRate = player.getGoldAndResearchRates().rbegin()->first;  // 100% commerce

                int iFreeUnits, iFreeMilitaryUnits, iPaidUnits, iPaidMilitaryUnits, iMilitaryCost, iBaseUnitCost, iExtraCost;
                const int supportCost = player.getCvPlayer()->calculateUnitCost(iFreeUnits, iFreeMilitaryUnits, iPaidUnits, iPaidMilitaryUnits, iMilitaryCost, iBaseUnitCost, iExtraCost);
                const int goldRate = currentCivOutput[OUTPUT_GOLD] / 100;

                int maxUnitCount = 0;

                if (maxResearchRate < 30)
                {
                    maxUnitCount = 1 + cityCount + (popCount / 5);
                }
                else if (maxResearchRate < 60)
                {
                    maxUnitCount = 2 + cityCount + (popCount / (possiblyIsolated ? 4 : 3));
                }
                else
                {
                    maxUnitCount = 1 + 2 * cityCount + (popCount / (possiblyIsolated ? 3 : 2));
                }

#ifdef ALTAI_DEBUG
                civLog << "\nUnit counts: (max unit count = " << maxUnitCount << ", free = "<< iFreeMilitaryUnits << ") ";
                civLog << "\nRequested unit: " << gGlobals.getUnitInfo(requestedUnit).getType() << ", requestedUnitCount: "
                    << requestedUnitCount << ", underConstructionCount: " << underConstructionCount
                    << ", mission request count: " << requestedUnitData.missionRequestCount
                    << ", is requested: " << isRequestedUnit;
                civLog << "\nmaxResearchRate: " << maxResearchRate << " maxResearchRateWithProcesses: " << maxResearchRateWithProcesses
                    << ", supportCost: " << supportCost << ", goldRate: " << goldRate << ", maxGoldRate: " << maxGoldRate;
                civLog << "\n iFreeUnits: " << iFreeUnits << ", iFreeMilitaryUnits: " << iFreeMilitaryUnits << ", iPaidUnits: " << iPaidUnits
                    << ", iPaidMilitaryUnits: " << iPaidMilitaryUnits << ", iMilitaryCost: " << iMilitaryCost << ", iBaseUnitCost: " << iBaseUnitCost
                    << ", iExtraCost: " << iExtraCost << "\n";

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

                // if not requested unit for mission and already have 
                if (player.getCvPlayer()->getNumMilitaryUnits() > maxUnitCount && !isRequestedUnit && requestedUnitData.missionRequestCount >= requestedUnitCount)
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
                    UnitTacticValue combatUnitValue = TacticSelectionData::getUnitValue(tacticSelectionData.fieldAttackUnits, requestedUnit);
                    std::pair<int, BuildingTypes> buildingAndValue = getBestMilitaryBuilding(requestedUnit, buildConditions);

                    MilitaryBuildingValue buildingValue = tacticSelectionData.getMilitaryBuildingValue(buildingAndValue.second);
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

                const bool haveCombatUnit = combatUnit != NO_UNIT;
                const bool haveDistinctAttackUnit = attackUnit != combatUnit && attackUnit != NO_UNIT;
                if (haveDistinctAttackUnit)
                {
                    if (thisAttackUnitCount <= thisCombatUnitCount && thisAttackUnitCount < 1 + maxUnitCount / 3)
                    {
                        UnitTacticValue thisUnitValue = (attackUnitValue.unitType == attackUnit ? attackUnitValue :
                            (collateralUnitValue.unitType == attackUnit ? collateralUnitValue : combatUnitValue));

                        if (chooseUnitOrAssociatedBuilding(attackUnit, thisUnitValue, "attack"))
                        {
                            return true;
                        }
                    }
                }
                    
                if (haveCombatUnit && thisCombatUnitCount < 1 + maxUnitCount / 3)
                {
                    UnitTacticValue thisUnitValue = (combatUnitValue.unitType == combatUnit ? combatUnitValue :
                        (collateralUnitValue.unitType == combatUnit ? collateralUnitValue : attackUnitValue));

                    if (chooseUnitOrAssociatedBuilding(combatUnit, thisUnitValue, "combat"))
                    {
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
                        const int subArea = city.getCvCity()->plot()->getSubArea();
                        boost::shared_ptr<MapAnalysis> pMapAnalysis = playerTactics.player.getAnalysis()->getMapAnalysis();
                        // doesn't count border plots (in the sense of the invisible plots at the edge of our map knowledge) that are in different sub areas
                        // (so a long coastline doesn't make us generate excess land explorers)
                        const int borderLength = pMapAnalysis->getUnrevealedBorderCount(subArea);  
                        // don't care if they are not the best type of scout - just that they are exploring in this sub area
                        size_t activeScoutUnits = player.getAnalysis()->getMilitaryAnalysis()->getUnitCount(MISSIONAI_EXPLORE, NO_UNIT, subArea);

                        if (activeScoutUnits < 1 + borderLength / 20 && activeScoutUnits < scoutConditions.maxExisting_)
                        {
                            if (setConstructItem(bestScoutUnit))
                            {
#ifdef ALTAI_DEBUG
                                civLog << "\n(getConstructItem) Returning land scout unit: " << gGlobals.getUnitInfo(bestScoutUnit).getType() << selection;
#endif
                                return true;
                            }
                        }
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) active land scouts = " << activeScoutUnits << ", border length = " << borderLength
                            << " cond: max exist = " << scoutConditions.maxExisting_;
#endif
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
                    std::vector<int> accessibleSubAreas = city.getAccessibleSubAreas(gGlobals.getUnitInfo(ci->first).getDomainType() == DOMAIN_SEA);
                    if (consumed && ci->second.getHighestConsumedBuildValue(accessibleSubAreas) > 0)
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
#ifdef ALTAI_DEBUG
                    civLog << "\n" << __FUNCTION__ << " checking build: " << gGlobals.getBuildingInfo((*li)->buildingType).getType();
                    buildingConditions.debug(civLog); 
#endif
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
                        civLog << "\n(getConstructItem) Returning def mil building for city: " << safeGetCityName(city.getCvCity()) << " " << selection
                               << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());
#endif
                        return true;
                    
                    }
                }

                buildingAndValue = getBestDefensiveMilitaryBuilding(buildingConditions);

                if (playerLands && buildingAndValue.first > 20 && setConstructItem(buildingAndValue.second))
                {
#ifdef ALTAI_DEBUG
                    civLog << "\n(getConstructItem) Returning mil building for city: " << safeGetCityName(city.getCvCity())
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

                    // todo - this will tend to build barracks/stables/etc regardless of their immediate utility
                    // would like to be more specific if we want to build say a barracks because there's not a lot else to do currently
                    // possibly add this into the buildingConditions
                    if (setConstructItem(ci->buildingType))
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\n(Military Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType() << " turns = " << ci->nTurns;
                        civLog << "\n(getConstructItem) Returning mil building for city: " << selection
                           << " value, mil building: " << buildingAndValue.first << ", " << (buildingAndValue.second == NO_BUILDING ? "none" : gGlobals.getBuildingInfo(buildingAndValue.second).getType());
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
                if (processConditions.forceProcess_ || maxResearchRate < processConditions.maxResearchRate_)
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
            
            bool chooseReligionSpread()
            {
                bool canTrainUnit = city.getCvCity()->canTrain(requestedReligionUnitData.unitType, city.getCvCity()->getProductionUnit() == requestedReligionUnitData.unitType);
                bool canConstructDepBuilding = !canTrainUnit && 
                    requestedReligionUnitData.depBuildingType != NO_BUILDING && 
                    city.getCvCity()->canConstruct(requestedReligionUnitData.depBuildingType, city.getCvCity()->getProductionBuilding() == requestedReligionUnitData.depBuildingType);
#ifdef ALTAI_DEBUG
                civLog << "\n " << __FUNCTION__ << " " << gGlobals.getUnitInfo(requestedReligionUnitData.unitType).getType() << " " << canTrainUnit << ", " << canConstructDepBuilding;
#endif

                if (canTrainUnit)
                {
                    std::map<UnitTypes, ReligionUnitValue>::const_iterator ruIter = tacticSelectionData.spreadReligionUnits.find(requestedReligionUnitData.unitType);
                    if (ruIter != tacticSelectionData.spreadReligionUnits.end())
                    {
                        if (ruIter->second.nTurns <= 2 * requestedReligionUnitData.bestCityBuildTime)
                        {
                            if (setConstructItem(requestedReligionUnitData.unitType))
                            {
#ifdef ALTAI_DEBUG
                                civLog << "\n(getConstructItem) Returning religion spread request unit: " << gGlobals.getUnitInfo(requestedReligionUnitData.unitType).getType() << selection;
#endif
                                return true;
                            }
                        }
#ifdef ALTAI_DEBUG
                        civLog << "\n(getConstructItem) not setting request unit: " << gGlobals.getUnitInfo(requestedReligionUnitData.unitType).getType() << " " << ruIter->second.nTurns;
                        
#endif
                    }
                }
                else if (canConstructDepBuilding)
                {                    
                    EconomicBuildingValue ebValue = tacticSelectionData.getEconomicBuildingValue(requestedReligionUnitData.depBuildingType);
                    if (ebValue.hasPositiveEconomicValue(numSimTurns, makeOutputW(1, 1, 1, 1, 1, 1)))
                    {
                        if (setConstructItem(requestedReligionUnitData.depBuildingType))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning religion spread request unit building: " << gGlobals.getBuildingInfo(requestedReligionUnitData.depBuildingType).getType() << selection;
#endif
                            return true;
                        }
                    }
#ifdef ALTAI_DEBUG                    
                    civLog << "\n(getConstructItem) not setting request unit building: " << gGlobals.getBuildingInfo(requestedReligionUnitData.depBuildingType).getType();;
                    ebValue.debug(civLog);
#endif
                }

#ifdef ALTAI_DEBUG
                civLog << "\n(getConstructItem) chooseReligionSpread - false";
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
                if (requestedUnitData.unitType != NO_UNIT && chooseUnit(requestedUnitData.unitType))
                {
                    return selection;
                }

                /*if (chooseCityDefence())
                {
                    return selection;
                }*/

                // handle through unit requests now
                /*SettlerEscortConditions settlerEscortConditions;
                if (buildSettlerEscort(settlerEscortConditions))
                {
                    return selection;
                }*/

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
                    // todo - add logic to build sea scouts (i.e. workboats) for single city if we are stuck on an island
                    scoutConditions.maxExisting(2);
                    if (chooseScoutUnit(scoutConditions))
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
                /*settlerEscortConditions.buildSeaTransport(true);
                if (buildSettlerEscort(settlerEscortConditions))
                {
                    return selection;
                }*/

                {
                    BuildingConditions buildingConditions;
                    buildingConditions.allowNoEconomicDelta();

                    if (chooseEconomicBuilding(economicBuildsData))
                    {
                        return selection;
                    }
                }

                /*if (keepGrowing && combatUnitCount < 4 && chooseCombatUnit())
                {
                    return selection;
                }*/

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

                if (cityCount == 1)
                {
                    return getSingleCitySelection(currentCivOutput);
                }

                /*if (chooseCityDefence())
                {
                    return selection;
                }*/
                
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
                    if (requestedUnitData.unitType != NO_UNIT && chooseUnit(requestedUnitData.unitType))
                    {
                        return selection;
                    }

                    if ((atWarCount > 0 || warPlanCount > 0) && chooseUnit(bestCombatUnit))
                    {
                        return selection;
                    }

                    /*if (buildSettlerEscort())
                    {
                        return selection;
                    }*/

                    // can build worker units, and no. of plots to improve greater than 3 * no. workers
                    if (chooseWorker())
                    {
                        return selection;
                    }
                    
                    // not growing too rapidly (don't want to stifle growth too much, particularly for smaller cities)
                    if (!isDanger && (city.getCurrentOutputProjection().getPopChange() < 2 || city.getCvCity()->getWorkingPopulation() > 5))
                    {
                        SettlerConditions settlerConditions;
                        settlerConditions.maxExisting(1 + (player.getSettlerManager()->getBestCitySites(140, 4).size() / 2));
                        if (chooseSettler(settlerConditions))
                        {
                            return selection;
                        }
                    }

                    if (requestedReligionUnitData.unitType != NO_UNIT)
                    {
                        if (chooseReligionSpread())
                        {
                            return selection;
                        }
                    }

                    if (bestCityDefenceUnit != bestCombatUnit || (isDanger && bestCombatUnit != NO_UNIT))
                    {
                        if (requestedUnitData.unitType != NO_UNIT && chooseUnit(requestedUnitData.unitType))
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

                    ScoutConditions scoutConditions;
                    // todo - add logic to build sea scouts (i.e. workboats) for single city if we are stuck on an island
                    scoutConditions.buildSeaScout(true);
                    scoutConditions.maxExisting(4);
                    if (chooseScoutUnit(scoutConditions))
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
                    if (rankAndMaxProduction.first == 1)
                    {
                        if (chooseEconomicBuilding(economicBuildsData))
                        {
                            return selection;
                        }
                        else if (bestEconomicWonder != NO_BUILDING && setConstructItem(bestEconomicWonder))
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getConstructItem) Returning best economic wonder: " << gGlobals.getBuildingInfo(bestEconomicWonder).getType() << selection;
#endif
                            return selection;
                        }
                    }

                    if (city.getCvCity()->getWorkingPopulation() > 3)
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

                        if (requestedUnitData.unitType != NO_UNIT && chooseUnit(requestedUnitData.unitType))
                        {
                            return selection;
                        }
                    }
                }

                if (bestCityDefenceUnit != bestCombatUnit && (atWarCount > 0 || warPlanCount > 0))
                {
                    if (requestedUnitData.unitType != NO_UNIT && chooseUnit(requestedUnitData.unitType))
                    {
                        return selection;
                    }
                }

                if (requestedReligionUnitData.unitType != NO_UNIT)
                {
                    if (chooseReligionSpread())
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

            PlayerTactics& playerTactics;
            Player& player;
            City& city;
            TacticSelectionData& tacticSelectionData;

            const int numSimTurns;
            const int timeHorizon;

            TotalOutput cityBaseOutput;

            bool possiblyIsolated;
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

            BuildQueueItem bestDependentBuild;

            // todo - move unit data into separate unit manager
            UnitTypes bestCombatUnit, bestScoutUnit, bestCityDefenceUnit, bestCollateralUnit, bestSeaDefenceUnit, bestSeaTransportUnit, bestSeaScoutUnit, bestSettlerUnit;
            UnitTypes bestDependentTacticUnit;
            UnitRequestData requestedUnitData;
            ReligionUnitRequestData requestedReligionUnitData;
            std::vector<UnitTypes> combatUnits, cityAttackUnits, cityDefenceUnits, seaDefenceUnits, seaTransportUnits, collateralUnits;
            std::map<UnitCombatTypes, std::vector<UnitTypes> > counterUnits;
            std::map<UnitCombatTypes, UnitTypes> bestUnitCombatTypes;
            ProcessTypes bestEconomicProcess, bestResearchProcess, bestCultureProcess;            
            EconomicBuildsData economicBuildsData, thisCityEconomicBuildsData;

            std::map<UnitTypes, int> unitDefenceValues, thisCityUnitDefenceValues, unitAttackValues, unitCollateralValues;
            std::multimap<int, const MilitaryBuildingValue*> militaryBuildingValuesMap, thisCityMilitaryBuildingValuesMap;

            ConstructItem selection;

            std::ostream& civLog;
        };
    }
    
    ConstructItem getConstructItem(PlayerTactics& playerTactics, City& city)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream();
#endif
        CityBuildSelectionData selectionData(playerTactics, city);
        TacticSelectionDataMap& globalSelectionData = playerTactics.tacticSelectionDataMap;
        TacticSelectionData& baseGlobalSelectionData = playerTactics.getBaseTacticSelectionData();
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
                // buildings we could build if we had the required religion in this city
                // (building tactic and dep will only exist if we have the religion somewhere in player's civ)
                else if (li->second->areDependenciesSatisfied(IDependentTactic::Religion_Dep))
                {
                    li->second->update(playerTactics.player, city.getCityData());
                    li->second->apply(globalSelectionData, IDependentTactic::Religion_Dep);
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
                    if (pCityUnitTacticsPtr->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                    {
                        pCityUnitTacticsPtr->apply(selectionData.tacticSelectionData, IDependentTactic::Ignore_None);
                    }
                    else
                    {
#ifdef ALTAI_DEBUG
                        os << "\n\t city unit tactics: skipping unit: " << gGlobals.getUnitInfo(iter->first).getType();
                        std::vector<DependencyItem> depItems = pCityUnitTacticsPtr->getDepItems(IDependentTactic::City_Buildings_Dep);
                        for (size_t depIndex = 0, depCount = depItems.size(); depIndex < depCount; ++depIndex)
                        {
                            debugDepItem(depItems[depIndex], os);
                        }
#endif
                    }
                }
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\n\t unit tactics: skipping unit: " << gGlobals.getUnitInfo(iter->first).getType();
                CityUnitTacticsPtr pCityUnitTacticsPtr = iter->second->getCityTactics(cityID);
                if (pCityUnitTacticsPtr)
                {
                    std::vector<DependencyItem> depItems = pCityUnitTacticsPtr->getDepItems(IDependentTactic::City_Buildings_Dep);
                    for (size_t depIndex = 0, depCount = depItems.size(); depIndex < depCount; ++depIndex)
                    {
                        debugDepItem(depItems[depIndex], os);
                    }
                }
#endif
            }
        }

        // wonders
        for (PlayerTactics::LimitedBuildingsTacticsMap::const_iterator iter(playerTactics.globalBuildingsTacticsMap_.begin()),
            endIter(playerTactics.globalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            ICityBuildingTacticsPtr pGlobalCityBuildingTactics = iter->second->getCityTactics(cityID);
            if (iter->second->areDependenciesSatisfied(cityID, IDependentTactic::Ignore_None))
            {
                iter->second->update(playerTactics.player);
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
                iter->second->update(playerTactics.player);
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

        // general religion tactics
        playerTactics.player.getAnalysis()->getReligionAnalysis()->update();
        // ignore tech deps here because religion tactics only calculated for religions we already have present in at least one city
        // this means it's possible we could have a religion we can't research the tech for 
        playerTactics.player.getAnalysis()->getReligionAnalysis()->apply(baseGlobalSelectionData);

        DependencyItemSet di;
        TacticSelectionData& currentSelectionData = playerTactics.tacticSelectionDataMap[di];
        currentSelectionData.eraseCityEntries(city.getCvCity()->getIDInfo());
        currentSelectionData.merge(selectionData.tacticSelectionData);

        selectionData.requestedUnitData = playerTactics.player.getAnalysis()->getMilitaryAnalysis()->getUnitRequestBuild(city.getCvCity(), selectionData.tacticSelectionData);
        // don't bother with religion unit check if we already requested a military unit
        // effectively always puts military consideration first - although may want to consider the case of a building a missionary unit to spread religion for extra unit experience
        playerTactics.player.getAnalysis()->getReligionAnalysis()->setUnitRequestBuild(city.getCvCity(), playerTactics);
        if (selectionData.requestedUnitData.unitType == NO_UNIT)  
        {
            selectionData.requestedReligionUnitData = playerTactics.player.getAnalysis()->getReligionAnalysis()->getUnitRequestBuild(city.getCvCity(), playerTactics);
        }

        selectionData.calculateImprovementStats();

        selectionData.processUnits();
        selectionData.processBuildings();
        selectionData.processProjects();

        selectionData.calculateSmallCultureBuilding();
        selectionData.calculateLargeCultureBuilding();
        selectionData.calculateBestEconomicBuilding();
        selectionData.calculateBestDependentBuilding();
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
        selectionData.calculateBestCollateralUnits();
        selectionData.countCombatUnits();

        if (city.getCvCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
        {
            selectionData.calculateBestSeaScoutUnit();
            selectionData.calculateBestSeaDefenceUnits();
            selectionData.calculateBestSeaTransportUnits();
        }

        selectionData.calculateBestScoutUnit();

        selectionData.debug();

        ConstructItem selection = selectionData.getSelection();

#ifdef ALTAI_DEBUG
        if (selection.unitType != NO_UNIT && selection.unitType == selectionData.requestedUnitData.unitType)
        {
            os << " selected requested unit type: " << gGlobals.getUnitInfo(selection.unitType).getType();
        }

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

        ProjectionLadder projection = 
            getProjectedOutput(playerTactics.player, pCityData, playerTactics.player.getAnalysis()->getNumSimTurns(), events, selection, __FUNCTION__, selection.buildingType != NO_BUILDING, true);

        os << "\n(getConstructItem): Turn = " << gGlobals.getGame().getGameTurn() << " projection for " << narrow(city.getCvCity()->getName()) << ": ";
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
        //projection.debug(os);    
#endif

        return selection;
    }
}