#include "./tictacs.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./building_tactics_items.h"
#include "./tech_tactics.h"
#include "./building_tactics.h"
#include "./city_tactics.h"
#include "./unit_tactics.h"
#include "./project_tactics.h"
#include "./building_tactics_visitors.h"
#include "./tech_tactics_visitors.h"
#include "./tech_info_visitors.h"
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
        const int lookAheadDepth = 2;

        for (size_t i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const int depth = player.getTechResearchDepth((TechTypes)i);
            if (techType == (TechTypes)i || depth > 0 && depth <= lookAheadDepth)
            {
                boost::shared_ptr<TechInfo> pTechInfo = player.getAnalysis()->getTechInfo((TechTypes)i);
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

                    ReligionTypes religionType = (ReligionTypes)gGlobals.getBuildingInfo(possibleBuildings[i]).getPrereqReligion();
                    if (religionType != NO_RELIGION)
                    {
                        if (player.getCvPlayer()->getHasReligionCount(religionType) == 0)
                        {
                            continue;
                        }
                    }

                    BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(possibleBuildings[i]).getBuildingClassType();
                    const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = isNationalWonderClass(buildingClassType);

                    // rebuild these each time - no updateDeps yet
                    if (isWorldWonder)
                    {
                        if (!(gGlobals.getGame().isBuildingClassMaxedOut(buildingClassType)))
                        {
                            globalBuildingsTacticsMap_[possibleBuildings[i]] = makeGlobalBuildingTactics(player, pBuildingInfo);
                        }
                    }
                    else if (isNationalWonder)
                    {
                        if (!player.getCvPlayer()->isBuildingClassMaxedOut(buildingClassType))
                        {
                            nationalBuildingsTacticsMap_[possibleBuildings[i]] = makeNationalBuildingTactics(player, pBuildingInfo);
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
                                if (couldConstructBuilding(player, player.getCity(pCity->getID()), 0, pBuildingInfo))
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

                if (!couldConstructSpecialBuilding(player, lookAheadDepth, pBuildingInfo))
                {
                    continue;
                }

                const bool isWorldWonder = isWorldWonderClass((BuildingClassTypes)i), isNationalWonder = isNationalWonderClass((BuildingClassTypes)i);
                if (isWorldWonder)
                {
                    globalBuildingsTacticsMap_[buildingType] = makeGlobalBuildingTactics(player, pBuildingInfo);
                }
                else if (isNationalWonder)
                {
                    nationalBuildingsTacticsMap_[buildingType] = makeNationalBuildingTactics(player, pBuildingInfo);
                }

                CityIter iter(*player.getCvPlayer());
                while (CvCity* pCity = iter())
                {
                    const City& city = player.getCity(pCity->getID());
                    if (!couldConstructBuilding(player, city, lookAheadDepth, pBuildingInfo))
                    {
                        continue;
                    }
#ifdef ALTAI_DEBUG
                    CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
                    cityBuildingTacticsMap_[city.getCvCity()->getIDInfo()][buildingType] = makeCityBuildingTactics(player, city, pBuildingInfo);
                }
            }
        }
    }

    void PlayerTactics::addNewCityBuildingTactics(IDInfo city)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\naddNewCityBuildingTactics for city: " << narrow(getCity(city)->getName());
#endif

        const int lookAheadDepth = 2;

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

            if (!couldConstructSpecialBuilding(player, lookAheadDepth, pBuildingInfo))
            {
//#ifdef ALTAI_DEBUG
//                os << "\nFailed special building check";
//#endif
                continue;
            }

            ReligionTypes religionType = (ReligionTypes)gGlobals.getBuildingInfo(buildingType).getPrereqReligion();
            if (religionType != NO_RELIGION)
            {
                if (player.getCvPlayer()->getHasReligionCount(religionType) == 0)
                {
#ifdef ALTAI_DEBUG
                    os << "\nFailed religion check";
#endif
                    continue;
                }
            }

            if (!couldConstructBuilding(player, player.getCity(city.iID), lookAheadDepth, pBuildingInfo))
            {
//#ifdef ALTAI_DEBUG
//                os << "\nFailed construct check";
//#endif
                continue;
            }

            const bool isWorldWonder = isWorldWonderClass((BuildingClassTypes)i), isNationalWonder = isNationalWonderClass((BuildingClassTypes)i);

            if (isWorldWonder)
            {
                globalBuildingsTacticsMap_[buildingType] = makeGlobalBuildingTactics(player, pBuildingInfo);
            }
            else if (isNationalWonder)
            {
                nationalBuildingsTacticsMap_[buildingType] = makeNationalBuildingTactics(player, pBuildingInfo);
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\n" << __FUNCTION__ " Adding tactic for building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
                cityBuildingTacticsMap_[city][buildingType] = makeCityBuildingTactics(player, player.getCity(city.iID), pBuildingInfo);
            }
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
    }

    void PlayerTactics::eraseGlobalBuildingTactics(BuildingTypes buildingType)
    {
        LimitedBuildingsTacticsMap::iterator iter = globalBuildingsTacticsMap_.find(buildingType);
        if (iter != globalBuildingsTacticsMap_.end())
        {
            globalBuildingsTacticsMap_.erase(iter);
        }
    }

    void PlayerTactics::updateUnitTactics()
    {
        possibleUnitTactics_ = makeUnitTactics(player);
        selectUnitTactics();
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
    }

    void PlayerTactics::read(FDataStreamBase* pStream)
    {
        readComplexList(pStream, possibleTechTactics_);
        readComplexList(pStream, selectedTechTactics_);
        readComplexList(pStream, possibleUnitTactics_);
        readComplexList(pStream, selectedUnitTactics_);
        readComplexList(pStream, possibleBuildingTactics_);

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

                    os << "\nDelta = " << li->second->getProjection().getOutput() - base;
                }            
            }
            else
            {
                os << "\nMissing city?";
            }
        }

        os << "\nLimited building tactics:\nWorld Wonders:\n";
        for (LimitedBuildingsTacticsMap::const_iterator ci(globalBuildingsTacticsMap_.begin()), ciEnd(globalBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nBuilding: " << gGlobals.getBuildingInfo(ci->first).getType();
            ci->second->update(player);
            ci->second->debug(os);
        }

        os << "\nNational wonders:\n";
        for (LimitedBuildingsTacticsMap::const_iterator ci(nationalBuildingsTacticsMap_.begin()), ciEnd(nationalBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nBuilding: " << gGlobals.getBuildingInfo(ci->first).getType();
            ci->second->update(player);
            ci->second->debug(os);
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
}