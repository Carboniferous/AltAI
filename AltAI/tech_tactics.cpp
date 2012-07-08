#include "./tech_tactics.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./tech_tactics_visitors.h"
#include "./building_tactics_visitors.h"
#include "./building_info_visitors.h"
#include "./unit_info_visitors.h"
#include "./resource_info_visitors.h"
#include "./trade_route_helper.h"
#include "./tech_info_visitors.h"
#include "./tech_info_streams.h"
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
        void addResearchTech(ResearchList& techTactics, const ResearchTech& researchTech)
        {
            ResearchListIter iter = std::find_if(techTactics.begin(), techTactics.end(), ResearchTechFinder(researchTech));
            if (iter != techTactics.end())
            {
                iter->merge(researchTech);
            }
            else
            {
                techTactics.push_back(researchTech);
            }
        }

        struct TechSelectionHelper
        {
            TechSelectionHelper() : isReligiousTech(false), featureProduction(0), workerScore(0), religionScore(0), bonusScore(0), processScore(0),
                buildingScore(0), unitScore(0), totalScore(0), techValue(0)
            {
            }
            
            explicit TechSelectionHelper(ResearchTech researchTech_) : researchTech(researchTech_), isReligiousTech(false), featureProduction(0),
                workerScore(0), religionScore(0), bonusScore(0), processScore(0), buildingScore(0), unitScore(0), civicScore(0), freeTechScore(0), 
                totalScore(0), techValue(0)
            {
            }

            ResearchTech researchTech;

            typedef std::map<ImprovementTypes, std::pair<TotalOutput, int> > ValuesImprovementsMap;
            ValuesImprovementsMap newValuedImprovements;

            typedef std::map<ImprovementTypes, int> UnvaluedImprovementsMap;
            UnvaluedImprovementsMap newUnvaluedImprovements;

			typedef std::map<IDInfo, std::pair<TotalOutput, int> > CityImprovementsBaseOutputsMap;
			CityImprovementsBaseOutputsMap cityImprovementsBaseOutputsMap;

            int featureProduction;

            bool isReligiousTech;

            typedef std::map<BuildingTypes, std::set<IDInfo> > CityBuildingsMap;
            CityBuildingsMap cityBuildings;

            typedef std::map<UnitTypes, int> UnitValuesMap;
            UnitValuesMap unitValuesMap;

            std::map<ProcessTypes, Commerce> possibleProcessOutputs;

            std::map<BuildingTypes, TotalOutput> possibleBuildingOutputs;

            std::map<CivicTypes, TotalOutput> possibleCivicOutputs;

            int workerScore, religionScore, bonusScore, processScore, buildingScore, unitScore, civicScore, freeTechScore, totalScore, techValue;

            int getImprovementsTotalValue() const
            {
                int totalValue = 0;
                for (ValuesImprovementsMap::const_iterator ci(newValuedImprovements.begin()), ciEnd(newValuedImprovements.end()); ci != ciEnd; ++ci)
                {
                    totalValue += ci->second.second;
                }
                return totalValue;
            }

            int getUnitsScore() const
            {
                int unitScore = 0;
                for (UnitValuesMap::const_iterator ci(unitValuesMap.begin()), ciEnd(unitValuesMap.end()); ci != ciEnd; ++ci)
                {
                    unitScore += ci->second;
                }
                return unitScore /= 2;
            }

            TotalOutputValueFunctor makeBuildingValueF() const
            {
                TotalOutputValueFunctor valueF(makeOutputW(0, 0, 0, 0, 0, 0));

                if (researchTech.economicFlags & EconomicFlags::Output_Food)
                {
                    valueF.outputWeights[OUTPUT_FOOD] += 4;
                }
                if (researchTech.economicFlags & EconomicFlags::Output_Production)
                {
                    valueF.outputWeights[OUTPUT_FOOD] += 2;
                }
                if (researchTech.economicFlags & EconomicFlags::Output_Commerce)
                {
                    valueF.outputWeights[OUTPUT_GOLD] += 2;
                    valueF.outputWeights[OUTPUT_RESEARCH] += 2;
                    valueF.outputWeights[OUTPUT_CULTURE] += 1;
                    valueF.outputWeights[OUTPUT_ESPIONAGE] += 1;
                }
                if (researchTech.economicFlags & (EconomicFlags::Output_Gold | EconomicFlags::Output_Maintenance_Reduction))
                {
                    valueF.outputWeights[OUTPUT_GOLD] += 2;
                }
                if (researchTech.economicFlags & EconomicFlags::Output_Research)
                {
                    valueF.outputWeights[OUTPUT_RESEARCH] += 2;
                }
                if (researchTech.economicFlags & EconomicFlags::Output_Culture)
                {
                    valueF.outputWeights[OUTPUT_CULTURE] += 2;
                }
                if (researchTech.economicFlags & EconomicFlags::Output_Espionage)
                {
                    valueF.outputWeights[OUTPUT_ESPIONAGE] += 2;
                }
                if (researchTech.economicFlags & (EconomicFlags::Output_Happy | EconomicFlags::Output_Health))
                {
                    valueF.outputWeights[OUTPUT_FOOD] += 3;
                    valueF.outputWeights[OUTPUT_PRODUCTION] += 2;
                    valueF.outputWeights[OUTPUT_GOLD] += 1;
                    valueF.outputWeights[OUTPUT_RESEARCH] += 1;
                }

                return valueF;
            }

            void debug(std::ostream& os) const
            {
#ifdef ALTAI_DEBUG
                os << "\nTech Selection Data: " << researchTech;

                for (std::map<ImprovementTypes, std::pair<TotalOutput, int> >::const_iterator ci(newValuedImprovements.begin()), ciEnd(newValuedImprovements.end()); ci != ciEnd; ++ci)
                {
                    if (ci != newValuedImprovements.begin()) os << ", ";
                    else os << "\n";

                    os << gGlobals.getImprovementInfo(ci->first).getType() << ", output = " << ci->second.first << ", total value = " << ci->second.second;
                }
                
                for (std::map<ImprovementTypes, int>::const_iterator ci(newUnvaluedImprovements.begin()), ciEnd(newUnvaluedImprovements.end()); ci != ciEnd; ++ci)
                {
                    if (ci != newUnvaluedImprovements.begin()) os << ", ";
                    else os << "\n";

                    os << gGlobals.getImprovementInfo(ci->first).getType() << ", count = " << ci->second;
                }

				for (CityImprovementsBaseOutputsMap::const_iterator ci(cityImprovementsBaseOutputsMap.begin()), ciEnd(cityImprovementsBaseOutputsMap.end()); ci != ciEnd; ++ci)
				{
					os << "\nBase output for city: " << narrow(getCity(ci->first)->getName()) << " = " << ci->second.first << " value = " << ci->second.second;
				}

                if (isReligiousTech)
                {
                    os << "\nis religious tech ";
                }

                for (CityBuildingsMap::const_iterator ci(cityBuildings.begin()), ciEnd(cityBuildings.end()); ci != ciEnd; ++ci)
                {
                    os << "\n" << gGlobals.getBuildingInfo(ci->first).getType() << " count = " << ci->second.size();
                }

                for (std::map<ProcessTypes, Commerce>::const_iterator ci(possibleProcessOutputs.begin()), ciEnd(possibleProcessOutputs.end()); ci != ciEnd; ++ci)
                {
                    os << "\n" << gGlobals.getProcessInfo(ci->first).getType() << " possible output = " << ci->second;
                }

                for (std::map<BuildingTypes, TotalOutput>::const_iterator ci(possibleBuildingOutputs.begin()), ciEnd(possibleBuildingOutputs.end()); ci != ciEnd; ++ci)
                {
                    os << "\n" << gGlobals.getBuildingInfo(ci->first).getType() << " projected output = " << ci->second;
                }

                for (std::map<CivicTypes, TotalOutput>::const_iterator ci(possibleCivicOutputs.begin()), ciEnd(possibleCivicOutputs.end()); ci != ciEnd; ++ci)
                {
                    os << "\n" << gGlobals.getCivicInfo(ci->first).getType() << " projected output = " << ci->second;
                }

                for (UnitValuesMap::const_iterator ci(unitValuesMap.begin()), ciEnd(unitValuesMap.end()); ci != ciEnd; ++ci)
                {
                    os << "\n" << gGlobals.getUnitInfo(ci->first).getType() << " value = " << ci->second;
                }

                os << "\nFeature production = " << featureProduction;
                os << "\nScores: worker = " << workerScore << ", religion = " << religionScore << ", bonus = " << bonusScore << ", process = " << processScore
                   << ", buildings = " << buildingScore << ", units = " << unitScore << ", civics = " << civicScore
                   << ", tech score = " << totalScore << ", value = " << techValue;
                os << "\n";
#endif
            }
        };

        struct TechSelectionData
        {
            explicit TechSelectionData(const PlayerTactics& playerTactics_)
                : playerTactics(playerTactics_), player(playerTactics_.player), 
                  bestTech(NO_TECH), selection(NO_TECH), civLog(CivLog::getLog(*player.getCvPlayer())->getStream())
            {
                pMapAnalysis = player.getAnalysis()->getMapAnalysis();
                civHelper = player.getCivHelper();
            }

            void removeTech(TechTypes ignoreTechType)
            {
                techSelectionData.erase(ignoreTechType);
            }

            void processFreeTechTech(TechSelectionHelper& techSelectionHelper)
            {
                const std::list<ResearchTech>& researchTechs = playerTactics.selectedTechTactics_;
                const CvTeamAI& team = CvTeamAI::getTeam(player.getTeamID());
                int maxCost = 0;

                for (std::list<ResearchTech>::const_iterator ci(researchTechs.begin()), ciEnd(researchTechs.end()); ci != ciEnd; ++ci)
                {
                    if (ci->depth == 1)
                    {
                        int cost = team.getResearchLeft(ci->techType);
                        if (cost > maxCost)
                        {
                            maxCost = cost;
                        }
                    }
                }

                techSelectionHelper.freeTechScore = maxCost;
            }

            void processWorkerTech(TechSelectionHelper& techSelectionHelper)
            {
#ifdef ALTAI_DEBUG
                civLog << "\n(processWorkerTech) processing tech: " << techSelectionHelper.researchTech;
#endif
                CityIter cityIter(*player.getCvPlayer());
                CvCity* pCity;

                while (pCity = cityIter())
                {
                    if (techSelectionHelper.researchTech.hasNewImprovements(pCity->getIDInfo()))
                    {
                        CityImprovementManager improvementManager(pCity->getIDInfo(), true);
                        const City& city = player.getCity(pCity->getID());
                        TotalOutputWeights outputWeights = city.getPlotAssignmentSettings().outputWeights;

                        improvementManager.simulateImprovements(outputWeights, __FUNCTION__);
                    
                        std::vector<CityImprovementManager::PlotImprovementData> baseImprovements = improvementManager.getImprovements();

                        std::list<TechTypes> pushedTechs = pushTechAndPrereqs(techSelectionHelper.researchTech.techType, player);
                        for (std::list<TechTypes>::const_iterator ci(pushedTechs.begin()), ciEnd(pushedTechs.end()); ci != ciEnd; ++ci)
                        {
                            civHelper->addTech(*ci);
                        }

                        civHelper->addTech(techSelectionHelper.researchTech.techType);
                        TotalOutput baseCityOutput = improvementManager.simulateImprovements(outputWeights, __FUNCTION__);
					    techSelectionHelper.cityImprovementsBaseOutputsMap[pCity->getIDInfo()] = std::make_pair(baseCityOutput, TotalOutputValueFunctor(outputWeights)(baseCityOutput));

                        std::vector<CityImprovementManager::PlotImprovementData> newImprovements = improvementManager.getImprovements();
                        std::vector<CityImprovementManager::PlotImprovementData> delta = findNewImprovements(baseImprovements, newImprovements);

                        for (size_t index = 0, count = delta.size(); index < count; ++index)
                        {
#ifdef ALTAI_DEBUG
                            civLog << "\n(processWorkerTech)delta imp: " << index << ": ";
                            improvementManager.logImprovement(civLog, delta[index]);
#endif

                            // selected and has TotalOutput value - indicating simulation was positive
                            if (boost::get<5>(delta[index]) == CityImprovementManager::Not_Built)
                            {
                                if (!isEmpty(boost::get<4>(delta[index])))
                                {
                                    // add its TotalOutput to total for this improvement type
                                    techSelectionHelper.newValuedImprovements[boost::get<2>(delta[index])].first += boost::get<4>(delta[index]);
                                    techSelectionHelper.newValuedImprovements[boost::get<2>(delta[index])].second += TotalOutputValueFunctor(outputWeights)(boost::get<4>(delta[index]));
                                }
                                else
                                {
                                    // count improvement
                                    ++techSelectionHelper.newUnvaluedImprovements[boost::get<2>(delta[index])];
                                }
                            }
                            else if (boost::get<5>(delta[index]) == CityImprovementManager::Not_Selected)
                            {
                                // count improvement
                                ++techSelectionHelper.newUnvaluedImprovements[boost::get<2>(delta[index])];
                            }
                        }

                        for (std::list<TechTypes>::const_iterator ci(pushedTechs.begin()), ciEnd(pushedTechs.end()); ci != ciEnd; ++ci)
                        {
                            civHelper->removeTech(*ci);
                        }
                    }

                    ResearchTech::WorkerTechDataMap::const_iterator workerIter = techSelectionHelper.researchTech.workerTechDataMap.find(pCity->getIDInfo());

                    if (workerIter != techSelectionHelper.researchTech.workerTechDataMap.end() && !workerIter->second->removableFeatureCounts.empty())
                    {
                        int featureProduction = 0;
                        for (std::map<FeatureTypes, int>::const_iterator featureCountIter(workerIter->second->removableFeatureCounts.begin()),
                            featureEndIter(workerIter->second->removableFeatureCounts.end()); featureCountIter != featureEndIter; ++featureCountIter)
                        {
                            BuildTypes buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureCountIter->first);
                            const CvBuildInfo& buildInfo = gGlobals.getBuildInfo(buildType);
                            int thisFeatureProduction = buildInfo.getFeatureProduction(featureCountIter->first) * featureCountIter->second;
                            thisFeatureProduction *= 100 + player.getCvPlayer()->getFeatureProductionModifier();
                            thisFeatureProduction /= 100;
                            featureProduction += thisFeatureProduction;
                        }

                        techSelectionHelper.featureProduction += featureProduction;
                    }
                }
            }

            void processReligiousTech(TechSelectionHelper& techSelectionHelper)
            {
                techSelectionHelper.isReligiousTech = true;
            }

            void processBuildings(TechSelectionHelper& techSelectionHelper)
            {
                for (TechSelectionHelper::CityBuildingsMap::const_iterator buildingsIter(techSelectionHelper.cityBuildings.begin()), buildingsEndIter(techSelectionHelper.cityBuildings.end());
                     buildingsIter != buildingsEndIter; ++buildingsIter)
                {
                    boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(buildingsIter->first);
#ifdef ALTAI_DEBUG
                    civLog << "\n(TechSelectionData): processing building: " << gGlobals.getBuildingInfo(buildingsIter->first).getType();
#endif
                    for (std::set<IDInfo>::const_iterator cityIter(buildingsIter->second.begin()), cityEndIter(buildingsIter->second.end()); cityIter != cityEndIter; ++cityIter)
                    {
                        const CvCity* pCity = getCity(*cityIter);
                        if (!pCity)
                        {
#ifdef ALTAI_DEBUG
                            civLog << " for city? " << cityIter->eOwner << ", " << cityIter->iID;
#endif
                            continue;
                        }
#ifdef ALTAI_DEBUG
                        civLog << " for city: " << narrow(getCity(*cityIter)->getName());
                        streamEconomicFlags(civLog, techSelectionHelper.researchTech.economicFlags);
#endif
                        TotalOutput base = player.getCity(cityIter->iID).getCurrentOutputProjection().getOutput();
                        
                        CityDataPtr pCityData = player.getCity(cityIter->iID).getCityData()->clone();
                        std::vector<IProjectionEventPtr> events;
                        pCityData->pushBuilding(buildingsIter->first);
                        events.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pCityData->getCity(), player.getAnalysis()->getBuildingInfo(buildingsIter->first))));
                        events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));

                        ProjectionLadder buildingLadder = getProjectedOutput(player, pCityData, 50, events);

                        //TotalOutput projectedOutput = getProjectedEconomicImpact(player, player.getCity(cityIter->iID), pBuildingInfo, techSelectionHelper.researchTech.economicFlags);

                        techSelectionHelper.possibleBuildingOutputs[buildingsIter->first] += buildingLadder.getOutput() - base;
                    }
                }
            }

            void processUnits(TechSelectionHelper& techSelectionHelper)
            {
                for (size_t i = 0, count = techSelectionHelper.researchTech.possibleUnits.size(); i < count; ++i)
                {
                    int unitValue = player.getAnalysis()->getUnitAnalysis()->getCurrentUnitValue(techSelectionHelper.researchTech.possibleUnits[i]);
                    techSelectionHelper.unitValuesMap[techSelectionHelper.researchTech.possibleUnits[i]] = unitValue;
                }
            }

            void processProcesses(TechSelectionHelper& techSelectionHelper)
            {
#ifdef ALTAI_DEBUG
                civLog << "\nChecking processes for tech: " << gGlobals.getTechInfo(techSelectionHelper.researchTech.techType).getType();
#endif
                CityIter iter(*player.getCvPlayer());
                while (CvCity* pCity = iter())
                {
                    const City& city = player.getCity(pCity->getID());
                    TotalOutput maxOutput = city.getMaxOutputs();
                    for (size_t i = 0, count = techSelectionHelper.researchTech.possibleProcesses.size(); i < count; ++i)
                    {
                        const CvProcessInfo& processInfo = gGlobals.getProcessInfo(techSelectionHelper.researchTech.possibleProcesses[i]);

                        CommerceModifier modifier;
                        Commerce processCommerce; 
                        for (int j = 0; j < NUM_COMMERCE_TYPES; ++j)
                        {
                            modifier[j] = processInfo.getProductionToCommerceModifier(j);
                            processCommerce[j] = modifier[j] * maxOutput[OUTPUT_PRODUCTION] / 100;
                        }
#ifdef ALTAI_DEBUG
                        civLog << "\nProcessing process: " << processInfo.getType() << " for city: " << narrow(pCity->getName()) << " = " << processCommerce;
#endif                        
                        techSelectionHelper.possibleProcessOutputs[techSelectionHelper.researchTech.possibleProcesses[i]] += processCommerce;
                    }
                }
            }

            void processCivics(TechSelectionHelper& techSelectionHelper)
            {
            }

            void addBuildingTechs()
            {
                const std::map<IDInfo, ConstructList >& cityBuildingsTactics = playerTactics.selectedCityBuildingTactics_;

                for (std::map<IDInfo, ConstructList >::const_iterator ci(cityBuildingsTactics.begin()), ciEnd(cityBuildingsTactics.end()); ci != ciEnd; ++ci)
                {
                    for (ConstructListConstIter itemIter(ci->second.begin()), itemEndIter(ci->second.end()); itemIter != itemEndIter; ++itemIter)
                    {
                        if (!itemIter->requiredTechs.empty() && itemIter->buildingType != NO_BUILDING)
                        {
                            for (size_t i = 0, count = itemIter->requiredTechs.size(); i < count; ++i)
                            {
                                std::map<TechTypes, TechSelectionHelper>::iterator techIter = techSelectionData.find(itemIter->requiredTechs[i]);
                                // didn't already have this tech as possible choice - so add it
                                if (techIter == techSelectionData.end())
                                {
                                    ResearchTech researchTech(itemIter->requiredTechs[i], player.getTechResearchDepth(itemIter->requiredTechs[i]));
                                    techIter = techSelectionData.insert(std::make_pair(itemIter->requiredTechs[i], TechSelectionHelper(researchTech))).first;
                                }
                                
                                techIter->second.researchTech.possibleBuildings.push_back(itemIter->buildingType);
                                techIter->second.researchTech.economicFlags |= itemIter->economicFlags;
                                techIter->second.researchTech.militaryFlags |= itemIter->militaryFlags;

                                techIter->second.cityBuildings[itemIter->buildingType].insert(ci->first);
                            }
                        }
                    }
                }
            }

            void addUnitTechs()
            {
                const ConstructList& unitTactics = playerTactics.selectedUnitTactics_;

                for (ConstructListConstIter itemIter(unitTactics.begin()), itemEndIter(unitTactics.end()); itemIter != itemEndIter; ++itemIter)
                {
                    if (!itemIter->requiredTechs.empty() && itemIter->unitType != NO_UNIT) // && couldConstructUnit(player, 2, player.getAnalysis()->getUnitInfo(itemIter->unitType), false)
                    {
                        for (size_t i = 0, count = itemIter->requiredTechs.size(); i < count; ++i)
                        {
                            std::map<TechTypes, TechSelectionHelper>::iterator techIter = techSelectionData.find(itemIter->requiredTechs[i]);
                            // didn't already have this tech as possible choice - so add it
                            if (techIter == techSelectionData.end())
                            {
                                ResearchTech researchTech(itemIter->requiredTechs[i], player.getTechResearchDepth(itemIter->requiredTechs[i]));
                                techIter = techSelectionData.insert(std::make_pair(itemIter->requiredTechs[i], TechSelectionHelper(researchTech))).first;
                            }
                            
                            techIter->second.researchTech.possibleUnits.push_back(itemIter->unitType);
                            techIter->second.researchTech.economicFlags |= itemIter->economicFlags;
                            techIter->second.researchTech.militaryFlags |= itemIter->militaryFlags;
#ifdef ALTAI_DEBUG
                            civLog << "\n(addUnitTechs): added unit: " << *itemIter;
#endif
                        }
                    }
                }
            }

            void sanityCheckTechs()
            {
                const CvTeam& team = CvTeamAI::getTeam(player.getTeamID());
                int leastTurns = MAX_INT;
                std::map<TechTypes, int> possiblyTooExpensiveTechs;

                std::map<TechTypes, TechSelectionHelper>::iterator iter(techSelectionData.begin()), iterEnd(techSelectionData.end());
                while (iter != iterEnd)
                {
                    int totalTechCost = std::max<int>(1, player.getCvPlayer()->findPathLength(iter->second.researchTech.techType));

                    const int rate = player.getCvPlayer()->calculateResearchRate(iter->second.researchTech.techType);

                    const int approxTurns = 1 + totalTechCost / rate;

                    if (approxTurns < leastTurns)
                    {
                        leastTurns = approxTurns;
                    }

                    if (approxTurns > (4 * gGlobals.getGame().getMaxTurns() / 100))
                    {
                        possiblyTooExpensiveTechs.insert(std::make_pair(iter->first, approxTurns));
#ifdef ALTAI_DEBUG
                        civLog << "\nMarking tech as expensive: " << gGlobals.getTechInfo(iter->second.researchTech.techType).getType() << " with cost = " << totalTechCost << " approx turns = " << approxTurns
                            << " rate = " << rate;
#endif
                    }
                    else
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\nKeeping tech: " << gGlobals.getTechInfo(iter->second.researchTech.techType).getType() << " with cost = " << totalTechCost << " approx turns = " << approxTurns
                            << " rate = " << rate;
#endif
                    }
                    ++iter;
                }

                for (std::map<TechTypes, int>::const_iterator ci(possiblyTooExpensiveTechs.begin()), ciEnd(possiblyTooExpensiveTechs.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second > 3 * leastTurns)
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\nErasing tech selection: " << gGlobals.getTechInfo(ci->first).getType();
#endif
                        techSelectionData.erase(ci->first);
                    }
                }
            }

            void processTechs()
            {
                addBuildingTechs();
                addUnitTechs();

                const std::list<ResearchTech>& researchTechs = playerTactics.selectedTechTactics_;

                for (std::list<ResearchTech>::const_iterator ci(researchTechs.begin()), ciEnd(researchTechs.end()); ci != ciEnd; ++ci)
                {
                    TechSelectionHelper techSelectionHelper(*ci);

                    if (ci->techFlags & WorkerFlags::Better_Improvements || !ci->possibleImprovements.empty() || !ci->possibleBonuses.empty() || !ci->possibleRemovableFeatures.empty())
                    {
                        processWorkerTech(techSelectionHelper);    
                    }

                    if (ci->techFlags & TechFlags::Found_Religion)
                    {
                        processReligiousTech(techSelectionHelper);
                    }

                    if (ci->techFlags & TechFlags::Free_Tech)
                    {
                        processFreeTechTech(techSelectionHelper);
                    }

                    if (!ci->possibleCivics.empty())
                    {
                        processCivics(techSelectionHelper);
                    }

                    techSelectionData.insert(std::make_pair(ci->techType, techSelectionHelper));
                }

                sanityCheckTechs();

                for (std::map<TechTypes, TechSelectionHelper>::iterator iter(techSelectionData.begin()), iterEnd(techSelectionData.end()); iter != iterEnd; ++iter)
                {
                    processBuildings(iter->second);
                    processUnits(iter->second);
                    processProcesses(iter->second);
                }
            }

            void scoreTechs()
            {
                TotalOutput totalCityOutput;

                CityIter iter(*player.getCvPlayer());

                while (CvCity* pCity = iter())
                {
                    const City& city = player.getCity(pCity->getID());
                    totalCityOutput += city.getCityData()->getActualOutput();
                }

                // TODO - drive this more situationally
                TotalOutputValueFunctor valueF(makeOutputW(4, 3, 2, 2, 1, 1));
#ifdef ALTAI_DEBUG
                civLog << "\nTotal city output = " << totalCityOutput << " value = " << valueF(totalCityOutput);
#endif
                int cityOutputValue = valueF(totalCityOutput);
                cityOutputValue = std::max<int>(1, cityOutputValue);

                int bestTechValue = 0;

                const CvTeamAI& team = CvTeamAI::getTeam(player.getTeamID());

                for (std::map<TechTypes, TechSelectionHelper>::iterator iter(techSelectionData.begin()), iterEnd(techSelectionData.end()); iter != iterEnd; ++iter)
                {
                    int thisTechCost = team.getResearchLeft(iter->second.researchTech.techType);
                    int totalTechCost = std::max<int>(1, player.getCvPlayer()->findPathLength(iter->second.researchTech.techType));

                    for (TechSelectionHelper::ValuesImprovementsMap::const_iterator impIter(iter->second.newValuedImprovements.begin()), impEndIter(iter->second.newValuedImprovements.end());
                         impIter != impEndIter; ++impIter)
                    {
                        iter->second.workerScore += valueF(impIter->second.first);
                    }
                    iter->second.workerScore += valueF.outputWeights[OUTPUT_PRODUCTION] * iter->second.featureProduction;

                    iter->second.workerScore = (50 * iter->second.workerScore) / cityOutputValue;
                   
                    if (iter->second.isReligiousTech)
                    {
                        // TODO - expand on this
                        ++iter->second.religionScore;
                    }

                    int thisTechImprovementValue = 0;
                    int thisTechBonusCount = iter->second.researchTech.possibleBonuses.size();

                    for (size_t i = 0, count = iter->second.researchTech.possibleBonuses.size(); i < count; ++i)
                    {
                        boost::shared_ptr<ResourceInfo> pResourceInfo = player.getAnalysis()->getResourceInfo(iter->second.researchTech.possibleBonuses[i]);
                        std::pair<int, int> militaryInfo = getResourceMilitaryUnitCount(pResourceInfo);
                        ResourceHappyInfo happyInfo = getResourceHappyInfo(pResourceInfo);
                        ResourceHealthInfo healthInfo = getResourceHealthInfo(pResourceInfo);
#ifdef ALTAI_DEBUG
                        //civLog << " units info = " << militaryInfo.first << ", " << militaryInfo.second;
                        //civLog << " actual happy = " << happyInfo.actualHappy << " potential = " << happyInfo.potentialHappy;
                        //civLog << " actual health = " << healthInfo.actualHealth << " potential = " << healthInfo.potentialHealth;
#endif
                        iter->second.bonusScore += 2 * militaryInfo.first + militaryInfo.second + 2 * happyInfo.actualHappy + happyInfo.potentialHappy + healthInfo.actualHealth;
                    }

                    Commerce thisProcessCommerce;
                    for (std::map<ProcessTypes, Commerce>::const_iterator processIter(iter->second.possibleProcessOutputs.begin()), processEndIter(iter->second.possibleProcessOutputs.end());
                         processIter != processEndIter; ++processIter)
                    {
                        thisProcessCommerce += processIter->second;
                    }

                    TotalOutput processOutput(makeOutput(0, 0, thisProcessCommerce[COMMERCE_GOLD], thisProcessCommerce[COMMERCE_RESEARCH], thisProcessCommerce[COMMERCE_CULTURE], thisProcessCommerce[COMMERCE_ESPIONAGE]));
                    
                    iter->second.processScore = 100 * valueF(processOutput) / cityOutputValue;

                    TotalOutput buildingOutputs;
                    for (std::map<BuildingTypes, TotalOutput>::const_iterator buildingsIter(iter->second.possibleBuildingOutputs.begin()), buildingsEndIter(iter->second.possibleBuildingOutputs.end());
                         buildingsIter != buildingsEndIter; ++buildingsIter)
                    {
                        buildingOutputs += buildingsIter->second;
                    }

                    TotalOutputValueFunctor valueF(iter->second.makeBuildingValueF());
                    
                    iter->second.buildingScore = 50 * valueF(buildingOutputs) / cityOutputValue;

                    // TODO - how much better for each Military flag is this unit than the current one (if any), how many cities would might want to build it?
                    // Do we have any resource required?
                    iter->second.unitScore = iter->second.getUnitsScore();

                    iter->second.totalScore = iter->second.workerScore + iter->second.religionScore + iter->second.bonusScore + iter->second.freeTechScore +
                        iter->second.processScore + iter->second.buildingScore + iter->second.unitScore;

                    iter->second.techValue = 100 * iter->second.totalScore / totalTechCost;
                    if (iter->second.techValue > bestTechValue)
                    {
                        bestTechValue = iter->second.techValue;
                        bestTech = iter->second.researchTech.techType;
                    }
#ifdef ALTAI_DEBUG
                    civLog << "\nTech = " << gGlobals.getTechInfo(iter->second.researchTech.techType).getType() << " cost = " << thisTechCost << ", total cost = " << totalTechCost;
                    civLog << " bonus count = " << thisTechBonusCount;
                    civLog << " process output = " << processOutput << " value = " << valueF(processOutput) << " % = " << 100 * valueF(processOutput) / cityOutputValue;
                    civLog << " building output = " << buildingOutputs << " value = " << valueF(buildingOutputs) << ", weights = " << valueF.outputWeights;
                    civLog << "\nScores: worker = " << iter->second.workerScore << ", religion = " << iter->second.religionScore
                           << ", bonus = " << iter->second.bonusScore << ", process = " << iter->second.processScore
                           << ", buildings = " << iter->second.buildingScore << ", units = " << iter->second.unitScore
                           << ", free techs = " << iter->second.freeTechScore
                           << ", tech score = " << iter->second.totalScore << ", value = " << iter->second.techValue;
#endif
                }
            }

            ResearchTech getSelection()
            {
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
                std::map<TechTypes, TechSelectionHelper>::const_iterator ci = techSelectionData.find(techType);
                if (ci != techSelectionData.end())
                {
                    selection = ci->second.researchTech;
                    return true;
                }
                return false;
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
                            std::map<TechTypes, TechSelectionHelper>::const_iterator orIter = techSelectionData.find(orTechs[i]);
                            if (orIter != techSelectionData.end())
                            {
                                int thisTechValue = orIter->second.techValue;
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
                for (std::map<TechTypes, TechSelectionHelper>::const_iterator ci(techSelectionData.begin()), ciEnd(techSelectionData.end()); ci != ciEnd; ++ci)
                {
                    ci->second.debug(civLog);
                }
#ifdef ALTAI_DEBUG
                bestTech != NO_TECH ? civLog << "\nbest tech = " << gGlobals.getTechInfo(bestTech).getType() : civLog << "\nNo best tech.";
#endif
            }

            std::map<TechTypes, TechSelectionHelper> techSelectionData;

            const PlayerTactics& playerTactics;
            const Player& player;
            boost::shared_ptr<MapAnalysis> pMapAnalysis;
            boost::shared_ptr<CivHelper> civHelper;

            TechTypes bestTech;
            ResearchTech selection;

            std::ostream& civLog;
        };

        std::set<FeatureTypes> getWorkerFeatureData(const ResearchTech& researchTech, ResearchTech::WorkerTechDataMap::iterator cityWorkerDataIter, const CvCity* pCity, const Player& player)
        {
            std::set<FeatureTypes> featureTypes;
            boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();

            CityPlotIter iter(pCity);
            XYCoords cityCoords(pCity->plot()->getX(), pCity->plot()->getY());

            while (IterPlot pLoopPlot = iter())
            {
                if (!pLoopPlot.valid())
                {
                    continue;
                }

                PlayerTypes plotOwner = pLoopPlot->getOwner();
                XYCoords plotCoords(pLoopPlot->getX(), pLoopPlot->getY());

                if (plotOwner == pCity->getOwner() && plotCoords != cityCoords)
                {
                    SharedPlotItemPtr sharedPlotPtr = pMapAnalysis->getSharedPlot(pCity->getIDInfo(), plotCoords);

                    if (sharedPlotPtr && !(sharedPlotPtr->assignedImprovement.first == pCity->getIDInfo()))
                    {
                        continue;
                    }

                    FeatureTypes plotFeatureType = pLoopPlot->getFeatureType();
                    if (pLoopPlot->getImprovementType() == NO_IMPROVEMENT && plotFeatureType != NO_FEATURE)
                    {
                        if (std::find(researchTech.possibleRemovableFeatures.begin(), researchTech.possibleRemovableFeatures.end(), plotFeatureType) != researchTech.possibleRemovableFeatures.end())
                        {
                            ++cityWorkerDataIter->second->removableFeatureCounts[plotFeatureType];
                            featureTypes.insert(plotFeatureType);
                        }
                    }
                }
            }
            return featureTypes;
        }
    }

    ResearchTech makeTechTactic(Player& player, TechTypes techType)
    {
        const CvTechInfo& techInfo = gGlobals.getTechInfo(techType);
        const CvPlayer* pPlayer = player.getCvPlayer();
        boost::shared_ptr<TechInfo> pTechInfo = player.getAnalysis()->getTechInfo(techType);
        ResearchTech researchTech;
        researchTech.techType = techType;
        researchTech.depth = player.getTechResearchDepth(techType);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer)->getStream();
        os << "\nChecking tech: " << techInfo.getType() << " with node: " << pTechInfo->getInfo();
