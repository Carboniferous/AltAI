#include "./city_building_tactics.h"
#include "./city_data.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./city_tactics.h"

namespace AltAI
{
    CityBuildingTactic::CityBuildingTactic(BuildingTypes buildingType, IDInfo city) : buildingType_(buildingType), city_(city)
    {
    }

    IDInfo CityBuildingTactic::getCity() const
    {
        return city_;
    }

    void CityBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        buildingTactics_.push_back(pBuildingTactic);
    }

    void CityBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        dependentTactics_.push_back(pDependentTactic);
    }

    std::vector<IDependentTacticPtr> CityBuildingTactic::getDependencies() const
    {
        return dependentTactics_;
    }

    void CityBuildingTactic::update(const Player& player, const CityDataPtr& pCityData)
    {
        CityDataPtr pCopyCityData = pCityData->clone();
        pCopyCityData->pushBuilding(buildingType_);
        std::vector<IProjectionEventPtr> events;
        events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));
        events.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pCopyCityData->getCity(), player.getAnalysis()->getBuildingInfo(buildingType_))));

        projection_ = getProjectedOutput(player, pCopyCityData, 50, events);
    }

    void CityBuildingTactic::updateDependencies(const Player& player, const CvCity* pCity)
    {
        std::vector<IDependentTacticPtr>::iterator iter = std::remove_if(dependentTactics_.begin(), dependentTactics_.end(), IsNotRequired(player, pCity));
        dependentTactics_.erase(iter, dependentTactics_.end());
    }

    bool CityBuildingTactic::areDependenciesSatisfied() const
    {
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            CvCity* pCity = ::getCity(city_);
            if (pCity && dependentTactics_[i]->required(pCity))
            {
                return false;
            }
        }
        return true;
    }

    void CityBuildingTactic::apply(TacticSelectionData& selectionData)
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

    ProjectionLadder CityBuildingTactic::getProjection() const
    {
        return projection_;
    }

    void CityBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nCity building: " << gGlobals.getBuildingInfo(buildingType_).getType() << " projection: ";
        //projection_.debug(os);
        for (std::list<ICityBuildingTacticPtr>::const_iterator ci(buildingTactics_.begin()), ciEnd(buildingTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->debug(os);
        }
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->debug(os);
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

        pStream->Write(buildingTactics_.size());
        for (std::list<ICityBuildingTacticPtr>::const_iterator ci(buildingTactics_.begin()), ciEnd(buildingTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->write(pStream);
        }

        projection_.write(pStream);
        pStream->Write(buildingType_);
        city_.write(pStream);
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

        size_t tacticCount;
        pStream->Read(&tacticCount);
        buildingTactics_.clear();
        for (size_t i = 0; i < tacticCount; ++i)
        {
            buildingTactics_.push_back(ICityBuildingTactic::factoryRead(pStream));
        }

        projection_.read(pStream);
        pStream->Read((int*)&buildingType_);
        city_.read(pStream);
    }


    ProcessTactic::ProcessTactic(ProcessTypes processType) : processType_(processType)
    {
    }

    void ProcessTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        dependentTactics_.push_back(pDependentTactic);
    }

    void ProcessTactic::updateDependencies(const Player& player)
    {
        std::vector<IDependentTacticPtr>::iterator iter = std::remove_if(dependentTactics_.begin(), dependentTactics_.end(), IsNotRequired(player));
        dependentTactics_.erase(iter, dependentTactics_.end());
    }

    ProjectionLadder ProcessTactic::getProjection(IDInfo city) const
    {
        ProjectionLadder ladder;
        const CvCity* pCity = getCity(city);
        if (pCity)
        {
            const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(city.eOwner);
            CityDataPtr pCopyCityData = player.getCity(city.iID).getCityData()->clone();
            pCopyCityData->pushProcess(processType_);
            std::vector<IProjectionEventPtr> events;
            events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));

            ladder = getProjectedOutput(player, pCopyCityData, 50, events);
        }
        return ladder;
    }

    ProcessTypes ProcessTactic::getProcessType() const
    {
        return processType_;
    }

    void ProcessTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nProcess: " << gGlobals.getProcessInfo(processType_).getType();
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->debug(os);
        }
