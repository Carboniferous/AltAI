#include "AltAI.h"

#include "./tictacs.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./city_building_tactics.h"
#include "./city_unit_tactics.h"
#include "./tech_tactics_items.h"
#include "./civic_tactics.h"
#include "./resource_tactics.h"
#include "./civic_tactics_visitors.h"
#include "./resource_tactics_visitors.h"
#include "./building_tactics_items.h"
#include "./building_tactics_deps.h"
#include "./tech_tactics.h"
#include "./city_tactics.h"
#include "./unit_tactics.h"
#include "./great_people_tactics.h"
#include "./project_tactics.h"
#include "./building_tactics_visitors.h"
#include "./unit_tactics_visitors.h"
#include "./tech_tactics_visitors.h"
#include "./tech_info_visitors.h"
#include "./unit_info.h"
#include "./unit_info_visitors.h"
#include "./building_info_visitors.h"
#include "./buildings_info.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./unit_analysis.h"
#include "./map_analysis.h"
#include "./city_simulator.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./civ_log.h"
#include "./save_utils.h"

namespace AltAI
{
    void PlayerTactics::init()
    {
        makeInitialUnitTactics();
        makeSpecialistUnitTactics();
        makeResourceTactics();
        makeCivicTactics();

        initialised_ = true;
        //debugTactics();
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

        for (UnitTacticsMap::iterator iter(unitTacticsMap_.begin()), endIter(unitTacticsMap_.end()); iter != endIter; ++iter)
        {
            iter->second->removeCityTactics(pCity->getIDInfo());
        }
    }

    void PlayerTactics::updateBuildingTactics()
    {
    }

    void PlayerTactics::updateTechTactics()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nupdateTechTactics(): turn = " << gGlobals.getGame().getGameTurn() << "\n";
#endif
        cityImprovementTacticsMap_.clear();
        techTacticsMap_.clear();

        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const int depth = player.getTechResearchDepth((TechTypes)i);
            if (depth > 3 || depth == 0)
            {
                continue;
            }

