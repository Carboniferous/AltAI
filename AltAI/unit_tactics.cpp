#include "./unit_tactics.h"
#include "./unit_tactics_visitors.h"
#include "./unit_info_visitors.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./gamedata_analysis.h"
#include "./settler_manager.h"
#include "./unit_analysis.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./iters.h"

namespace AltAI
{
    ConstructList makeUnitTactics(Player& player)
    {
        ConstructList unitTactics;
        const CvPlayer* pPlayer = player.getCvPlayer();

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer)->getStream();
        os << "\nmakeUnitTactics():\n";
#endif

        for (int i = 0, count = gGlobals.getNumUnitInfos(); i < count; ++i)
        {
            bool haveTechPrereqs = true;
            boost::shared_ptr<UnitInfo> pUnitInfo = player.getAnalysis()->getUnitInfo((UnitTypes)i);

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
                os << "\nChecking unit: " << gGlobals.getUnitInfo((UnitTypes)i).getType();
#endif
                ConstructItem constructItem = getEconomicUnitTactics(player, (UnitTypes)i, pUnitInfo);
#ifdef ALTAI_DEBUG
                os << constructItem;
#endif
                if (constructItem.unitType != NO_UNIT)
                {
                    constructItem.requiredTechs = neededTechs;
                    unitTactics.push_back(constructItem);
#ifdef ALTAI_DEBUG
                    os << "\n" << gGlobals.getUnitInfo((UnitTypes)i).getType() << " = " << *unitTactics.rbegin();
#endif
                }

                constructItem = getMilitaryExpansionUnitTactics(player, (UnitTypes)i, pUnitInfo);
#ifdef ALTAI_DEBUG
                os << constructItem;
#endif
                if (constructItem.unitType != NO_UNIT)
                {
                    constructItem.requiredTechs = neededTechs;
                    unitTactics.push_back(constructItem);
#ifdef ALTAI_DEBUG
                    os << "\n" << gGlobals.getUnitInfo((UnitTypes)i).getType() << " = " << *unitTactics.rbegin();
#endif
                }
            }
        }

        return unitTactics;
    }

    ConstructItem selectExpansionUnitTactics(const Player& player, const ConstructItem& constructItem)
    {
#ifdef ALTAI_DEBUG
        // debug
        //boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
        //std::ostream& os = pCivLog->getStream();
        //os << "\n(selectExpansionUnitTactics) checking: " << constructItem;
#endif
        ConstructItem selectedConstructItem(NO_UNIT);

        // todo - need to include units which we also need resources for
        if (couldConstructUnit(player, 2, player.getAnalysis()->getUnitInfo(constructItem.unitType)))
        {
            if (player.getMaxResearchRate() > 20 && !player.getSettlerManager()->getBestCitySites(120, 1).empty() ||
                player.getMaxResearchRate() > 40 && !player.getSettlerManager()->getBestCitySites(80, 1).empty())
            {
                if (constructItem.economicFlags & EconomicFlags::Output_Settler)
                {
                    selectedConstructItem.economicFlags |= EconomicFlags::Output_Settler;
                }
            }

            if (constructItem.militaryFlags & MilitaryFlags::Output_Combat_Unit)
            {
                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Combat_Unit;
            }
            if (constructItem.militaryFlags & MilitaryFlags::Output_Collateral)
            {
                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Collateral;
            }
            if (constructItem.militaryFlags & MilitaryFlags::Output_Defence)
            {
                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Defence;
            }
            if (constructItem.militaryFlags & MilitaryFlags::Output_Extra_Mobility)
            {
                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Extra_Mobility;
            }
            if (constructItem.militaryFlags & MilitaryFlags::Output_Explore)
            {
                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Explore;
            }

            // todo - count sites/improvements which need transport
            if (constructItem.militaryFlags & MilitaryFlags::Output_Unit_Transport)
            {
                selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Unit_Transport;
            }

            if (!constructItem.possibleBuildTypes.empty())
            {
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(constructItem.unitType);
                boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();
                CityIter cityIter(*player.getCvPlayer());
                CvCity* pCity;

                std::map<BuildTypes, int> selectedBuildTypes;

                while (pCity = cityIter())
                {
                    const CityImprovementManager& improvementManager = pMapAnalysis->getImprovementManager(pCity->getIDInfo());
                    const std::vector<CityImprovementManager::PlotImprovementData>& improvements = improvementManager.getImprovements();

                    int possibleBuildCount = 0;
                    for (size_t i = 0, count = improvements.size(); i < count; ++i)
                    {
                        if (boost::get<5>(improvements[i]) == CityImprovementManager::Not_Built)
                        {
#ifdef ALTAI_DEBUG
                            //improvementManager.logImprovement(os, improvements[i]);
#endif
                            BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(boost::get<2>(improvements[i]));
                            std::map<BuildTypes, int>::const_iterator buildsIter = constructItem.possibleBuildTypes.find(buildType);
                            if (buildsIter != constructItem.possibleBuildTypes.end() && unitInfo.getBuilds(buildType))
                            {
#ifdef ALTAI_DEBUG
                                //os << "\nAdding selected build type: " << gGlobals.getBuildInfo(buildType).getType();
#endif
                                ++selectedBuildTypes[buildType];
                                ++possibleBuildCount;
                            }

                            FeatureTypes featureType = boost::get<1>(improvements[i]);
                            if (featureType != NO_FEATURE)
                            {
                                buildType = GameDataAnalysis::getBuildTypeToRemoveFeature(featureType);
                                std::map<BuildTypes, int>::const_iterator buildsIter = constructItem.possibleBuildTypes.find(buildType);
                                if (buildsIter != constructItem.possibleBuildTypes.end() && unitInfo.getBuilds(buildType))
                                {
#ifdef ALTAI_DEBUG
                                    //os << "\nAdding selected build type: " << gGlobals.getBuildInfo(buildType).getType() << " for removing feature: " << gGlobals.getFeatureInfo(featureType).getType();
#endif
                                    ++selectedBuildTypes[buildType];
                                    ++possibleBuildCount;
                                }
                            }
                        }
                    }
                }

                if (!selectedBuildTypes.empty())
                {
                    for (std::map<BuildTypes, int>::const_iterator ci(selectedBuildTypes.begin()), ciEnd(selectedBuildTypes.end()); ci != ciEnd; ++ci)
                    {
#ifdef ALTAI_DEBUG
                        //os << "\nCopying selected build type: " << gGlobals.getBuildInfo(ci->first).getType();
#endif
                        selectedConstructItem.possibleBuildTypes.insert(*ci);
                    }
                }
                else  // check upcoming techs for new build types
                {
                    const std::list<ResearchTech>& selectedTechs = player.getAnalysis()->getPlayerTactics()->selectedTechTactics_;
                    for (std::list<ResearchTech>::const_iterator ci(selectedTechs.begin()), ciEnd(selectedTechs.end()); ci != ciEnd; ++ci)
                    {
                        for (size_t i = 0, count = ci->possibleImprovements.size(); i < count; ++i)
                        {
                            BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(ci->possibleImprovements[i]);
                            if (unitInfo.getBuilds(buildType))
                            {
#ifdef ALTAI_DEBUG
                                //os << "\nAdding tech selected build type: " << gGlobals.getBuildInfo(GameDataAnalysis::getBuildTypeForImprovementType(ci->possibleImprovements[i])).getType();
#endif
                                // add 1 as a count - TODO - source this better (store info in TechTactics?)
                                selectedBuildTypes.insert(std::make_pair(GameDataAnalysis::getBuildTypeForImprovementType(ci->possibleImprovements[i]), 1));
                            }
                        }
                    }

                    if (!selectedBuildTypes.empty())
                    {
                        for (std::map<BuildTypes, int>::const_iterator ci(selectedBuildTypes.begin()), ciEnd(selectedBuildTypes.end()); ci != ciEnd; ++ci)
                        {
#ifdef ALTAI_DEBUG
                            //os << "\nCopying selected build type: " << gGlobals.getBuildInfo(ci->first).getType();
#endif
                            selectedConstructItem.possibleBuildTypes.insert(*ci);
                        }
                    }
                }
            }
        }

        if (!selectedConstructItem.isEmpty())
        {
            std::vector<TechTypes> requiredTechs = getRequiredTechs(player.getAnalysis()->getUnitInfo(constructItem.unitType));
            for (size_t i = 0, count = requiredTechs.size(); i < count; ++i)
            {
                if (!player.getCivHelper()->hasTech(requiredTechs[i]))
                {
                    selectedConstructItem.requiredTechs.push_back(requiredTechs[i]);
                }
            }
            selectedConstructItem.militaryFlagValuesMap = constructItem.militaryFlagValuesMap;
            selectedConstructItem.unitType = constructItem.unitType;
        }

        return selectedConstructItem;
    }

    UnitTypes getConstructItem(const PlayerTactics& playerTactics)
    {
        return NO_UNIT;
    }
}