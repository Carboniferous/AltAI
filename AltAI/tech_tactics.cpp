#include "AltAI.h"

#include "./tech_tactics.h"
#include "./city_unit_tactics.h"
#include "./tech_tactics_items.h"
#include "./tactic_actions.h"
#include "./tactic_selection_data.h"
#include "./tactic_streams.h"
#include "./tech_tactics_visitors.h"
#include "./building_tactics_visitors.h"
#include "./building_tactics_deps.h"
#include "./civic_tactics.h"
#include "./building_info_visitors.h"
#include "./unit_info_visitors.h"
#include "./resource_info_visitors.h"
#include "./resource_tactics.h"
#include "./trade_route_helper.h"
#include "./tech_info_visitors.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./city_simulator.h"
#include "./iters.h"
#include "./settler_manager.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./unit_analysis.h"
#include "./helper_fns.h"
#include "./civ_helper.h"
#include "./civ_log.h"

namespace AltAI
{
    namespace
    {
        struct TechSelectionData
        {
            explicit TechSelectionData(const PlayerTactics& playerTactics_)
                : playerTactics(playerTactics_), player(playerTactics_.player), 
                  bestTech(NO_TECH), selection(NO_TECH), civLog(CivLog::getLog(*player.getCvPlayer())->getStream())
            {
                pMapAnalysis = player.getAnalysis()->getMapAnalysis();
                civHelper = player.getCivHelper();
                currentOutputProjection = player.getCurrentProjectedOutput();

                possiblyIsolated = false;
                // todo - consider teams instead of players?
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
            }

            void sanityCheckTechs()
            {
                const CvTeam& team = CvTeamAI::getTeam(player.getTeamID());
                int leastTurns = MAX_INT;
                std::map<DependencyItemSet, int> possiblyTooExpensiveTechs;

                for (TacticSelectionDataMap::const_iterator ci(tacticSelectionDataMap.begin()), ciEnd(tacticSelectionDataMap.end());
                    ci != ciEnd; ++ci)
                {
                    for (DependencyItemSet::const_iterator di(ci->first.begin()), diEnd(ci->first.end()); di != diEnd; ++di)
                    {
                        if (di->first == ResearchTechDependency::ID)
                        {
                            TechTypes techType = (TechTypes)di->second;
                            // only works for techs we aren't researching
                            int totalTechCost = std::max<int>(1, player.getCvPlayer()->findPathLength(techType));

                            const int rate = player.getCvPlayer()->calculateResearchRate(techType);

                            const int approxTurns = 1 + totalTechCost / (rate == 0 ? 1 : rate);

                            techCostsMap[techType] = approxTurns;

                            if (approxTurns < leastTurns)
                            {
                                leastTurns = approxTurns;
                            }

                            if (approxTurns > (4 * gGlobals.getGame().getMaxTurns() / 100))
                            {
                                possiblyTooExpensiveTechs[ci->first] += approxTurns;
#ifdef ALTAI_DEBUG
                                civLog << "\nMarking tech as expensive: " << gGlobals.getTechInfo(techType).getType()
                                       << " with cost = " << totalTechCost << " approx turns = " << approxTurns << " rate = " << rate;
#endif
                            }
                            else
                            {
#ifdef ALTAI_DEBUG
                                civLog << "\nKeeping tech: " << gGlobals.getTechInfo(techType).getType()
                                       << " with cost = " << totalTechCost << " approx turns = " << approxTurns << " rate = " << rate;
#endif
                            }
                        }
                    }
                }

                for (std::map<DependencyItemSet, int>::const_iterator ci(possiblyTooExpensiveTechs.begin()),
                    ciEnd(possiblyTooExpensiveTechs.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second > 3 * leastTurns)
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\nErasing tech selection: ";
                        for (DependencyItemSet::const_iterator di(ci->first.begin()), diEnd(ci->first.end()); di != diEnd; ++di)
                        {
                            if (di != ci->first.begin()) civLog << "+";
                            debugDepItem(*di, civLog);                            
                        }
#endif
                        tacticSelectionDataMap.erase(ci->first);
                    }
                }
            }

            void processTechs()
            {
                sanityCheckTechs();
            }

            int getBaseTechScore(TechTypes techType, const TacticSelectionData& selectionData) const
            {
                TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));
                int score = 0;

                if (selectionData.possibleFreeTech != NO_TECH)
                {
                    const int techCost = calculateTechResearchCost(selectionData.possibleFreeTech, player.getPlayerID());
#ifdef ALTAI_DEBUG
                    const int rate = player.getCvPlayer()->calculateResearchRate(selectionData.possibleFreeTech);
                    civLog << "\nChecking free tech data for tech: " << gGlobals.getTechInfo(selectionData.possibleFreeTech).getType()
                        << " research rate for tech: " << rate << " cost = " << techCost << " approx turns = " << (1 + techCost / (rate == 0 ? 1 : rate));
#endif
                    score += techCost;
                }

                for (std::map<BuildingTypes, TechTypes>::const_iterator ci(selectionData.possibleFreeTechs.begin()), ciEnd(selectionData.possibleFreeTechs.end());
                    ci != ciEnd; ++ci)
                {
                    const int techCost = calculateTechResearchCost(ci->second, player.getPlayerID());
#ifdef ALTAI_DEBUG
                    const int rate = player.getCvPlayer()->calculateResearchRate(ci->second);
                    civLog << "\nChecking free tech data for tech: " << gGlobals.getTechInfo(ci->second).getType()
                        << " research rate for tech: " << rate << " cost = " << techCost << " approx turns = " << (1 + techCost / (rate == 0 ? 1 : rate));
#endif
                    score += techCost;
                }

                if (!isEmpty(selectionData.baselineDelta))
                {
                    score += valueF(selectionData.baselineDelta);
#ifdef ALTAI_DEBUG
                    civLog << "\n baseline delta for tech: " << selectionData.baselineDelta << " score = " << valueF(selectionData.baselineDelta)
                        << " % of current output = " << asPercentageOf(selectionData.baselineDelta, currentOutputProjection);
#endif
                }

