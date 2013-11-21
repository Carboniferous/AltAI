#include "AltAI.h"

#include "./project_tactics.h"
#include "./player.h"
#include "./city.h"
#include "./civ_helper.h"
#include "./project_info.h"
#include "./project_info_visitors.h"
#include "./project_tactics_visitors.h"
#include "./player_analysis.h"
#include "./tactic_streams.h"
#include "./civ_log.h"


namespace AltAI
{
//    ConstructList makeProjectTactics(Player& player)
//    {
//        ConstructList projectTactics;
//        const CvPlayer* pPlayer = player.getCvPlayer();
//
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*pPlayer)->getStream();
//        os << "\nmakeProjectTactics():\n";
//#endif
//
//        for (int i = 0, count = gGlobals.getNumProjectInfos(); i < count; ++i)
//        {
//            bool haveTechPrereqs = true;
//            boost::shared_ptr<ProjectInfo> pProjectInfo = player.getAnalysis()->getProjectInfo((ProjectTypes)i);
//
//            if (pProjectInfo)
//            {
//                TechTypes requiredTech = getRequiredTech(pProjectInfo);
//                std::vector<TechTypes> neededTechs;
//
//                const int depth = player.getTechResearchDepth(requiredTech);
//                if (depth > 2)
//                {
//                    haveTechPrereqs = false;
//                }
//                else if (depth > 0)
//                {
//                    neededTechs.push_back(requiredTech);
//                }
//
//                if (!haveTechPrereqs)
//                {
//#ifdef ALTAI_DEBUG
//                    os << "\nSkipping project: " << gGlobals.getProjectInfo((ProjectTypes)i).getType();
//#endif
//                    continue;
//                }
//#ifdef ALTAI_DEBUG
//                os << "\nChecking project: " << gGlobals.getProjectInfo((ProjectTypes)i).getType();
//#endif
//                ConstructItem constructItem = getProjectTactics(player, (ProjectTypes)i, NO_VICTORY, pProjectInfo);
//
//#ifdef ALTAI_DEBUG
//                os << constructItem;
//#endif
//                if (constructItem.projectType != NO_PROJECT)
//                {
//                    constructItem.requiredTechs = neededTechs;
//                    projectTactics.push_back(constructItem);
//#ifdef ALTAI_DEBUG
//                    os << "\n" << gGlobals.getProjectInfo((ProjectTypes)i).getType() << " = " << *projectTactics.rbegin();
//#endif
//                }
//
//                // todo - this should be driven from set of victories we are persuing
//                for (int j = 0, victoryCount = gGlobals.getNumVictoryInfos(); j < victoryCount; ++j)
//                {
//                    constructItem = getProjectTactics(player, (ProjectTypes)i, (VictoryTypes)j, pProjectInfo);
//                
//#ifdef ALTAI_DEBUG
//                    os << constructItem;
//#endif
//                    if (constructItem.projectType != NO_PROJECT)
//                    {
//                        constructItem.requiredTechs = neededTechs;
//                        projectTactics.push_back(constructItem);
//#ifdef ALTAI_DEBUG
//                        os << "\n" << gGlobals.getProjectInfo((ProjectTypes)i).getType() << " = " << *projectTactics.rbegin();
//#endif
//                    }
//                }
//            }
//        }
//
//        return projectTactics;
//    }

//    ConstructItem selectProjectTactics(const Player& player, const City& city, const ConstructItem& constructItem)
//    {
//#ifdef ALTAI_DEBUG
//        // debug
//        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
//        std::ostream& os = pCivLog->getStream();
//        if (constructItem.projectType != NO_PROJECT)
//        {
//            os << "\nChecking project: " << gGlobals.getProjectInfo(constructItem.projectType).getType() << " for city: " << narrow(city.getCvCity()->getName());
//        }
//#endif
//        ConstructItem selectedConstructItem(NO_PROJECT);
//
//        const CvPlayer* pPlayer = player.getCvPlayer();
//        const CvCity* pCity = city.getCvCity();
//
//        if (!constructItem.prerequisites.empty())
//        {
//            for (size_t i = 0, count = constructItem.prerequisites.size(); i < count; ++i)
//            {
//                selectedConstructItem = selectProjectTactics(player, city, constructItem.prerequisites[i]);
//                if (!selectedConstructItem.isEmpty())
//                {
//                    return selectedConstructItem;
//                }
//            }
//        }
//        else
//        {
//            if ((constructItem.projectType != NO_PROJECT &&
//                couldConstructProject(player, city, 2, player.getAnalysis()->getProjectInfo(constructItem.projectType)) && 
//                pCity->getFirstProjectOrder(constructItem.projectType) == -1))
//            {
//                const CityDataPtr& pCityData = city.getCityData();
//
//                const int cityCount = pPlayer->getNumCities();
//                const std::pair<int, int> rankAndMaxProduction = player.getCityRank(pCity->getIDInfo(), OUTPUT_PRODUCTION);
//
//    #ifdef ALTAI_DEBUG
//                os << " rank = " << rankAndMaxProduction.first << " of: " << cityCount << ", max prod = " << rankAndMaxProduction.second;
//    #endif
//                if (rankAndMaxProduction.first > 0 && rankAndMaxProduction.first <= std::max<int>(1, cityCount / 3))
//                {
//                    selectedConstructItem.projectType = constructItem.projectType;
//                    for (size_t i = 0, count = constructItem.positiveBonuses.size(); i < count; ++i)
//                    {
//                        if (pCity->hasBonus(constructItem.positiveBonuses[i]))
//                        {
//                            selectedConstructItem.economicFlags |= EconomicFlags::Output_Production;
//                            selectedConstructItem.positiveBonuses.push_back(constructItem.positiveBonuses[i]);
//                        }
//                    }
//                }
//            }
//#ifdef ALTAI_DEBUG
//            os << " => " << selectedConstructItem;
//#endif
//        }
//
//        if (selectedConstructItem.projectType != NO_PROJECT)
//        {
//            //selectedConstructItem.projectType = constructItem.projectType;
//            selectedConstructItem.victoryFlags |= constructItem.victoryFlags;
//
//            TechTypes requiredTech;
//
//            if (selectedConstructItem.projectType != NO_PROJECT)
//            {
//                requiredTech = getRequiredTech(player.getAnalysis()->getProjectInfo(selectedConstructItem.projectType));
//            }
//
//            if (!player.getCivHelper()->hasTech(requiredTech))
//            {
//                selectedConstructItem.requiredTechs.push_back(requiredTech);
//            }
//        }
//
//        return selectedConstructItem;
//    }
}