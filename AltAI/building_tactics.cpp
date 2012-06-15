#include "./building_tactics.h"
#include "./building_tactics_visitors.h"
#include "./building_info_visitors.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./player.h"
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
        void addConstructItem(ConstructList& constructItems, const ConstructItem& constructItem)
        {
            ConstructListIter iter = std::find_if(constructItems.begin(), constructItems.end(), ConstructItemFinder(constructItem));
            if (iter != constructItems.end())
            {
                iter->merge(constructItem);
            }
            else
            {
                constructItems.push_back(constructItem);
            }
        }
    }

    ConstructList makeBuildingTactics(Player& player)
    {
        ConstructList buildingTactics;
        const CvPlayer* pPlayer = player.getCvPlayer();
        const int lookAheadDepth = 2;

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer)->getStream();
        os << "\nmakeBuildingTactics():\n";
#endif

        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            const bool isWorldWonder = isWorldWonderClass((BuildingClassTypes)i), isNationalWonder = isNationalWonderClass((BuildingClassTypes)i);

            BuildingTypes buildingType = getPlayerVersion(player.getPlayerID(), (BuildingClassTypes)i);
            if (buildingType == NO_BUILDING)
            {
                continue;
            }

            const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);
            if (buildingInfo.getProductionCost() <= 0)
            {
                continue;
            }

            if (isNationalWonder)
            {
                if (buildingInfo.isGovernmentCenter())
                {
                    continue;
                }
            }

            if (isWorldWonder && gGlobals.getGame().isBuildingClassMaxedOut((BuildingClassTypes)i))
            {
                continue;
            }

            bool haveTechPrereqs = true;
            boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(buildingType);

            if (pBuildingInfo)
            {
                const std::vector<TechTypes>& requiredTechs = getRequiredTechs(pBuildingInfo);

                for (size_t j = 0, count = requiredTechs.size(); j < count; ++j)
                {
                    const int depth = player.getTechResearchDepth(requiredTechs[j]);
                    if (depth > lookAheadDepth)
                    {
                        haveTechPrereqs = false;
                        break;
                    }
                }

                if (!haveTechPrereqs)
                {
                    continue;
                }

                if (!couldConstructSpecialBuilding(player, lookAheadDepth, pBuildingInfo))
                {
                    continue;
                }

                ReligionTypes religionType = (ReligionTypes)gGlobals.getBuildingInfo(buildingType).getPrereqReligion();
                if (religionType != NO_RELIGION)
                {
                    if (player.getCvPlayer()->getHasReligionCount(religionType) == 0)
                    {
                        continue;
                    }
                }

                ConstructItem constructItem = getEconomicBuildingTactics(player, buildingType, pBuildingInfo);

                if (constructItem.buildingType != NO_BUILDING)
                {
                    if (isWorldWonder)
                    {
                        constructItem.buildingFlags |= BuildingFlags::Building_World_Wonder;
                    }
                    else if (isNationalWonder)
                    {
                        constructItem.buildingFlags |= BuildingFlags::Building_National_Wonder;
                    }
                    addConstructItem(buildingTactics, constructItem);
                }

                constructItem = getMilitaryBuildingTactics(player, buildingType, pBuildingInfo);

                if (constructItem.buildingType != NO_BUILDING)
                {
                    if (isWorldWonder)
                    {
                        constructItem.buildingFlags |= BuildingFlags::Building_World_Wonder;
                    }
                    addConstructItem(buildingTactics, constructItem);
                }
            }
        }

        for (int i = 0, count = gGlobals.getNumProcessInfos(); i < count; ++i)
        {
            const CvProcessInfo& processInfo = gGlobals.getProcessInfo((ProcessTypes)i);
            const int depth = player.getTechResearchDepth((TechTypes)processInfo.getTechPrereq());
            if (depth <= 2)
            {
                ConstructItem constructItem((ProcessTypes)i);

                if (processInfo.getProductionToCommerceModifier(COMMERCE_GOLD) > 0)
                {
                    constructItem.economicFlags |= EconomicFlags::Output_Gold;
                }
                if (processInfo.getProductionToCommerceModifier(COMMERCE_RESEARCH) > 0)
                {
                    constructItem.economicFlags |= EconomicFlags::Output_Research;
                }
                if (processInfo.getProductionToCommerceModifier(COMMERCE_CULTURE) > 0)
                {
                    constructItem.economicFlags |= EconomicFlags::Output_Culture;
                }
                if (processInfo.getProductionToCommerceModifier(COMMERCE_ESPIONAGE) > 0)
                {
                    constructItem.economicFlags |= EconomicFlags::Output_Espionage;
                }

                addConstructItem(buildingTactics, constructItem);
            }
        }

        return buildingTactics;
    }

    ConstructItem selectExpansionBuildingTactics(const Player& player, const City& city, const ConstructItem& constructItem)
    {
#ifdef ALTAI_DEBUG
        // debug
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
        std::ostream& os = pCivLog->getStream();
        if (constructItem.buildingType != NO_BUILDING)
        {
            os << "\nChecking building: " << gGlobals.getBuildingInfo(constructItem.buildingType).getType() << " for city: " << narrow(city.getCvCity()->getName());
        }
        else if (constructItem.processType != NO_PROCESS)
        {
            os << "\nChecking process: " << gGlobals.getProcessInfo(constructItem.processType).getType() << " for city: " << narrow(city.getCvCity()->getName());
        }
#endif
        ConstructItem selectedConstructItem(NO_BUILDING);

        const CvPlayer* pPlayer = player.getCvPlayer();
        const CvCity* pCity = city.getCvCity();
        
        if ((constructItem.buildingType != NO_BUILDING &&
            couldConstructBuilding(player, city, 2, player.getAnalysis()->getBuildingInfo(constructItem.buildingType), false) && 
            pCity->getFirstBuildingOrder(constructItem.buildingType) == -1) ||
            (constructItem.processType != NO_PROCESS))
        //if (pCity->canConstruct(constructItem.buildingType) && pCity->getFirstBuildingOrder(constructItem.buildingType) == -1)
        {
            const CityDataPtr& pCityData = city.getCityData();

            const int cityCount = pPlayer->getNumCities();
            const int maxResearchRate = player.getMaxResearchRate();
            const std::pair<int, int> rankAndMaxProduction = player.getCityRank(pCity->getIDInfo(), OUTPUT_PRODUCTION);
            const std::pair<int, int> rankAndMaxGoldOutput = player.getCityRank(pCity->getIDInfo(), OUTPUT_GOLD);
            const std::pair<int, int> rankAndMaxResearchOutput = player.getCityRank(pCity->getIDInfo(), OUTPUT_RESEARCH);

            const bool isWorldWonder = constructItem.buildingFlags & BuildingFlags::Building_World_Wonder;
            const bool isNationalWonder = constructItem.buildingFlags & BuildingFlags::Building_National_Wonder;

            // add check to avoid selecting wonders if city has rubbish production

            bool isProcess = constructItem.processType != NO_PROCESS;
            bool isBuildCulture = constructItem.processType != NO_PROCESS && constructItem.requiredTechs.empty() && (constructItem.economicFlags & EconomicFlags::Output_Culture);

            bool canBuildCulture = false;
            ProcessTypes cultureProcessType = getProcessType(COMMERCE_CULTURE);
            if (cultureProcessType != NO_PROCESS)
            {
                int depth = player.getTechResearchDepth((TechTypes)gGlobals.getProcessInfo(cultureProcessType).getTechPrereq());
                canBuildCulture = depth == 0;
            }

            bool needCulture = pCity->getCultureLevel() == 1 && pCity->getCommerceRate(COMMERCE_CULTURE) == 0;
            bool culturePressure = false;

            if (pCityData && pCityData->getNumUncontrolledPlots(true) > 0)
            {
                culturePressure = true;
            }
#ifdef ALTAI_DEBUG
            os << " rank = " << rankAndMaxProduction.first << " of: " << cityCount << ", max prod = " << rankAndMaxProduction.second;
            os << " need culture = " << needCulture << ", culture pressure = " << culturePressure;

            if (isBuildCulture) os << " is culture process ";
            os << " max research rate = " << maxResearchRate;
#endif
            if (needCulture && !isWorldWonder && !isNationalWonder && constructItem.economicFlags & EconomicFlags::Output_Culture)
            {
                // this can select build culture
                selectedConstructItem.economicFlags |= EconomicFlags::Output_Culture;
            }
            // don't build culture to relieve culture pressure
            else if (culturePressure && !isProcess && constructItem.economicFlags & EconomicFlags::Output_Culture)
            {
                if (!isNationalWonder && !isWorldWonder)
                {
                    selectedConstructItem.economicFlags |= EconomicFlags::Output_Culture;
                }
                else if (isWorldWonder || isNationalWonder)
                {
                    if (rankAndMaxProduction.first < cityCount / 2 && culturePressure)
                    {
                        // only select wonders with no other economic flags
                        if (!(constructItem.economicFlags & ~EconomicFlags::Output_Culture))
                        {
#ifdef ALTAI_DEBUG
                            os << " selecting: " << gGlobals.getBuildingInfo(constructItem.buildingType).getType() << " as culture, non economic (national) wonder";
#endif
                            selectedConstructItem.economicFlags |= EconomicFlags::Output_Culture;
                        }
                    }
                }
            }

            const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
            if (pCityData->getHappyCap() <= 0 && city.getMaxOutputs()[OUTPUT_FOOD] > 100 * foodPerPop * pCity->getPopulation())
            {
                if (constructItem.economicFlags & EconomicFlags::Output_Happy)
                {
                    selectedConstructItem.economicFlags |= EconomicFlags::Output_Happy;
                }
            }

            if (pCityData->getLostFood() > 0 && constructItem.economicFlags & EconomicFlags::Output_Health)
            {
                selectedConstructItem.economicFlags |= EconomicFlags::Output_Health;
            }

            if (maxResearchRate < 80)
            {
                if (constructItem.economicFlags & EconomicFlags::Output_Maintenance_Reduction && pCity->getMaintenance() > 3)
                {
                    selectedConstructItem.economicFlags |= EconomicFlags::Output_Maintenance_Reduction;
                }
            }

            if (maxResearchRate < 50)
            {
                bool bGold = constructItem.economicFlags & EconomicFlags::Output_Gold, bCommerce = constructItem.economicFlags & EconomicFlags::Output_Commerce;
                bool selected = false;

                if (rankAndMaxGoldOutput.first > 0 && (bGold || bCommerce))
                {
                    if (isNationalWonder)
                    {
                        if (rankAndMaxGoldOutput.first <= 3)
                        {
                            selected = true;
                        }
                    }
                    else if (rankAndMaxGoldOutput.first < cityCount / 3)
                    {
                        selected = true;
                    }
                    else if (constructItem.buildingType != NO_BUILDING)
                    {
                        if (rankAndMaxGoldOutput.second > city.getCvCity()->getProductionNeeded(constructItem.buildingType) / 10)  // todo - scale with game speed
                        {
                            selected = true;
                        }
                    }

                    if (selected)
                    {
                        if (bCommerce)
                        {
                            selectedConstructItem.economicFlags |= EconomicFlags::Output_Commerce;
                        }
                        if (bGold)
                        {
                            selectedConstructItem.economicFlags |= EconomicFlags::Output_Gold;
                        }
                    }
                }
            }
            else
            {
                bool bResearch = constructItem.economicFlags & EconomicFlags::Output_Research, bCommerce = constructItem.economicFlags & EconomicFlags::Output_Commerce;
                bool selected = false;

                if (rankAndMaxResearchOutput.first > 0 && (bResearch || bCommerce))
                {
                    if (isNationalWonder)
                    {
                        if (rankAndMaxResearchOutput.first <= 3)  // national wonder and in top three cities for research output
                        {
                            selected = true;
                        }
                    }
                    else if (rankAndMaxResearchOutput.first < 1 + cityCount / 3)
                    {
                        selected = true;
                    }
                    else if (constructItem.buildingType != NO_BUILDING)
                    {
                        if (rankAndMaxResearchOutput.second > city.getCvCity()->getProductionNeeded(constructItem.buildingType) / 10)  // todo - scale with game speed
                        {
                            selected = true;
                        }
                    }

                    if (selected)
                    {
                        if (bCommerce)
                        {
                            selectedConstructItem.economicFlags |= EconomicFlags::Output_Commerce;
                        }
                        if (bResearch)
                        {
                            selectedConstructItem.economicFlags |= EconomicFlags::Output_Research;
                        }
                    }
                }
            }

            if (constructItem.economicFlags & EconomicFlags::Output_Food)
            {
                selectedConstructItem.economicFlags |= EconomicFlags::Output_Food;
            }

            if (cityCount > 1 && rankAndMaxProduction.first > 0 && (rankAndMaxProduction.first < 1 + cityCount / 3 || rankAndMaxProduction.second > 20) &&
                (constructItem.economicFlags & EconomicFlags::Output_Production))
            {
                selectedConstructItem.economicFlags |= EconomicFlags::Output_Production;
            }

            if (isWorldWonder && rankAndMaxProduction.first > 0 && rankAndMaxProduction.first <= std::max<int>(1, cityCount / 3))
            {
                for (size_t i = 0, count = constructItem.positiveBonuses.size(); i < count; ++i)
                {
                    if (pCity->hasBonus(constructItem.positiveBonuses[i]))
                    {
                        // todo - separate flag for wonders?
                        selectedConstructItem.economicFlags |= EconomicFlags::Output_Production;
                        selectedConstructItem.positiveBonuses.push_back(constructItem.positiveBonuses[i]);
                    }
                }
            }

            bool bGold = constructItem.economicFlags & EconomicFlags::Output_Gold, bCommerce = constructItem.economicFlags & EconomicFlags::Output_Commerce;

            if (rankAndMaxGoldOutput.first > 0 && (rankAndMaxGoldOutput.first <= 2 || rankAndMaxGoldOutput.second > 20) && (bGold || bCommerce))
            {
                if (bCommerce)
                {
                    selectedConstructItem.economicFlags |= EconomicFlags::Output_Commerce;
                }
                if (bGold)
                {
                    selectedConstructItem.economicFlags |= EconomicFlags::Output_Gold;
                }
            }

            // nothing better to do... (maybe)
            if (selectedConstructItem.isEmpty() && isProcess && !isBuildCulture)
            {
                // TODO - select based on whether we have better gold or research multipliers
                // if research - better to build gold (generally), as can run slider higher
                // if gold, maybe better to build research and run lower slider
                selectedConstructItem.economicFlags |= constructItem.economicFlags;
            }
#ifdef ALTAI_DEBUG
            os << " => " << selectedConstructItem;
#endif
        }

        if (!selectedConstructItem.isEmpty())
        {
            selectedConstructItem.buildingType = constructItem.buildingType;
            selectedConstructItem.processType = constructItem.processType;
            std::vector<TechTypes> requiredTechs;

            if (selectedConstructItem.buildingType != NO_BUILDING)
            {
                requiredTechs = getRequiredTechs(player.getAnalysis()->getBuildingInfo(selectedConstructItem.buildingType));
            }
            else if (selectedConstructItem.processType != NO_PROCESS)
            {
                TechTypes techType = (TechTypes)gGlobals.getProcessInfo(selectedConstructItem.processType).getTechPrereq();
                if (techType != NO_TECH)
                {
                    requiredTechs.push_back(techType);
                }
            }

            for (size_t i = 0, count = requiredTechs.size(); i < count; ++i)
            {
                if (!player.getCivHelper()->hasTech(requiredTechs[i]))
                {
                    selectedConstructItem.requiredTechs.push_back(requiredTechs[i]);
                }
            }
        }

        return selectedConstructItem;
    }

    ConstructItem selectExpansionMilitaryBuildingTactics(const Player& player, const City& city, const ConstructItem& constructItem)
    {
#ifdef ALTAI_DEBUG
        // debug
        boost::shared_ptr<CivLog> pCivLog = CivLog::getLog(*player.getCvPlayer());
        std::ostream& os = pCivLog->getStream();
        if (constructItem.buildingType != NO_BUILDING)
        {
            os << "\nChecking building: " << gGlobals.getBuildingInfo(constructItem.buildingType).getType();
        }
#endif
        ConstructItem selectedConstructItem(NO_BUILDING);

        const CvPlayer* pPlayer = player.getCvPlayer();
        const CvCity* pCity = city.getCvCity();

        if (constructItem.buildingType != NO_BUILDING && 
            couldConstructBuilding(player, city, 2, player.getAnalysis()->getBuildingInfo(constructItem.buildingType), false) && pCity->getFirstBuildingOrder(constructItem.buildingType) == -1)
        //if (pCity->canConstruct(constructItem.buildingType) && pCity->getFirstBuildingOrder(constructItem.buildingType) == -1)
        {
            const CityDataPtr& pCityData = city.getCityData();

            const int cityCount = pPlayer->getNumCities();
            const std::pair<int, int> rankAndMaxProduction = player.getCityRank(pCity->getIDInfo(), OUTPUT_PRODUCTION);
            const bool isWorldWonder = constructItem.buildingFlags & BuildingFlags::Building_World_Wonder;

            if (gGlobals.getGame().isOption(GAMEOPTION_RAGING_BARBARIANS))
            {
                if (constructItem.militaryFlags & MilitaryFlags::Output_Experience)
                {
                    selectedConstructItem.militaryFlags |= MilitaryFlags::Output_Experience;
                }
            }

            if (cityCount > 2 && rankAndMaxProduction.first <= cityCount / 2)
            {
                // todo get military production rank, and flag cities we want to use for military production
                const bool isExperience = constructItem.militaryFlags & MilitaryFlags::Output_Experience, isProduction = constructItem.militaryFlags & MilitaryFlags::Output_Production;
                if (isExperience || isProduction)
                {
                    selectedConstructItem.militaryFlags |= isExperience ? MilitaryFlags::Output_Experience : MilitaryFlags::None;
                    selectedConstructItem.militaryFlags |= isProduction ? MilitaryFlags::Output_Production : MilitaryFlags::None;
                }
            }
        }

        if (!selectedConstructItem.isEmpty())
        {
            std::vector<TechTypes> requiredTechs = getRequiredTechs(player.getAnalysis()->getBuildingInfo(constructItem.buildingType));
            for (size_t i = 0, count = requiredTechs.size(); i < count; ++i)
            {
                if (!player.getCivHelper()->hasTech(requiredTechs[i]))
                {
                    selectedConstructItem.requiredTechs.push_back(requiredTechs[i]);
                }
            }
            selectedConstructItem.buildingType = constructItem.buildingType;
        }

        return selectedConstructItem;
    }
}