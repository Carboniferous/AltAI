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
#include "./resource_info_visitors.h"
#include "./save_utils.h"
#include "./civ_log.h"

namespace AltAI
{
    CityUnitTactics::CityUnitTactics(UnitTypes unitType, IDInfo city) : unitType_(unitType), city_(city)
    {
    }

    IDInfo CityUnitTactics::getCity() const
    {
        return city_;
    }

    void CityUnitTactics::addTactic(const ICityUnitTacticPtr& pUnitTactic)
    {
        unitTactics_.push_back(pUnitTactic);
    }

    void CityUnitTactics::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        dependentTactics_.push_back(pDependentTactic);
    }

    const std::vector<IDependentTacticPtr>& CityUnitTactics::getDependencies() const
    {
        return dependentTactics_;
    }

    void CityUnitTactics::update(Player& player, const CityDataPtr& pCityData)
    {
        if (gGlobals.getUnitInfo(unitType_).isFoodProduction())
        {
            pCityData_ = pCityData->clone();  // keep hold of CityData ptr as we need some of the simulated data (e.g. culture/building defence)
            pCityData_->pushUnit(unitType_);
            std::vector<IProjectionEventPtr> events;
            events.push_back(IProjectionEventPtr(new ProjectionUnitEvent(pCityData_->getCity(), player.getAnalysis()->getUnitInfo(unitType_))));
            ConstructItem constructItem(unitType_);
            projection_ = getProjectedOutput(player, pCityData_, player.getAnalysis()->getNumSimTurns(), events, constructItem, __FUNCTION__);
        }
        else
        {
            pCityData_ = pCityData;
        }
    }

    void CityUnitTactics::updateDependencies(const Player& player, const CvCity* pCity)
    {
        std::vector<IDependentTacticPtr>::iterator iter = std::remove_if(dependentTactics_.begin(), dependentTactics_.end(), IsNotRequired(player, pCity));
        dependentTactics_.erase(iter, dependentTactics_.end());
    }

    bool CityUnitTactics::areDependenciesSatisfied(int depTacticFlags) const
    {
        const CvCity* pCity = ::getCity(city_);

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {            
            if (pCity && dependentTactics_[i]->required(pCity, depTacticFlags))
            {
                return false;
            }
        }
        return true;
    }

    void CityUnitTactics::apply(TacticSelectionDataMap& tacticSelectionDataMap, int depTacticFlags)
    {
        const std::vector<DependencyItem> depItems = getDepItems(depTacticFlags);
        DependencyItemSet depSet(depItems.begin(), depItems.end());

        apply_(tacticSelectionDataMap[depSet]);

#ifdef ALTAI_DEBUG
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(city_.eOwner);
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nunit tactics deps: ";
        for (size_t i = 0, depCount = depItems.size(); i < depCount; ++i)
        {
            debugDepItem(depItems[i], os);
        }
        debug(os);
#endif
    }

    void CityUnitTactics::apply(TacticSelectionData& selectionData, int depTacticFlags)
    {
        if (areDependenciesSatisfied(depTacticFlags))
        {
            const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(city_.eOwner);
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
            os << "\n\tCityUnitTactics::apply: ignore flags = " << depTacticFlagsToString(depTacticFlags);
            debug(os);
#endif
            apply_(selectionData);
        }
    }

    void CityUnitTactics::apply_(TacticSelectionData& selectionData)
    {
        for (std::list<ICityUnitTacticPtr>::iterator iter(unitTactics_.begin()), endIter(unitTactics_.end()); iter != endIter; ++iter)
        {
            (*iter)->apply(shared_from_this(), selectionData);
        }
    }

    std::list<ICityUnitTacticPtr> CityUnitTactics::getUnitTactics() const
    {
        return unitTactics_;
    }

    std::vector<DependencyItem> CityUnitTactics::getDepItems(int depTacticFlags) const
    {
        std::vector<DependencyItem> depItems;
        const CvCity* pCity = ::getCity(getCity());
        if (!pCity)
        {
            return depItems;
        }
        //bool allDependenciesSatisfied = true;

        const std::vector<IDependentTacticPtr> deps = getDependencies();      
        for (size_t i = 0, count = deps.size(); i < count; ++i)
        {
            // if we have a dependency of a type encoded in depTacticFlags, this check will be true for that dep
            // and we will return it. E.g., if unit requires a resource - that will be listed in deps and extracted here
            // if IDependentTactic::Resource_Dep is set.
            if (!deps[i]->required(pCity, depTacticFlags))
            {
                const std::vector<DependencyItem>& thisDepItems = deps[i]->getDependencyItems();
                std::copy(thisDepItems.begin(), thisDepItems.end(), std::back_inserter(depItems));
            }
            //else
            //{
            //    // a dependency is required when taking into account the tactics to ignore
            //    allDependenciesSatisfied = false;
            //}
        }

        //if (allDependenciesSatisfied && depItems.empty())
        //{
        //    depItems.push_back(std::make_pair(-1, -1));
        //}

        return depItems;
    }

    UnitTypes CityUnitTactics::getUnitType() const
    {
        return unitType_;
    }

    ProjectionLadder CityUnitTactics::getProjection() const
    {
        return projection_;
    }

    CityDataPtr CityUnitTactics::getCityData() const
    {
        return pCityData_;
    }

    void CityUnitTactics::debug(std::ostream& os) const
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

    CityUnitTacticsPtr CityUnitTactics::factoryRead(FDataStreamBase* pStream)
    {
        CityUnitTacticsPtr pCityUnitTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case 0:
            pCityUnitTactics = CityUnitTacticsPtr(new CityUnitTactics());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in CityUnitTactics::factoryRead");
            break;
        }

        pCityUnitTactics->read(pStream);
        return pCityUnitTactics;
    }

    void CityUnitTactics::write(FDataStreamBase* pStream) const
    {
        pStream->Write(CityUnitTacticsID);

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

    void CityUnitTactics::read(FDataStreamBase* pStream)
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

    UnitTacticsPtr UnitTactics::factoryRead(FDataStreamBase* pStream)
    {
        UnitTacticsPtr pUnitTactics;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case -1:
            return UnitTacticsPtr();  // Null tactic - valid for unit tactics if no city can build the unit currently
        case 0:
            pUnitTactics = UnitTacticsPtr(new UnitTactics());
            break;
        default:
            FAssertMsg(false, "Unexpected ID in UnitTactics::factoryRead");
            break;
        }

        pUnitTactics->read(pStream);
        return pUnitTactics;
    }

    UnitTactics::UnitTactics(PlayerTypes playerType, UnitTypes unitType) : playerType_(playerType), unitType_(unitType)
    {
    }

    void UnitTactics::addTactic(const IBuiltUnitTacticPtr& pPlayerTactic)
    {
        playerUnitTacticsList_.push_back(pPlayerTactic);
    }

    void UnitTactics::addTactic(const ICityUnitTacticPtr& pUnitTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addTactic(pUnitTactic);
        }
    }

    void UnitTactics::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->addDependency(pDependentTactic);
        }
    }

    void UnitTactics::addTechDependency(const ResearchTechDependencyPtr& pTechDependency)
    {
        techDependencies_.push_back(pTechDependency);
    }

    void UnitTactics::update(Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (getCity(iter->first))
            {
                City& city = player.getCity(iter->first.iID);
                iter->second->update(player, city.getCityData());
            }
        }
    }

    void UnitTactics::updateDependencies(const Player& player)
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

    void UnitTactics::addCityTactic(IDInfo city, const CityUnitTacticsPtr& pCityTactic)
    {
        cityTactics_[city] = pCityTactic;
    }

    CityUnitTacticsPtr UnitTactics::getCityTactics(IDInfo city) const
    {
        CityTacticsMap::const_iterator ci = cityTactics_.find(city);
        if (ci != cityTactics_.end())
        {
            return ci->second;
        }
        return CityUnitTacticsPtr();
    }

    bool UnitTactics::areDependenciesSatisfied(const Player& player, int depTacticFlags) const
    {
        if (!(depTacticFlags & IDependentTactic::Tech_Dep) && !areTechDependenciesSatisfied(player))
        {
            return false;
        }

        for (CityTacticsMap::const_iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            if (iter->second->areDependenciesSatisfied(depTacticFlags))
            {
                return true;
            }
        }

        return cityTactics_.empty();  // if we pass the tech test and city tactics are empty - treat as deps are satisfied (so we know what we can build before we actually build our first city)
    }

    bool UnitTactics::areTechDependenciesSatisfied(const Player& player) const
    {
        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            if (techDependencies_[i]->required(player, IDependentTactic::Ignore_None))
            {
                return false;
            }
        }

        return true;
    }

    const std::vector<ResearchTechDependencyPtr>& UnitTactics::getTechDependencies() const
    {
        return techDependencies_;
    }

    void UnitTactics::apply(TacticSelectionDataMap& tacticSelectionDataMap, int depTacticFlags)
    {
        const CvCity* pCity = NULL;
        if (!cityTactics_.empty())
        {
            pCity = getCity(cityTactics_.begin()->first);
        }

        if (!pCity)
        {
            return;
        }

        if (depTacticFlags & IDependentTactic::Tech_Dep)
        {
            PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);
            DependencyItemSet techDeps;
            for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
            {
                const std::vector<DependencyItem>& thisDepItems = techDependencies_[i]->getDependencyItems();
                techDeps.insert(thisDepItems.begin(), thisDepItems.end());
            }

            // can't build, but no tech required - get resource deps and check if any of them are techs we don't have yet
            // if so - add the unit as dep on that tech
            if (techDeps.empty() && !areDependenciesSatisfied(*pPlayer, IDependentTactic::Ignore_None))
            {
                PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);

                if (areDependenciesSatisfied(*pPlayer, depTacticFlags))
                {
                    const std::vector<DependencyItem>& thisDepBonusItems = cityTactics_.begin()->second->getDepItems(IDependentTactic::Resource_Dep);

                    for (size_t j = 0, depItemCount = thisDepBonusItems.size(); j < depItemCount; ++j)
                    {
                        // todo - maybe use Resource_Tech_Dep flag instead to allow 
                        // finer distinction between needing the tech for the unit and the tech(s) for (a) required resource(s)
                        TechTypes resourceRevealTech = getTechForResourceReveal(pPlayer->getAnalysis()->getResourceInfo((BonusTypes)thisDepBonusItems[j].second));
                        if (resourceRevealTech != NO_TECH && pPlayer->getAnalysis()->getTechResearchDepth(resourceRevealTech) > 0)
                        {
                            techDeps.insert(DependencyItem(ResearchTechDependency::ID, resourceRevealTech));
                            depTacticFlags |= IDependentTactic::Resource_Dep;
                        }
                    }
                }
            }

            for (size_t i = 0, count = techDeps.size(); i < count; ++i)
            {
                for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
                {
                    iter->second->apply(tacticSelectionDataMap[techDeps], depTacticFlags);
                }
            }
        }
        else
        {
            for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
            {
                iter->second->apply(tacticSelectionDataMap, depTacticFlags);
            }
        }
    }

    void UnitTactics::apply(TacticSelectionData& selectionData)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            iter->second->apply(selectionData, IDependentTactic::Ignore_None);
        }

        for (BuiltUnitTacticsList::const_iterator ci(playerUnitTacticsList_.begin()), ciEnd(playerUnitTacticsList_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->apply(shared_from_this(), selectionData);
        }
    }

    void UnitTactics::removeCityTactics(IDInfo city)
    {
        cityTactics_.erase(city);
    }

    bool UnitTactics::empty() const
    {
        return cityTactics_.empty();
    }

    UnitTypes UnitTactics::getUnitType() const
    {
        return unitType_;
    }

    PlayerTypes UnitTactics::getPlayerType() const
    {
        return playerType_;
    }

    void UnitTactics::debug(std::ostream& os) const
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

        for (BuiltUnitTacticsList::const_iterator iter(playerUnitTacticsList_.begin()), endIter(playerUnitTacticsList_.end()); iter != endIter; ++iter)
        {
            (*iter)->debug(os);
        }

        for (size_t i = 0, count = techDependencies_.size(); i < count; ++i)
        {
            techDependencies_[i]->debug(os);
        }
        os << "\n";
    }

    void UnitTactics::write(FDataStreamBase* pStream) const
    {
        pStream->Write(UnitTacticsID);
        pStream->Write(unitType_);
        pStream->Write(playerType_);

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

    void UnitTactics::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&unitType_);
        pStream->Read((int*)&playerType_);

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
            cityTactics_.insert(std::make_pair(city, CityUnitTactics::factoryRead(pStream)));
        }
    }
}
