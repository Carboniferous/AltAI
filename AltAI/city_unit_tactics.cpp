#include "./city_unit_tactics.h"
#include "./city_data.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./city_tactics.h"

namespace AltAI
{
    CityUnitTactic::CityUnitTactic(UnitTypes unitType, IDInfo city) : unitType_(unitType), city_(city)
    {
    }

    IDInfo CityUnitTactic::getCity() const
    {
        return city_;
    }

    void CityUnitTactic::addTactic(const ICityUnitTacticPtr& pUnitTactic)
    {
        unitTactics_.push_back(pUnitTactic);
    }

    void CityUnitTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        dependentTactics_.push_back(pDependentTactic);
    }

    std::vector<IDependentTacticPtr> CityUnitTactic::getDependencies() const
    {
        return dependentTactics_;
    }

    void CityUnitTactic::update(const Player& player, const CityDataPtr& pCityData)
    {
        CityDataPtr pCopyCityData = pCityData->clone();
        pCopyCityData->pushUnit(unitType_);
        std::vector<IProjectionEventPtr> events;
        events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));
        events.push_back(IProjectionEventPtr(new ProjectionUnitEvent(pCopyCityData->getCity(), player.getAnalysis()->getUnitInfo(unitType_))));

        projection_ = getProjectedOutput(player, pCopyCityData, 50, events);
    }

    void CityUnitTactic::updateDependencies(const Player& player, const CvCity* pCity)
    {
        std::vector<IDependentTacticPtr>::iterator iter = std::remove_if(dependentTactics_.begin(), dependentTactics_.end(), IsNotRequired(player, pCity));
        dependentTactics_.erase(iter, dependentTactics_.end());
    }

    bool CityUnitTactic::areDependenciesSatisfied() const
    {
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            const CvCity* pCity = ::getCity(city_);
            if (pCity && dependentTactics_[i]->required(pCity))
            {
                return false;
            }
        }
        return true;
    }

    void CityUnitTactic::apply(TacticSelectionData& selectionData)
    {
        for (std::list<ICityUnitTacticPtr>::iterator iter(unitTactics_.begin()), endIter(unitTactics_.end()); iter != endIter; ++iter)
        {
            (*iter)->apply(shared_from_this(), selectionData);
        }
    }

    UnitTypes CityUnitTactic::getUnitType() const
    {
        return unitType_;
    }

    ProjectionLadder CityUnitTactic::getProjection() const
    {
        return projection_;
    }

    void CityUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\t\tCity unit: " << gGlobals.getUnitInfo(unitType_).getType(); // << " projection: ";
        //projection_.debug(os);
        for (std::list<ICityUnitTacticPtr>::const_iterator ci(unitTactics_.begin()), ciEnd(unitTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->debug(os);
        }
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->debug(os);
        }
#endif
    }

    void CityUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        const size_t depCount = dependentTactics_.size();
        pStream->Write(depCount);
        for (size_t i = 0; i < depCount; ++i)
        {
            dependentTactics_[i]->write(pStream);
        }

        pStream->Write(unitTactics_.size());
        for (std::list<ICityUnitTacticPtr>::const_iterator ci(unitTactics_.begin()), ciEnd(unitTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->write(pStream);
        }

        projection_.write(pStream);
        pStream->Write(unitType_);
        city_.write(pStream);
    }

    void CityUnitTactic::read(FDataStreamBase* pStream)
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
        unitTactics_.clear();
        for (size_t i = 0; i < tacticCount; ++i)
        {
            unitTactics_.push_back(ICityUnitTactic::factoryRead(pStream));
        }

        projection_.read(pStream);
        pStream->Read((int*)&unitType_);
        city_.read(pStream);
    }

    UnitTactic::UnitTactic(UnitTypes unitType) : unitType_(unitType)
    {
    }

    void UnitTactic::addTactic(const ICityUnitTacticPtr& pUnitTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addTactic(pUnitTactic);
        }
    }

    void UnitTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addDependency(pDependentTactic);
        }
    }

    void UnitTactic::update(const Player& player)
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

    void UnitTactic::updateDependencies(const Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (getCity(iter->first))
            {
                const CvCity* pCity = getCity(iter->first);
                iter->second->updateDependencies(player, pCity);
            }
        }
    }

    void UnitTactic::addCityTactic(IDInfo city, const ICityUnitTacticsPtr& pCityTactic)
    {
        cityTactics_[city] = pCityTactic;
    }

    ICityUnitTacticsPtr UnitTactic::getCityTactics(IDInfo city) const
    {
        CityTacticsMap::const_iterator ci = cityTactics_.find(city);
        if (ci != cityTactics_.end())
        {
            return ci->second;
        }
        return ICityUnitTacticsPtr();
    }

    bool UnitTactic::areDependenciesSatisfied() const
    {
        for (CityTacticsMap::const_iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (iter->second->areDependenciesSatisfied())
            {
                return true;
            }
        }

        return false;
    }

    void UnitTactic::apply(TacticSelectionData& selectionData)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            TacticSelectionData thisCityData;
            iter->second->apply(thisCityData);
        }
    }

    void UnitTactic::removeCityTactics(IDInfo city)
    {
        cityTactics_.erase(city);
    }

    bool UnitTactic::empty() const
    {
        return cityTactics_.empty();
    }

    UnitTypes UnitTactic::getUnitType() const
    {
        return unitType_;
    }

    void UnitTactic::debug(std::ostream& os) const
    {
        for (CityTacticsMap::const_iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            const CvCity* pCity = getCity(iter->first);
            if (pCity)
            {
                os << "\n\tCity: " << narrow(pCity->getName());
                iter->second->debug(os);
            }
        }
    }

    void UnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(unitType_);

        pStream->Write(cityTactics_.size());

        for (CityTacticsMap::const_iterator ci(cityTactics_.begin()), ciEnd(cityTactics_.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            ci->second->write(pStream);
        }
    }

    void UnitTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&unitType_);

        size_t cityTacticsCount;
        pStream->Read(&cityTacticsCount);
        cityTactics_.clear();
        for (size_t i = 0; i < cityTacticsCount; ++i)
        {
            IDInfo city;
            city.read(pStream);
            cityTactics_.insert(std::make_pair(city, ICityUnitTactics::factoryRead(pStream)));
        }
    }
}
