#include "AltAI.h"

#include "./city_building_tactics.h"
#include "./tactic_selection_data.h"
#include "./building_tactics_deps.h"
#include "./city_data.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./city_tactics.h"
#include "./iters.h"
#include "./civ_log.h"
#include "./helper_fns.h"

namespace AltAI
{
    CityBuildingTactic::CityBuildingTactic(BuildingTypes buildingType, int buildingCost, IDInfo city, ComparisonFlags compFlag)
        : buildingType_(buildingType), buildingCost_(buildingCost), city_(city), compFlag_(compFlag)
    {
    }

    IDInfo CityBuildingTactic::getCity() const
    {
        return city_;
    }

    CityDataPtr CityBuildingTactic::getCityData() const
    {
        return pCityData_;
    }

    void CityBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        buildingTactics_.push_back(pBuildingTactic);
    }

    void CityBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        dependentTactics_.push_back(pDependentTactic);
    }

    void CityBuildingTactic::addTechDependency(const ResearchTechDependencyPtr& pDependentTactic)
    {
        techDependencies_.push_back(pDependentTactic);
    }

    const std::vector<IDependentTacticPtr>& CityBuildingTactic::getDependencies() const
    {
        return dependentTactics_;
    }

    const std::vector<ResearchTechDependencyPtr>& CityBuildingTactic::getTechDependencies() const
    {
        return techDependencies_;
    }

    void CityBuildingTactic::update(Player& player, const CityDataPtr& pCityData)
    {
        hurryProjections_.clear();
        pCityData_ = pCityData->clone();
        pCityData_->pushBuilding(buildingType_);

        std::vector<IProjectionEventPtr> events;
        boost::shared_ptr<BuildingInfo> pBuildingInfo = player.getAnalysis()->getBuildingInfo(buildingType_);
        if (!pBuildingInfo)
        {
            pBuildingInfo = player.getAnalysis()->getSpecialBuildingInfo(buildingType_);
        }

        events.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pCityData_->getCity(), pBuildingInfo)));

        if (compFlag_ != No_Comparison)
        {
            ConstructItem constructItem(buildingType_);
            const int numSimTurns = player.getAnalysis()->getNumSimTurns();
            projection_ = getProjectedOutput(player, pCityData_, numSimTurns, events, constructItem, __FUNCTION__, false, true);

            int accumulatedTurns = 0;
            for (size_t i = 0, entryCount = projection_.entries.size(); i < entryCount; ++i)
            {
                /*for (size_t j = 0, hurryCount = projection_.entries[i].hurryData.size(); j < hurryCount; ++j)
                {
                    CityDataPtr pHurryCityData = pCityData->clone();
                    pHurryCityData->pushBuilding(buildingType_);

                    std::vector<IProjectionEventPtr> hurryEvents;
                    hurryEvents.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pHurryCityData->getCity(), pBuildingInfo, 
                        std::make_pair(projection_.entries[i].hurryData[j].hurryType, accumulatedTurns))));
                    hurryProjections_.push_back(getProjectedOutput(player, pHurryCityData, numSimTurns, hurryEvents, constructItem, __FUNCTION__, false));
                }*/
                accumulatedTurns += projection_.entries[i].turns;
            }
        }
    }

    void CityBuildingTactic::updateDependencies(Player& player, const CvCity* pCity)
    {
        std::vector<IDependentTacticPtr>::iterator iter = std::remove_if(dependentTactics_.begin(), dependentTactics_.end(), IsNotRequired(player, pCity));
        dependentTactics_.erase(iter, dependentTactics_.end());

        std::vector<ResearchTechDependencyPtr>::iterator techIter = std::remove_if(techDependencies_.begin(), techDependencies_.end(), IsNotRequired(player, pCity));
        techDependencies_.erase(techIter, techDependencies_.end());
    }

    bool CityBuildingTactic::areDependenciesSatisfied(int ignoreFlags) const
    {
        CvCity* pCity = ::getCity(city_);
        if (!pCity)
        {
            return false;
        }

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            if (dependentTactics_[i]->required(pCity, ignoreFlags))
            {
                return false;
            }
        }

        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            if (techDependencies_[i]->required(pCity, ignoreFlags))
            {
                return false;
            }
        }
        return true;
    }

    void CityBuildingTactic::apply(TacticSelectionDataMap& selectionDataMap, int ignoreFlags)
    {
        if (areDependenciesSatisfied(ignoreFlags))
        {
            const std::vector<DependencyItem> depItems = getDepItems(ignoreFlags);
            DependencyItemSet depSet(depItems.begin(), depItems.end());
            apply_(selectionDataMap[depSet]);
        }
    }

    void CityBuildingTactic::apply(TacticSelectionData& selectionData)
    {
        if (areDependenciesSatisfied(IDependentTactic::Ignore_None))
        {
            apply_(selectionData);
        }
    }

    void CityBuildingTactic::apply_(TacticSelectionData& selectionData)
    {
        for (std::list<ICityBuildingTacticPtr>::iterator iter(buildingTactics_.begin()), endIter(buildingTactics_.end()); iter != endIter; ++iter)
        {
            (*iter)->apply(shared_from_this(), selectionData);
        }
    }

    BuildingTypes CityBuildingTactic::getBuildingType() const
    {
        return buildingType_;
    }

    int CityBuildingTactic::getBuildingCost() const
    {
        return buildingCost_;
    }

    ProjectionLadder CityBuildingTactic::getProjection() const
    {
        return projection_;
    }

    ICityBuildingTactics::ComparisonFlags CityBuildingTactic::getComparisonFlag() const
    {
        return compFlag_;
    }

    void CityBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nCity building: " << gGlobals.getBuildingInfo(buildingType_).getType() << " projection: ";
        projection_.debug(os);
        /*for (size_t i = 0, count = hurryProjections_.size(); i < count; ++i)
        {
            os << "\n\thurry projection:\n";
            hurryProjections_[i].debug(os);
        }*/
        for (std::list<ICityBuildingTacticPtr>::const_iterator ci(buildingTactics_.begin()), ciEnd(buildingTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->debug(os);
        }
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->debug(os);
        }
        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            techDependencies_[i]->debug(os);
        }
