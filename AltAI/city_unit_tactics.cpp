#include "AltAI.h"

#include "./city_unit_tactics.h"
#include "./tactic_selection_data.h"
#include "./city_data.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./city_tactics.h"
#include "./building_tactics_deps.h"
#include "./save_utils.h"
#include "./civ_log.h"

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

    const std::vector<IDependentTacticPtr>& CityUnitTactic::getDependencies() const
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

    bool CityUnitTactic::areDependenciesSatisfied(int ignoreFlags) const
    {
        const CvCity* pCity = ::getCity(city_);

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {            
            if (pCity && dependentTactics_[i]->required(pCity, ignoreFlags))
            {
                return false;
            }
        }
        return true;
    }

    void CityUnitTactic::apply(TacticSelectionDataMap& tacticSelectionDataMap, int ignoreFlags)
    {
        const std::vector<DependencyItem> depItems = getDepItems(ignoreFlags);

        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(city_.eOwner);
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nunit tactics deps: ";

        for (size_t i = 0, depCount = depItems.size(); i < depCount; ++i)
        {
            debugDepItem(depItems[i], os);
            apply_(tacticSelectionDataMap[depItems[i]]);
        }
        debug(os);
    }

    void CityUnitTactic::apply(TacticSelectionData& selectionData)
    {
        if (areDependenciesSatisfied(IDependentTactic::Ignore_None))
        {
            apply_(selectionData);
        }
    }

    void CityUnitTactic::apply_(TacticSelectionData& selectionData)
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

    void UnitTactic::addTactic(const IUnitTacticPtr& pPlayerTactic)
    {
        playerUnitTacticsList_.push_back(pPlayerTactic);
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

    void UnitTactic::addTechDependency(const ResearchTechDependencyPtr& pTechDependency)
    {
        techDependencies_.push_back(pTechDependency);
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

        std::vector<ResearchTechDependencyPtr>::iterator techIter = std::remove_if(techDependencies_.begin(), techDependencies_.end(), IsNotRequired(player));
        techDependencies_.erase(techIter, techDependencies_.end());
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

    bool UnitTactic::areDependenciesSatisfied(const Player& player, int ignoreFlags) const
    {
        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            if (techDependencies_[i]->required(player, ignoreFlags))
            {
                return false;
            }
        }

        for (CityTacticsMap::const_iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (iter->second->areDependenciesSatisfied(ignoreFlags))
            {
                return true;
            }
        }

        return false;
    }

    const std::vector<ResearchTechDependencyPtr>& UnitTactic::getTechDependencies() const
    {
        return techDependencies_;
    }

    void UnitTactic::apply(TacticSelectionDataMap& tacticSelectionDataMap, int ignoreFlags)
    {
        std::vector<DependencyItem> depItems;

        const CvCity* pCity = NULL;
        if (!cityTactics_.empty())
        {
            pCity = getCity(cityTactics_.begin()->first);
        }

        if (!pCity)
        {
            return;
        }

        bool allDependenciesSatisfied = true;
        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            if (!techDependencies_[i]->required(pCity, ignoreFlags))
            {
                depItems.push_back(techDependencies_[i]->getDependencyItem());
            }
            else
            {
                allDependenciesSatisfied = false;
            }
        }

        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (iter->second->areDependenciesSatisfied(ignoreFlags))
            {
                for (size_t i = 0, depCount = depItems.size(); i < depCount; ++i)
                {
                    iter->second->apply(tacticSelectionDataMap[depItems[i]]);
                }
            }
        }
    }

    void UnitTactic::apply(TacticSelectionData& selectionData)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->apply(selectionData);
        }

        for (PlayerUnitTacticsList::const_iterator ci(playerUnitTacticsList_.begin()), ciEnd(playerUnitTacticsList_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->apply(shared_from_this(), selectionData);
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

        for (PlayerUnitTacticsList::const_iterator iter(playerUnitTacticsList_.begin()), endIter(playerUnitTacticsList_.end()); iter != endIter; ++iter)
        {
            (*iter)->debug(os);
        }

        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            techDependencies_[i]->debug(os);
        }
        os << "\n";
    }

    void UnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(unitType_);

        pStream->Write(techDependencies_.size());
        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            techDependencies_[i]->write(pStream);
        }

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

        size_t depCount;
        pStream->Read(&depCount);
        for (size_t i = 0; i < depCount; ++i)
        {
            ResearchTechDependencyPtr pTechDependency(new ResearchTechDependency());
            int depID;
            pStream->Read(&depID); // should be 0
            pTechDependency->read(pStream);
            techDependencies_.push_back(pTechDependency);
        }

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