                return score;
            }

            int getBestUnitValue(const std::set<UnitTacticValue>& unitValues) const
            {
                int bestValue = 0;
                std::map<UnitTypes, int> unitAnalysisValues, unitCosts;
                for (std::set<UnitTacticValue>::const_iterator ci(unitValues.begin()), ciEnd(unitValues.end()); ci != ciEnd; ++ci)
                {
                    unitAnalysisValues[ci->unitType] += ci->unitAnalysisValue;
                    unitCosts[ci->unitType] = ci->nTurns;
                }

                return unitAnalysisValues.empty() ? 0 : unitAnalysisValues.rbegin()->second;
            }

            int getUnitValue(TechTypes techType, const TacticSelectionData& selectionData) const
            {
                int value = TacticSelectionData::getUnitTacticValue(selectionData.cityAttackUnits) +
                    TacticSelectionData::getUnitTacticValue(selectionData.cityDefenceUnits) +
                    TacticSelectionData::getUnitTacticValue(selectionData.collateralUnits);

                if (atWarCount == 0 && warPlanCount == 0)
                {
                    value /= 3;
                }

                // ignore question of war for now - want to also value escorts for expansion if isolated as well as cargo ships
                const int seaMultiplier = (possiblyIsolated ? 4 : 1);
                value += TacticSelectionData::getUnitTacticValue(selectionData.seaCombatUnits) * seaMultiplier;

                return value;
            }

            int getExpansionValue(TechTypes techType, const TacticSelectionData& selectionData) const
            {
                if (selectionData.cultureSources.empty())
                {
                    return 0;
                }

                XYCoords bestTargetPlot = player.getSettlerManager()->getBestPlot();
                if (bestTargetPlot == XYCoords())
                {
                    return 0;
                }

                const int freeCityCulture = player.getCvPlayer()->getFreeCityCommerce(COMMERCE_CULTURE);
                int bestIndex = -1, bestValue = 0;
                for (size_t i = 0, count = selectionData.cultureSources.size(); i < count; ++i)
                {
                    int thisValue = (std::max<int>(0, selectionData.cultureSources[i].globalValue - freeCityCulture) * 20 * 100) / selectionData.cultureSources[i].cityCost;
                    if (thisValue > bestValue)
                    {
                        bestValue = thisValue;
                        bestIndex = i;
                    }
                }

                if (bestIndex == -1)
                {
                    return 0;
                }

                PlotYield delta;
                DotMapItem targetDotMap = player.getSettlerManager()->getPlotDotMap(bestTargetPlot);
                const DotMapItem::PlotDataSet& plotDataSet = targetDotMap.plotDataSet;
                for (DotMapItem::PlotDataSet::const_iterator ci(plotDataSet.begin()), ciEnd(plotDataSet.end()); ci != ciEnd; ++ci)
                {
                    if (stepDistance(targetDotMap.coords.iX, targetDotMap.coords.iY, ci->coords.iX, ci->coords.iY) > 1)
                    {
                        delta += ci->getPlotYield();
                    }
                }
#ifdef ALTAI_DEBUG
                //civLog << "\ngetExpansionValue: " << bestValue << ", " << delta;
#endif
                return bestValue * YieldValueFunctor(makeYieldW(1, 1, 1))(delta);
            }

            int getConnectableResourceValue(TechTypes techType, TacticSelectionData& selectionData)
            {
                if (selectionData.connectableResources.empty())
                {
                    return 0;
                }

                int score = 0;

                boost::shared_ptr<PlayerTactics> pPlayerTactics = player.getAnalysis()->getPlayerTactics();
                std::map<BonusTypes, std::vector<UnitTypes> > unitBonusMap;

#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
                os << "\ngetConnectableResourceValue() for tech: " << gGlobals.getTechInfo(techType).getType();
                for (std::map<BonusTypes, std::vector<XYCoords> >::const_iterator ci(selectionData.connectableResources.begin()), 
                    ciEnd(selectionData.connectableResources.end()); ci != ciEnd; ++ci)
                {
                    os << " resource: " << gGlobals.getBonusInfo(ci->first).getType() << " at: ";
                    for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                    {
                        os << ci->second[i] << " ";
                    }
                }
#endif

                for (PlayerTactics::UnitTacticsMap::const_iterator iter(pPlayerTactics->unitTacticsMap_.begin()),
                    endIter(player.getAnalysis()->getPlayerTactics()->unitTacticsMap_.end()); iter != endIter; ++iter)
                {
                    const CvCity* pCapitalCity = player.getCvPlayer()->getCapitalCity();
                    if (pCapitalCity)  // barbs have no capital (and nor do we on first turn); probably don't care about them researching road building too much
                    {
                        CityUnitTacticsPtr pCityTactics = iter->second->getCityTactics(pCapitalCity->getIDInfo());
                        if (pCityTactics)
                        {
                            const std::vector<IDependentTacticPtr>& pDepItems = pCityTactics->getDependencies();
                            for (size_t i = 0, count = pDepItems.size(); i < count; ++i)
                            {
                                const std::vector<DependencyItem>& thisDepItems = pDepItems[i]->getDependencyItems();
                                for (size_t j = 0, depItemCount = thisDepItems.size(); j < depItemCount; ++j)
                                {
                                    if (thisDepItems[j].first == CityBonusDependency::ID)
                                    {
                                        unitBonusMap[(BonusTypes)thisDepItems[j].second].push_back(iter->first);
#ifdef ALTAI_DEBUG
                                        os << "\n\tadded dep item: " << gGlobals.getBonusInfo((BonusTypes)thisDepItems[j].second).getType();
#endif
                                    }
                                }
                            }
                        }
                    }
                }

                const TacticSelectionData& baseSelectionData = playerTactics.getBaseTacticSelectionData();

                for (std::map<BonusTypes, std::vector<XYCoords> >::const_iterator ci(selectionData.connectableResources.begin()), 
                    ciEnd(selectionData.connectableResources.end()); ci != ciEnd; ++ci)
                {
                    std::map<BonusTypes, std::pair<TotalOutput, TotalOutput> >::const_iterator potentialResourceOutputIter = baseSelectionData.potentialResourceOutputDeltas.find(ci->first);
                    if (potentialResourceOutputIter != baseSelectionData.potentialResourceOutputDeltas.end())
                    {
                        if (!isEmpty(potentialResourceOutputIter->second.first))
                        {
                            TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));                        
                            score += valueF(potentialResourceOutputIter->second.first);
#ifdef ALTAI_DEBUG
                            os << "\n\t" << gGlobals.getBonusInfo(ci->first).getType() << " added connectable resource value: " << valueF(potentialResourceOutputIter->second.first);
#endif
                        }
                        else
                        {
#ifdef ALTAI_DEBUG
                            os << "\n\t no impact for connection of resource: " << gGlobals.getBonusInfo(ci->first).getType();
#endif
                        }
                    }
                    else
                    {
#ifdef ALTAI_DEBUG
                        os << "\n\t resource not found in potentialResourceOutputDeltas: " << gGlobals.getBonusInfo(ci->first).getType();
#endif
                    }

                    std::map<BonusTypes, std::vector<UnitTypes> >::const_iterator unitsIter = unitBonusMap.find(ci->first);
                    if (unitsIter != unitBonusMap.end())
                    {
                        for (size_t unitIndex = 0, unitCount = unitsIter->second.size(); unitIndex < unitCount; ++unitIndex)
                        {
                            score += getUnitValue(NO_TECH, baseSelectionData);
#ifdef ALTAI_DEBUG
                            os << "\n\t unit value from base selection date: " << getUnitValue(NO_TECH, baseSelectionData);
                            //os << "\n\tadded unit value: " << gGlobals.getUnitInfo(unitsIter->second[unitIndex]).getType() << " = "
                            //    << player.getAnalysis()->getUnitAnalysis()->getCurrentUnitValue(unitsIter->second[unitIndex]);
#endif                            
                        }
                    }
                }

                for (TacticSelectionDataMap::iterator tacticMapIter(tacticSelectionDataMap.begin()), tacticMapEndIter(tacticSelectionDataMap.end()); tacticMapIter != tacticMapEndIter; ++tacticMapIter)
                {
                    for (DependencyItemSet::const_iterator depIter(tacticMapIter->first.begin()), depEndIter(tacticMapIter->first.end()); depIter != depEndIter; ++depIter)
                    {
                        if (depIter->first == ResouceProductionBonusDependency::ID)
                        {
                            std::map<BonusTypes, std::vector<XYCoords> >::const_iterator crIter = selectionData.connectableResources.find((BonusTypes)depIter->second);
                            if (crIter != selectionData.connectableResources.end())
                            {
#ifdef ALTAI_DEBUG
                                os << "\n\t found building tactic with usable resource acceleration dependency: " << gGlobals.getBonusInfo((BonusTypes)depIter->second).getType();
                                tacticMapIter->second.debug(os);
                                os << "\n";
#endif
                                for (std::set<EconomicBuildingValue>::const_iterator ebIter(tacticMapIter->second.economicBuildings.begin()), ebEndIter(tacticMapIter->second.economicBuildings.end());
                                    ebIter != ebEndIter; ++ebIter)
                                {
                                    BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(ebIter->buildingType).getBuildingClassType();
                                    if (isWorldWonderClass(buildingClassType))
                                    {
                                        PlayerTactics::LimitedBuildingsTacticsMap::iterator pBuildingTacticIter = pPlayerTactics->globalBuildingsTacticsMap_.find(ebIter->buildingType);
                                        if (pBuildingTacticIter != pPlayerTactics->globalBuildingsTacticsMap_.end())
                                        {
                                            CityDataPtr pCopyCityData = player.getCity(ebIter->city.iID).getCityData()->clone();
                                            ICityBuildingTacticsPtr pCityBuildingTactics = pBuildingTacticIter->second->getCityTactics(ebIter->city);
                                            std::vector<IDependentTacticPtr> pCityDeps =  pCityBuildingTactics->getDependencies();
                                            for (size_t depIndex = 0, depCount = pCityDeps.size(); depIndex < depCount; ++depIndex)
                                            {
                                                pCityDeps[depIndex]->apply(pCopyCityData);
                                            }

                                            std::vector<DependencyItem> newDepItems = pCityBuildingTactics->getDepItems(IDependentTactic::Ignore_Techs);
                                            DependencyItemSet newDepSet(newDepItems.begin(), newDepItems.end());

                                            pCityBuildingTactics->update(player, pCopyCityData);
                                            TacticSelectionDataMap tacticSelectionDataMap;
                                            pCityBuildingTactics->apply(tacticSelectionDataMap, IDependentTactic::Ignore_Techs);

                                            for (size_t depIndex = 0, depCount = pCityDeps.size(); depIndex < depCount; ++depIndex)
                                            {
                                                pCityDeps[depIndex]->remove(pCopyCityData);
                                            }
#ifdef ALTAI_DEBUG
                                            os << " diff with resource for city: " << narrow(player.getCity(ebIter->city.iID).getCvCity()->getName()) << " orig: ";
                                            tacticMapIter->second.debug(os);
                                            os << " after: ";
                                            tacticSelectionDataMap[newDepSet].debug(os);
#endif
                                        }
                                    }
                                    else if (isNationalWonderClass(buildingClassType))
                                    {
                                    }
                                    else
                                    {
                                    }
                                }                                
                            }
                        }
                    }
                }

                return score;
            }

            void calcBaseCityBuildItems(const TacticSelectionData& baseSelectionData, TotalOutput currentOutput)
            {
                const int turnsAvailable = player.getAnalysis()->getNumSimTurns();
                TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));
                const int currentOutputValue = valueF(currentOutput) * turnsAvailable / 100;  // ~1% output threshold

                std::vector<OutputTypes> outputTypes;
                outputTypes.push_back(OUTPUT_FOOD);
                outputTypes.push_back(OUTPUT_PRODUCTION);
                outputTypes.push_back(OUTPUT_RESEARCH);
                outputTypes.push_back(OUTPUT_GOLD);

                for (std::set<EconomicBuildingValue>::const_iterator baseIter(baseSelectionData.economicBuildings.begin()), baseEndIter(baseSelectionData.economicBuildings.end()); baseIter != baseEndIter; ++baseIter)
                {
                    if (economicBuildsData.usedCityTurns[baseIter->city] <= turnsAvailable && 
                        TacticSelectionData::isSignificantTacticItem(*baseIter, currentOutput, turnsAvailable, outputTypes))
                    {
                        economicBuildsData.cityBuildItemsMap[baseIter->city].push_back(&*baseIter);
                        economicBuildsData.usedCityTurns[baseIter->city] += baseIter->nTurns;
                    }
                }

                civLog << "\nBase selection:" ;
                for (std::map<IDInfo, std::list<const EconomicBuildingValue*> >::const_iterator ci(economicBuildsData.cityBuildItemsMap.begin()), ciEnd(economicBuildsData.cityBuildItemsMap.end()); ci != ciEnd; ++ci)
                {
                    civLog << "\n\tCity: " << narrow(::getCity(ci->first)->getName()) << " ";
                    for (std::list<const EconomicBuildingValue*>::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                    {
                        (*li)->debug(civLog);
                    }
                }
            }

            std::list<const EconomicBuildingValue*> compareTacticToBase(const TacticSelectionData& selectionData, TotalOutput currentOutput)
            {
                const int turnsAvailable = player.getAnalysis()->getNumSimTurns();
                TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));
                const int currentOutputValue = valueF(currentOutput) * turnsAvailable / 100;  // ~1% output threshold

                std::vector<OutputTypes> outputTypes;
                outputTypes.push_back(OUTPUT_FOOD);
                outputTypes.push_back(OUTPUT_PRODUCTION);
                outputTypes.push_back(OUTPUT_RESEARCH);
                outputTypes.push_back(OUTPUT_GOLD);

                std::map<IDInfo, int> usedCityTurns(economicBuildsData.usedCityTurns);
                std::map<IDInfo, std::list<const EconomicBuildingValue*> > cityBuildItemsMapForTech(economicBuildsData.cityBuildItemsMap);

                std::list<const EconomicBuildingValue*> selectedTacticItems;
                // iterate over tech tactic economic buildings
                for (std::set<EconomicBuildingValue>::const_iterator iter(selectionData.economicBuildings.begin()), endIter(selectionData.economicBuildings.end()); iter != endIter; ++iter)
                {
                    std::map<IDInfo, std::list<const EconomicBuildingValue*> >::iterator currentBuildsIter(cityBuildItemsMapForTech.find(iter->city));
                    if (currentBuildsIter != cityBuildItemsMapForTech.end())
                    {
                        // check tech selected build v. base tactic set of builds
                        for (std::list<const EconomicBuildingValue*>::iterator li(currentBuildsIter->second.begin()), liEnd(currentBuildsIter->second.end()); li != liEnd; ++li)
                        {
                            if (valueF(iter->output) > valueF((*li)->output))  // better building from tech...
                            {
                                currentBuildsIter->second.insert(li, &*iter);
                                usedCityTurns[iter->city] += iter->nTurns;

                                while (usedCityTurns[iter->city] - (*currentBuildsIter->second.rbegin())->nTurns > turnsAvailable)
                                {                                    
                                    usedCityTurns[iter->city] -= (*currentBuildsIter->second.rbegin())->nTurns;
                                    currentBuildsIter->second.pop_back();
                                }
                                
                                selectedTacticItems.push_back(&*iter);
                                break;
                            }
                        }
                    }
                    else if (TacticSelectionData::isSignificantTacticItem(*iter, currentOutput, turnsAvailable, outputTypes))
                    {
                        cityBuildItemsMapForTech[iter->city].push_back(&*iter);
                        usedCityTurns[iter->city] += iter->nTurns;
                        selectedTacticItems.push_back(&*iter);
                    }
                }

                civLog << "\n\tSelected items: ";
                for (std::list<const EconomicBuildingValue*>::iterator li(selectedTacticItems.begin()), liEnd(selectedTacticItems.end()); li != liEnd; ++li)
                {
                    civLog << narrow(::getCity((*li)->city)->getName()) << " ";
                    (*li)->debug(civLog);
                }

                for (std::map<IDInfo, std::list<const EconomicBuildingValue*> >::const_iterator ci(cityBuildItemsMapForTech.begin()), ciEnd(cityBuildItemsMapForTech.end()); ci != ciEnd; ++ci)
                {
                    civLog << "\n\tCity: " << narrow(::getCity(ci->first)->getName()) << " ";
                    for (std::list<const EconomicBuildingValue*>::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                    {
                        (*li)->debug(civLog);
                    }
                }
                civLog << "\n";

                return selectedTacticItems;
            }

            void scoreTechs()
            {
                int bestTechScore = 0;
                TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1)), processValueF(makeOutputW(0, 0, 4, 3, 2, 1));

                TotalOutput currentOutput = player.getCurrentOutput();
                int baseAttackUnitScore = 0, baseDefenceUnitScore = 0, baseCollateralUnitScore = 0;
                const int turnsAvailable = player.getAnalysis()->getNumSimTurns();

                civLog << "\nCiv base output = " << currentOutput;

                int maxResearchRate = player.getMaxResearchPercent();  // % max rate

                DependencyItemSet noDep;
                noDep.insert(DependencyItem(-1, -1));
                // todo - check if this is correct in case of other non-tech deps
                TacticSelectionDataMap::const_iterator noDepIter = tacticSelectionDataMap.find(noDep);
                if (noDepIter != tacticSelectionDataMap.end())
                {
                    calcBaseCityBuildItems(noDepIter->second, currentOutput);
                    baseAttackUnitScore = getBestUnitValue(noDepIter->second.cityAttackUnits);
                    baseDefenceUnitScore = getBestUnitValue(noDepIter->second.cityDefenceUnits);
                    baseCollateralUnitScore = getBestUnitValue(noDepIter->second.collateralUnits);
                }

                civLog << " current best attack unit score = " << baseAttackUnitScore << ", current best defence unit score = " << baseDefenceUnitScore
                                << ", current best collateral unit score = " << baseCollateralUnitScore;

                for (TacticSelectionDataMap::iterator ci(tacticSelectionDataMap.begin()), ciEnd(tacticSelectionDataMap.end());
                    ci != ciEnd; ++ci)
                {
                    for (DependencyItemSet::const_iterator di(ci->first.begin()), diEnd(ci->first.end()); di != diEnd; ++di)
                    {
                        if (di->first == ResearchTechDependency::ID)  // skip no dependency ('base' items)
                        {
                            TechTypes thisTechType = (TechTypes)di->second;
                            civLog << "\nTech score for: " << gGlobals.getTechInfo(thisTechType).getType();
                            //getSignificantTactics(ci->second, currentOutput);
                            std::list<const EconomicBuildingValue*> selectedItems = compareTacticToBase(ci->second, currentOutput);

                            if (ci->second.possibleFreeTech != NO_TECH)
                            {
                                freeTechTechs.push_back((TechTypes)di->second);
                            }
                            int baseTechScore = getBaseTechScore((TechTypes)di->second, ci->second);

                            if (baseTechScore != 0)
                            {
                                civLog << " base tech score = " << baseTechScore;
                            }

                            int workerScore = valueF(ci->second.cityImprovementsDelta);
                            if (workerScore != 0)
                            {
                                civLog << " cityImprovementsDelta = " << ci->second.cityImprovementsDelta << ", score = " << workerScore;
                            }

                            int bonusScore = getConnectableResourceValue(thisTechType, ci->second);

                            int resourceScore = valueF(ci->second.resourceOutput);

                            int expansionScore = getExpansionValue(thisTechType, ci->second);

                            int unitScore = getUnitValue(thisTechType, ci->second);

                            int bestAttackUnitScore = getBestUnitValue(ci->second.cityAttackUnits);
                            int bestDefenceUnitScore = getBestUnitValue(ci->second.cityDefenceUnits);
                            int bestCollateralUnitScore = getBestUnitValue(ci->second.collateralUnits);

                            int buildingScore = 0;

                            /*for (std::set<EconomicBuildingValue>::const_iterator vi(ci->second.economicBuildings.begin()),
                                viEnd(ci->second.economicBuildings.end()); vi != viEnd; ++vi)
                            {
                                buildingScore += valueF(vi->output);
                            }*/
                            for (std::list<const EconomicBuildingValue*>::const_iterator selectedEconomicBuildingsIter(selectedItems.begin()),
                                selectedEconomicBuildingsEndIter(selectedItems.end()); selectedEconomicBuildingsIter != selectedEconomicBuildingsEndIter; ++selectedEconomicBuildingsIter)
                            {
                                buildingScore += valueF((*selectedEconomicBuildingsIter)->output);
                            }

                            for (std::set<MilitaryBuildingValue>::const_iterator mi(ci->second.militaryBuildings.begin()),
                                miEnd(ci->second.militaryBuildings.end()); mi != miEnd; ++mi)
                            {
                                civLog << "\n\t mil building: " << gGlobals.getBuildingInfo(mi->buildingType).getType() << ' ';
                                std::list<std::pair<UnitTypes, int> > unitValueDiffs = TacticSelectionData::getUnitValueDiffs(mi->cityAttackUnits, noDepIter->second.cityAttackUnits);

                                for (std::list<std::pair<UnitTypes, int> >::const_iterator di(unitValueDiffs.begin()), diEnd(unitValueDiffs.end()); di != diEnd; ++di)
                                {
                                    civLog << " unit: " << gGlobals.getUnitInfo(di->first).getType() << " %city attack value %inc = " << di->second;
                                }

                                unitValueDiffs.clear();
                                unitValueDiffs = TacticSelectionData::getUnitValueDiffs(mi->cityDefenceUnits, noDepIter->second.cityDefenceUnits);

                                for (std::list<std::pair<UnitTypes, int> >::const_iterator di(unitValueDiffs.begin()), diEnd(unitValueDiffs.end()); di != diEnd; ++di)
                                {
                                    civLog << " unit: " << gGlobals.getUnitInfo(di->first).getType() << " %city defence value %inc = " << di->second;
                                }

                                unitValueDiffs.clear();
                                unitValueDiffs = TacticSelectionData::getUnitValueDiffs(mi->thisCityDefenceUnits, noDepIter->second.thisCityDefenceUnits);

                                for (std::list<std::pair<UnitTypes, int> >::const_iterator di(unitValueDiffs.begin()), diEnd(unitValueDiffs.end()); di != diEnd; ++di)
                                {
                                    civLog << " unit: " << gGlobals.getUnitInfo(di->first).getType() << " %this city defence value %inc = " << di->second;
                                }
                                if (!unitValueDiffs.empty())
                                {
                                    civLog << " for city: " << safeGetCityName(mi->city);
                                }
                            }

                            if (buildingScore != 0)
                            {
                                civLog << " building score = " << buildingScore;
                            }
                            if (bonusScore != 0)
                            {
                                civLog << " bonus score = " << bonusScore;
                            }
                            if (resourceScore != 0)
                            {
                                civLog << " resource score = " << resourceScore;
                            }
                            if (expansionScore != 0)
                            {
                                civLog << " expansion score = " << expansionScore;
                            }
                            
                            if (unitScore != 0)
                            {
                                civLog << " unit score = " << unitScore
                                    << " att % incr: " << asPercentageOf(bestAttackUnitScore, baseAttackUnitScore)
                                    << " def % incr: " << asPercentageOf(bestDefenceUnitScore, baseDefenceUnitScore)
                                    << " coll % incr: " << asPercentageOf(bestCollateralUnitScore, baseCollateralUnitScore);
                            }
                            if (bestAttackUnitScore != 0)
                            {
                                civLog << ", best attack unit score = " << bestAttackUnitScore;
                            }
                            if (bestDefenceUnitScore != 0)
                            {
                                civLog << ", best defence unit score = " << bestDefenceUnitScore;
                            }
                            if (bestCollateralUnitScore != 0)
                            {
                                civLog << ", best collateral unit score = " << bestCollateralUnitScore;
                            }

                            int processScore = 0;

                            for (std::map<ProcessTypes, TotalOutput>::const_iterator pi(ci->second.processOutputsMap.begin()),
                                piEnd(ci->second.processOutputsMap.end()); pi != piEnd; ++pi)
                            {
                                // if process generates research, evaluate the additional research rate as %age of current max research rate
                                // if process generates gold, instead evaluate the additional research possible through ability to 
                                // lower gold rate as %age of current max research rate
                                int maxResearchPercent = player.getMaxResearchPercent();  // as %age

                                const std::vector<std::pair<int, int> >& goldAndResearchOutputsByCommerceRate = player.getGoldAndResearchRates();
                                int currentGoldRate = goldAndResearchOutputsByCommerceRate[(100 - maxResearchPercent) / 10].first,
                                    currentResearchRate = goldAndResearchOutputsByCommerceRate[(100 - maxResearchPercent) / 10].second;

                                const CvProcessInfo& processInfo = gGlobals.getProcessInfo(pi->first);
                                // can process generate gold?
                                if (processInfo.getProductionToCommerceModifier(COMMERCE_GOLD) > 0)
                                {
                                    int processGoldRate = pi->second[OUTPUT_GOLD] / turnsAvailable;

                                    // how much extra gold/turn do we need to move up to 100% research? (0% gold slider)
                                    int requiredAdditionalGoldOutput = currentGoldRate - goldAndResearchOutputsByCommerceRate[0].first;
                                    // what %age of that amount can we generate from this process?
                                    int percentOfProcessGoldAvailableForResearch = asPercentageOf(processGoldRate, requiredAdditionalGoldOutput);

                                    int newMaxPossibleResearchRate = std::min<int>(100, maxResearchPercent + (percentOfProcessGoldAvailableForResearch * (100 - maxResearchPercent)) / 100);
                                    int remainderProcessGold = std::max<int>(0, processGoldRate - requiredAdditionalGoldOutput);

                                    int additionalResearchPerTurn = goldAndResearchOutputsByCommerceRate[(100 - newMaxPossibleResearchRate) / 10].second - currentResearchRate;
                                    int additionalResearchPercent = asPercentageOf(additionalResearchPerTurn, currentResearchRate);

                                    int additionalGoldPercent = asPercentageOf(remainderProcessGold, currentGoldRate);

                                    civLog << " process - gold - requiredAdditionalGoldOutput = " << requiredAdditionalGoldOutput
                                        << " newMaxPossibleResearchRate = " << newMaxPossibleResearchRate
                                        << " additionalResearchPerTurn = " << additionalResearchPerTurn << " additionalGoldPerTurn = " << remainderProcessGold
                                        << " additionalResearchPercent = " << additionalResearchPercent << " additionalGoldPercent = " << additionalGoldPercent;

                                    processScore += (valueF(pi->second) * percentOfProcessGoldAvailableForResearch) / 100;
                                }

                                // can process generate research?
                                if (processInfo.getProductionToCommerceModifier(COMMERCE_RESEARCH) > 0)
                                {
                                    // if our max research rate is low - this increases the value of being able to build research
                                    // calc %age of build research per turn compared to max research - gives some indication of how much of our
                                    // slider driven research could be replaced
                                    int currentResearch = goldAndResearchOutputsByCommerceRate[(100 - maxResearchPercent) / 10].second;
                                    int percentOfResearch = asPercentageOf(pi->second[OUTPUT_RESEARCH] / turnsAvailable, currentResearch);
                                    int weightedValue = percentOfResearch * (100 - maxResearchPercent) / 100;

                                    civLog << " process - research - max research rate: " << maxResearchPercent << " => " << currentResearch 
                                        << " per turn - process rate per turn: " << pi->second[OUTPUT_RESEARCH] / turnsAvailable 
                                        << " rate % incr = " << percentOfResearch;

                                    processScore += (valueF(pi->second) * weightedValue) / 100;
                                }

                                // don't bother with build culture here - todo - add into culture manager
                            }

                            if (processScore != 0)
                            {
                                civLog << " process score = " << processScore;
                            }

                            int thisTechScore = baseTechScore + workerScore + resourceScore + buildingScore + processScore + expansionScore + unitScore + bonusScore;
                            civLog << "\n\t total score for: " << gGlobals.getTechInfo(thisTechType).getType() << " = " << thisTechScore << ", scaled by tech cost: " << thisTechScore / techCostsMap[thisTechType];

                            thisTechScore /= techCostsMap[thisTechType];

                            // scale by no. of techs in this dep set (assumes all deps are techs)
                            // todo - fix this to split items with mulitple tech deps across those techs weighting by research cost
                            // means we have to defer picking best tech and store scores in a map keyed by tech
                            techScoresMap[thisTechType] += thisTechScore / ci->first.size();

                            if (thisTechScore > bestTechScore)
                            {
                                bestTechScore = thisTechScore;
                                bestTech = thisTechType;
                            }
                        }
                    }
                }
            }

            ResearchTech getSelection()
            {
                if (bestTech != NO_TECH && !freeTechTechs.empty())
                {
                    int bestTechCost = techCostsMap[bestTech];                    
                    for (size_t i = 0, count = freeTechTechs.size(); i < count; ++i)
                    {
                        int freeTechCost = techCostsMap[freeTechTechs[i]];
                        // research free tech tech to get best tech + whatever the free tech tech is
                        if (freeTechCost < bestTechCost)
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(getResearchTech) Choosing first-to tech: " << gGlobals.getTechInfo(freeTechTechs[i]).getType()
                                << " over tech: " << gGlobals.getTechInfo(bestTech).getType();
#endif
                            bestTech = freeTechTechs[i];
                            bestTechCost = freeTechCost;
                        }
                    }                    
                }

                if (bestTech != NO_TECH && setResearchTech(bestTech))
                {
                    // was this tech selected with a wonder in mind?
#ifdef ALTAI_DEBUG
                    civLog << "\n(getResearchTech) Returning best tech: " << gGlobals.getTechInfo(bestTech).getType();
#endif
                }

                // choose first tech on path
                if (selection.techType != NO_TECH && chooseOrTech())
                {
                    selection.targetTechType = bestTech;
#ifdef ALTAI_DEBUG
                    civLog << "\nChosen or tech: " << gGlobals.getTechInfo(selection.techType).getType();
#endif
                }

                return selection;
            }

            bool setResearchTech(TechTypes techType)
            {
                selection = ResearchTech(techType);
                return true;
            }

            bool chooseOrTech()
            {
                std::vector<TechTypes> orTechs = getOrTechs(player.getAnalysis()->getTechInfo(selection.techType));
                bool haveOrTech = false;
                int bestTechValue = 0;
                TechTypes bestOrTech = NO_TECH;
                if (orTechs.size() > 1)
                {
                    for (int i = 0, count = orTechs.size(); i < count; ++i)
                    {
                        if (civHelper->hasTech(orTechs[i]))
                        {
                            haveOrTech = true;
                            break;
                        }
                        else
                        {
                            std::map<TechTypes, int>::const_iterator orIter = techScoresMap.find(orTechs[i]);
                            if (orIter != techScoresMap.end())
                            {
                                int thisTechValue = orIter->second;
                                if (thisTechValue > bestTechValue)
                                {
                                    bestOrTech = orTechs[i];
                                }
                            }
                        }
                    }
                }

                if (!haveOrTech)
                {
                    if (bestOrTech != NO_TECH && setResearchTech(bestOrTech))
                    {
                        return true;
                    }
                }
                return false;
            }

            void debug() const
            {
#ifdef ALTAI_DEBUG
                civLog << "\nTech tactics selection data:";
                for (TacticSelectionDataMap::const_iterator ci(tacticSelectionDataMap.begin()), ciEnd(tacticSelectionDataMap.end());
                    ci != ciEnd; ++ci)
                {
                    civLog << "\nDep items: ";
                    for (DependencyItemSet::const_iterator di(ci->first.begin()), diEnd(ci->first.end()); di != diEnd; ++di)
                    {
                        debugDepItem(*di, civLog);
                        civLog << " ";
                    }
                    ci->second.debug(civLog);
                }

                bestTech != NO_TECH ? civLog << "\nbest tech = " << gGlobals.getTechInfo(bestTech).getType() : civLog << "\nNo best tech.";
#endif
            }

            TacticSelectionDataMap tacticSelectionDataMap;

            TotalOutput currentOutputProjection;
            bool possiblyIsolated;
            int warPlanCount, atWarCount;

            struct EconomicBuildsData
            {
                std::map<IDInfo, std::list<const EconomicBuildingValue*> > cityBuildItemsMap;
                std::map<IDInfo, int> usedCityTurns;
            };
            EconomicBuildsData economicBuildsData;

            std::map<TechTypes, int> techScoresMap, techCostsMap;
            std::vector<TechTypes> freeTechTechs;

            const PlayerTactics& playerTactics;
            Player& player;
            boost::shared_ptr<MapAnalysis> pMapAnalysis;
            boost::shared_ptr<CivHelper> civHelper;

            TechTypes bestTech;
            ResearchTech selection;

            std::ostream& civLog;
        };
    }

    ResearchTech getResearchTech(PlayerTactics& playerTactics, TechTypes ignoreTechType)
    {
        std::ostream& os = CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream();

        if (playerTactics.cityBuildingTacticsMap_.empty())
        {
            // can be called before this is called on first turn
            playerTactics.player.initCities();
        }

        TechSelectionData selectionData(playerTactics);

        // update base resource tactics as may need them for connectable resource evaluations
        TacticSelectionData& baseSelectionData = playerTactics.getBaseTacticSelectionData();
        std::set<BonusTypes> ownedAndUnownedResources;
        // combine owned and unowned as we want both here (owned as may not be connected and unowned as we may be about to own them)
        playerTactics.player.getAnalysis()->getMapAnalysis()->getResources(ownedAndUnownedResources, ownedAndUnownedResources);
        for (std::set<BonusTypes>::const_iterator ci(ownedAndUnownedResources.begin()), ciEnd(ownedAndUnownedResources.end()); ci != ciEnd; ++ci)
        {
#ifdef ALTAI_DEBUG
            os << "\n\t recalc'ing base resource value for: " << gGlobals.getBonusInfo(*ci).getType();
#endif
            PlayerTactics::ResourceTacticsMap::const_iterator bonusTacticIter = playerTactics.resourceTacticsMap_.find(*ci);
            if (bonusTacticIter != playerTactics.resourceTacticsMap_.end())
            {
                bonusTacticIter->second->apply(baseSelectionData);
            }
        }

        playerTactics.debugTactics();

        // pure tech tactics ('first to tech' items, open borders, etc...)
        for (PlayerTactics::TechTacticsMap::const_iterator ci = playerTactics.techTacticsMap_.begin(), ciEnd = playerTactics.techTacticsMap_.end();
            ci != ciEnd; ++ci)
        {
            if (ci->second)
            {
                DependencyItemSet di;
                di.insert(DependencyItem(ResearchTechDependency::ID, ci->first));
                ci->second->apply(selectionData.tacticSelectionDataMap[di]);
            }
        }

        // buildings
        for (PlayerTactics::CityBuildingTacticsMap::const_iterator ci = playerTactics.cityBuildingTacticsMap_.begin(), 
            ciEnd = playerTactics.cityBuildingTacticsMap_.end(); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity);

            for (PlayerTactics::CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
            {
                li->second->update(playerTactics.player, city.getCityData());
                li->second->apply(selectionData.tacticSelectionDataMap, IDependentTactic::Ignore_Techs);
            }
        }

        // global buildings
        for (PlayerTactics::LimitedBuildingsTacticsMap::const_iterator iter(playerTactics.globalBuildingsTacticsMap_.begin()),
            endIter(playerTactics.globalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            if (iter->second)
            {
                iter->second->update(playerTactics.player);
                iter->second->apply(selectionData.tacticSelectionDataMap, IDependentTactic::Ignore_Techs);
            }
        }

        // units
        for (PlayerTactics::UnitTacticsMap::const_iterator iter(playerTactics.unitTacticsMap_.begin()),
            endIter(playerTactics.unitTacticsMap_.end()); iter != endIter; ++iter)
        {
            if (iter->second)
            {
                iter->second->update(playerTactics.player);
                iter->second->apply(selectionData.tacticSelectionDataMap, IDependentTactic::Ignore_Techs);

                // no tech deps
                if (iter->second->areTechDependenciesSatisfied(playerTactics.player))
                {
                    DependencyItemSet di;
                    di.insert(DependencyItem(-1, -1));
                    iter->second->apply(selectionData.tacticSelectionDataMap[di]);
                }
            }
        }

        // civics
        for (PlayerTactics::CivicTacticsMap::const_iterator iter(playerTactics.civicTacticsMap_.begin()),
            endIter(playerTactics.civicTacticsMap_.end()); iter != endIter; ++iter)
        {
            // might already be able to run this civic because of other source (i.e. some wonders)
            if (playerTactics.player.getCvPlayer()->canDoCivics(iter->first))
            {
                continue;
            }

            ResearchTechDependencyPtr pTechDependency = iter->second->getTechDependency();
            if (playerTactics.player.getTechResearchDepth(pTechDependency->getResearchTech()) <= 3)
            {
                iter->second->update(playerTactics.player);
                iter->second->apply(selectionData.tacticSelectionDataMap, IDependentTactic::Ignore_Techs);
            }
        }

        // city improvements
        for (PlayerTactics::CityImprovementTacticsMap::const_iterator ci(playerTactics.cityImprovementTacticsMap_.begin()), ciEnd(playerTactics.cityImprovementTacticsMap_.end());
            ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity)
            {
                City& city = playerTactics.player.getCity(pCity);
                TotalOutput base = city.getCurrentOutputProjection().getOutput();

                os << "\ngetResearchTech: city: " << narrow(pCity->getName());

                for (PlayerTactics::CityImprovementTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                {
                    (*li)->update(playerTactics.player, city.getCityData());

                    const std::vector<ResearchTechDependencyPtr> techs = (*li)->getTechDependencies();
                    for (size_t i = 0, count = techs.size(); i < count; ++i)
                    {
                        const std::vector<DependencyItem>& thisDepItems = techs[i]->getDependencyItems();

                        DependencyItemSet di;
                        di.insert(thisDepItems.begin(), thisDepItems.end());

                        selectionData.tacticSelectionDataMap[di].cityImprovementsDelta +=
                            (*li)->getProjection().getOutput() - (*li)->getBaseProjection().getOutput();

                        os << "\ngetResearchTech: adding imp delta: " << (*li)->getProjection().getOutput() - (*li)->getBaseProjection().getOutput()
                           << " %age: " << asPercentageOf((*li)->getProjection().getOutput() - (*li)->getBaseProjection().getOutput(), (*li)->getBaseProjection().getOutput())
                           << " for tech(s): ";
                        for (size_t j = 0, depItemCount = thisDepItems.size(); j < depItemCount; ++j)
                        {
                            os << gGlobals.getTechInfo((TechTypes)thisDepItems[j].second).getType() << " ";
                        }
                    }
                }

                CityImprovementManager improvementManager(IDInfo(playerTactics.player.getPlayerID(), city.getID()), true);
                //std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);
                //const int targetSize = 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel());
                CityDataPtr pCityData(new CityData(pCity, true));
                //improvementManager.calcImprovements(pCityData, yieldTypes, targetSize, 3);
                improvementManager.simulateImprovements(pCityData, 3, __FUNCTION__);
                
                os << "\ngetResearchTech: improvements = ";
                improvementManager.logImprovements(CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream());

                const std::vector<PlotImprovementData>& improvements = improvementManager.getImprovements();

                for (size_t i = 0, count = improvements.size(); i < count; ++i)
                {
                    if (improvements[i].isSelectedAndNotBuilt())
                    {
                        PlotDataListConstIter iter = city.getCityData()->findPlot(improvements[i].coords);
                        PlotYield baseYield = (iter == city.getCityData()->getPlotOutputs().end()) ? PlotYield() : iter->plotYield;

                        ImprovementTypes improvementType = improvements[i].improvement;
                        TechTypes techType = GameDataAnalysis::getTechTypeForBuildType(GameDataAnalysis::getBuildTypeForImprovementType(getBaseImprovement(improvementType)));
						TechTypes featureRemoveTech = improvements[i].removedFeature == NO_FEATURE ? NO_TECH : GameDataAnalysis::getTechTypeToRemoveFeature(improvements[i].removedFeature);
                        os << "\nImprovement: " << gGlobals.getImprovementInfo(improvementType).getType() << " at: " << improvements[i].coords
                           << " tech: " << gGlobals.getTechInfo(techType).getType();
						if (featureRemoveTech != NO_TECH)
						{
							os << " feature remove tech: " << gGlobals.getTechInfo(featureRemoveTech).getType();
						}
						os << " delta = " << improvements[i].yield - baseYield;
                    }
                }
            }
        }

        // processes
        for (PlayerTactics::ProcessTacticsMap::const_iterator ci(playerTactics.processTacticsMap_.begin()), ciEnd(playerTactics.processTacticsMap_.end());
            ci != ciEnd; ++ci)
        {
            if (ci->second->areDependenciesSatisfied(playerTactics.player, IDependentTactic::Ignore_Techs))
            {
                CityIter iter(*playerTactics.player.getCvPlayer());
                TotalOutput processOutput;
                while (CvCity* pCity = iter())
                {
                    ProjectionLadder projection = ci->second->getProjection(pCity->getIDInfo());
                    processOutput += projection.getProcessOutput();
                }

                const std::vector<ResearchTechDependencyPtr>& techs = ci->second->getTechDependencies();
                for (size_t i = 0, count = techs.size(); i < count; ++i)
                {
                    const std::vector<DependencyItem>& thisDepItems = techs[i]->getDependencyItems();
                    DependencyItemSet di;
                    di.insert(thisDepItems.begin(), thisDepItems.end());

                    selectionData.tacticSelectionDataMap[di].processOutputsMap[ci->second->getProcessType()] += processOutput;
                }
            }
        }

        selectionData.processTechs();
        if (ignoreTechType != NO_TECH)
        {
            //selectionData.removeTech(ignoreTechType);
        }
        selectionData.scoreTechs();
        selectionData.debug();

        return selectionData.getSelection();
    }
}