#endif

        ResearchTech religionResearchTech = getReligionTechTactics(player, pTechInfo);

        if (religionResearchTech.techType != NO_TECH)
        {
#ifdef ALTAI_DEBUG
            os << "\nreligionResearchTech: " << religionResearchTech;
#endif
            researchTech.merge(religionResearchTech);
        }

        ResearchTech economicResearchTech = getEconomicTechTactics(player, pTechInfo);

        if (economicResearchTech.techType != NO_TECH)
        {
#ifdef ALTAI_DEBUG
            os << "\neconomicResearchTech: " << economicResearchTech;
#endif
            researchTech.merge(economicResearchTech);
        }

        ResearchTech militaryResearchTech = getMilitaryTechTactics(player, pTechInfo);

        if (militaryResearchTech.techType != NO_TECH)
        {
#ifdef ALTAI_DEBUG
            os << "\nmilitaryResearchTech:" << militaryResearchTech;
#endif
            researchTech.merge(militaryResearchTech);
        }

        ResearchTech workerResearchTech = getWorkerTechTactics(player, pTechInfo);

        if (workerResearchTech.techType != NO_TECH)
        {
#ifdef ALTAI_DEBUG
            os << "\nworkerResearchTech: " << workerResearchTech;
#endif
            researchTech.merge(workerResearchTech);
        }

        return researchTech;
    }

    std::list<ResearchTech> makeTechTactics(Player& player)
    {
        std::list<ResearchTech> techTactics;
        const CvPlayer* pPlayer = player.getCvPlayer();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer)->getStream();
        os << "\nmakeTechTactics(): turn = " << gGlobals.getGame().getGameTurn() << "\n";