#endif
    }

    void CityBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        const size_t depCount = dependentTactics_.size();
        pStream->Write(depCount);
        for (size_t i = 0; i < depCount; ++i)
        {
            dependentTactics_[i]->write(pStream);
        }

        const size_t techDepCount = techDependencies_.size();
        pStream->Write(techDepCount);
        for (size_t i = 0; i < techDepCount; ++i)
        {
            techDependencies_[i]->write(pStream);
        }

        pStream->Write(buildingTactics_.size());
        for (std::list<ICityBuildingTacticPtr>::const_iterator ci(buildingTactics_.begin()), ciEnd(buildingTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->write(pStream);
        }

        projection_.write(pStream);
        pStream->Write(buildingType_);
        pStream->Write(buildingCost_);
        city_.write(pStream);
        // pCityData_ is transitory - modified via calls to update()
        pStream->Write(compFlag_);
    }

    void CityBuildingTactic::read(FDataStreamBase* pStream)
    {
        size_t depCount;
        pStream->Read(&depCount);
        dependentTactics_.clear();
        for (size_t i = 0; i < depCount; ++i)
        {
            dependentTactics_.push_back(IDependentTactic::factoryRead(pStream));
        }

        size_t techDepCount;
        pStream->Read(&techDepCount);
        techDependencies_.clear();
        for (size_t i = 0; i < techDepCount; ++i)
        {
            ResearchTechDependencyPtr pDependentTactic(new ResearchTechDependency());

            int ID;
            pStream->Read(&ID);  // this should always be 0

            pDependentTactic->read(pStream);
            techDependencies_.push_back(pDependentTactic);
        }

        size_t tacticCount;
        pStream->Read(&tacticCount);
        buildingTactics_.clear();
        for (size_t i = 0; i < tacticCount; ++i)
        {
            buildingTactics_.push_back(ICityBuildingTactic::factoryRead(pStream));
        }

        projection_.read(pStream);
        pStream->Read((int*)&buildingType_);
        pStream->Read(&buildingCost_);
        city_.read(pStream);
        pStream->Read((int*)&compFlag_);
    }


    ProcessTactic::ProcessTactic(ProcessTypes processType) : processType_(processType)
    {
    }

    void ProcessTactic::addTechDependency(const ResearchTechDependencyPtr& pDependentTactic)
    {
        techDependencies_.push_back(pDependentTactic);
    }

    const std::vector<ResearchTechDependencyPtr>& ProcessTactic::getTechDependencies() const
    {
        return techDependencies_;
    }

    void ProcessTactic::updateDependencies(const Player& player)
    {
        std::vector<ResearchTechDependencyPtr>::iterator iter = std::remove_if(techDependencies_.begin(), techDependencies_.end(), IsNotRequired(player));
        techDependencies_.erase(iter, techDependencies_.end());
    }

    ProjectionLadder ProcessTactic::getProjection(IDInfo city) const
    {
        ProjectionLadder ladder;
        const CvCity* pCity = getCity(city);
        if (pCity)
        {
            Player& player = *gGlobals.getGame().getAltAI()->getPlayer(city.eOwner);
            CityDataPtr pCopyCityData = player.getCity(city.iID).getCityData()->clone();
            pCopyCityData->pushProcess(processType_);
            std::vector<IProjectionEventPtr> events;
            ConstructItem constructItem(processType_);
            ladder = getProjectedOutput(player, pCopyCityData, player.getAnalysis()->getNumSimTurns(), events, constructItem, __FUNCTION__);
        }
        return ladder;
    }

    ProcessTypes ProcessTactic::getProcessType() const
    {
        return processType_;
    }

    bool ProcessTactic::areDependenciesSatisfied(const Player& player, int ignoreFlags) const
    {
        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            if (techDependencies_[i]->required(player, ignoreFlags))
            {
                return false;
            }
        }

        return true;
    }

    void ProcessTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nProcess: " << gGlobals.getProcessInfo(processType_).getType();
        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            techDependencies_[i]->debug(os);
        }
