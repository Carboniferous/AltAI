#include "AltAI.h"

#include "./tech_tactics.h"
#include "./tactic_actions.h"
#include "./tactic_selection_data.h"
#include "./tactic_streams.h"
#include "./tech_tactics_visitors.h"
#include "./building_tactics_visitors.h"
#include "./building_tactics_deps.h"
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

            void addBuildingTechs()
            {
                for (PlayerTactics::LimitedBuildingsTacticsMap::const_iterator ci(playerTactics.globalBuildingsTacticsMap_.begin()), ciEnd(playerTactics.globalBuildingsTacticsMap_.end());
                    ci != ciEnd; ++ci)
                {
                    int firstBuiltTurn = MAX_INT;
                    IDInfo firstBuiltCity;

                    ci->second->update(player);
                    boost::tie(firstBuiltTurn, firstBuiltCity) = ci->second->getFirstBuildCity();

                    if (firstBuiltCity.eOwner != NO_PLAYER)
                    {
                        ICityBuildingTacticsPtr pCityTactics = ci->second->getCityTactics(firstBuiltCity);

                        if (!pCityTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None) &&
                            pCityTactics->areDependenciesSatisfied(IDependentTactic::Ignore_Techs))
                        {
                            boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(ci->first);
                            std::vector<IProjectionEventPtr> possibleEvents = getPossibleEvents(player, pBuildingInfo, firstBuiltTurn);

                            const CvCity* pBuiltCity = getCity(firstBuiltCity);

                            TotalOutput globalDelta;
                            const City& city = player.getCity(pBuiltCity->getID());
                            TotalOutput base = city.getCurrentOutputProjection().getOutput();

                            globalDelta += pCityTactics->getProjection().getOutput() - base;

                            CityIter iter(*player.getCvPlayer());
                            while (CvCity* pOtherCity = iter())
                            {
                                if (pOtherCity->getIDInfo().iID != firstBuiltCity.iID)
                                {
                                    const City& otherCity = player.getCity(pOtherCity->getID());

                                    CityDataPtr pCityData = otherCity.getCityData()->clone();
                                    std::vector<IProjectionEventPtr> events;
                                    events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));
                                    events.push_back(IProjectionEventPtr(new ProjectionGlobalBuildingEvent(pBuildingInfo, firstBuiltTurn, pBuiltCity)));

                                    ProjectionLadder otherCityProjection = getProjectedOutput(player, pCityData, 50, events);
                                    TotalOutput otherBase = otherCity.getCurrentOutputProjection().getOutput();

                                    globalDelta += otherCityProjection.getOutput() - otherBase;
                                }
                            }

                            for (size_t i = 0, count = possibleEvents.size(); i < count; ++i)
                            {
                                TotalOutput globalDelta;

                                CityIter iter(*player.getCvPlayer());
                                while (CvCity* pCity = iter())
                                {
                                    const City& thisCity = player.getCity(pCity);
                                    CityDataPtr pCityData = thisCity.getCityData()->clone();
                                    std::vector<IProjectionEventPtr> events;
                                    events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));
                                    events.push_back(possibleEvents[i]);

                                    if (pCity->getIDInfo().iID != firstBuiltCity.iID)
                                    {   
                                        events.push_back(IProjectionEventPtr(new ProjectionGlobalBuildingEvent(pBuildingInfo, firstBuiltTurn, pBuiltCity)));
                                    }
                                    else
                                    {
                                        events.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pCity, pBuildingInfo)));
                                    }

                                    ProjectionLadder thisCityProjection = getProjectedOutput(player, pCityData, 50, events);
                                    TotalOutput thisBase = thisCity.getCurrentOutputProjection().getOutput();

                                    globalDelta += thisCityProjection.getOutput() - thisBase;

                                }
                            }

                            /*for (size_t i = 0, count = pCityTactics->getTechDependencies().size(); i < count; ++i)
                            {
                                techBuildingsMap[pCityTactics->getTechDependencies()[i]->getResearchTech()].push_back(std::make_pair(ci->first, globalDelta / count));
                            }*/
                        }
                    }
                }
            }

            void sanityCheckTechs()
            {
                const CvTeam& team = CvTeamAI::getTeam(player.getTeamID());
                int leastTurns = MAX_INT;
                std::map<TechTypes, int> possiblyTooExpensiveTechs;

                for (TacticSelectionDataMap::const_iterator ci(tacticSelectionDataMap.begin()), ciEnd(tacticSelectionDataMap.end());
                    ci != ciEnd; ++ci)
                {
                    if (ci->first.first == ResearchTechDependency::ID)
                    {
                        TechTypes techType = (TechTypes)ci->first.second;
                        int totalTechCost = std::max<int>(1, player.getCvPlayer()->findPathLength(techType));

                        const int rate = player.getCvPlayer()->calculateResearchRate(techType);

                        const int approxTurns = 1 + totalTechCost / rate;

                        if (approxTurns < leastTurns)
                        {
                            leastTurns = approxTurns;
                        }

                        if (approxTurns > (4 * gGlobals.getGame().getMaxTurns() / 100))
                        {
                            possiblyTooExpensiveTechs.insert(std::make_pair(techType, approxTurns));
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

                for (std::map<TechTypes, int>::const_iterator ci(possiblyTooExpensiveTechs.begin()),
                    ciEnd(possiblyTooExpensiveTechs.end()); ci != ciEnd; ++ci)
                {
                    if (ci->second > 3 * leastTurns)
                    {
#ifdef ALTAI_DEBUG
                        civLog << "\nErasing tech selection: " << gGlobals.getTechInfo(ci->first).getType();
#endif
                        DependencyItem depItem(ResearchTechDependency::ID, ci->first);
                        tacticSelectionDataMap.erase(depItem);
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

            void scoreTechs()
            {
                int bestTechScore = 0;
                TotalOutputValueFunctor valueF(makeOutputW(4, 3, 2, 2, 1, 1)), processValueF(makeOutputW(0, 0, 4, 3, 2, 1));

                for (TacticSelectionDataMap::const_iterator ci(tacticSelectionDataMap.begin()), ciEnd(tacticSelectionDataMap.end());
                    ci != ciEnd; ++ci)
                {
                    if (ci->first.first == -1)
                    {
                        // no dep ref data...
                    }
                    else if (ci->first.first == ResearchTechDependency::ID)
                    {
                        civLog << "\nTech score for: " << gGlobals.getTechInfo((TechTypes)ci->first.second).getType();

                        int baseTechScore = getBaseTechScore((TechTypes)ci->first.second, ci->second);

                        civLog << " base tech score = " << baseTechScore;

                        int workerScore = valueF(ci->second.cityImprovementsDelta);

                        int bonusScore = 0;

                        int buildingScore = 0;

                        for (std::multiset<EconomicBuildingValue>::const_iterator vi(ci->second.economicBuildings.begin()),
                            viEnd(ci->second.economicBuildings.end()); vi != viEnd; ++vi)
                        {
                            buildingScore += valueF(vi->output);
                        }

                        civLog << " building score = " << buildingScore << " worker score = " << workerScore;

                        int processScore = 0;

                        for (std::map<ProcessTypes, TotalOutput>::const_iterator pi(ci->second.processOutputsMap.begin()),
                            piEnd(ci->second.processOutputsMap.end()); pi != piEnd; ++pi)
                        {
                            processScore += processValueF(pi->second);
                        }

                        civLog << " process score = " << processScore;

                        int thisTechScore = baseTechScore + workerScore + buildingScore + processScore;

                        techScoresMap[(TechTypes)ci->first.second] = thisTechScore;

                        if (thisTechScore > bestTechScore)
                        {
                            bestTechScore = thisTechScore;
                            bestTech = (TechTypes)ci->first.second;
                        }
                    }
                }

//                TotalOutput totalCityOutput;
//
//                CityIter iter(*player.getCvPlayer());
//
//                while (CvCity* pCity = iter())
//                {
//                    const City& city = player.getCity(pCity);
//                    totalCityOutput += city.getCityData()->getActualOutput();
//                }
//
//                // TODO - drive this more situationally
//                TotalOutputValueFunctor valueF(makeOutputW(4, 3, 2, 2, 1, 1));
//#ifdef ALTAI_DEBUG
//                civLog << "\nTotal city output = " << totalCityOutput << " value = " << valueF(totalCityOutput);
//#endif
//                int cityOutputValue = valueF(totalCityOutput);
//                cityOutputValue = std::max<int>(1, cityOutputValue);
//
//                int bestTechValue = 0;
//
//                const CvTeamAI& team = CvTeamAI::getTeam(player.getTeamID());
//
//                for (std::map<TechTypes, TechSelectionHelper>::iterator iter(techSelectionData.begin()), iterEnd(techSelectionData.end()); iter != iterEnd; ++iter)
//                {
//                    int thisTechCost = team.getResearchLeft(iter->second.researchTech.techType);
//                    int totalTechCost = std::max<int>(1, player.getCvPlayer()->findPathLength(iter->second.researchTech.techType));
//
//                    for (TechSelectionHelper::ValuesImprovementsMap::const_iterator impIter(iter->second.newValuedImprovements.begin()), impEndIter(iter->second.newValuedImprovements.end());
//                         impIter != impEndIter; ++impIter)
//                    {
//                        iter->second.workerScore += valueF(impIter->second.first);
//                    }
//                    iter->second.workerScore += valueF.outputWeights[OUTPUT_PRODUCTION] * iter->second.featureProduction;
//
//                    iter->second.workerScore = (50 * iter->second.workerScore) / cityOutputValue;
//                   
//                    if (iter->second.isReligiousTech)
//                    {
//                        // TODO - expand on this
//                        ++iter->second.religionScore;
//                    }
//
//                    int thisTechImprovementValue = 0;
//                    //int thisTechBonusCount = iter->second.researchTech.possibleBonuses.size();
//
////                    for (size_t i = 0, count = iter->second.researchTech.possibleBonuses.size(); i < count; ++i)
////                    {
////                        boost::shared_ptr<ResourceInfo> pResourceInfo = player.getAnalysis()->getResourceInfo(iter->second.researchTech.possibleBonuses[i]);
////                        std::pair<int, int> militaryInfo = getResourceMilitaryUnitCount(pResourceInfo);
////                        ResourceHappyInfo happyInfo = getResourceHappyInfo(pResourceInfo);
////                        ResourceHealthInfo healthInfo = getResourceHealthInfo(pResourceInfo);
////#ifdef ALTAI_DEBUG
////                        //civLog << " units info = " << militaryInfo.first << ", " << militaryInfo.second;
////                        //civLog << " actual happy = " << happyInfo.actualHappy << " potential = " << happyInfo.potentialHappy;
////                        //civLog << " actual health = " << healthInfo.actualHealth << " potential = " << healthInfo.potentialHealth;
////#endif
////                        iter->second.bonusScore += 2 * militaryInfo.first + militaryInfo.second + 2 * happyInfo.actualHappy + happyInfo.potentialHappy + healthInfo.actualHealth;
////                    }
//
//                    Commerce thisProcessCommerce;
//                    for (std::map<ProcessTypes, Commerce>::const_iterator processIter(iter->second.possibleProcessOutputs.begin()), processEndIter(iter->second.possibleProcessOutputs.end());
//                         processIter != processEndIter; ++processIter)
//                    {
//                        thisProcessCommerce += processIter->second;
//                    }
//
//                    TotalOutput processOutput(makeOutput(0, 0, thisProcessCommerce[COMMERCE_GOLD], thisProcessCommerce[COMMERCE_RESEARCH], thisProcessCommerce[COMMERCE_CULTURE], thisProcessCommerce[COMMERCE_ESPIONAGE]));
//                    
//                    iter->second.processScore = 100 * valueF(processOutput) / cityOutputValue;
//
//                    TotalOutput buildingOutputs;
//                    for (std::map<BuildingTypes, TotalOutput>::const_iterator buildingsIter(iter->second.possibleBuildingOutputs.begin()), buildingsEndIter(iter->second.possibleBuildingOutputs.end());
//                         buildingsIter != buildingsEndIter; ++buildingsIter)
//                    {
//                        buildingOutputs += buildingsIter->second;
//                    }
//
//                    TotalOutputValueFunctor valueF(iter->second.makeBuildingValueF());
//                    
//                    iter->second.buildingScore = 50 * valueF(buildingOutputs) / cityOutputValue;
//
//                    // TODO - how much better for each Military flag is this unit than the current one (if any), how many cities would might want to build it?
//                    // Do we have any resource required?
//                    iter->second.unitScore = iter->second.getUnitsScore();
//
//                    iter->second.totalScore = iter->second.workerScore + iter->second.religionScore + iter->second.bonusScore + iter->second.freeTechScore +
//                        iter->second.processScore + iter->second.buildingScore + iter->second.unitScore;
//
//                    iter->second.techValue = 100 * iter->second.totalScore / totalTechCost;
//                    if (iter->second.techValue > bestTechValue)
//                    {
//                        bestTechValue = iter->second.techValue;
//                        bestTech = iter->second.researchTech.techType;
//                    }
//#ifdef ALTAI_DEBUG
//                    civLog << "\nTech = " << gGlobals.getTechInfo(iter->second.researchTech.techType).getType() << " cost = " << thisTechCost << ", total cost = " << totalTechCost;
//                    //civLog << " bonus count = " << thisTechBonusCount;
//                    civLog << " process output = " << processOutput << " value = " << valueF(processOutput) << " % = " << 100 * valueF(processOutput) / cityOutputValue;
//                    civLog << " building output = " << buildingOutputs << " value = " << valueF(buildingOutputs) << ", weights = " << valueF.outputWeights;
//                    civLog << "\nScores: worker = " << iter->second.workerScore << ", religion = " << iter->second.religionScore
//                           << ", bonus = " << iter->second.bonusScore << ", process = " << iter->second.processScore
//                           << ", buildings = " << iter->second.buildingScore << ", units = " << iter->second.unitScore
//                           << ", free techs = " << iter->second.freeTechScore
//                           << ", tech score = " << iter->second.totalScore << ", value = " << iter->second.techValue;
//#endif
//                }
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

                for (TacticSelectionDataMap::const_iterator ci(tacticSelectionDataMap.begin()), ciEnd(tacticSelectionDataMap.end());
                    ci != ciEnd; ++ci)
                {
                    civLog << "\nDep item = ";
                    debugDepItem(ci->first, civLog);
                    ci->second.debug(civLog);
                }

                bestTech != NO_TECH ? civLog << "\nbest tech = " << gGlobals.getTechInfo(bestTech).getType() : civLog << "\nNo best tech.";
#endif
            }

            TacticSelectionDataMap tacticSelectionDataMap;

            std::map<TechTypes, int> techScoresMap;

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
        TechSelectionData selectionData(playerTactics);

        // pure tech tactics ('first to tech' items, open borders, etc...)
        for (PlayerTactics::TechTacticsMap::const_iterator ci = playerTactics.techTacticsMap_.begin(), ciEnd = playerTactics.techTacticsMap_.end();
            ci != ciEnd; ++ci)
        {
            if (ci->second)
            {
                ci->second->apply(selectionData.tacticSelectionDataMap[DependencyItem(ResearchTechDependency::ID, ci->first)]);
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

        // units
        for (PlayerTactics::UnitTacticsMap::const_iterator iter(playerTactics.unitTacticsMap_.begin()),
            endIter(playerTactics.unitTacticsMap_.end()); iter != endIter; ++iter)
        {
            if (iter->second)
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

                for (PlayerTactics::CityImprovementTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                {
                    (*li)->update(playerTactics.player, city.getCityData());

                    const std::vector<ResearchTechDependencyPtr> techs = (*li)->getTechDependencies();
                    for (size_t i = 0, count = techs.size(); i < count; ++i)
                    {
                        selectionData.tacticSelectionDataMap[techs[i]->getDependencyItem()].cityImprovementsDelta +=
                            (*li)->getProjection().getOutput() - base;
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
                    selectionData.tacticSelectionDataMap[techs[i]->getDependencyItem()].
                        processOutputsMap[ci->second->getProcessType()] += processOutput;
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