            const boost::shared_ptr<TechInfo>& pTechInfo = player.getAnalysis()->getTechInfo((TechTypes)i);
            techTacticsMap_[(TechTypes)i] = makeTechTactics(player, pTechInfo);
        }

        CityIter iter(*player.getCvPlayer());
        while (CvCity* pCity = iter())
        {
            addCityImprovementTactics(pCity->getIDInfo());
        }
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
                availableGeneralBuildingsList_.erase(obsoletedBuildings[i]);
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
                        if (techType == (TechTypes)i)
                        {
                            availableGeneralBuildingsList_.insert(possibleBuildings[i]);
                        }

                        CityIter iter(*player.getCvPlayer());
                        while (CvCity* pCity = iter())
                        {       
                            CityBuildingTacticsList::iterator buildingsIter = cityBuildingTacticsMap_[pCity->getIDInfo()].find(possibleBuildings[i]);
                            if (buildingsIter == cityBuildingTacticsMap_[pCity->getIDInfo()].end())
                            {
                                if (couldConstructBuilding(player, player.getCity(pCity), lookAheadDepth, pBuildingInfo, true))
                                {
#ifdef ALTAI_DEBUG
                                    CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__
                                        << " Adding tactic for building: " << gGlobals.getBuildingInfo(possibleBuildings[i]).getType();
#endif
                                    cityBuildingTacticsMap_[pCity->getIDInfo()][possibleBuildings[i]] = makeCityBuildingTactics(player, player.getCity(pCity), pBuildingInfo);
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

    void PlayerTactics::updateCityBuildingTactics(IDInfo city, BuildingTypes buildingType, int buildingChangeCount)
    {
        if (buildingChangeCount > 0)
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
        else if (buildingChangeCount < 0)  // potentially re-add tactic if we lost the building (might not be able to build it anymore though)
        {
            boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(buildingType);

            if (pBuildingInfo)
            {
                addBuildingTactics_(pBuildingInfo, getCity(city));
            }
            else
            {
                pBuildingInfo = player.getAnalysis()->getSpecialBuildingInfo(buildingType);

                if (pBuildingInfo)
                {
                    // buildings which need great people to construct
                    addSpecialBuildingTactics_(pBuildingInfo, getCity(city));
                }
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

                if (pBuildingInfo)
                {
                    addBuildingTactics_(pBuildingInfo, NULL);
                    continue;
                }

                pBuildingInfo = player.getAnalysis()->getSpecialBuildingInfo(buildingType);

                if (pBuildingInfo)
                {
                    addSpecialBuildingTactics_(pBuildingInfo, NULL);
                }
            }
        }
    }

    void PlayerTactics::updateCivicTactics(TechTypes techType)
    {
        for (CivicTacticsMap::iterator civicTacticIter(civicTacticsMap_.begin()), civicTacticEndIter(civicTacticsMap_.end());
            civicTacticIter != civicTacticEndIter; ++civicTacticIter)
        {
            civicTacticIter->second->updateDependencies(player);
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

            if (pBuildingInfo)
            {
                addBuildingTactics_(pBuildingInfo, getCity(city));
                continue;
            }

            pBuildingInfo = player.getAnalysis()->getSpecialBuildingInfo(buildingType);

            if (pBuildingInfo)
            {
                // buildings which need great people to construct
                addSpecialBuildingTactics_(pBuildingInfo, getCity(city));
            }
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

    void PlayerTactics::addCityImprovementTactics(IDInfo city)
    {
        std::list<CityImprovementTacticsPtr> cityImprovementTactics = makeCityBuildTactics(player, player.getCity(city.iID));
        std::copy(cityImprovementTactics.begin(), cityImprovementTactics.end(), std::back_inserter(cityImprovementTacticsMap_[city]));
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

    bool PlayerTactics::hasBuildingTactic(BuildingTypes buildingType) const
    {
        BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType).getBuildingClassType();
        if (isWorldWonderClass(buildingClassType))
        {
            return globalBuildingsTacticsMap_.find(buildingType) != globalBuildingsTacticsMap_.end();
        }
        else if (isNationalWonderClass(buildingClassType))
        {
            return nationalBuildingsTacticsMap_.find(buildingType) != nationalBuildingsTacticsMap_.end();
        }
        else
        {
            for (CityBuildingTacticsMap::const_iterator ci(cityBuildingTacticsMap_.begin()), ciEnd(cityBuildingTacticsMap_.end()); ci != ciEnd; ++ci)
            {
                if (ci->second.find(buildingType) != ci->second.end())
                {
                    return true;
                }
            }
            for (CityBuildingTacticsMap::const_iterator ci(specialCityBuildingsTacticsMap_.begin()), ciEnd(specialCityBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
            {
                if (ci->second.find(buildingType) != ci->second.end())
                {
                    return true;
                }
            }
        }
        return false;
    }

    void PlayerTactics::updateCityUnitTactics(TechTypes techType)
    {
        boost::shared_ptr<TechInfo> pTechInfo = player.getAnalysis()->getTechInfo(techType);
        // todo
        //std::vector<UnitTypes> obsoletedUnits = getObsoletedUnits(pTechInfo);

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
                        UnitTacticsPtr pUnitTactics = makeUnitTactics(player, pUnitInfo);
                        // can return null tactic, as units which require multiple techs come back from 
                        // getPossibleUnits() if we have one of the techs or it's not too deep
                        if (pUnitTactics) 
                        {
                            unitTacticsMap_[possibleUnits[i]] = pUnitTactics;
                        }
                    }
                    else
                    {
                        iter->second->updateDependencies(player);
                    }
                }
            }
        }
    }

    void PlayerTactics::updateCityUnitTacticsExperience(IDInfo city)
    {
        for (UnitTacticsMap::iterator iter(unitTacticsMap_.begin()), endIter(unitTacticsMap_.end()); iter != endIter; ++iter)
        {
            boost::shared_ptr<UnitInfo> pUnitInfo = player.getAnalysis()->getUnitInfo(iter->first);

            if (pUnitInfo)
            {
                if (iter->second)
                {
                    if (iter->second->getCityTactics(city))
                    {
                        // this creates a new city tactic (and updates its entry in the unit's UnitTactic object) for each unit to reflect the change in unit's free experience
                        iter->second->addCityTactic(city, makeCityUnitTactics(player, player.getCity(city.iID), pUnitInfo));
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

    TacticSelectionData& PlayerTactics::getBaseTacticSelectionData()
    {
        DependencyItemSet di;
        di.insert(DependencyItem(-1, -1));

        return tacticSelectionDataMap[di];
    }

    const TacticSelectionData& PlayerTactics::getBaseTacticSelectionData() const
    {
        DependencyItemSet di;
        di.insert(DependencyItem(-1, -1));
        static TacticSelectionData dummy;

        TacticSelectionDataMap::const_iterator baseTacticIter = tacticSelectionDataMap.find(di);
        if (baseTacticIter != tacticSelectionDataMap.end())
        {
            return baseTacticIter->second;
        }
        else
        {
            return dummy;
        }
    }

    std::map<int, ICityBuildingTacticsPtr> PlayerTactics::getCityBuildingTactics(BuildingTypes buildingType) const
    {
        std::map<int, ICityBuildingTacticsPtr> cityBuildingTactics;

        CityIter iter(*player.getCvPlayer());
        CvCity* pCity;
        while (pCity = iter())
        {
            CityBuildingTacticsMap::const_iterator ci = cityBuildingTacticsMap_.find(pCity->getIDInfo());
            if (ci == cityBuildingTacticsMap_.end())
            {
                continue;
            }
            CityBuildingTacticsList::const_iterator ci2 = ci->second.find(buildingType);
            if (ci2 != ci->second.end())
            {
                cityBuildingTactics.insert(std::make_pair(pCity->getID(), ci2->second));
            }
        }

        return cityBuildingTactics;
    }

    std::map<int, ICityBuildingTacticsPtr> PlayerTactics::getCitySpecialBuildingTactics(BuildingTypes buildingType) const
    {
        std::map<int, ICityBuildingTacticsPtr> citySpecialBuildingTactics;

        CityIter iter(*player.getCvPlayer());
        CvCity* pCity;
        while (pCity = iter())
        {
            CityBuildingTacticsMap::const_iterator ci = specialCityBuildingsTacticsMap_.find(pCity->getIDInfo());
            if (ci == specialCityBuildingsTacticsMap_.end())
            {
                continue;
            }
            CityBuildingTacticsList::const_iterator ci2 = ci->second.find(buildingType);
            if (ci2 != ci->second.end())
            {
                citySpecialBuildingTactics.insert(std::make_pair(pCity->getID(), ci2->second));
            }
        }

        return citySpecialBuildingTactics;
    }

    ResearchTech PlayerTactics::getResearchTech(TechTypes ignoreTechType)
    {
        //debugTactics();
        return AltAI::getResearchTech(*this, ignoreTechType);
    }

    ConstructItem PlayerTactics::getBuildItem(City& city)
    {
        //debugTactics();
        return AltAI::getConstructItem(*this, city);
    }

    bool PlayerTactics::getSpecialistBuild(CvUnitAI* pUnit)
    {
        return AltAI::getSpecialistBuild(*this, pUnit);
    }

    void PlayerTactics::write(FDataStreamBase* pStream) const
    {
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

        pStream->Write(specialCityBuildingsTacticsMap_.size());
        for (CityBuildingTacticsMap::const_iterator ci(specialCityBuildingsTacticsMap_.begin()), ciEnd(specialCityBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
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
        
        pStream->Write(unitTacticsMap_.size());

        for (UnitTacticsMap::const_iterator ci(unitTacticsMap_.begin()), ciEnd(unitTacticsMap_.end()); ci != ciEnd; ++ci)
        {       
            pStream->Write(ci->first);

            if (ci->second)
            {                
                ci->second->write(pStream);
            }
            else
            {
                pStream->Write(-1);
            }
        }

        pStream->Write(specialUnitTacticsMap_.size());

        for (UnitTacticsMap::const_iterator ci(specialUnitTacticsMap_.begin()), ciEnd(specialUnitTacticsMap_.end()); ci != ciEnd; ++ci)
        {       
            pStream->Write(ci->first);

            if (ci->second)
            {                
                ci->second->write(pStream);
            }
            else
            {
                pStream->Write(-1);
            }
        }

        pStream->Write(techTacticsMap_.size());
        for (TechTacticsMap::const_iterator ci(techTacticsMap_.begin()), ciEnd(techTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            if (ci->second)
            {
                ci->second->write(pStream);
            }
            else
            {
                pStream->Write(-1);
            }
        }

        pStream->Write(civicTacticsMap_.size());
        for (CivicTacticsMap::const_iterator ci(civicTacticsMap_.begin()), ciEnd(civicTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            pStream->Write(ci->first);
            ci->second->write(pStream);
        }
    }

    void PlayerTactics::read(FDataStreamBase* pStream)
    {
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

        specialCityBuildingsTacticsMap_.clear();
        size_t specialCityBuildingTacticsMapSize;
        pStream->Read(&specialCityBuildingTacticsMapSize);
        for (size_t i = 0; i < specialCityBuildingTacticsMapSize; ++i)
        {
            IDInfo city;
            city.read(pStream);

            size_t tacticCount;
            pStream->Read(&tacticCount);
            for (size_t j = 0; j < tacticCount; ++j)
            {
                BuildingTypes buildingType;
                pStream->Read((int*)&buildingType);

                specialCityBuildingsTacticsMap_[city][buildingType] = ICityBuildingTactics::factoryRead(pStream);
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
                cityImprovementTacticsMap_[city].push_back(CityImprovementTactics::factoryRead(pStream));
            }
        }

        unitTacticsMap_.clear();
        size_t unitTacticsMapSize;
        pStream->Read(&unitTacticsMapSize);
        for (size_t i = 0; i < unitTacticsMapSize; ++i)
        {
            UnitTypes unitType;
            pStream->Read((int*)&unitType);
            unitTacticsMap_[unitType] = UnitTactics::factoryRead(pStream);
        }

        specialUnitTacticsMap_.clear();
        pStream->Read(&unitTacticsMapSize);
        for (size_t i = 0; i < unitTacticsMapSize; ++i)
        {
            UnitTypes unitType;
            pStream->Read((int*)&unitType);
            specialUnitTacticsMap_[unitType] = UnitTactics::factoryRead(pStream);
        }

        techTacticsMap_.clear();
        size_t techTacticsMapSize;
        pStream->Read(&techTacticsMapSize);
        for (size_t i = 0; i < techTacticsMapSize; ++i) 
        {
            TechTypes techType;
            pStream->Read((int*)&techType);
            techTacticsMap_[techType] = ITechTactics::factoryRead(pStream);
        }

        civicTacticsMap_.clear();
        size_t civicTacticsMapSize;
        pStream->Read(&civicTacticsMapSize);
        for (size_t i = 0; i < civicTacticsMapSize; ++i) 
        {
            CivicTypes civicType;
            pStream->Read((int*)&civicType);
            civicTacticsMap_[civicType] = CivicTactics::factoryRead(pStream);
        }
    }

    std::map<BuildingTypes, std::vector<BuildingTypes> > PlayerTactics::getBuildingsCityCanAssistWith(IDInfo city) const
    {
        std::map<BuildingTypes, std::vector<BuildingTypes> > buildingsCityCanAssistWith;

        const CvCity* pCity = getCity(city);
        if (!pCity)
        {
            return buildingsCityCanAssistWith;
        }

        CityBuildingTacticsMap::const_iterator thisCityBuildingTacticsIter = cityBuildingTacticsMap_.find(city);

        for (LimitedBuildingsTacticsMap::const_iterator ci(nationalBuildingsTacticsMap_.begin()), ciEnd(nationalBuildingsTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            CityIter cityIter(*player.getCvPlayer());
            ICityBuildingTacticsPtr pCityLimitedBuildingTactics = ICityBuildingTacticsPtr();

            while (CvCity* pLoopCity = cityIter())
            {
                if (!ci->second->areDependenciesSatisfied(pLoopCity->getIDInfo(), IDependentTactic::Ignore_None) &&
                    ci->second->areDependenciesSatisfied(pLoopCity->getIDInfo(), IDependentTactic::Ignore_Civ_Buildings))
                {
                    // found a city which can build this
                    pCityLimitedBuildingTactics = ci->second->getCityTactics(pLoopCity->getIDInfo());
                    break;
                }
            }

            if (pCityLimitedBuildingTactics)
            {
                const std::vector<DependencyItem>& civBuildItems = pCityLimitedBuildingTactics->getDepItems(IDependentTactic::Ignore_Civ_Buildings);
                for (size_t j = 0, depItemCount = civBuildItems.size(); j < depItemCount; ++j)
                {
                    if (civBuildItems[j].first == CivBuildingDependency::ID)
                    {
                        CityBuildingTacticsList::const_iterator li = thisCityBuildingTacticsIter->second.find((BuildingTypes)civBuildItems[j].second);
                        if (li != thisCityBuildingTacticsIter->second.end())
                        {
                            if (li->second->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                            {
                                buildingsCityCanAssistWith[(BuildingTypes)civBuildItems[j].second].push_back(ci->first);
                            }
                        }
                    }
                }
            }
        }

        /*for (CityBuildingTacticsMap::const_iterator ci(cityBuildingTacticsMap_.begin()), ciEnd(cityBuildingTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pTargetCity = getCity(ci->first);
            if (pTargetCity && pTargetCity != pCity)
            {
                for (CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                {
                    if (li->second->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                    {
                        buildingsCityCanAssistWith[ci->first].push_back(li->first);
                    }
                }
            }
        }*/

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
        const int turn = gGlobals.getGame().getGameTurn();
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\n\nPlayerTactics::debugTactics():\nPossible tactics: (turn = " << gGlobals.getGame().getGameTurn() << ") ";
        os << "\nCity building tactics:";
        for (CityBuildingTacticsMap::iterator ci(cityBuildingTacticsMap_.begin()), ciEnd(cityBuildingTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity)
            {
                os << "\n\nturn = " << turn << " City: " << narrow(pCity->getName());
                City& city = player.getCity(pCity);

                TotalOutput base = city.getCurrentOutputProjection().getOutput();
                os << " \n\tcurrent output projection: ";
                city.getCurrentOutputProjection().debug(os);
                os << " \n\tbase output projection: ";
                city.getBaseOutputProjection().debug(os);

                for (CityBuildingTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                {                    
                    li->second->update(player, city.getCityData());
                    li->second->debug(os);

                    const ProjectionLadder& ladder = li->second->getProjection();
                    if (ladder.buildings.empty())
                    {
                        os << "\nBuilding: " << gGlobals.getBuildingInfo(li->second->getBuildingType()).getType() << " not built.";
                    }
                    else
                    {
                        os << "\nBuilding: " << gGlobals.getBuildingInfo(li->second->getBuildingType()).getType();
                        os << " delta = " << ladder.getOutput() - gGlobals.getGame().getAltAI()->getPlayer(ci->first.eOwner)->getCity(ci->first.iID).getBaseOutputProjection().getOutput();
                        /*if (!ladder.comparisons.empty())
                        {
                            os << " delta = " << ladder.getOutput() - ladder.comparisons[0].getOutput();
                        }*/
                    }
                    os << "\n";
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
                std::map<BuildingTypes, std::vector<BuildingTypes> > buildingsCityCanAssistWith = getBuildingsCityCanAssistWith(pCity->getIDInfo());

                os << "\nCity: " << narrow(pCity->getName()) << " can with: ";
                for (std::map<BuildingTypes, std::vector<BuildingTypes> >::const_iterator ci(buildingsCityCanAssistWith.begin()), ciEnd(buildingsCityCanAssistWith.end()); ci != ciEnd; ++ci)
                {
                    os << "\n\tbuilding: " << gGlobals.getBuildingInfo(ci->first).getType() << " for building(s): ";
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

                os << "\nCity: " << narrow(pCity->getName()) << " deps:";
                for (std::map<BuildingTypes, std::vector<BuildingTypes> >::const_iterator ci(buildingsMap.begin()), ciEnd(buildingsMap.end()); ci != ciEnd; ++ci)
                {
                    os << " " << gGlobals.getBuildingInfo(ci->first).getType() << " = ";
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

            boost::tie(firstBuiltTurn, firstBuiltCity) = ci->second->getFirstBuildCity();

            if (firstBuiltCity.eOwner != NO_PLAYER)
            {
                os << "\nFirst built in: " << safeGetCityName(firstBuiltCity) << " turn = " << firstBuiltTurn;

                boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(ci->first);
                //std::vector<IProjectionEventPtr> possibleEvents = getPossibleEvents(player, pBuildingInfo, firstBuiltTurn);

                ICityBuildingTacticsPtr cityTactics = ci->second->getCityTactics(firstBuiltCity);
                const CvCity* pBuiltCity = getCity(firstBuiltCity);
                {
                    TotalOutput globalDelta;
                    City& city = player.getCity(pBuiltCity->getID());

                    //cityTactics->debug(os);
                    /*if (cityTactics->getComparisonFlag() != ICityBuildingTactics::No_Comparison && !cityTactics->getProjection().comparisons.empty())
                    {
                        globalDelta += cityTactics->getProjection().getOutput() - cityTactics->getProjection().comparisons[0].getOutput();
                        os << "\nBuild city delta = " << cityTactics->getProjection().getOutput() - cityTactics->getProjection().comparisons[0].getOutput();
                    }*/

                    if (cityTactics->getComparisonFlag() != ICityBuildingTactics::No_Comparison)
                    {
                        globalDelta += cityTactics->getProjection().getOutput() - city.getBaseOutputProjection().getOutput();
                        os << "\nBuild city delta = " << cityTactics->getProjection().getOutput() - city.getBaseOutputProjection().getOutput();
                    }

                    if (cityTactics->getComparisonFlag() == ICityBuildingTactics::Area_Comparison || cityTactics->getComparisonFlag() == ICityBuildingTactics::Global_Comparison)
                    {
                        CityIter iter(*player.getCvPlayer());
                        while (CvCity* pOtherCity = iter())
                        {
                            if (pOtherCity->getIDInfo().iID != firstBuiltCity.iID)
                            {
                                City& otherCity = player.getCity(pOtherCity->getID());

                                CityDataPtr pCityData = otherCity.getCityData()->clone();
                                std::vector<IProjectionEventPtr> events;
                                events.push_back(IProjectionEventPtr(new ProjectionGlobalBuildingEvent(pBuildingInfo, firstBuiltTurn, pBuiltCity)));

                                ConstructItem constructItem;
                                ProjectionLadder otherCityProjection = getProjectedOutput(player, pCityData, player.getAnalysis()->getNumSimTurns(), events, constructItem, __FUNCTION__, true);

                                //if (!otherCityProjection.comparisons.empty())
                                {
                                    //globalDelta += otherCityProjection.getOutput() - otherCityProjection.comparisons[0].getOutput();
                                    globalDelta += otherCityProjection.getOutput() - otherCity.getBaseOutputProjection().getOutput();
                                    //otherCityProjection.debug(os);

                                    os << "\nDelta for city: " << narrow(pOtherCity->getName()) << " = " << otherCityProjection.getOutput() - otherCity.getBaseOutputProjection().getOutput();
                                }
                            }
                        }
                        os << "\nGlobal base delta = " << globalDelta;
                    }
                }

                // todo - this is meant to handle civics events
                /*for (size_t i = 0, count = possibleEvents.size(); i < count; ++i)
                {
                    TotalOutput globalDelta;

                    CityIter iter(*player.getCvPlayer());
                    while (CvCity* pCity = iter())
                    {
                        const City& thisCity = player.getCity(pCity);
                        CityDataPtr pCityData = thisCity.getCityData()->clone();
                        std::vector<IProjectionEventPtr> events;
                        events.push_back(possibleEvents[i]);

                        if (pCity->getIDInfo().iID != firstBuiltCity.iID)
                        {   
                            events.push_back(IProjectionEventPtr(new ProjectionGlobalBuildingEvent(pBuildingInfo, firstBuiltTurn, pBuiltCity)));
                        }
                        else
                        {
                            events.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pCity, pBuildingInfo)));
                        }

                        ConstructItem constructItem(pBuildingInfo->getBuildingType());
                        ProjectionLadder thisCityProjection = getProjectedOutput(player, pCityData, player.getAnalysis()->getNumSimTurns(), events, constructItem, __FUNCTION__);
                        TotalOutput thisBase = thisCity.getCurrentOutputProjection().getOutput();

                        globalDelta += thisCityProjection.getOutput() - thisBase;
                        os << "\nDelta for city: " << narrow(pCity->getName()) << " = " << thisCityProjection.getOutput() - thisBase;

                    }
                    os << "\nGlobal delta = " << globalDelta << " for event: ";
                    possibleEvents[i]->debug(os);
                }*/
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
            os << "\nUnit: " << gGlobals.getUnitInfo(ci->first).getType();
            if (ci->second)
            {
                ci->second->debug(os);
            }
            else
            {
                os << " No tactic.\n";
            }
        }

        os << "\nSpecial unit tactics:\n";
        for (UnitTacticsMap::const_iterator ci(specialUnitTacticsMap_.begin()), ciEnd(specialUnitTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nUnit: " << gGlobals.getUnitInfo(ci->first).getType();
            if (ci->second)
            {
                ci->second->debug(os);
            }
            else
            {
                os << " No tactic.\n";
            }
        }

        std::vector<UnitTypes> combatUnits, possibleCombatUnits;
        boost::tie(combatUnits, possibleCombatUnits) = getActualAndPossibleCombatUnits(player, NULL, DOMAIN_LAND);

        os << "\nPossible units: ";
        for (size_t i = 0, count = possibleCombatUnits.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getUnitInfo(possibleCombatUnits[i]).getType();
        }

        os << "\nActual units: ";
        for (size_t i = 0, count = combatUnits.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getUnitInfo(combatUnits[i]).getType();
        }

        UnitValueHelper unitValueHelper;
        UnitValueHelper::MapT cityDefenceUnitData, cityAttackUnitData;

        UnitData::CombatDetails combatDetails;
        combatDetails.flags = UnitData::CombatDetails::CityAttack;

        for (size_t i = 0, count = combatUnits.size(); i < count; ++i)
        {
            std::vector<int> odds = player.getAnalysis()->getUnitAnalysis()->getOdds(combatUnits[i], possibleCombatUnits, 1, 1, combatDetails, true);
            unitValueHelper.addMapEntry(cityAttackUnitData, combatUnits[i], possibleCombatUnits, odds);
        }

        for (size_t i = 0, count = combatUnits.size(); i < count; ++i)
        {
            std::vector<int> odds = player.getAnalysis()->getUnitAnalysis()->getOdds(combatUnits[i], possibleCombatUnits, 1, 1, combatDetails, false);
            unitValueHelper.addMapEntry(cityDefenceUnitData, combatUnits[i], possibleCombatUnits, odds);
        }

        os << "\nCity Defence unit data values: ";
        unitValueHelper.debug(cityDefenceUnitData, os);

        os << "\nCity Attack unit data values: ";
        unitValueHelper.debug(cityAttackUnitData, os);

        os << "\nProcess tactics:";
        for (ProcessTacticsMap::const_iterator ci(processTacticsMap_.begin()), ciEnd(processTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nProcess: " << gGlobals.getProcessInfo(ci->first).getType();
            ci->second->debug(os);
            CityIter iter(*player.getCvPlayer());
            while (CvCity* pCity = iter())
            {
                ProjectionLadder projection = ci->second->getProjection(pCity->getIDInfo());
                const City& city = player.getCity(pCity);
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
                City& city = player.getCity(pCity);
                //TotalOutput base = city.getCurrentOutputProjection().getOutput();

                for (CityImprovementTacticsList::const_iterator li(ci->second.begin()), liEnd(ci->second.end()); li != liEnd; ++li)
                {
                    (*li)->debug(os);
                    (*li)->update(player, city.getCityData());

                    os << "\nTotal Delta = " << (*li)->getProjection().getOutput() - (*li)->getBaseProjection().getOutput();
                }            
            }
            else
            {
                os << "\nMissing city?";
            }
        }

        os << "\nTech tactics:\n";
        for (TechTacticsMap::const_iterator ci(techTacticsMap_.begin()), ciEnd(techTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nTech: " << gGlobals.getTechInfo(ci->first).getType();
            if (ci->second)
            {
                ci->second->debug(os);
            }
        }

        os << "\nResource tactics:\n";
        for (ResourceTacticsMap::const_iterator ci(resourceTacticsMap_.begin()), ciEnd(resourceTacticsMap_.end()); ci != ciEnd; ++ci)
        {
            os << "\nResource: " << gGlobals.getBonusInfo(ci->first).getType();
            if (ci->second)
            {
                ci->second->debug(os);
            }
        }
#endif
    }

    void PlayerTactics::addBuildingTactics_(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, CvCity* pCity)
    {
        const BuildingTypes buildingType = pBuildingInfo->getBuildingType();
        const BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType).getBuildingClassType();
        const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = !isWorldWonder && isNationalWonderClass(buildingClassType);
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
                    const City& city = player.getCity(pCity);
                    if (couldConstructBuilding(player, city, lookAheadDepth, pBuildingInfo, true))
                    {
                        LimitedBuildingsTacticsMap::iterator iter = globalBuildingsTacticsMap_.find(buildingType);
                        if (iter == globalBuildingsTacticsMap_.end())
                        {
                            iter = globalBuildingsTacticsMap_.insert(std::make_pair(buildingType, ILimitedBuildingTacticsPtr(new LimitedBuildingTactic(buildingType)))).first;
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
                    const City& city = player.getCity(pCity);
                    if (couldConstructBuilding(player, city, lookAheadDepth, pBuildingInfo, true))
                    {
                        LimitedBuildingsTacticsMap::iterator iter = nationalBuildingsTacticsMap_.find(buildingType);

                        if (iter == nationalBuildingsTacticsMap_.end())
                        {
                            iter = nationalBuildingsTacticsMap_.insert(std::make_pair(buildingType, ILimitedBuildingTacticsPtr(new LimitedBuildingTactic(buildingType)))).first;
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
                if (!couldConstructBuilding(player, player.getCity(pCity), lookAheadDepth, pBuildingInfo, true))
                {
                    return;
                }

#ifdef ALTAI_DEBUG
                CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
                cityBuildingTacticsMap_[pCity->getIDInfo()][buildingType] = makeCityBuildingTactics(player, player.getCity(pCity), pBuildingInfo);
            }
            else
            {
                CityIter iter(*player.getCvPlayer());
                while (CvCity* pCity = iter())
                {
                    const City& city = player.getCity(pCity);
                    if (!couldConstructBuilding(player, city, lookAheadDepth, pBuildingInfo, true))
                    {
                        continue;
                    }
                
                    cityBuildingTacticsMap_[pCity->getIDInfo()][buildingType] = makeCityBuildingTactics(player, city, pBuildingInfo);
#ifdef ALTAI_DEBUG
                    CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for building: " << gGlobals.getBuildingInfo(buildingType).getType();
                    cityBuildingTacticsMap_[pCity->getIDInfo()][buildingType]->debug(CivLog::getLog(*player.getCvPlayer())->getStream());
#endif
                }
            }
        }
    }

    void PlayerTactics::addSpecialBuildingTactics_(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, CvCity* pCity)
    {
        const BuildingTypes buildingType = pBuildingInfo->getBuildingType();
        const int lookAheadDepth = 2;

#ifdef ALTAI_DEBUG
        //CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Checking tactic for special building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif

        if (!couldConstructSpecialBuilding(player, lookAheadDepth, pBuildingInfo))
        {
#ifdef ALTAI_DEBUG
            //CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Failed civ check for special building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
            return;
        }

        if (pCity)
        {
            if (!couldConstructUnitBuilding(player, player.getCity(pCity), lookAheadDepth, pBuildingInfo))
            {
#ifdef ALTAI_DEBUG
                /*CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Failed city check for special building: " << gGlobals.getBuildingInfo(buildingType).getType()
                    << " for city: " << narrow(pCity->getName());*/
#endif
                return;
            }

#ifdef ALTAI_DEBUG
            CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for special building: " << gGlobals.getBuildingInfo(buildingType).getType()
                << " for city: " << narrow(pCity->getName());
#endif
            specialCityBuildingsTacticsMap_[pCity->getIDInfo()][buildingType] = makeCityBuildingTactics(player, player.getCity(pCity), pBuildingInfo);
        }
        else
        {
            CityIter iter(*player.getCvPlayer());
            while (CvCity* pCity = iter())
            {
                const City& city = player.getCity(pCity);
                if (!couldConstructUnitBuilding(player, city, lookAheadDepth, pBuildingInfo))
                {
                    continue;
                }
                
#ifdef ALTAI_DEBUG
                CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for special building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
                specialCityBuildingsTacticsMap_[pCity->getIDInfo()][buildingType] = makeCityBuildingTactics(player, city, pBuildingInfo);
            }
        }
    }

    void PlayerTactics::addUnitTactics_(const boost::shared_ptr<UnitInfo>& pUnitInfo, CvCity* pCity)
    {
        const UnitTypes unitType = pUnitInfo->getUnitType();
        const UnitClassTypes unitClassType = (UnitClassTypes)gGlobals.getUnitInfo(unitType).getUnitClassType();
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        const int lookAheadDepth = 2;

        if (!couldConstructUnit(player, lookAheadDepth, pUnitInfo, true, true))
        {
            return;
        }

        if (pCity)
        {
            const City& city = player.getCity(pCity);
            
            if (couldEverConstructUnit(player, city, pUnitInfo, lookAheadDepth) && couldConstructUnit(player, city, lookAheadDepth, pUnitInfo, true, true))
            {
                UnitTacticsMap::iterator iter = unitTacticsMap_.find(unitType);

                if (iter == unitTacticsMap_.end())
                {
                    iter = unitTacticsMap_.insert(std::make_pair(unitType, makeUnitTactics(player, pUnitInfo))).first;
                }
#ifdef ALTAI_DEBUG
                CivLog::getLog(*player.getCvPlayer())->getStream() << "\n" << __FUNCTION__ << " Adding tactic for unit: " << unitInfo.getType() << " for city: " << narrow(pCity->getName());
#endif
                iter->second->addCityTactic(pCity->getIDInfo(), makeCityUnitTactics(player, city, pUnitInfo));
            }
        }
        else
        {
            unitTacticsMap_[unitType] = makeUnitTactics(player, pUnitInfo);
        }
    }

    void PlayerTactics::makeSpecialistUnitTactics()
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

            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

            // count special units as those which cannot be constructed through normal production
            if (unitInfo.getProductionCost() < 0)
            {
                specialUnitTacticsMap_[unitType] = makeSpecialUnitTactics(player, pUnitInfo);
            }
        }
    }

    void PlayerTactics::makeInitialUnitTactics()
    {
        // add initial unit tactics (units which we can build without any techs) - units will get added to this set when initial techs are assigned
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

            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

            addUnitTactics_(pUnitInfo, NULL);
        }
    }

    void PlayerTactics::makeCivicTactics()
    {
        for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
        {
            boost::shared_ptr<CivicInfo> pCivicInfo = player.getAnalysis()->getCivicInfo((CivicTypes)i);
            if (pCivicInfo)
            {
                civicTacticsMap_[(CivicTypes)i] = AltAI::makeCivicTactics(player, pCivicInfo);
            }
        }
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
//        for (CivicTacticsMap::const_iterator ci(civicTacticsMap_.begin()), ciEnd(civicTacticsMap_.end()); ci != ciEnd; ++ci)
//        {
//            os << "\nTactics for civic: " << gGlobals.getCivicInfo(ci->first).getType();
//            ci->second->debug(os);
//        }
//#endif
    }

    void PlayerTactics::makeResourceTactics()
    {
        for (int i = 0, count = gGlobals.getNumBonusInfos(); i < count; ++i)
        {
            boost::shared_ptr<ResourceInfo> pResourceInfo = player.getAnalysis()->getResourceInfo((BonusTypes)i);
            if (pResourceInfo)
            {
                resourceTacticsMap_[(BonusTypes)i] = AltAI::makeResourceTactics(player, pResourceInfo);
            }
        }
//#ifdef ALTAI_DEBUG
//        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
//        for (ResourceTacticsMap::const_iterator ci(resourceTacticsMap_.begin()), ciEnd(resourceTacticsMap_.end()); ci != ciEnd; ++ci)
//        {
//            os << "\nTactics for resource: " << gGlobals.getBonusInfo(ci->first).getType();
//            ci->second->debug(os);
//        }
//#endif
    }

    void PlayerTactics::updateFirstToTechTactics(TechTypes techType)
    {
        TechTacticsMap::iterator techTacticsIter = techTacticsMap_.find(techType);
        if (techTacticsIter != techTacticsMap_.end())
        {
            techTacticsIter->second->removeTactic(FreeTechTactic::ID);
        }
    }

    UnitAction PlayerTactics::getConstructedUnitAction(const CvUnit* pUnit) const
    {
        UnitAction unitAction;
        XYCoords location(pUnit->plot()->getCoords());
        const CvCity* pCity = pUnit->plot()->getPlotCity();

        if (pCity)
        {
            UnitTacticsMap::const_iterator unitTacticsIter = unitTacticsMap_.find(pUnit->getUnitType());
            if (unitTacticsIter != unitTacticsMap_.end())
            {
                CityUnitTacticsPtr pCityUnitTactics = unitTacticsIter->second->getCityTactics(pCity->getIDInfo());
                if (pCityUnitTactics)
                {
                    std::list<ICityUnitTacticPtr> pCityUnitTacticsList = pCityUnitTactics->getUnitTactics();
                    for (std::list<ICityUnitTacticPtr>::const_iterator ci(pCityUnitTacticsList.begin()), ciEnd = pCityUnitTacticsList.end();
                        ci != ciEnd; ++ci)
                    {
                        std::vector<XYCoords> possibleTargets = (*ci)->getPossibleTargets(player, pCity->getIDInfo());
                        if (!possibleTargets.empty())
                        {
                            unitAction.targetPlot = possibleTargets[0];
                            break;
                        }
                    }
                }
            }
        }

        return unitAction;
    }
}