#endif
    }

    void ProcessTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        const size_t techDepCount = techDependencies_.size();
        pStream->Write(techDepCount);
        for (size_t i = 0; i < techDepCount; ++i)
        {
            techDependencies_[i]->write(pStream);
        }

        pStream->Write(processType_);
    }

    void ProcessTactic::read(FDataStreamBase* pStream)
    {
        size_t techDepCount;
        pStream->Read(&techDepCount);
        techDependencies_.clear();
        for (size_t i = 0; i < techDepCount; ++i)
        {
            ResearchTechDependencyPtr pDependentTactic(new ResearchTechDependency());

            int ID;
            pStream->Read(&ID);  // this should always be 0

            pDependentTactic->read(pStream);
            techDependencies_.push_back(pDependentTactic);
        }

        pStream->Read((int*)&processType_);
    }


    LimitedBuildingTactic::LimitedBuildingTactic(BuildingTypes buildingType) : buildingType_(buildingType)
    {
        isGlobal_ = isWorldWonderClass((BuildingClassTypes)gGlobals.getBuildingInfo(buildingType).getBuildingClassType());
    }

    void LimitedBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addTactic(pBuildingTactic);
        }
    }

    void LimitedBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addDependency(pDependentTactic);
        }
    }

    void LimitedBuildingTactic::update(Player& player)
    {
        const int numCities = player.getCvPlayer()->getNumCities();
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (getCity(iter->first))
            {
                // todo - refine this check to % of best city
                if (numCities == 1 || player.getCityRank(iter->first, OUTPUT_PRODUCTION).first < 1 + numCities / 2)
                {
                    City& city = player.getCity(iter->first.iID);
                    iter->second->update(player, city.getCityData());
                }
            }
        }
    }

    void LimitedBuildingTactic::update(Player& player, const CityDataPtr& pCityData)
    {
        CityTacticsMap::iterator iter = cityTactics_.find(pCityData->getCity()->getIDInfo());
        if (iter != cityTactics_.end())
        {
            const int numCities = player.getCvPlayer()->getNumCities();
            if (numCities == 1 || player.getCityRank(iter->first, OUTPUT_PRODUCTION).first < 1 + numCities / 2)
            {
                iter->second->update(player, pCityData);
            }
        }
    }

    void LimitedBuildingTactic::updateDependencies(Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            const CvCity* pCity = getCity(iter->first);
            if (pCity)
            {
                iter->second->updateDependencies(player, pCity);
            }
        }
    }

    bool LimitedBuildingTactic::areDependenciesSatisfied(IDInfo city, int ignoreFlags) const
    {
        CityTacticsMap::const_iterator cityTacticsIter = cityTactics_.find(city);
        if (cityTacticsIter == cityTactics_.end())
        {
            return false;
        }
        else
        {
            return cityTacticsIter->second->areDependenciesSatisfied(ignoreFlags);
        }
    }

    void LimitedBuildingTactic::addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic)
    {
        cityTactics_[city] = pCityTactic;
    }

    ICityBuildingTacticsPtr LimitedBuildingTactic::getCityTactics(IDInfo city) const
    {
        CityTacticsMap::const_iterator ci = cityTactics_.find(city);
        if (ci != cityTactics_.end())
        {
            return ci->second;
        }
        return ICityBuildingTacticsPtr();
    }

    TotalOutput LimitedBuildingTactic::getGlobalDelta_(IDInfo builtCity, int buildTime)
    {
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(builtCity.eOwner);
        const City& city = pPlayer->getCity(builtCity.iID);
        TotalOutput globalDelta;
        const int numSimTurns = pPlayer->getAnalysis()->getNumSimTurns();

        ICityBuildingTactics::ComparisonFlags compFlag = cityTactics_[builtCity]->getComparisonFlag();
        if (compFlag == ICityBuildingTactics::Global_Comparison || compFlag == ICityBuildingTactics::Area_Comparison)
        {
            boost::shared_ptr<BuildingInfo> pBuildingInfo = pPlayer->getAnalysis()->getBuildingInfo(buildingType_);

            for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
            {
                IDInfo thisCity = iter->second->getCity();
                if (thisCity != builtCity)
                {
                    City& city = pPlayer->getCity(thisCity.iID);

                    if (compFlag == ICityBuildingTactics::Area_Comparison)
                    {
                        if (city.getCvCity()->getArea() != city.getCvCity()->getArea())
                        {
                            continue;
                        }
                    }
                        
                    CityDataPtr pCityData = city.getCityData()->clone();
                            
                    std::vector<IProjectionEventPtr> events;
                    events.push_back(IProjectionEventPtr(new ProjectionGlobalBuildingEvent(pBuildingInfo, buildTime, city.getCvCity())));

                    ProjectionLadder thisCityProjection = getProjectedOutput(*pPlayer, pCityData, numSimTurns, events, ConstructItem(), __FUNCTION__, true);
                    globalDelta += thisCityProjection.getOutput() - city.getBaseOutputProjection().getOutput();

                    /*if (!thisCityProjection.comparisons.empty())
                    {
                        globalDelta += thisCityProjection.getOutput() - thisCityProjection.comparisons[0].getOutput();
                    }*/
                }
            }

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(builtCity.eOwner))->getStream();
            os << "\nCalculated global delta for: " << gGlobals.getBuildingInfo(buildingType_).getType() << " = " << globalDelta << " - built in: " << safeGetCityName(builtCity);
#endif
        }

        return globalDelta;
    }

    void LimitedBuildingTactic::apply(TacticSelectionDataMap& selectionDataMap, int ignoreFlags)
    {
        std::map<IDInfo, TacticSelectionDataMap> cityTacticsMap;
        IDInfo bestCity;
        int bestBuildTime = MAX_INT;
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (iter->second->areDependenciesSatisfied(ignoreFlags))
            {
                IDInfo thisCity = iter->second->getCity();
                {
                    int numCities = CvPlayerAI::getPlayer(thisCity.eOwner).getNumCities();
                    int rank = gGlobals.getGame().getAltAI()->getPlayer(thisCity.eOwner)->getCityRank(thisCity, OUTPUT_PRODUCTION).first;
                    if (numCities > 1 && rank >= 1 + numCities / 2)
                    {
                        continue;
                    }
                }

                iter->second->apply(cityTacticsMap[thisCity], ignoreFlags);

                const std::vector<DependencyItem> depItems = iter->second->getDepItems(ignoreFlags);
                DependencyItemSet depSet(depItems.begin(), depItems.end());

                TacticSelectionData& techSelectionData = selectionDataMap[depSet];
                TacticSelectionData& thisCitysData = cityTacticsMap[thisCity][depSet];

                if (!thisCitysData.economicBuildings.empty())
                {
                    int thisBuildTime = thisCitysData.economicBuildings.begin()->nTurns;
                    if (thisBuildTime < bestBuildTime)
                    {
                        bestBuildTime = thisBuildTime;
                        bestCity = thisCity;
                    }

                    std::set<EconomicBuildingValue>::iterator valueIter(thisCitysData.economicBuildings.begin());

                    while (valueIter != thisCitysData.economicBuildings.end())
                    {
                        if (valueIter->buildingType == buildingType_)
                        {
                            if (isGlobal_)
                            {
                                techSelectionData.economicWonders[buildingType_].buildCityValues.push_back(
                                    std::make_pair(thisCity, *valueIter));
                            }
                            else
                            {
                                techSelectionData.nationalWonders[buildingType_].buildCityValues.push_back(
                                    std::make_pair(thisCity, *valueIter));
                            }
                        }
                        ++valueIter;
                    }
                }
            }
        }

        if (bestCity != IDInfo())
        {
            const std::vector<DependencyItem> depItems = cityTactics_[bestCity]->getDepItems(ignoreFlags);
            DependencyItemSet depSet(depItems.begin(), depItems.end());

            TotalOutput globalDelta = getGlobalDelta_(bestCity, bestBuildTime);
            cityTacticsMap[bestCity][depSet].economicBuildings.begin()->output += globalDelta;            

            selectionDataMap[depSet].merge(cityTacticsMap[bestCity][depSet]);
        }
    }

    void LimitedBuildingTactic::apply(TacticSelectionData& selectionData)
    {
        std::map<IDInfo, TacticSelectionData> cityTacticsMap;
        int bestBuildTime = MAX_INT;
        IDInfo bestCity;
        std::map<IDInfo, int> cityBuildTimes;

        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (iter->second->areDependenciesSatisfied(IDependentTactic::Ignore_None))
            {
                {
                    int numCities = CvPlayerAI::getPlayer(iter->first.eOwner).getNumCities();
                    int rank = gGlobals.getGame().getAltAI()->getPlayer(iter->first.eOwner)->getCityRank(iter->first, OUTPUT_PRODUCTION).first;
                    if (numCities > 1 && rank >= 1 + numCities / 2)
                    {
                        continue;
                    }
                }

                iter->second->apply(cityTacticsMap[iter->first]);
                TacticSelectionData& thisCitysData = cityTacticsMap[iter->first];

                if (!thisCitysData.economicBuildings.empty())
                {
                    int thisBuildTime = thisCitysData.economicBuildings.begin()->nTurns;
                    cityBuildTimes[iter->first] = thisBuildTime;
                }
            }
        }

        if (!cityBuildTimes.empty())
        {
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(cityBuildTimes.begin()->first.eOwner);
            const int citySearchCount = 1 + pPlayer->getCvPlayer()->getNumCities() / 3;
            int count = 0;
            for (std::map<IDInfo, int>::const_iterator bi(cityBuildTimes.begin()), biEnd(cityBuildTimes.end()); bi != biEnd; ++bi)
            {
                if (++count > citySearchCount)
                {
                    break;
                }
                else
                {
                    TotalOutput globalDelta = getGlobalDelta_(bi->first, cityTacticsMap[bi->first].economicBuildings.begin()->nTurns);
                    cityTacticsMap[bi->first].economicBuildings.begin()->output += globalDelta;
                    if (isGlobal_)
                    {
                        selectionData.economicWonders[buildingType_].buildCityValues.push_back(
                            std::make_pair(bi->first, *cityTacticsMap[bi->first].economicBuildings.begin()));
                    }
                    else
                    {
                        selectionData.nationalWonders[buildingType_].buildCityValues.push_back(
                            std::make_pair(bi->first, *cityTacticsMap[bi->first].economicBuildings.begin()));
                    }
                }
            }            
        }
    }

    void LimitedBuildingTactic::removeCityTactics(IDInfo city)
    {
        cityTactics_.erase(city);
    }

    bool LimitedBuildingTactic::empty() const
    {
        return cityTactics_.empty();
    }

    BuildingTypes LimitedBuildingTactic::getBuildingType() const
    {
        return buildingType_;
    }

    void LimitedBuildingTactic::debug(std::ostream& os) const
    {
        for (CityTacticsMap::const_iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            const CvCity* pCity = getCity(iter->first);
            if (pCity)
            {
                os << "\nCity: " << narrow(pCity->getName());
                iter->second->debug(os);
            }
        }
    }

    std::pair<int, IDInfo> LimitedBuildingTactic::getFirstBuildCity() const
    {
        int firstBuiltTurn = MAX_INT;
        IDInfo firstBuiltCity;

        for (CityTacticsMap::const_iterator ci = cityTactics_.begin(), ciEnd(cityTactics_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity && ci->second)
            {
                const ProjectionLadder& ladder = ci->second->getProjection();
                if (!ladder.buildings.empty())
                {
                    if (ladder.buildings[0].first < firstBuiltTurn)
                    {
                        firstBuiltTurn = ladder.buildings[0].first;
                        firstBuiltCity = ci->first;
                    }
                }
            }
        }

        return std::make_pair(firstBuiltTurn, firstBuiltCity);
    }

    void LimitedBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(buildingType_);
        pStream->Write(isGlobal_);

        pStream->Write(cityTactics_.size());

        for (CityTacticsMap::const_iterator ci(cityTactics_.begin()), ciEnd(cityTactics_.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            ci->second->write(pStream);
        }
    }

    void LimitedBuildingTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType_);
        pStream->Read((bool*)&isGlobal_);

        size_t cityTacticsCount;
        pStream->Read(&cityTacticsCount);
        cityTactics_.clear();
        for (size_t i = 0; i < cityTacticsCount; ++i)
        {
            IDInfo city;
            city.read(pStream);
            cityTactics_.insert(std::make_pair(city, ICityBuildingTactics::factoryRead(pStream)));
        }
    }
}