#endif
    }

    void ProcessTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        const size_t depCount = dependentTactics_.size();
        pStream->Write(depCount);
        for (size_t i = 0; i < depCount; ++i)
        {
            dependentTactics_[i]->write(pStream);
        }

        pStream->Write(processType_);
    }

    void ProcessTactic::read(FDataStreamBase* pStream)
    {
        size_t depCount;
        pStream->Read(&depCount);
        dependentTactics_.clear();
        for (size_t i = 0; i < depCount; ++i)
        {
            dependentTactics_.push_back(IDependentTactic::factoryRead(pStream));
        }

        pStream->Read((int*)&processType_);
    }


    GlobalBuildingTactic::GlobalBuildingTactic(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void GlobalBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addTactic(pBuildingTactic);
        }
    }

    void GlobalBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addDependency(pDependentTactic);
        }
    }

    void GlobalBuildingTactic::update(const Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (getCity(iter->first))
            {
                const City& city = player.getCity(iter->first.iID);
                iter->second->update(player, city.getCityData());
            }
        }
    }

    void GlobalBuildingTactic::updateDependencies(const Player& player)
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

    void GlobalBuildingTactic::addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic)
    {
        cityTactics_[city] = pCityTactic;
    }

    ICityBuildingTacticsPtr GlobalBuildingTactic::getCityTactics(IDInfo city) const
    {
        CityTacticsMap::const_iterator ci = cityTactics_.find(city);
        if (ci != cityTactics_.end())
        {
            return ci->second;
        }
        return ICityBuildingTacticsPtr();
    }

    void GlobalBuildingTactic::apply(TacticSelectionData& selectionData)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            TacticSelectionData thisCityData(iter->second->getCity());
            iter->second->apply(thisCityData);
            if (!thisCityData.economicBuildings.empty())
            {
                selectionData.economicWonders[buildingType_].buildCityValues.push_back(std::make_pair(iter->second->getCity(), *thisCityData.economicBuildings.begin()));
            }
        }
    }

    void GlobalBuildingTactic::removeCityTactics(IDInfo city)
    {
        cityTactics_.erase(city);
    }

    bool GlobalBuildingTactic::empty() const
    {
        return cityTactics_.empty();
    }

    BuildingTypes GlobalBuildingTactic::getBuildingType() const
    {
        return buildingType_;
    }

    void GlobalBuildingTactic::debug(std::ostream& os) const
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

    void GlobalBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(buildingType_);

        pStream->Write(cityTactics_.size());

        for (CityTacticsMap::const_iterator ci(cityTactics_.begin()), ciEnd(cityTactics_.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            ci->second->write(pStream);
        }
    }

    void GlobalBuildingTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType_);

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


    NationalBuildingTactic::NationalBuildingTactic(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void NationalBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addTactic(pBuildingTactic);
        }
    }

    void NationalBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addDependency(pDependentTactic);
        }
    }

    void NationalBuildingTactic::update(const Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            const City& city = player.getCity(iter->first.iID);
            iter->second->update(player, city.getCityData());
        }
    }

    void NationalBuildingTactic::updateDependencies(const Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->updateDependencies(player, getCity(iter->first));
        }
    }

    void NationalBuildingTactic::addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic)
    {
        cityTactics_[city] = pCityTactic;
    }

    void NationalBuildingTactic::apply(TacticSelectionData& selectionData)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            TacticSelectionData thisCityData(iter->second->getCity());
            iter->second->apply(thisCityData);
            if (!thisCityData.economicBuildings.empty())
            {
                selectionData.nationalWonders[buildingType_].buildCityValues.push_back(std::make_pair(iter->second->getCity(), *thisCityData.economicBuildings.begin()));
            }
            if (thisCityData.exclusions.find(buildingType_) != thisCityData.exclusions.end())
            {
                selectionData.exclusions.insert(buildingType_);
            }
        }
    }

    ICityBuildingTacticsPtr NationalBuildingTactic::getCityTactics(IDInfo city) const
    {
        CityTacticsMap::const_iterator ci = cityTactics_.find(city);
        if (ci != cityTactics_.end())
        {
            return ci->second;
        }
        return ICityBuildingTacticsPtr();
    }

    void NationalBuildingTactic::removeCityTactics(IDInfo city)
    {
        cityTactics_.erase(city);
    }

    bool NationalBuildingTactic::empty() const
    {
        return cityTactics_.empty();
    }

    BuildingTypes NationalBuildingTactic::getBuildingType() const
    {
        return buildingType_;
    }

    void NationalBuildingTactic::debug(std::ostream& os) const
    {
        for (CityTacticsMap::const_iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            const CvCity* pCity = getCity(iter->first);
            if (pCity)
            {
                os << "\nCity: " << narrow(pCity->getName());
            }
            else
            {
                os << "\nMissing city:";
            }
            iter->second->debug(os);
        } 
    }

    void NationalBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(buildingType_);

        pStream->Write(cityTactics_.size());

        for (CityTacticsMap::const_iterator ci(cityTactics_.begin()), ciEnd(cityTactics_.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            ci->second->write(pStream);
        }
    }

    void NationalBuildingTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType_);

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