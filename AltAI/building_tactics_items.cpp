#include "./building_tactics_items.h"
#include "./city_data.h"
#include "./building_helper.h"
#include "./religion_helper.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./civ_helper.h"
#include "./player_analysis.h"
#include "./city_tactics.h"
#include "./iters.h"

namespace AltAI
{
    namespace
    {
        struct IsNotRequired
        {
            IsNotRequired(const Player& player_, const CvCity* pCity_) : player(player_), pCity(pCity_)
            {
            }

            bool operator() (const IDependentTacticPtr& pDependentTactic) const
            {
                return !pDependentTactic->required(pCity);
            }

            const Player& player;
            const CvCity* pCity;
        };

        void updateEconomicTactics(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
        {
            const CvCity* pCity = getCity(pCityBuildingTactics->getCity());
            const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity->getID());

            const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

            EconomicBuildingValue economicValue;

            economicValue.buildingType = pCityBuildingTactics->getBuildingType();
            economicValue.output = ladder.getOutput() - city.getCurrentOutputProjection().getOutput();

            if (!ladder.buildings.empty())
            {
                economicValue.nTurns = ladder.buildings[0].first;
            }

            if (pCityBuildingTactics->getDependencies().empty())
            {
                selectionData.economicBuildings.insert(economicValue);
            }
        }
    }

    ResearchTechDependency::ResearchTechDependency(TechTypes techType) : techType_(techType)
    {
    }

    void ResearchTechDependency::apply(const CityDataPtr& pCityData)
    {
        pCityData->getCivHelper()->addTech(techType_);
    }

    void ResearchTechDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getCivHelper()->removeTech(techType_);
    }

    bool ResearchTechDependency::required(const CvCity* pCity) const
    {
        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity->getOwner());
        return !CvTeamAI::getTeam(player.getTeam()).isHasTech(techType_);
    }

    std::pair<BuildQueueTypes, int> ResearchTechDependency::getBuildItem() const
    {
        return std::make_pair(NoItem, -1);
    }

    void ResearchTechDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on tech: " << gGlobals.getTechInfo(techType_).getType();
#endif
    }

    CityBuildingDependency::CityBuildingDependency(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void CityBuildingDependency::apply(const CityDataPtr& pCityData)
    {
        pCityData->getBuildingsHelper()->changeNumRealBuildings(buildingType_);
    }

    void CityBuildingDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getBuildingsHelper()->changeNumRealBuildings(buildingType_, false);
    }

    bool CityBuildingDependency::required(const CvCity* pCity) const
    {
        return pCity->getNumBuilding(buildingType_) == 0;
    }

    std::pair<BuildQueueTypes, int> CityBuildingDependency::getBuildItem() const
    {
        return std::make_pair(BuildingItem, buildingType_);
    }

    void CityBuildingDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on city building: " << gGlobals.getBuildingInfo(buildingType_).getType();
#endif
    }

    CivBuildingDependency::CivBuildingDependency(BuildingTypes buildingType, int count) : buildingType_(buildingType), count_(count)
    {        
    }

    void CivBuildingDependency::apply(const CityDataPtr& pCityData)
    {
        // nothing to do - presumes city will also get building through likely CityBuildingDependency (if not, should be done here)
    }

    void CivBuildingDependency::remove(const CityDataPtr& pCityData)
    {
        // nothing to do - presumes city will also get building through likely CityBuildingDependency
    }

    bool CivBuildingDependency::required(const CvCity* pCity) const
    {
        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity->getOwner());
        int buildingCount = 0;
        CityIter iter(player);
        while (CvCity* pCity = iter())
        {
            buildingCount += pCity->getNumBuilding(buildingType_);
        }
        return buildingCount < count_;
    }

    std::pair<BuildQueueTypes, int> CivBuildingDependency::getBuildItem() const
    {
        return std::make_pair(BuildingItem, buildingType_);
    }

    void CivBuildingDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on civ building: " << gGlobals.getBuildingInfo(buildingType_).getType() << ", count = " << count_;
#endif
    }

    ReligiousDependency::ReligiousDependency(ReligionTypes religionType, UnitTypes unitType) : religionType_(religionType), unitType_(unitType)
    {
    }

    void ReligiousDependency::apply(const CityDataPtr& pCityData)
    {
        pCityData->getReligionHelper()->changeReligionCount(religionType_);
    }

    void ReligiousDependency::remove(const CityDataPtr& pCityData)
    {
        pCityData->getReligionHelper()->changeReligionCount(religionType_, -1);
    }

    bool ReligiousDependency::required(const CvCity* pCity) const
    {
        return !pCity->isHasReligion(religionType_);
    }

    std::pair<BuildQueueTypes, int> ReligiousDependency::getBuildItem() const
    {
        return std::make_pair(UnitItem, unitType_);
    }

    void ReligiousDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on religion: " << gGlobals.getReligionInfo(religionType_).getType();
#endif
    }

    void FoodBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        updateEconomicTactics(pCityBuildingTactics, selectionData);
    }

    void FoodBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tFood building";
#endif
    }

    void HappyBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        updateEconomicTactics(pCityBuildingTactics, selectionData);
    }

    void HappyBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHappy building";
#endif
    }

    void HealthBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHealth building";
#endif
    }

    void HealthBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        updateEconomicTactics(pCityBuildingTactics, selectionData);
    }

    void GoldBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tGold building";
#endif
    }

    void GoldBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        updateEconomicTactics(pCityBuildingTactics, selectionData);
    }

    void ScienceBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tScience building";