#endif

        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const int depth = player.getTechResearchDepth((TechTypes)i);
            if (depth > 3 || depth < 1)
            {
                continue;
            }

            ResearchTech researchTech = makeTechTactic(player, (TechTypes)i);
            if (researchTech.techType != NO_TECH)
            {
#ifdef ALTAI_DEBUG
                os << "\nAdding tech tactic: " << researchTech;
#endif
                techTactics.push_back(researchTech);
            }
        }

        return techTactics;
    }

    ResearchTech selectWorkerTechTactics(const Player& player, const ResearchTech& researchTech)
    {
#ifdef ALTAI_DEBUG
        // debug
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
        std::ostream& os = pCivLog->getStream();
        os << "\nselectWorkerTechTactics(): turn = " << gGlobals.getGame().getGameTurn() << "\n";
#endif
        boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();

        ResearchTech selectedResearchTech;
        std::set<BonusTypes> selectedResources;

        // todo - add improved improvements to list and check we are using them as for new improvements below
        if (researchTech.workerFlags & WorkerFlags::Better_Improvements)
        {
            selectedResearchTech.workerFlags |= WorkerFlags::Better_Improvements;
        }

        if (researchTech.workerFlags & WorkerFlags::New_Improvements || researchTech.workerFlags & WorkerFlags::Remove_Features)
        {
            if (player.getCvPlayer()->getNumCities() == 0)
            {
                std::set<ImprovementTypes> improvementTypes = player.getSettlerManager()->getImprovementTypesForSites(1);

#ifdef ALTAI_DEBUG
                // debug
                os << "\nImprovements for potential city: ";
                for (std::set<ImprovementTypes>::const_iterator ci(improvementTypes.begin()), ciEnd(improvementTypes.end()); ci != ciEnd; ++ci)
                {
                    os << gGlobals.getImprovementInfo(*ci).getType() << ", ";
                }
#endif
                std::copy(improvementTypes.begin(), improvementTypes.end(), std::back_inserter(selectedResearchTech.possibleImprovements));
            }
            else
            {
                boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();
                CityIter cityIter(*player.getCvPlayer());
                CvCity* pCity;

                boost::shared_ptr<CivHelper> civHelper = player.getCivHelper();
                std::set<ImprovementTypes> selectedImprovements;
                std::set<FeatureTypes> removableFeatures;
                std::vector<YieldTypes> yieldTypes = boost::assign::list_of(YIELD_PRODUCTION)(YIELD_COMMERCE);

                while (pCity = cityIter())
                {
                    ResearchTech::WorkerTechDataMap::iterator cityWorkerDataIter =
                        selectedResearchTech.workerTechDataMap.insert(std::make_pair(pCity->getIDInfo(), boost::shared_ptr<WorkerTechCityData>(new WorkerTechCityData()))).first;

                    if (!researchTech.possibleRemovableFeatures.empty())
                    {
                        std::set<FeatureTypes> foundFeatures = getWorkerFeatureData(researchTech, cityWorkerDataIter, pCity, player);
                        removableFeatures.insert(foundFeatures.begin(), foundFeatures.end());
                    }

                    // include unclaimed plots (presumes that if culture has not expanded, we are working on it)
                    CityImprovementManager improvementManager(pCity->getIDInfo(), true);

                    const int targetSize = 3 + std::max<int>(pCity->getPopulation(), pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel());
                    improvementManager.calcImprovements(yieldTypes, targetSize);
                    std::vector<CityImprovementManager::PlotImprovementData> baseImprovements = improvementManager.getImprovements();

#ifdef ALTAI_DEBUG
                    os << "\n" << narrow(pCity->getName()) << ": base improvements: ";
                    for (size_t i = 0, count = baseImprovements.size(); i < count; ++i)
                    {
                        improvementManager.logImprovement(os, baseImprovements[i]);
                    }
#endif
                    std::list<TechTypes> pushedTechs = pushTechAndPrereqs(researchTech.techType, player);
                    for (std::list<TechTypes>::const_iterator ci(pushedTechs.begin()), ciEnd(pushedTechs.end()); ci != ciEnd; ++ci)
                    {
                        civHelper->addTech(*ci);
                    }
#ifdef ALTAI_DEBUG
                    os << "\nPushed techs for " << gGlobals.getTechInfo(researchTech.techType).getType() << " = ";
                    for (std::list<TechTypes>::const_iterator ci(pushedTechs.begin()), ciEnd(pushedTechs.end()); ci != ciEnd; ++ci)
                    {
                        os << gGlobals.getTechInfo(*ci).getType() << ", ";
                    }
#endif
                    improvementManager.calcImprovements(yieldTypes, targetSize);

                    std::vector<CityImprovementManager::PlotImprovementData> delta = findNewImprovements(baseImprovements, improvementManager.getImprovements());
                    cityWorkerDataIter->second->newImprovements = delta;

                    for (size_t i = 0, count = delta.size(); i < count; ++i)
                    {
#ifdef ALTAI_DEBUG
                        os << "\n(selectWorkerTechTactics)delta imp: " << i << ": ";
                        improvementManager.logImprovement(os, delta[i]);
#endif

                        if (boost::get<5>(delta[i]) != CityImprovementManager::Not_Selected)
                        {
                            ImprovementTypes improvementType = boost::get<2>(delta[i]);
                            if (std::find(researchTech.possibleImprovements.begin(), researchTech.possibleImprovements.end(), improvementType) != researchTech.possibleImprovements.end())
                            {
                                selectedImprovements.insert(boost::get<2>(delta[i]));
                                selectedResearchTech.workerFlags |= WorkerFlags::New_Improvements;
                                if (boost::get<6>(delta[i]) & CityImprovementManager::ImprovementMakesBonusValid)
                                {
                                    XYCoords coords = boost::get<0>(delta[i]);
                                    const CvPlot* pPlot = gGlobals.getMap().plot(coords.iX, coords.iY);
                                    BonusTypes bonusType = pPlot->getBonusType(player.getTeamID());
                                    selectedResources.insert(bonusType);
                                }
                            }
                        }
                    }
                    
                    for (std::list<TechTypes>::const_iterator ci(pushedTechs.begin()), ciEnd(pushedTechs.end()); ci != ciEnd; ++ci)
                    {
                        civHelper->removeTech(*ci);
                    }
                    
                }

                std::copy(selectedImprovements.begin(), selectedImprovements.end(), std::back_inserter(selectedResearchTech.possibleImprovements));
                std::copy(removableFeatures.begin(), removableFeatures.end(), std::back_inserter(selectedResearchTech.possibleRemovableFeatures));
            }
        }

        if (!researchTech.possibleBonuses.empty())
        {
            boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();
            for (size_t i = 0, count = researchTech.possibleBonuses.size(); i < count; ++i)
            {
                if (pMapAnalysis->getControlledResourceCount(researchTech.possibleBonuses[i]) > 0)
                {
                    selectedResources.insert(researchTech.possibleBonuses[i]);
                }
            }

            std::set<BonusTypes> potentialBonusTypes = player.getSettlerManager()->getBonusesForSites(2);

            for (size_t bonusIndex = 0, bonusCount = researchTech.possibleBonuses.size(); bonusIndex < bonusCount; ++bonusIndex)
            {
                BonusTypes bonusType = researchTech.possibleBonuses[bonusIndex];
                if (potentialBonusTypes.find(bonusType) != potentialBonusTypes.end())
                {
                    selectedResources.insert(bonusType);
                }
            }
        }

        std::copy(selectedResources.begin(), selectedResources.end(), std::back_inserter(selectedResearchTech.possibleBonuses));

        if (!selectedResearchTech.isEmpty())
        {
            selectedResearchTech.techType = researchTech.techType;
            selectedResearchTech.depth = researchTech.depth;
#ifdef ALTAI_DEBUG
            os << "\n(selectWorkerTechTactics): selected tech: " << selectedResearchTech;
#endif
        }
        else
        {
#ifdef ALTAI_DEBUG
            os << "\n(selectWorkerTechTactics): discarded tech: " << researchTech;
#endif
        }

        return selectedResearchTech;
    }

    ResearchTech selectReligionTechTactics(const Player& player, const ResearchTech& researchTech)
    {
        if (researchTech.techFlags & TechFlags::Found_Religion)
        {
            std::vector<TechTypes> availableTechs(player.getAnalysis()->getTechsWithDepth(1));
            bool canResearch = std::find(availableTechs.begin(), availableTechs.end(), researchTech.techType) != availableTechs.end();

            if (canResearch)
            {
                const CvPlayer* pPlayer = player.getCvPlayer();
                int religionCount = 0;

                for (int i = 0, count = gGlobals.getNumReligionInfos(); i < count; ++i)
                {
                    if (pPlayer->getHasReligionCount((ReligionTypes)i) > 0)
                    {
                        ++religionCount;
                    }
                }
#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(*pPlayer)->getStream();
                os << " current religion count = " << religionCount;
#endif
                for (int i = 0, count = gGlobals.getNumReligionInfos(); i < count; ++i)
                {
                    const CvReligionInfo& religionInfo = gGlobals.getReligionInfo((ReligionTypes)i);
                    TechTypes religionTech = (TechTypes)religionInfo.getTechPrereq();
                    if (religionTech == researchTech.techType)
                    {
                        // todo - double-check this works correctly for choose religions
                        if (!gGlobals.getGame().isReligionSlotTaken((ReligionTypes)i))
                        {
                            const CvLeaderHeadInfo& leaderInfo = gGlobals.getLeaderHeadInfo(pPlayer->getPersonalityType());

                            const int flavorValue = leaderInfo.getFlavorValue(AI_FLAVOR_RELIGION);
                            const int randomValue = gGlobals.getGame().getSorenRandNum(10 + 2 * religionCount, "HasFlavorInt");
#ifdef ALTAI_DEBUG
                            os << " flavour value = " << flavorValue << ", random value = " << randomValue;
#endif
                            if (randomValue < std::min<int>(flavorValue + 2, 10))
                            {
#ifdef ALTAI_DEBUG
                                os << " selecting... ";
#endif
                                return researchTech;
                            }
                        }
                    }
                }
            }
        }
        return ResearchTech();
    }

    ResearchTech selectExpansionTechTactics(const Player& player, const ResearchTech& researchTech)
    {
#ifdef ALTAI_DEBUG
        // debug
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
        std::ostream& os = pCivLog->getStream();
        os << "\nselectExpansionTechTactics(): turn = " << gGlobals.getGame().getGameTurn() << "\n";
#endif
        if (!researchTech.possibleProcesses.empty() || researchTech.techFlags & TechFlags::Flexible_Commerce)
        {
            if (researchTech.economicFlags & EconomicFlags::Output_Gold || researchTech.economicFlags & EconomicFlags::Output_Research)
            {
                return researchTech;
            }
        }

        if (researchTech.techFlags & TechFlags::Free_Tech || researchTech.techFlags & TechFlags::Free_GP)
        {
            return researchTech;
        }

        return ResearchTech();
    }

    ResearchTech getResearchTech(const PlayerTactics& playerTactics, TechTypes ignoreTechType)
    {
        TechSelectionData selectionData(playerTactics);

        selectionData.processTechs();
        if (ignoreTechType != NO_TECH)
        {
            selectionData.removeTech(ignoreTechType);
        }
        selectionData.scoreTechs();
        selectionData.debug();

        return selectionData.getSelection();
    }
}
