#include "./tictacs.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./city_building_tactics.h"
#include "./city_unit_tactics.h"
#include "./building_tactics_items.h"
#include "./tech_tactics.h"
#include "./building_tactics.h"
#include "./city_tactics.h"
#include "./unit_tactics.h"
#include "./project_tactics.h"
#include "./building_tactics_visitors.h"
#include "./unit_tactics_visitors.h"
#include "./tech_tactics_visitors.h"
#include "./tech_info_visitors.h"
#include "./unit_info.h"
#include "./unit_info_visitors.h"
#include "./building_info_visitors.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./city.h"
#include "./city_simulator.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./civ_log.h"
#include "./save_utils.h"

namespace AltAI
{
    namespace
    {
        struct TechTacticFinder
        {
            explicit TechTacticFinder(TechTypes techType_) : techType(techType_)
            {
            }

            bool operator() (const ResearchTech& other) const
            {
                return techType == other.techType;
            }

            TechTypes techType;
        };
    }

    void PlayerTactics::init()
    {
        possibleTechTactics_ = makeTechTactics(player);
        possibleUnitTactics_ = makeUnitTactics(player);
        possibleBuildingTactics_ = makeBuildingTactics(player);
        possibleProjectTactics_ = makeProjectTactics(player);

        selectCityTactics();

        debugTactics();
    }

    void PlayerTactics::deleteCity(const CvCity* pCity)
    {
        cityBuildingTacticsMap_.erase(pCity->getIDInfo());

        for (LimitedBuildingsTacticsMap::iterator iter(globalBuildingsTacticsMap_.begin()), endIter(globalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            iter->second->removeCityTactics(pCity->getIDInfo());
        }

        for (LimitedBuildingsTacticsMap::iterator iter(nationalBuildingsTacticsMap_.begin()), endIter(nationalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            iter->second->removeCityTactics(pCity->getIDInfo());
        }
    }

    void PlayerTactics::updateBuildingTactics()
    {
        possibleBuildingTactics_ = makeBuildingTactics(player);
        selectBuildingTactics();
    }

    void PlayerTactics::updateTechTactics()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nupdateTechTactics(): turn = " << gGlobals.getGame().getGameTurn() << "\n";
#endif
        cityImprovementTacticsMap_.clear();

        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const int depth = player.getTechResearchDepth((TechTypes)i);
            if (depth > 3)
            {
                continue;
            }

            std::list<ResearchTech>::iterator iter = std::find_if(possibleTechTactics_.begin(), possibleTechTactics_.end(), TechTacticFinder((TechTypes)i));

            if (depth == 0)
            {
                if (iter != possibleTechTactics_.end())
                {
                    possibleTechTactics_.erase(iter);
                }
            }
            else
            {
                if (iter == possibleTechTactics_.end())
                {
                    ResearchTech researchTech = makeTechTactic(player, (TechTypes)i);
                    if (researchTech.techType != NO_TECH)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nAdding tech tactic: " << researchTech;
#endif
                        possibleTechTactics_.push_back(researchTech);
                    }
                }
                else  // update tech depth
                {
                    iter->depth = depth;
                }

                const boost::shared_ptr<TechInfo>& pTechInfo = player.getAnalysis()->getTechInfo((TechTypes)i);
                if (techAffectsImprovements(pTechInfo))
                {
                    updateCityImprovementTactics(pTechInfo);
                }
            }
        }
        