#endif
    }

    void ScienceBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        updateEconomicTactics(pCityBuildingTactics, selectionData);
    }

    void CultureBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCulture building";
#endif
    }

    void CultureBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityBuildingTactics->getCity());
        const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity->getID());

        updateEconomicTactics(pCityBuildingTactics, selectionData);

        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        CultureBuildingValue cultureValue;

        cultureValue.buildingType = pCityBuildingTactics->getBuildingType();
        cultureValue.output = ladder.getOutput() - city.getCurrentOutputProjection().getOutput();

        if (!ladder.buildings.empty())
        {
            cultureValue.nTurns = ladder.buildings[0].first;
        }

        if (pCityBuildingTactics->getDependencies().empty())
        {
            selectionData.smallCultureBuildings.insert(cultureValue);
        }
    }

    void EspionageBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tEspionage building";
#endif
    }

    void EspionageBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        updateEconomicTactics(pCityBuildingTactics, selectionData);
    }

    void SpecialistBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tSpecialist building";
#endif
    }

    void SpecialistBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        updateEconomicTactics(pCityBuildingTactics, selectionData);
    }
     
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
        events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent(pCopyCityData)));
        events.push_back(IProjectionEventPtr(new ProjectionBuildingEvent(pCopyCityData, player.getAnalysis()->getBuildingInfo(buildingType_))));

        projection_ = getProjectedOutput(player, pCopyCityData, 50, events);
    }

    void CityBuildingTactic::updateDependencies(const Player& player, const CvCity* pCity)
    {
        std::vector<IDependentTacticPtr>::iterator iter = std::remove_if(dependentTactics_.begin(), dependentTactics_.end(), IsNotRequired(player, pCity));
        dependentTactics_.erase(iter, dependentTactics_.end());
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
        projection_.debug(os);
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

    GlobalBuildingTactic::GlobalBuildingTactic(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void GlobalBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                (*cityIter)->addTactic(pBuildingTactic);
            } 
        }
    }

    void GlobalBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                (*cityIter)->addDependency(pDependentTactic);
            }
        }
    }

    void GlobalBuildingTactic::update(const Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            const City& city = player.getCity(iter->first.iID);
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                (*cityIter)->update(player, city.getCityData());
            }
        }
    }

    void GlobalBuildingTactic::updateDependencies(const Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            const CvCity* pCity = getCity(iter->first);
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                (*cityIter)->updateDependencies(player, pCity);
            }
        }
    }

    void GlobalBuildingTactic::addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic)
    {
        cityTactics_[city].push_back(pCityTactic);
    }

    std::list<ICityBuildingTacticsPtr> GlobalBuildingTactic::getCityTactics(IDInfo city) const
    {
        CityTacticsMap::const_iterator ci = cityTactics_.find(city);
        if (ci != cityTactics_.end())
        {
            return ci->second;
        }
        return std::list<ICityBuildingTacticsPtr>();
    }

    void GlobalBuildingTactic::apply(TacticSelectionData& selectionData)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                TacticSelectionData thisCityData;
                (*cityIter)->apply(thisCityData);
                if (!thisCityData.economicBuildings.empty())
                {
                    selectionData.economicWonders[buildingType_].buildCityValues.push_back(std::make_pair((*cityIter)->getCity(), *thisCityData.economicBuildings.begin()));
                }
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
                for (std::list<ICityBuildingTacticsPtr>::const_iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)       
                {
                    (*cityIter)->debug(os);
                }
            }
        }
    }

    NationalBuildingTactic::NationalBuildingTactic(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void NationalBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                (*cityIter)->addTactic(pBuildingTactic);
            }            
        }
    }

    void NationalBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                (*cityIter)->addDependency(pDependentTactic);
            }
        }
    }

    void NationalBuildingTactic::update(const Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            const City& city = player.getCity(iter->first.iID);
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                (*cityIter)->update(player, city.getCityData());
            }
        }
    }

    void NationalBuildingTactic::updateDependencies(const Player& player)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                (*cityIter)->updateDependencies(player, getCity(iter->first));
            } 
        }
    }

    void NationalBuildingTactic::addCityTactic(IDInfo city, const ICityBuildingTacticsPtr& pCityTactic)
    {
        cityTactics_[city].push_back(pCityTactic);
    }

    void NationalBuildingTactic::apply(TacticSelectionData& selectionData)
    {
        for (CityTacticsMap::iterator iter(cityTactics_.begin()), endIter(cityTactics_.end()); iter != endIter; ++iter)
        {
            for (std::list<ICityBuildingTacticsPtr>::iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
            {
                TacticSelectionData thisCityData;
                (*cityIter)->apply(thisCityData);
                if (!thisCityData.economicBuildings.empty())
                {
                    selectionData.economicWonders[buildingType_].buildCityValues.push_back(std::make_pair((*cityIter)->getCity(), *thisCityData.economicBuildings.begin()));
                }
            } 
        }
    }

    std::list<ICityBuildingTacticsPtr> NationalBuildingTactic::getCityTactics(IDInfo city) const
    {
        CityTacticsMap::const_iterator ci = cityTactics_.find(city);
        if (ci != cityTactics_.end())
        {
            return ci->second;
        }
        return std::list<ICityBuildingTacticsPtr>();
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
            for (std::list<ICityBuildingTacticsPtr>::const_iterator cityIter(iter->second.begin()), cityEndIter(iter->second.end()); cityIter != cityEndIter; ++cityIter)
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
                (*cityIter)->debug(os);
            } 
        }
    }
}