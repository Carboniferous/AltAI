#include "AltAI.h"

#include "./unit_tactics.h"
#include "./unit_tactics_visitors.h"
#include "./unit_info_visitors.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./gamedata_analysis.h"
#include "./settler_manager.h"
#include "./helper_fns.h"
#include "./unit_analysis.h"
#include "./tactic_selection_data.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./iters.h"

namespace AltAI
{
    void UnitValueHelper::debug(const UnitValueHelper::MapT& unitCombatData, std::ostream& os) const
    {
        for (MapT::const_iterator ci(unitCombatData.begin()), ciEnd(unitCombatData.end()); ci != ciEnd; ++ci)
        {
            os << "\nUnit: " << gGlobals.getUnitInfo(ci->first).getType() << " (cost = " << ci->second.first << ")";
            for (size_t i = 0, count = ci->second.second.size(); i < count; ++i)
            {
                os << "\n\t" << gGlobals.getUnitInfo(ci->second.second[i].first).getType() << " odds = " << ci->second.second[i].second;
            }
            os << "\n\t\tvalue = " << getValue(ci->second);
        }
    }

    void UnitValueHelper::addMapEntry(MapT& unitCombatData, UnitTypes unitType, const std::vector<UnitTypes>& possibleCombatUnits, const std::vector<int>& odds) const
    {
        unitCombatData[unitType].first = gGlobals.getUnitInfo(unitType).getProductionCost();

        for (size_t j = 0, oddsCounter = odds.size(); j < oddsCounter; ++j)
        {
            if (odds[j] > 0)
            {
                unitCombatData[unitType].second.push_back(std::make_pair(possibleCombatUnits[j], odds[j]));
            }
        }
    }

    int UnitValueHelper::getValue(const std::pair<int, std::vector<std::pair<UnitTypes, int> > >& mapEntry) const
    {
        const int maxTries = 3;
        const double oddsThreshold = 0.65;
        const int cost = mapEntry.first;

        double value = 0;

        for (size_t i = 0, count = mapEntry.second.size(); i < count; ++i)
        {
            int thisCost = cost;
            const double thisOdds = (double)(mapEntry.second[i].second) * 0.001;
            double odds = thisOdds;
            int nTries = 1;

            while (odds < oddsThreshold && nTries++ < maxTries)
            {
                odds += (1.0 - odds) * thisOdds;
                thisCost += cost;
            }

            if (odds > oddsThreshold)
            {
                value += 1000.0 * odds / (double)thisCost;
            }
        }

        return (int)value;
    }

