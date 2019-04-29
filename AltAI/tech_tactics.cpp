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
#include "./trade_route_helper.h"
#include "./tech_info_visitors.h"
#include "./tech_info_streams.h"
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

                            const int approxTurns = 1 + totalTechCost / rate;

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
                int score = 0;

                if (selectionData.getFreeTech)
                {
#ifdef ALTAI_DEBUG
                    civLog << "\nChecking free tech data for tech: " << gGlobals.getTechInfo(techType).getType();
#endif
                    score += selectionData.freeTechValue;
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
                int value = 0;
                for (std::set<UnitTacticValue>::const_iterator ci(selectionData.cityAttackUnits.begin()), ciEnd(selectionData.cityAttackUnits.end()); ci != ciEnd; ++ci)
                {
                    value += ci->unitAnalysisValue;
                }
                for (std::set<UnitTacticValue>::const_iterator ci(selectionData.cityDefenceUnits.begin()), ciEnd(selectionData.cityDefenceUnits.end()); ci != ciEnd; ++ci)
                {
                    value += ci->unitAnalysisValue;
                }
                for (std::set<UnitTacticValue>::const_iterator ci(selectionData.collateralUnits.begin()), ciEnd(selectionData.collateralUnits.end()); ci != ciEnd; ++ci)
                {
                    value += ci->unitAnalysisValue;
                }

                if (CvTeamAI::getTeam(player.getTeamID()).getAtWarCount(true) == 0 &&
                    CvTeamAI::getTeam(player.getTeamID()).getAnyWarPlanCount(true) == 0)
                {
                    value /= 3;
                }

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

                int bestIndex = -1, bestValue = 0;
                for (size_t i = 0, count = selectionData.cultureSources.size(); i < count; ++i)
                {
                    int thisValue = (selectionData.cultureSources[i].globalValue * 20 * 100) / selectionData.cultureSources[i].cityCost;
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
                const DotMapItem::PlotDataSet& plotDataSet = targetDotMap.plotData;
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

            int getConnectableResourceValue(TechTypes techType, const TacticSelectionData& selectionData) const
            {
                if (selectionData.connectableResources.empty())
                {
                    return 0;
                }

                int score = 0;

                std::map<BonusTypes, std::vector<UnitTypes> > unitBonusMap;

#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
                os << "\ngetConnectableResourceValue() for tech: " << gGlobals.getTechInfo(techType).getType();
#endif

                for (PlayerTactics::UnitTacticsMap::const_iterator iter(player.getAnalysis()->getPlayerTactics()->unitTacticsMap_.begin()),
                    endIter(player.getAnalysis()->getPlayerTactics()->unitTacticsMap_.end()); iter != endIter; ++iter)
                {
                    const CvCity* pCapitalCity = player.getCvPlayer()->getCapitalCity();
                    if (pCapitalCity)  // barbs have no capital; probably don't care about them researching road building too much
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

                for (std::map<BonusTypes, std::vector<XYCoords> >::const_iterator ci(selectionData.connectableResources.begin()), 
                    ciEnd(selectionData.connectableResources.end()); ci != ciEnd; ++ci)
                {
                    std::map<BonusTypes, std::vector<UnitTypes> >::const_iterator unitsIter = unitBonusMap.find(ci->first);
                    if (unitsIter != unitBonusMap.end())
                    {
                        for (size_t unitIndex = 0, unitCount = unitsIter->second.size(); unitIndex < unitCount; ++unitIndex)
                        {
                            score += player.getAnalysis()->getUnitAnalysis()->getCurrentUnitValue(unitsIter->second[unitIndex]);
#ifdef ALTAI_DEBUG
                            os << "\n\tadded unit value: " << gGlobals.getUnitInfo(unitsIter->second[unitIndex]).getType() << " = "
                                << player.getAnalysis()->getUnitAnalysis()->getCurrentUnitValue(unitsIter->second[unitIndex]);
#endif                            
                        }
                    }
                }

                return score;
            }

            void calcBaseCityBuildItems(const TacticSelectionData& baseSelectionData, TotalOutput currentOutput)
            {
                const int turnsAvailable = 30;
                TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1));
                const int currentOutputValue = valueF(currentOutput) * turnsAvailable / 100;  // ~1% output threshold

                std::vector<OutputTypes> outputTypes;
                outputTypes.push_back(OUTPUT_FOOD);
                outputTypes.push_back(OUTPUT_PRODUCTION);
                outputTypes.push_back(OUTPUT_RESEARCH);
                outputTypes.push_back(OUTPUT_GOLD);

                for (std::multiset<EconomicBuildingValue>::const_iterator baseIter(baseSelectionData.economicBuildings.begin()), baseEndIter(baseSelectionData.economicBuildings.end()); baseIter != baseEndIter; ++baseIter)
                {
                    if (economicBuildsData.usedCityTurns[baseIter->city] <= turnsAvailable && TacticSelectionData::isSignificantTacticItem(*baseIter, currentOutput, outputTypes))
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

            void compareTacticToBase(const TacticSelectionData& selectionData, TotalOutput currentOutput)
            {
                const int turnsAvailable = 30;
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
                for (std::multiset<EconomicBuildingValue>::const_iterator iter(selectionData.economicBuildings.begin()), endIter(selectionData.economicBuildings.end()); iter != endIter; ++iter)
                {
                    std::map<IDInfo, std::list<const EconomicBuildingValue*> >::iterator currentBuildsIter(cityBuildItemsMapForTech.find(iter->city));
                    if (currentBuildsIter != cityBuildItemsMapForTech.end())
                    {
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
                    else if (TacticSelectionData::isSignificantTacticItem(*iter, currentOutput, outputTypes))
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
            }

            void scoreTechs()
            {
                int bestTechScore = 0;
                TotalOutputValueFunctor valueF(makeOutputW(1, 1, 1, 1, 1, 1)), processValueF(makeOutputW(0, 0, 4, 3, 2, 1));

                TotalOutput currentOutput;
                CityIter cityIter(*player.getCvPlayer());
                while (CvCity* pCity = cityIter())
                {
                    const City& city = player.getCity(pCity);
                    currentOutput += city.getCityData()->getOutput();  // use this output fn as this is what is stored in the ProjectionLadders
                }

                civLog << "\nCiv base output = " << currentOutput;

                DependencyItemSet noDep;
                noDep.insert(DependencyItem(-1, -1));
                TacticSelectionDataMap::const_iterator noDepIter = tacticSelectionDataMap.find(noDep);
                if (noDepIter != tacticSelectionDataMap.end())
                {
                    calcBaseCityBuildItems(noDepIter->second, currentOutput);
                }

                for (TacticSelectionDataMap::const_iterator ci(tacticSelectionDataMap.begin()), ciEnd(tacticSelectionDataMap.end());
                    ci != ciEnd; ++ci)
                {
                    for (DependencyItemSet::const_iterator di(ci->first.begin()), diEnd(ci->first.end()); di != diEnd; ++di)
                    {
                        if (di->first == -1)
                        {
                            // no dep ref data...
                            int bestAttackUnitScore = getBestUnitValue(ci->second.cityAttackUnits);
                            int bestDefenceUnitScore = getBestUnitValue(ci->second.cityDefenceUnits);
                            int bestCollateralUnitScore = getBestUnitValue(ci->second.collateralUnits);

                            civLog << " current best attack unit score = " << bestAttackUnitScore << ", current best defence unit score = " << bestDefenceUnitScore
                                << ", current best collateral unit score = " << bestCollateralUnitScore;
                            //getSignificantTactics(ci->second, currentOutput);
                        }
                        else if (di->first == ResearchTechDependency::ID)
                        {
                            TechTypes thisTechType = (TechTypes)di->second;
                            civLog << "\nTech score for: " << gGlobals.getTechInfo(thisTechType).getType();
                            //getSignificantTactics(ci->second, currentOutput);
                            compareTacticToBase(ci->second, currentOutput);

                            if (ci->second.getFreeTech)
                            {
                                freeTechTechs.push_back((TechTypes)di->second);
                            }
                            int baseTechScore = getBaseTechScore((TechTypes)di->second, ci->second);

                            civLog << " base tech score = " << baseTechScore;

                            int workerScore = valueF(ci->second.cityImprovementsDelta);
                            civLog << " cityImprovementsDelta = " << ci->second.cityImprovementsDelta;

                            int bonusScore = getConnectableResourceValue(thisTechType, ci->second);

                            int resourceScore = valueF(ci->second.resourceOutput);

                            int expansionScore = getExpansionValue(thisTechType, ci->second);

                            int unitScore = getUnitValue(thisTechType, ci->second);

                            int bestAttackUnitScore = getBestUnitValue(ci->second.cityAttackUnits);
                            int bestDefenceUnitScore = getBestUnitValue(ci->second.cityDefenceUnits);
                            int bestCollateralUnitScore = getBestUnitValue(ci->second.collateralUnits);

                            int buildingScore = 0;

                            for (std::multiset<EconomicBuildingValue>::const_iterator vi(ci->second.economicBuildings.begin()),
                                viEnd(ci->second.economicBuildings.end()); vi != viEnd; ++vi)
                            {
                                buildingScore += valueF(vi->output);
                            }

                            civLog << " building score = " << buildingScore << " worker score = " << workerScore << " bonus score = " << bonusScore
                                << " resource score = " << resourceScore << " expansion score = " << expansionScore << " unit score = " << unitScore
                                << ", best attack unit score = " << bestAttackUnitScore << ", best defence unit score = " << bestDefenceUnitScore
                                << ", best collateral unit score = " << bestCollateralUnitScore;

                            int processScore = 0;

                            for (std::map<ProcessTypes, TotalOutput>::const_iterator pi(ci->second.processOutputsMap.begin()),
                                piEnd(ci->second.processOutputsMap.end()); pi != piEnd; ++pi)
                            {
                                // todo - come up with better scaling logic here
                                processScore += processValueF(pi->second) / player.getCvPlayer()->getNumCities();
                            }

                            civLog << " process score = " << processScore;

                            int thisTechScore = baseTechScore + workerScore + resourceScore + buildingScore + processScore + expansionScore + unitScore;
                            thisTechScore /= techCostsMap[thisTechType];

                            // scale by no. of techs in this dep set (assumes all deps are techs)
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

            struct EconomicBuildsData
            {
                std::map<IDInfo, std::list<const EconomicBuildingValue*> > cityBuildItemsMap;
                std::map<IDInfo, int> usedCityTurns;
            };
            EconomicBuildsData economicBuildsData;

            std::map<TechTypes, int> techScoresMap, techCostsMap;
            std::vector<TechTypes> freeTechTechs;

            const PlayerTactics& playerTactics;
            const Player& player;
            boost::shared_ptr<MapAnalysis> pMapAnalysis;
            boost::shared_ptr<CivHelper> civHelper;

            TechTypes bestTech;
            ResearchTech selection;

            std::ostream& civLog;
        };
    }

    ResearchTech getResearchTech(const PlayerTactics& playerTactics, TechTypes ignoreTechType)
    {
        std::ostream& os = CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream();

        if (playerTactics.cityBuildingTacticsMap_.empty())
        {
            // can be called before this is called on first turn
            playerTactics.player.initCities();
        }

        TechSelectionData selectionData(playerTactics);

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
            const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity);

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
                const City& city = playerTactics.player.getCity(pCity);
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