        selectTechTactics();
    }

    void PlayerTactics::updateCityBuildingTactics(TechTypes techType)
    {
        boost::shared_ptr<TechInfo> pTechInfo = player.getAnalysis()->getTechInfo(techType);
        std::vector<BuildingTypes> obsoletedBuildings = getObsoletedBuildings(pTechInfo);

        for (size_t i = 0 , count = obsoletedBuildings.size(); i < count; ++i)
        {
            BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(obsoletedBuildings[i]).getBuildingClassType();
            const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = isNationalWonderClass(buildingClassType);

            if (isWorldWonder)
            {
                globalBuildingsTacticsMap_.erase(obsoletedBuildings[i]);
            }
            else if (isNationalWonder)
            {
                nationalBuildingsTacticsMap_.erase(obsoletedBuildings[i]);
            }
            else
            {
                CityIter iter(*player.getCvPlayer());
                while (CvCity* pCity = iter())
                {
                    cityBuildingTacticsMap_[pCity->getIDInfo()].erase(obsoletedBuildings[i]);
                }
            }
        }      

        const int lookAheadDepth = 2;

        for (size_t i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const int depth = player.getTechResearchDepth((TechTypes)i);
            if (techType == (TechTypes)i || depth > 0 && depth <= lookAheadDepth)
            {
                pTechInfo = player.getAnalysis()->getTechInfo((TechTypes)i);
                std::vector<BuildingTypes> possibleBuildings = getPossibleBuildings(pTechInfo);

                for (size_t i = 0, count = possibleBuildings.size(); i < count; ++i)
                {
                    boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(possibleBuildings[i]);

                    if (!pBuildingInfo)
                    {
                        continue;
                    }

                    if (!couldConstructSpecialBuilding(player, lookAheadDepth, pBuildingInfo))
                    {
                        continue;
                    }

                    BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(possibleBuildings[i]).getBuildingClassType();
                    const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = isNationalWonderClass(buildingClassType);

                    if (isWorldWonder)
                    {
                        if (!(gGlobals.getGame().isBuildingClassMaxedOut(buildingClassType)))
                        {
                            LimitedBuildingsTacticsMap::iterator iter = globalBuildingsTacticsMap_.find(possibleBuildings[i]);
                            if (iter == globalBuildingsTacticsMap_.end() || !iter->second || iter->second->empty())
                            {
                                globalBuildingsTacticsMap_[possibleBuildings[i]] = makeGlobalBuildingTactics(player, pBuildingInfo);
                            }
                            else
                            {
                                iter->second->updateDependencies(player);
                            }                            
                        }
                    }
                    else if (isNationalWonder)
                    {
                        if (!player.getCvPlayer()->isBuildingClassMaxedOut(buildingClassType))
                        {
                            LimitedBuildingsTacticsMap::iterator iter = nationalBuildingsTacticsMap_.find(possibleBuildings[i]);
                            if (iter == nationalBuildingsTacticsMap_.end() || !iter->second || iter->second->empty())
                            {
                                nationalBuildingsTacticsMap_[possibleBuildings[i]] = makeNationalBuildingTactics(player, pBuildingInfo);
                            }
                            else
                            {
                                iter->second->updateDependencies(player);
                            }
                        }
                    }
                    else
                    {
                        CityIter iter(*player.getCvPlayer());
                        while (CvCity* pCity = iter())
                        {       
                            CityBuildingTacticsList::iterator buildingsIter = cityBuildingTacticsMap_[pCity->getIDInfo()].find(possibleBuildings[i]);
                            if (buildingsIter == cityBuildingTacticsMap_[pCity->getIDInfo()].end())
                            {
                                if (couldConstructBuilding(player, player.getCity(pCity->getID()), 0, pBuildingInfo, true))
                                {
#ifdef ALTAI_DEBUG
                                    CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__
                                        << " Adding tactic for building: " << gGlobals.getBuildingInfo(possibleBuildings[i]).getType();
#endif
                                    cityBuildingTacticsMap_[pCity->getIDInfo()][possibleBuildings[i]] = makeCityBuildingTactics(player, player.getCity(pCity->getID()), pBuildingInfo);
                                }
                            }
                            else
                            {
#ifdef ALTAI_DEBUG
                                CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__
                                    << " Updating (new tech) tactic for building: " << gGlobals.getBuildingInfo(possibleBuildings[i]).getType();
#endif
                                buildingsIter->second->updateDependencies(player, pCity);
                            }
                        }
                    }
                }

                std::vector<ProcessTypes> possibleProcesses = getPossibleProcesses(pTechInfo);

                for (size_t i = 0, count = possibleProcesses.size(); i < count; ++i)
                {
                    processTacticsMap_[possibleProcesses[i]] = makeProcessTactics(player, possibleProcesses[i]);
                }
            }
        }
    }

    void PlayerTactics::updateCityImprovementTactics(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        CityIter iter(*player.getCvPlayer());
        while (CvCity* pCity = iter())
        {
            std::list<ICityImprovementTacticsPtr> cityImprovementTactics = makeCityBuildTactics(player, player.getCity(pCity->getID()), pTechInfo);
            std::copy(cityImprovementTactics.begin(), cityImprovementTactics.end(), std::back_inserter(cityImprovementTacticsMap_[pCity->getIDInfo()]));
        }
    }

    void PlayerTactics::updateCityBuildingTactics(IDInfo city, BuildingTypes buildingType, int newCount)
    {
        if (newCount > 0)
        {
            CityBuildingTacticsMap::iterator iter = cityBuildingTacticsMap_.find(city);
            if (iter != cityBuildingTacticsMap_.end())
            {
                iter->second.erase(buildingType);
                updateCityBuildingTacticsDependencies();
            }

            updateLimitedBuildingTacticsDependencies();
            updateGlobalBuildingTacticsDependencies();
        }
    }

    void PlayerTactics::updateCityBuildingTactics(IDInfo city)
    {
        CityBuildingTacticsMap::iterator iter = cityBuildingTacticsMap_.find(city);
        if (iter != cityBuildingTacticsMap_.end())
        {
            for (CityBuildingTacticsList::iterator buildingTacticsIter(iter->second.begin()), endIter(iter->second.end()); buildingTacticsIter != endIter; ++buildingTacticsIter)
            {
                buildingTacticsIter->second->update(player, player.getCity(city.iID).getCityData());
            }
        }

        for (LimitedBuildingsTacticsMap::iterator iter(nationalBuildingsTacticsMap_.begin()), endIter(nationalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            if (iter->second->getCityTactics(city))
            {
                iter->second->getCityTactics(city)->update(player, player.getCity(city.iID).getCityData());
            }
        }

        for (LimitedBuildingsTacticsMap::iterator iter(globalBuildingsTacticsMap_.begin()), endIter(globalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            if (iter->second->getCityTactics(city))
            {
                iter->second->getCityTactics(city)->update(player, player.getCity(city.iID).getCityData());
            }
        }
    }

    void PlayerTactics::updateCityReligionBuildingTactics(ReligionTypes religionType)
    {
        const int lookAheadDepth = 2;

        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            BuildingTypes buildingType = getPlayerVersion(player.getPlayerID(), (BuildingClassTypes)i);

            if (buildingType == NO_BUILDING)
            {
                continue;
            }
            
            const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);
            if (buildingInfo.getPrereqReligion() == religionType)
            {
                boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(buildingType);

                if (!pBuildingInfo)
                {
                    continue;
                }

                addBuildingTactics_(pBuildingInfo, NULL);
            }
        }
    }

    void PlayerTactics::addNewCityBuildingTactics(IDInfo city)
    {
        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            BuildingTypes buildingType = getPlayerVersion(player.getPlayerID(), (BuildingClassTypes)i);

            if (buildingType == NO_BUILDING)
            {
                continue;
            }

            boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(buildingType);

            if (!pBuildingInfo)
            {
                continue;
            }

            addBuildingTactics_(pBuildingInfo, getCity(city));
        }
    }

    void PlayerTactics::addNewCityUnitTactics(IDInfo city)
    {
        for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
        {
            UnitTypes unitType = getPlayerVersion(player.getPlayerID(), (UnitClassTypes)i);

            if (unitType == NO_UNIT)
            {
                continue;
            }

            boost::shared_ptr<UnitInfo> pUnitInfo = player.getAnalysis()->getUnitInfo(unitType);

            if (!pUnitInfo)
            {
                continue;
            }

            addUnitTactics_(pUnitInfo, getCity(city));
        }
    }

    void PlayerTactics::updateCityBuildingTacticsDependencies()
    {
        for (CityBuildingTacticsMap::iterator iter(cityBuildingTacticsMap_.begin()), endIter(cityBuildingTacticsMap_.end()); iter != endIter; ++iter)
        {
            CvCity* pCity = getCity(iter->first);

            if (pCity)
            {
                CityBuildingTacticsList::iterator buildingTacticsIter(iter->second.begin()), buildingTacticsEndIter(iter->second.end());
                while (buildingTacticsIter != buildingTacticsEndIter)
                {
                    buildingTacticsIter->second->updateDependencies(player, pCity);
                    ++buildingTacticsIter;
                }
            }
        }
    }

    void PlayerTactics::updateLimitedBuildingTacticsDependencies()
    {
        for (LimitedBuildingsTacticsMap::iterator iter(nationalBuildingsTacticsMap_.begin()), endIter(nationalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            iter->second->updateDependencies(player);
        }
    }

    void PlayerTactics::updateGlobalBuildingTacticsDependencies()
    {
        for (LimitedBuildingsTacticsMap::iterator iter(globalBuildingsTacticsMap_.begin()), endIter(globalBuildingsTacticsMap_.end()); iter != endIter; ++iter)
        {
            iter->second->updateDependencies(player);
        }
    }

    void PlayerTactics::eraseLimitedBuildingTactics(BuildingTypes buildingType)
    {
        BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType).getBuildingClassType();
        if (isWorldWonderClass(buildingClassType))
        {
            globalBuildingsTacticsMap_.erase(buildingType);
        }
        else if (isNationalWonderClass(buildingClassType))
        {
            nationalBuildingsTacticsMap_.erase(buildingType);
        }
    }

    void PlayerTactics::updateUnitTactics()
    {
        possibleUnitTactics_ = makeUnitTactics(player);
        selectUnitTactics();
    }

    void PlayerTactics::updateCityUnitTactics(TechTypes techType)
    {
        boost::shared_ptr<TechInfo> pTechInfo = player.getAnalysis()->getTechInfo(techType);
        // todo
        //std::vector<UnitTypes> obsoletedBuildings = getObsoletedUnits(pTechInfo);

        const int lookAheadDepth = 2;

        for (size_t i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const int depth = player.getTechResearchDepth((TechTypes)i);
            if (techType == (TechTypes)i || depth > 0 && depth <= lookAheadDepth)
            {
                pTechInfo = player.getAnalysis()->getTechInfo((TechTypes)i);
                std::vector<UnitTypes> possibleUnits = getPossibleUnits(pTechInfo);

                for (size_t i = 0, count = possibleUnits.size(); i < count; ++i)
                {
                    boost::shared_ptr<UnitInfo> pUnitInfo = player.getAnalysis()->getUnitInfo(possibleUnits[i]);

                    if (!pUnitInfo)
                    {
                        continue;
                    }

                    UnitTacticsMap::iterator iter = unitTacticsMap_.find(possibleUnits[i]);
                    if (iter == unitTacticsMap_.end() || !iter->second || iter->second->empty())
                    {
                        unitTacticsMap_[possibleUnits[i]] = makeUnitTactics(player, pUnitInfo);
                    }
                    else
                    {
                        iter->second->updateDependencies(player);
                    }
                }
            }
        }
    }

    void PlayerTactics::updateCityUnitTactics(IDInfo city)
    {
        for (UnitTacticsMap::iterator iter(unitTacticsMap_.begin()), endIter(unitTacticsMap_.end()); iter != endIter; ++iter)
        {
            iter->second->getCityTactics(city)->update(player, player.getCity(city.iID).getCityData());
        }
    }

    void PlayerTactics::updateProjectTactics()
    {
        possibleProjectTactics_ = makeProjectTactics(player);
        selectProjectTactics();
    }

    void PlayerTactics::updateFirstToTechTactics(TechTypes techType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        std::list<ResearchTech>::iterator iter(possibleTechTactics_.begin()), iterEnd(possibleTechTactics_.end());
        while (iter != iterEnd)
        {
            if (iter->techType == techType)
            {
                if (iter->techFlags & TechFlags::Free_Tech)
                {
                    iter->techFlags &= ~TechFlags::Free_Tech;
#ifdef ALTAI_DEBUG
                    os << "\n(updateFirstToTechTactics) Removing free tech flag from research tech tactic: " << *iter;
#endif
                }

                if (iter->techFlags & TechFlags::Free_GP)
                {
                    iter->techFlags &= ~TechFlags::Free_GP;
#ifdef ALTAI_DEBUG
                    os << "\n(updateFirstToTechTactics) Removing free GP flag from research tech tactic: " << *iter;
#endif
                }

                if (iter->isEmpty())
                {
#ifdef ALTAI_DEBUG
                    os << "\n(updateFirstToTechTactics) Removing research tech tactic: " << *iter;
#endif
                    possibleTechTactics_.erase(iter++);
                    continue;
                }
            }
            ++iter;
        }
    }

    ResearchTech PlayerTactics::getResearchTech(TechTypes ignoreTechType)
    {
        if (selectedTechTactics_.empty() || gGlobals.getGame().getGameTurn() == 0)
        {
            selectTechTactics();
        }
        return AltAI::getResearchTech(*this, ignoreTechType);
    }

    ResearchTech PlayerTactics::getResearchTechData(TechTypes techType) const
    {
        for (std::list<ResearchTech>::const_iterator ci(selectedTechTactics_.begin()), ciEnd(selectedTechTactics_.end()); ci != ciEnd; ++ci)
        {
            if (ci->techType == techType)
            {
                return *ci;
            }
        }
        return ResearchTech();
    }

    ConstructItem PlayerTactics::getBuildItem(const City& city)
    {
        //if (selectedTechTactics_.empty() || gGlobals.getGame().getGameTurn() == 0)
        //{
        //    updateTechTactics();
        //}
        updateUnitTactics();
        //updateBuildingTactics();
        debugTactics();
        return AltAI::getConstructItem(*this, city);
    }

    void PlayerTactics::selectTechTactics()
    {
        selectedTechTactics_.clear();

        std::list<ResearchTech>::iterator iter(possibleTechTactics_.begin());
        while (iter != possibleTechTactics_.end())
        {
            ResearchTech selectedTech = selectReligionTechTactics(player, *iter);
            if (selectedTech.techType != NO_TECH)
            {
                selectedTechTactics_.push_back(selectedTech);
            }

            selectedTech = selectWorkerTechTactics(player, *iter);
            if (selectedTech.techType != NO_TECH)
            {
                selectedTechTactics_.push_back(selectedTech);
            }

            selectedTech = selectExpansionTechTactics(player, *iter);
            if (selectedTech.techType != NO_TECH)
            {
                selectedTechTactics_.push_back(selectedTech);
            }
            ++iter;
        }

        //debugTactics();
    }

    void PlayerTactics::selectUnitTactics()
    {
        selectedUnitTactics_.clear();

        ConstructListIter iter(possibleUnitTactics_.begin());
        while (iter != possibleUnitTactics_.end())
        {
            ConstructItem selectedUnit = selectExpansionUnitTactics(player, *iter);
            if (selectedUnit.unitType != NO_UNIT)
            {
                selectedUnitTactics_.push_back(selectedUnit);
            }
            ++iter;
        }

        //debugTactics();
    }

    void PlayerTactics::selectBuildingTactics()
    {
        const CvPlayer* pPlayer = player.getCvPlayer();

        CityIter cityIter(*pPlayer);
        CvCity* pCity;
        while (pCity = cityIter())
        {
            selectBuildingTactics(player.getCity(pCity->getID()));
        }
    }

    void PlayerTactics::selectBuildingTactics(const City& city)
    {
        selectedCityBuildingTactics_[city.getCvCity()->getIDInfo()].clear();
        ConstructListIter iter(possibleBuildingTactics_.begin());
        while (iter != possibleBuildingTactics_.end())
        {
            ConstructItem selectedBuilding = selectExpansionBuildingTactics(player, city, *iter);
            if (selectedBuilding.buildingType != NO_BUILDING || selectedBuilding.processType != NO_PROCESS)
            {
                selectedCityBuildingTactics_[city.getCvCity()->getIDInfo()].push_back(selectedBuilding);
            }

            selectedBuilding = selectExpansionMilitaryBuildingTactics(player, city, *iter);
            if (selectedBuilding.buildingType != NO_BUILDING)
            {
                selectedCityBuildingTactics_[city.getCvCity()->getIDInfo()].push_back(selectedBuilding);
            }

            ++iter;
        }

        //debugTactics();           
    }

    void PlayerTactics::selectProjectTactics()
    {
        const CvPlayer* pPlayer = player.getCvPlayer();

        CityIter cityIter(*pPlayer);
        CvCity* pCity;
        while (pCity = cityIter())
        {
            selectProjectTactics(player.getCity(pCity->getID()));
        }
    }

    void PlayerTactics::selectProjectTactics(const City& city)
    {
        selectedCityProjectTactics_[city.getCvCity()->getIDInfo()].clear();
        ConstructListIter iter(possibleProjectTactics_.begin());
        while (iter != possibleProjectTactics_.end())
        {
            ConstructItem selectedProject = AltAI::selectProjectTactics(player, city, *iter);
            if (selectedProject.projectType != NO_PROJECT)
            {
                ConstructList& constructList = selectedCityProjectTactics_[city.getCvCity()->getIDInfo()];

                ConstructList::iterator listIter = std::find_if(constructList.begin(), constructList.end(), ConstructItemFinder(selectedProject));

                if (listIter != constructList.end())
                {
                    listIter->merge(selectedProject);
                }
                else
                {
                    constructList.push_back(selectedProject);
                }
            }

            ++iter;
        }

        //debugTactics();           
    }

    void PlayerTactics::selectCityTactics()
    {
        typedef std::map<YieldTypes, std::multimap<int, IDInfo, std::greater<int> > > CityYieldMap;
        CityYieldMap cityYieldMap;

        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();
        YieldPriority yieldP = makeYieldP(YIELD_FOOD);
        YieldWeights yieldW = makeYieldW(0, 2, 1);
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        int bestFoodDelta = 0;
        IDInfo bestCity;

        CityIter iter(*player.getCvPlayer());
        while (CvCity* pCity = iter())
        {
            const CityImprovementManager& improvementManager = pMapAnalysis->getImprovementManager(pCity->getIDInfo());

            const int size = pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel();
            PlotYield projectedYield = improvementManager.getProjectedYield(size, yieldP, yieldW);

            int thisFoodDelta = projectedYield[YIELD_FOOD] - foodPerPop * size;
            if (thisFoodDelta > bestFoodDelta)
            {
                bestFoodDelta = thisFoodDelta;
                bestCity = pCity->getIDInfo();
            }
#ifdef ALTAI_DEBUG
            os << "\nCity: " << narrow(pCity->getName()) << ", projected yield = " << projectedYield << " for size: " << size;
#endif
            CityImprovementManager testImprovementManager(pCity->getIDInfo(), true);
            for (int i = 0; i < NUM_YIELD_TYPES; ++i)
            {
                testImprovementManager.calcImprovements(std::vector<YieldTypes>(1, (YieldTypes)i), size, 3);
                PlotYield projectedYield = testImprovementManager.getProjectedYield(size, yieldP, yieldW);
#ifdef ALTAI_DEBUG
                os << "\nCity: " << narrow(pCity->getName()) << ", projected yield = " << projectedYield << " for yieldtype: " << i;
#endif
                cityYieldMap[(YieldTypes)i].insert(std::make_pair(projectedYield[i], pCity->getIDInfo()));
            }
        }

        if (bestCity.eOwner != NO_PLAYER)
        {
#ifdef ALTAI_DEBUG
            os << "\nBest city = " << narrow(getCity(bestCity)->getName()) << " with delta = " << bestFoodDelta;
#endif
        }

#ifdef ALTAI_DEBUG
        for (CityYieldMap::const_iterator ci(cityYieldMap.begin()), ciEnd(cityYieldMap.end()); ci != ciEnd; ++ci)
        {
            os << "\nYieldtype : " << ci->first;
            for (std::multimap<int, IDInfo, std::greater<int> >::const_iterator mi(ci->second.begin()), miEnd(ci->second.end()); mi != miEnd; ++mi)
            {
                os << " " << narrow(getCity(mi->second)->getName()) << " = " << mi->first;
            }
        }
#endif
    }

    void PlayerTactics::write(FDataStreamBase* pStream) const
    {
        writeComplexList(pStream, possibleTechTactics_);
        writeComplexList(pStream, selectedTechTactics_);
        writeComplexList(pStream, possibleUnitTactics_);
        writeComplexList(pStream, selectedUnitTactics_);
        writeComplexList(pStream, possibleBuildingTactics_);

        pStream->Write(selectedCityBuildingTactics_.size());
        for (std::map<IDInfo, ConstructList>::const_iterator ci(selectedCityBuildingTactics_.begin()), ciEnd(selectedCityBuildingTactics_.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            writeComplexList(pStream, ci->second);
        }

        pStream->Write(cityBuildingTacticsMap_.size());
        for (CityBuildingTacticsMap::const_iterator ci(cityBuildingTacticsMap_.begin()), ciEnd(cityBuildingTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            pStream->Write(ci->second.size());
            for (CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
            {
                pStream->Write(li->first);
                li->second->write(pStream);
            }
        }

        pStream->Write(processTacticsMap_.size());
        for (ProcessTacticsMap::const_iterator ci(processTacticsMap_.begin()), ciEnd(processTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            ci->second->write(pStream);
        }

        pStream->Write(globalBuildingsTacticsMap_.size());
        for (LimitedBuildingsTacticsMap::const_iterator ci(globalBuildingsTacticsMap_.begin()), ciEnd(globalBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            ci->second->write(pStream);
        }

        pStream->Write(nationalBuildingsTacticsMap_.size());
        for (LimitedBuildingsTacticsMap::const_iterator ci(nationalBuildingsTacticsMap_.begin()), ciEnd(nationalBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            ci->second->write(pStream);
        }

        pStream->Write(cityImprovementTacticsMap_.size());
        for (CityImprovementTacticsMap::const_iterator ci(cityImprovementTacticsMap_.begin()), ciEnd(cityImprovementTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            pStream->Write(ci->second.size());
            for (CityImprovementTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
            {
                (*li)->write(pStream);
            }
        }
    }

    void PlayerTactics::read(FDataStreamBase* pStream)
    {
        readComplexList(pStream, possibleTechTactics_);
        readComplexList(pStream, selectedTechTactics_);
        readComplexList(pStream, possibleUnitTactics_);
        readComplexList(pStream, selectedUnitTactics_);
        readComplexList(pStream, possibleBuildingTactics_);

        selectedCityBuildingTactics_.clear();
        size_t size;
        pStream->Read(&size);
        for (size_t i = 0; i < size; ++i)
        {
            IDInfo city;
            city.read(pStream);
            ConstructList constructList;
            readComplexList(pStream, constructList);

            selectedCityBuildingTactics_.insert(std::make_pair(city, constructList));
        }

        cityBuildingTacticsMap_.clear();
        size_t cityBuildingTacticsMapSize;
        pStream->Read(&cityBuildingTacticsMapSize);
        for (size_t i = 0; i < cityBuildingTacticsMapSize; ++i)
        {
            IDInfo city;
            city.read(pStream);

            size_t tacticCount;
            pStream->Read(&tacticCount);
            for (size_t j = 0; j < tacticCount; ++j)
            {
                BuildingTypes buildingType;
                pStream->Read((int*)&buildingType);

                cityBuildingTacticsMap_[city][buildingType] = ICityBuildingTactics::factoryRead(pStream);
            }
        }

        processTacticsMap_.clear();
        size_t processTacticsMapSize;
        pStream->Read(&processTacticsMapSize);
        for (size_t i = 0; i < processTacticsMapSize; ++i)
        {
            ProcessTypes processType;
            pStream->Read((int*)&processType);
            processTacticsMap_[processType] = IProcessTactics::factoryRead(pStream);
        }

        globalBuildingsTacticsMap_.clear();
        size_t globalBuildingsTacticsMapSize;
        pStream->Read(&globalBuildingsTacticsMapSize);
        for (size_t i = 0; i < globalBuildingsTacticsMapSize; ++i) 
        {
            BuildingTypes buildingType;
            pStream->Read((int*)&buildingType);
            globalBuildingsTacticsMap_[buildingType] = IGlobalBuildingTactics::factoryRead(pStream);
        }

        nationalBuildingsTacticsMap_.clear();
        size_t nationalBuildingsTacticsMapSize;
        pStream->Read(&nationalBuildingsTacticsMapSize);
        for (size_t i = 0; i < nationalBuildingsTacticsMapSize; ++i) 
        {
            BuildingTypes buildingType;
            pStream->Read((int*)&buildingType);
            nationalBuildingsTacticsMap_[buildingType] = IGlobalBuildingTactics::factoryRead(pStream);
        }

        cityImprovementTacticsMap_.clear();
        size_t cityImprovementTacticsMapSize;
        pStream->Read(&cityImprovementTacticsMapSize);
        for (size_t i = 0; i < cityImprovementTacticsMapSize; ++i) 
        {
            IDInfo city;
            city.read(pStream);

            size_t tacticCount;
            pStream->Read(&tacticCount);
            for (size_t j = 0; j < tacticCount; ++j)
            {
                cityImprovementTacticsMap_[city].push_back(ICityImprovementTactics::factoryRead(pStream));
            }
        }
    }

    std::map<IDInfo, std::vector<BuildingTypes> > PlayerTactics::getBuildingsCityCanAssistWith(IDInfo city) const
    {
        std::map<IDInfo, std::vector<BuildingTypes> > buildingsCityCanAssistWith;

        const CvCity* pCity = getCity(city);
        if (!pCity)
        {
            return buildingsCityCanAssistWith;
        }        

        for (CityBuildingTacticsMap::const_iterator ci(cityBuildingTacticsMap_.begin()), ciEnd(cityBuildingTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pTargetCity = getCity(ci->first);
            if (pTargetCity && pTargetCity != pCity)
            {
                for (CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                {
                    const std::vector<IDependentTacticPtr>& dependentTactics = li->second->getDependencies();
                    if (!dependentTactics.empty())
                    {
                        bool conditionsAllMet = true;
                        for (size_t i = 0, count = dependentTactics.size(); i < count; ++i)
                        {
                            if (dependentTactics[i]->required(pCity))
                            {
                                conditionsAllMet = false;
                                break;
                            }
                        }

                        if (conditionsAllMet)
                        {
                            buildingsCityCanAssistWith[ci->first].push_back(li->first);
                        }
                    }
                }
            }
        }

        return buildingsCityCanAssistWith;
    }

    std::map<BuildingTypes, std::vector<BuildingTypes> > PlayerTactics::getPossibleDependentBuildings(IDInfo city) const
    {
        std::map<BuildingTypes, std::vector<BuildingTypes> > possibleDependentBuildingsMap;

        CityBuildingTacticsMap::const_iterator ci = cityBuildingTacticsMap_.find(city);
        if (ci != cityBuildingTacticsMap_.end())
        {
            std::set<BuildingTypes> dependentBuildings;
            for (CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
            {
                const std::vector<IDependentTacticPtr>& dependentTactics = li->second->getDependencies();
                if (dependentTactics.size() == 1)
                {
                    const std::pair<BuildQueueTypes, int> buildItem = dependentTactics[0]->getBuildItem();

                    if (buildItem.first == BuildingItem && ci->second.find((BuildingTypes)buildItem.second) != ci->second.end())
                    {
                        possibleDependentBuildingsMap[(BuildingTypes)buildItem.second].push_back(li->second->getBuildingType());
                    }
                }
            }
        }

        return possibleDependentBuildingsMap;
    }

    void PlayerTactics::debugTactics()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\n\nPlayerTactics::debugTactics():\nPossible tactics: (turn = " << gGlobals.getGame().getGameTurn() << ") ";

        os << "\nPossible tech tactics:\n";
        for (std::list<ResearchTech>::const_iterator ci(possibleTechTactics_.begin()), ciEnd(possibleTechTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nPossible building tactics:\n";
        for (ConstructListConstIter ci(possibleBuildingTactics_.begin()), ciEnd(possibleBuildingTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nPossible project tactics:\n";
        for (ConstructListConstIter ci(possibleProjectTactics_.begin()), ciEnd(possibleProjectTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nSelected building tactics:\n";
        for (std::map<IDInfo, ConstructList >::const_iterator ci(selectedCityBuildingTactics_.begin()), ciEnd(selectedCityBuildingTactics_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity)
            {
                os << "\nCity: " << narrow(pCity->getName()) << "\n";
            }

            for (ConstructListConstIter ci2(ci->second.begin()), ci2End(ci->second.end()); ci2 != ci2End; ++ci2)
            {
                os << *ci2 << "\n";
            }
        }

        os << "\nSelected project tactics:\n";
        for (std::map<IDInfo, ConstructList >::const_iterator ci(selectedCityProjectTactics_.begin()), ciEnd(selectedCityProjectTactics_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity)
            {
                os << "\nCity: " << narrow(pCity->getName()) << "\n";
            }

            for (ConstructListConstIter ci2(ci->second.begin()), ci2End(ci->second.end()); ci2 != ci2End; ++ci2)
            {
                os << *ci2 << "\n";
            }
        }

        os << "\nPossible unit tactics:\n";
        for (ConstructListConstIter ci(possibleUnitTactics_.begin()), ciEnd(possibleUnitTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nSelected tech tactics:\n";
        for (std::list<ResearchTech>::const_iterator ci(selectedTechTactics_.begin()), ciEnd(selectedTechTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nSelected general unit tactics:\n";
        for (ConstructListConstIter ci(selectedUnitTactics_.begin()), ciEnd(selectedUnitTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }
        os << "\n";

        os << "\nCity building tactics:";
        for (CityBuildingTacticsMap::iterator ci(cityBuildingTacticsMap_.begin()), ciEnd(cityBuildingTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity)
            {
                os << "\nCity: " << narrow(pCity->getName());
                const City& city = player.getCity(pCity->getID());

                TotalOutput base = city.getCurrentOutputProjection().getOutput();

                for (CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                {
                    li->second->debug(os);
                    li->second->update(player, city.getCityData());

                    const ProjectionLadder& ladder = li->second->getProjection();
                    if (ladder.buildings.empty())
                    {
                        os << "\nBuilding: " << gGlobals.getBuildingInfo(li->second->getBuildingType()).getType() << " not built.";
                    }
                    else
                    {
                        os << "\nBuilding: " << gGlobals.getBuildingInfo(li->second->getBuildingType()).getType() << " delta = " << ladder.getOutput() - base;
                    }
                }
            }
            else
            {
                os << "\nMissing city?";
            }
        }

        os << "\nCities which satisfy requirements:";
        {
            CityIter iter(*player.getCvPlayer());
            while (CvCity* pCity = iter())
            {                
                std::map<IDInfo, std::vector<BuildingTypes> > buildingsCityCanAssistWith = getBuildingsCityCanAssistWith(pCity->getIDInfo());

                os << "\nCity: " << narrow(pCity->getName()) << " can assist: ";
                for (std::map<IDInfo, std::vector<BuildingTypes> >::const_iterator ci(buildingsCityCanAssistWith.begin()), ciEnd(buildingsCityCanAssistWith.end()); ci != ciEnd; ++ci)
                {
                    os << "\n\tcity: " << narrow(getCity(ci->first)->getName()) << " buildings: ";
                    for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                    {
                        if (i > 0) os << ", ";
                        os << gGlobals.getBuildingInfo(ci->second[i]).getType();
                    }
                }
            }
        }

        os << "\nDependent buildings:";
        {
            CityIter iter(*player.getCvPlayer());
            while (CvCity* pCity = iter())
            {                
                std::map<BuildingTypes, std::vector<BuildingTypes> > buildingsMap = getPossibleDependentBuildings(pCity->getIDInfo());

                os << "\nCity: " << narrow(pCity->getName()) << " deps: ";
                for (std::map<BuildingTypes, std::vector<BuildingTypes> >::const_iterator ci(buildingsMap.begin()), ciEnd(buildingsMap.end()); ci != ciEnd; ++ci)
                {
                    os << gGlobals.getBuildingInfo(ci->first).getType() << " = ";
                    for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                    {
                        if (i > 0) os << ", ";
                        os << gGlobals.getBuildingInfo(ci->second[i]).getType();
                    }
                }
            }
        }

        os << "\nLimited building tactics:\nWorld Wonders:\n";
        for (LimitedBuildingsTacticsMap::const_iterator ci(globalBuildingsTacticsMap_.begin()), ciEnd(globalBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nBuilding: " << gGlobals.getBuildingInfo(ci->first).getType();
            ci->second->update(player);
            ci->second->debug(os);

            int firstBuiltTurn = MAX_INT;
            IDInfo firstBuiltCity;

            CityIter iter(*player.getCvPlayer());
            while (CvCity* pCity = iter())
            {                
                const City& city = player.getCity(pCity->getID());
                ICityBuildingTacticsPtr cityTactics = ci->second->getCityTactics(pCity->getIDInfo());
                if (cityTactics)
                {
                    const ProjectionLadder& ladder = cityTactics->getProjection();
                    if (!ladder.buildings.empty())
                    {
                        if (ladder.buildings[0].first < firstBuiltTurn)
                        {
                            firstBuiltTurn = ladder.buildings[0].first;
                            firstBuiltCity = pCity->getIDInfo();
                        }
                    }
                }
            }
            if (firstBuiltCity.eOwner != NO_PLAYER)
            {
                os << "\nFirst built in: " << narrow(getCity(firstBuiltCity)->getName()) << " turn = " << firstBuiltTurn;
            }

            if (firstBuiltCity.eOwner != NO_PLAYER)
            {
                boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(ci->first);
                std::vector<IProjectionEventPtr> possibleEvents = getPossibleEvents(player, pBuildingInfo, firstBuiltTurn);

                ICityBuildingTacticsPtr cityTactics = ci->second->getCityTactics(firstBuiltCity);
                const CvCity* pBuiltCity = getCity(firstBuiltCity);
                {
                    TotalOutput globalDelta;
                    const City& city = player.getCity(pBuiltCity->getID());
                    TotalOutput base = city.getCurrentOutputProjection().getOutput();

                    //cityTactics->debug(os);
                    globalDelta += cityTactics->getProjection().getOutput() - base;
                    os << "\nDelta = " << cityTactics->getProjection().getOutput() - base;

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
                            //otherCityProjection.debug(os);

                            globalDelta += otherCityProjection.getOutput() - otherBase;
                            os << "\nDelta for city: " << narrow(pOtherCity->getName()) << " = " << otherCityProjection.getOutput() - otherBase;
                        }
                    }
                    os << "\nGlobal base delta = " << globalDelta;
                }

                for (size_t i = 0, count = possibleEvents.size(); i < count; ++i)
                {
                    TotalOutput globalDelta;

                    CityIter iter(*player.getCvPlayer());
                    while (CvCity* pCity = iter())
                    {
                        const City& thisCity = player.getCity(pCity->getID());
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
                        os << "\nDelta for city: " << narrow(pCity->getName()) << " = " << thisCityProjection.getOutput() - thisBase;

                    }
                    os << "\nGlobal delta = " << globalDelta << " for event: ";
                    possibleEvents[i]->debug(os);
                }
            }
        }

        os << "\nNational wonders:\n";
        for (LimitedBuildingsTacticsMap::const_iterator ci(nationalBuildingsTacticsMap_.begin()), ciEnd(nationalBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nBuilding: " << gGlobals.getBuildingInfo(ci->first).getType();
            ci->second->update(player);
            ci->second->debug(os);
        }

        os << "\nUnit tactics:\n";
        for (UnitTacticsMap::const_iterator ci(unitTacticsMap_.begin()), ciEnd(unitTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            if (ci->first != NO_UNIT)
            {
                os << "\nUnit: " << gGlobals.getUnitInfo(ci->first).getType();
                if (ci->second)
                {
                    ci->second->update(player);
                    ci->second->debug(os);
                }
            }
            else
            {
                os << "\nUnit: NO_UNIT";
            }
        }

        os << "\nProcess tactics:";
        for (ProcessTacticsMap::const_iterator ci(processTacticsMap_.begin()), ciEnd(processTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nProcess: " << gGlobals.getProcessInfo(ci->first).getType();
            ci->second->debug(os);
            CityIter iter(*player.getCvPlayer());
            while (CvCity* pCity = iter())
            {
                ProjectionLadder projection = ci->second->getProjection(pCity->getIDInfo());
                const City& city = player.getCity(pCity->getID());
                os << "\nProcess output for city: " << narrow(pCity->getName()) << " = " << projection.getProcessOutput();
            }
        }

        os << "\nCity improvement tactics:";
        for (CityImprovementTacticsMap::iterator ci(cityImprovementTacticsMap_.begin()), ciEnd(cityImprovementTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity)
            {
                os << "\nCity: " << narrow(pCity->getName());
                const City& city = player.getCity(pCity->getID());
                TotalOutput base = city.getCurrentOutputProjection().getOutput();

                for (CityImprovementTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                {
                    (*li)->debug(os);
                    (*li)->update(player, city.getCityData());

                    os << "\nDelta = " << (*li)->getProjection().getOutput() - base;
                }            
            }
            else
            {
                os << "\nMissing city?";
            }
        }
#endif
    }

    void PlayerTactics::addBuildingTactics_(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, CvCity* pCity)
    {
        const BuildingTypes buildingType = pBuildingInfo->getBuildingType();
        const BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType).getBuildingClassType();
        const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = isNationalWonderClass(buildingClassType);
        const int lookAheadDepth = 2;        

        if (!couldConstructSpecialBuilding(player, lookAheadDepth, pBuildingInfo))
        {
            return;
        }

        if (isWorldWonder)
        {
            if (!(gGlobals.getGame().isBuildingClassMaxedOut(buildingClassType)))
            {
                if (pCity)
                {
                    const City& city = player.getCity(pCity->getID());
                    if (couldConstructBuilding(player, city, lookAheadDepth, pBuildingInfo, true))
                    {
                        LimitedBuildingsTacticsMap::iterator iter = globalBuildingsTacticsMap_.find(buildingType);
                        if (iter == globalBuildingsTacticsMap_.end())
                        {
                            iter = globalBuildingsTacticsMap_.insert(std::make_pair(buildingType, ILimitedBuildingTacticsPtr(new GlobalBuildingTactic(buildingType)))).first;
                        }

                        iter->second->addCityTactic(pCity->getIDInfo(), makeCityBuildingTactics(player, city, pBuildingInfo));
                    }
                }
                else
                {
                    globalBuildingsTacticsMap_[buildingType] = makeGlobalBuildingTactics(player, pBuildingInfo);
                }
            }
        }
        else if (isNationalWonder)
        {
            if (!player.getCvPlayer()->isBuildingClassMaxedOut(buildingClassType))
            {
                if (pCity)
                {
                    const City& city = player.getCity(pCity->getID());
                    if (couldConstructBuilding(player, city, lookAheadDepth, pBuildingInfo, true))
                    {
                        LimitedBuildingsTacticsMap::iterator iter = nationalBuildingsTacticsMap_.find(buildingType);

                        if (iter == nationalBuildingsTacticsMap_.end())
                        {
                            iter = nationalBuildingsTacticsMap_.insert(std::make_pair(buildingType, ILimitedBuildingTacticsPtr(new NationalBuildingTactic(buildingType)))).first;
                        }

                        iter->second->addCityTactic(pCity->getIDInfo(), makeCityBuildingTactics(player, city, pBuildingInfo));
                    }
                }
                else
                {
                    nationalBuildingsTacticsMap_[buildingType] = makeNationalBuildingTactics(player, pBuildingInfo);
                }
            }
        }
        else
        {
            if (pCity)
            {
                if (!couldConstructBuilding(player, player.getCity(pCity->getID()), lookAheadDepth, pBuildingInfo, true))
                {
                    return;
                }

#ifdef ALTAI_DEBUG
                CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
                cityBuildingTacticsMap_[pCity->getIDInfo()][buildingType] = makeCityBuildingTactics(player, player.getCity(pCity->getID()), pBuildingInfo);
            }
            else
            {
                CityIter iter(*player.getCvPlayer());
                while (CvCity* pCity = iter())
                {
                    const City& city = player.getCity(pCity->getID());
                    if (!couldConstructBuilding(player, city, lookAheadDepth, pBuildingInfo, true))
                    {
                        continue;
                    }
                
#ifdef ALTAI_DEBUG
                    CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
                    cityBuildingTacticsMap_[pCity->getIDInfo()][buildingType] = makeCityBuildingTactics(player, city, pBuildingInfo);
                }
            }
        }
    }

    void PlayerTactics::addUnitTactics_(const boost::shared_ptr<UnitInfo>& pUnitInfo, CvCity* pCity)
    {
        const UnitTypes unitType = pUnitInfo->getUnitType();
        const UnitClassTypes unitClassType = (UnitClassTypes)gGlobals.getUnitInfo(unitType).getUnitClassType();
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        if (unitInfo.getProductionCost() < 0)
        {
            return;
        }

        const int lookAheadDepth = 2;

        if (!couldConstructUnit(player, lookAheadDepth, pUnitInfo, true))
        {
            return;
        }                

        if (pCity)
        {
            const City& city = player.getCity(pCity->getID());
            
            // TODO - add city to check
            if (couldConstructUnit(player, lookAheadDepth, pUnitInfo, true))
            {
                UnitTacticsMap::iterator iter = unitTacticsMap_.find(unitType);

                if (iter == unitTacticsMap_.end())
                {
                    iter = unitTacticsMap_.insert(std::make_pair(unitType, IUnitTacticsPtr(new UnitTactic(unitType)))).first;
                }

                iter->second->addCityTactic(pCity->getIDInfo(), makeCityUnitTactics(player, city, pUnitInfo));
            }
        }
        else
        {
            unitTacticsMap_[unitType] = makeUnitTactics(player, pUnitInfo);
        }
    }
}