    /*ConstructList makeUnitTactics(Player& player)
    {
        ConstructList unitTactics;
        const CvPlayer* pPlayer = player.getCvPlayer();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer)->getStream();
        os << "\nmakeUnitTactics():\n";
#endif

        for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
        {
            UnitTypes unitType = getPlayerVersion(player.getPlayerID(), (UnitClassTypes)i);

            if (unitType != NO_UNIT)
            {
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);
                if (unitInfo.getProductionCost() < 0)
                {
                    continue;
                }

                bool haveTechPrereqs = true;
                boost::shared_ptr<UnitInfo> pUnitInfo = player.getAnalysis()->getUnitInfo(unitType);

                if (pUnitInfo)
                {
                    const std::vector<TechTypes>& requiredTechs = getRequiredTechs(pUnitInfo);
                    std::vector<TechTypes> neededTechs;

                    for (size_t j = 0, count = requiredTechs.size(); j < count; ++j)
                    {
                        const int depth = player.getTechResearchDepth(requiredTechs[j]);
                        if (depth > 2)
                        {
                            haveTechPrereqs = false;
                            break;
                        }
                        else if (depth > 0)
                        {
                            neededTechs.push_back(requiredTechs[j]);
                        }
                    }

                    if (!haveTechPrereqs)
                    {
                        continue;
                    }
#ifdef ALTAI_DEBUG
                    os << "\nChecking unit: " << gGlobals.getUnitInfo(unitType).getType();
#endif
                    ConstructItem constructItem = getEconomicUnitTactics(player, unitType, pUnitInfo);
#ifdef ALTAI_DEBUG
                    os << constructItem;
#endif
                    if (constructItem.unitType != NO_UNIT)
                    {
                        constructItem.requiredTechs = neededTechs;
                        unitTactics.push_back(constructItem);
#ifdef ALTAI_DEBUG
                        os << "\n" << gGlobals.getUnitInfo(unitType).getType() << " = " << *unitTactics.rbegin();
#endif
                    }

                    constructItem = getMilitaryExpansionUnitTactics(player, unitType, pUnitInfo);
#ifdef ALTAI_DEBUG
                    os << constructItem;
#endif
                    if (constructItem.unitType != NO_UNIT)
                    {
                        constructItem.requiredTechs = neededTechs;
                        unitTactics.push_back(constructItem);
#ifdef ALTAI_DEBUG
                        os << "\n" << gGlobals.getUnitInfo(unitType).getType() << " = " << *unitTactics.rbegin();
#endif
                    }
                }
            }
        }

        return unitTactics;
    }*/

//    ConstructItem selectExpansionUnitTactics(const Player& player, const ConstructItem& constructItem)
//    {
//#ifdef ALTAI_DEBUG
//        // debug
//        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
//        std::ostream& os = pCivLog->getStream();
//        os << "\n(selectExpansionUnitTactics) checking: " << constructItem;
//#endif
//        ConstructItem selectedConstructItem(NO_UNIT);
//
//        // todo - need to include units which we also need resources for
//        if (couldConstructUnit(player, 2, player.getAnalysis()->getUnitInfo(constructItem.unitType), false))
//        {
//            if (player.getMaxResearchRate() > 40 && !player.getSettlerManager()->getBestCitySites(140, 1).empty() ||
//                player.getMaxResearchRate() > 60 && !player.getSettlerManager()->getBestCitySites(80, 1).empty())
//            {
//                if (constructItem.economicFlags & EconomicFlags::Output_Settler)
//                {
//                    selectedConstructItem.economicFlags |= EconomicFlags::Output_Settler;
//                }
//            }
//
//            if (constructItem.militaryFlags & MilitaryFlags::Output_Combat_Unit)
//            {
//                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Combat_Unit;
//            }
//            if (constructItem.militaryFlags & MilitaryFlags::Output_Collateral)
//            {
//                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Collateral;
//            }
//            if (constructItem.militaryFlags & MilitaryFlags::Output_Defence)
//            {
//                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Defence;
//            }
//            if (constructItem.militaryFlags & MilitaryFlags::Output_Extra_Mobility)
//            {
//                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Extra_Mobility;
//            }
//            if (constructItem.militaryFlags & MilitaryFlags::Output_Explore)
//            {
//                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Explore;
//            }
//
//            // todo - count sites/improvements which need transport
//            if (constructItem.militaryFlags & MilitaryFlags::Output_Unit_Transport)
//            {
//                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Unit_Transport;
//            }
//            
//            if (!constructItem.possibleBuildTypes.empty())
//            {
//                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(constructItem.unitType);
//
//                boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();
//                CityIter cityIter(*player.getCvPlayer());
//                CvCity* pCity;
//
//                std::map<BuildTypes, int> selectedBuildTypes;
//
//                while (pCity = cityIter())
//                {
//                    const CityImprovementManager& improvementManager = pMapAnalysis->getImprovementManager(pCity->getIDInfo());
//                    const std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();
//
//                    int possibleBuildCount = 0;
//                    for (size_t i = 0, count = improvements.size(); i < count; ++i)
//                    {
//                        if (boost::get<5>(improvements[i]) == CityImprovementManager::Not_Built)
//                        {
//#ifdef ALTAI_DEBUG
//                            //improvementManager.logImprovement(os, improvements[i]);
//#endif
//                            BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(boost::get<2>(improvements[i]));
//                            std::map<BuildTypes, int>::const_iterator buildsIter = constructItem.possibleBuildTypes.find(buildType);
//                            if (buildsIter != constructItem.possibleBuildTypes.end() && unitInfo.getBuilds(buildType))
//                            {
//#ifdef ALTAI_DEBUG
//                                //os << "\nAdding selected build type: " << gGlobals.getBuildInfo(buildType).getType();
//#endif
//                                ++selectedBuildTypes[buildType];
//                                ++possibleBuildCount;
//                            }
//
//                            FeatureTypes featureType = boost::get<1>(improvements[i]);
//                            if (featureType != NO_FEATURE)
//                            {
//                                buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureType);
//                                std::map<BuildTypes, int>::const_iterator buildsIter = constructItem.possibleBuildTypes.find(buildType);
//                                if (buildsIter != constructItem.possibleBuildTypes.end() && unitInfo.getBuilds(buildType))
//                                {
//#ifdef ALTAI_DEBUG
//                                    //os << "\nAdding selected build type: " << gGlobals.getBuildInfo(buildType).getType() << " for removing feature: " << gGlobals.getFeatureInfo(featureType).getType();
//#endif
//                                    ++selectedBuildTypes[buildType];
//                                    ++possibleBuildCount;
//                                }
//                            }
//                        }
//                    }
//                }
//
//                if (!selectedBuildTypes.empty())
//                {
//                    for (std::map<BuildTypes, int>::const_iterator ci(selectedBuildTypes.begin()), ciEnd(selectedBuildTypes.end()); ci != ciEnd; ++ci)
//                    {
//#ifdef ALTAI_DEBUG
//                        os << "\nCopying selected build type: " << gGlobals.getBuildInfo(ci->first).getType();
//#endif
//                        selectedConstructItem.possibleBuildTypes.insert(*ci);
//                    }
//                }
//                else
//                {
//                    // check upcoming techs for new build types
//                    ResearchTech currentResearchTech = player.getCurrentResearchTech();
//                    std::vector<ImprovementTypes> possibleImprovements = currentResearchTech.possibleImprovements;
//
//                    if (currentResearchTech.techType != NO_TECH && currentResearchTech.targetTechType != currentResearchTech.techType)
//                    {
//                        currentResearchTech = player.getAnalysis()->getPlayerTactics()->getResearchTechData(currentResearchTech.targetTechType);
//                        std::copy(currentResearchTech.possibleImprovements.begin(), currentResearchTech.possibleImprovements.end(), std::back_inserter(possibleImprovements));
//                    }
//
//                    for (size_t i = 0, count = possibleImprovements.size(); i < count; ++i)
//                    {
//                        BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(possibleImprovements[i]);
//                        if (buildType != NO_BUILD && unitInfo.getBuilds(buildType))
//                        {
//    #ifdef ALTAI_DEBUG
//                            os << "\nAdding tech selected build type: " << gGlobals.getBuildInfo(GameDataAnalysis::getBuildTypeForImprovementType(possibleImprovements[i])).getType();
//    #endif
//                            // add 1 as a count - TODO - source this better (store info in TechTactics?)
//                            selectedBuildTypes.insert(std::make_pair(GameDataAnalysis::getBuildTypeForImprovementType(possibleImprovements[i]), 1));
//                        }
//                    }
//
//                    if (!selectedBuildTypes.empty())
//                    {
//                        for (std::map<BuildTypes, int>::const_iterator ci(selectedBuildTypes.begin()), ciEnd(selectedBuildTypes.end()); ci != ciEnd; ++ci)
//                        {
//    #ifdef ALTAI_DEBUG
//                            os << "\nCopying selected build type: " << gGlobals.getBuildInfo(ci->first).getType();
//    #endif
//                            selectedConstructItem.possibleBuildTypes.insert(*ci);
//                        }
//                    }
//                }
//            }
//        
//            // missionaries...
//            if (!constructItem.religionTypes.empty())
//            {
//                if (constructItem.economicFlags & EconomicFlags::Output_Culture || constructItem.economicFlags & EconomicFlags::Output_Happy)
//                {
//                    CityIter iter(*player.getCvPlayer());
//                    while (CvCity* pCity = iter())
//                    {
//                        const City& city = player.getCity(pCity);
//                        if (constructItem.economicFlags & EconomicFlags::Output_Culture)
//                        {
//                            const CityDataPtr& pCityData = city.getCityData(); 
//                            if (pCityData && pCityData->getNumUncontrolledPlots(true) > 0)
//                            {
//                                for (size_t i = 0, count = constructItem.religionTypes.size(); i < count; ++i)
//                                {
//                                    if (!pCity->isHasReligion(constructItem.religionTypes[i]))
//                                    {
//                                        selectedConstructItem.economicFlags |= EconomicFlags::Output_Culture;
//                                        if (std::find(selectedConstructItem.religionTypes.begin(), selectedConstructItem.religionTypes.end(), constructItem.religionTypes[i]) ==
//                                            selectedConstructItem.religionTypes.end())
//                                        {
//                                            selectedConstructItem.religionTypes.push_back(constructItem.religionTypes[i]);
//                                        }
//                                    }
//                                }
//                            }
//                        }
//
//                        if (constructItem.economicFlags & EconomicFlags::Output_Happy)
//                        {
//                            if (pCity->happyLevel() - pCity->unhappyLevel() < 0)
//                            {
//                                for (size_t i = 0, count = constructItem.religionTypes.size(); i < count; ++i)
//                                {
//                                    if (!pCity->isHasReligion(constructItem.religionTypes[i]))
//                                    {
//                                        selectedConstructItem.economicFlags |= EconomicFlags::Output_Happy;
//                                        if (std::find(selectedConstructItem.religionTypes.begin(), selectedConstructItem.religionTypes.end(), constructItem.religionTypes[i]) ==
//                                            selectedConstructItem.religionTypes.end())
//                                        {
//                                            selectedConstructItem.religionTypes.push_back(constructItem.religionTypes[i]);
//                                        }
//                                    }
//                                }
//                            }
//                        }
//                    }
//                }
//            }
//        }
//
//        if (!selectedConstructItem.isEmpty())
//        {
//            std::vector<TechTypes> requiredTechs = getRequiredTechs(player.getAnalysis()->getUnitInfo(constructItem.unitType));
//            for (size_t i = 0, count = requiredTechs.size(); i < count; ++i)
//            {
//                if (!player.getCivHelper()->hasTech(requiredTechs[i]))
//                {
//                    selectedConstructItem.requiredTechs.push_back(requiredTechs[i]);
//                }
//            }
//            selectedConstructItem.militaryFlagValuesMap = constructItem.militaryFlagValuesMap;
//            selectedConstructItem.unitType = constructItem.unitType;
//#ifdef ALTAI_DEBUG
//            os << "\nSelected build item: " << selectedConstructItem;
//#endif
//        }
//
//        return selectedConstructItem;
//    }

    /*UnitTypes getConstructItem(const PlayerTactics& playerTactics)
    {
        return NO_UNIT;
    }*/

    std::pair<std::vector<UnitTypes>, std::vector<UnitTypes> > getActualAndPossibleCombatUnits(const Player& player, const CvCity* pCity, DomainTypes domainType)
    {
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
//#endif
        std::vector<UnitTypes> combatUnits, possibleCombatUnits;

        boost::shared_ptr<PlayerTactics> pTactics = player.getAnalysis()->getPlayerTactics();

        for (PlayerTactics::UnitTacticsMap::const_iterator ci(pTactics->unitTacticsMap_.begin()), ciEnd(pTactics->unitTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            if (ci->first != NO_UNIT)
            {
//#ifdef ALTAI_DEBUG
//                os << "\nChecking unit: " << gGlobals.getUnitInfo(ci->first).getType();
//#endif
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(ci->first);
                if (unitInfo.getDomainType() == domainType && unitInfo.getProductionCost() >= 0 && unitInfo.getCombat() > 0)
                {
                    if (ci->second && !isUnitObsolete(player, player.getAnalysis()->getUnitInfo(ci->first)))
                    {
                        const boost::shared_ptr<UnitInfo> pUnitInfo = player.getAnalysis()->getUnitInfo(ci->first);
                        bool depsSatisfied = false;

                        if (pCity)
                        {
                            ICityUnitTacticsPtr pCityTactics = ci->second->getCityTactics(pCity->getIDInfo());
                            depsSatisfied = pCityTactics && pCityTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None);
                        }
                        else
                        {
                            depsSatisfied = ci->second->areDependenciesSatisfied(player, IDependentTactic::Ignore_None);
                        }
                        bool couldConstruct = couldConstructUnit(player, 0, pUnitInfo, false);
//#ifdef ALTAI_DEBUG
//                        os << " deps = " << depsSatisfied << ", could construct = " << couldConstruct;
//#endif
                        if (depsSatisfied)
                        {
                            combatUnits.push_back(ci->first);
                            possibleCombatUnits.push_back(ci->first);
                        }
                        else if (couldConstructUnit(player, 1, player.getAnalysis()->getUnitInfo(ci->first), true))
                        {
                            possibleCombatUnits.push_back(ci->first);
                        }
                    }
                }
            }
        }

        return std::make_pair(combatUnits, possibleCombatUnits);
    }

    ConstructItem getSpecialistBuild(const PlayerTactics& playerTactics, UnitTypes unitType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream();
#endif
        PlayerTactics::UnitTacticsMap::const_iterator unitIter = playerTactics.unitTacticsMap_.find(unitType);
        TacticSelectionData selectionData;

        if (unitIter != playerTactics.unitTacticsMap_.end())
        {
            unitIter->second->update(playerTactics.player);
            unitIter->second->apply(selectionData);
        }

        return ConstructItem();
    }
}