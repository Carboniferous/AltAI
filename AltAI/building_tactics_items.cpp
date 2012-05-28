#include "./building_tactics_items.h"
#include "./city_data.h"
#include "./building_helper.h"
#include "./religion_helper.h"
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

    void CivBuildingDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on civ building: " << gGlobals.getBuildingInfo(buildingType_).getType() << ", count = " << count_;
#endif
    }

    ReligiousDependency::ReligiousDependency(ReligionTypes religionType) : religionType_(religionType)
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

    void ReligiousDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on religion: " << gGlobals.getReligionInfo(religionType_).getType();
#endif
    }

    void FoodBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        EconomicBuildingValue economicValue;

        economicValue.buildingType = pCityBuildingTactics->getBuildingType();
        economicValue.output = ladder.getOutput();

        if (!ladder.buildings.empty())
        {
            economicValue.nTurns = ladder.buildings[0].first;
        }

        selectionData.economicBuildings.insert(economicValue);
    }

    void FoodBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tFood building";
#endif
    }

    void HappyBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        EconomicBuildingValue economicValue;

        economicValue.buildingType = pCityBuildingTactics->getBuildingType();
        economicValue.output = ladder.getOutput();

        if (!ladder.buildings.empty())
        {
            economicValue.nTurns = ladder.buildings[0].first;
        }

        selectionData.economicBuildings.insert(economicValue);
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
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        EconomicBuildingValue economicValue;

        economicValue.buildingType = pCityBuildingTactics->getBuildingType();
        economicValue.output = ladder.getOutput();

        if (!ladder.buildings.empty())
        {
            economicValue.nTurns = ladder.buildings[0].first;
        }

        selectionData.economicBuildings.insert(economicValue);
    }

    void GoldBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tGold building";
#endif
    }

    void GoldBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        EconomicBuildingValue economicValue;

        economicValue.buildingType = pCityBuildingTactics->getBuildingType();
        economicValue.output = ladder.getOutput();

        if (!ladder.buildings.empty())
        {
            economicValue.nTurns = ladder.buildings[0].first;
        }

        selectionData.economicBuildings.insert(economicValue);
    }

    void ScienceBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tScience building";
#endif
    }

    void ScienceBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        EconomicBuildingValue economicValue;

        economicValue.buildingType = pCityBuildingTactics->getBuildingType();
        economicValue.output = ladder.getOutput();

        if (!ladder.buildings.empty())
        {
            economicValue.nTurns = ladder.buildings[0].first;
        }

        selectionData.economicBuildings.insert(economicValue);
    }

    void CultureBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCulture building";
#endif
    }

    void CultureBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        CultureBuildingValue cultureValue;
        EconomicBuildingValue economicValue;

        economicValue.buildingType = cultureValue.buildingType = pCityBuildingTactics->getBuildingType();
        economicValue.output = cultureValue.output = ladder.getOutput();

        if (!ladder.buildings.empty())
        {
            economicValue.nTurns = cultureValue.nTurns = ladder.buildings[0].first;
        }

        selectionData.smallCultureBuildings.insert(cultureValue);
        selectionData.economicBuildings.insert(economicValue);
    }

    void EspionageBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tEspionage building";
#endif
    }

    void EspionageBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        EconomicBuildingValue economicValue;

        economicValue.buildingType = pCityBuildingTactics->getBuildingType();
        economicValue.output = ladder.getOutput();

        if (!ladder.buildings.empty())
        {
            economicValue.nTurns = ladder.buildings[0].first;
        }

        selectionData.economicBuildings.insert(economicValue);
    }

    void SpecialistBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tSpecialist building";
#endif
    }

    void SpecialistBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        EconomicBuildingValue economicValue;

        economicValue.buildingType = pCityBuildingTactics->getBuildingType();
        economicValue.output = ladder.getOutput();

        if (!ladder.buildings.empty())
        {
            economicValue.nTurns = ladder.buildings[0].first;
        }

        selectionData.economicBuildings.insert(economicValue);
    }
     
    CityBuildingTactic::CityBuildingTactic(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void CityBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        buildingTactics_.push_back(pBuildingTactic);
    }

    void CityBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        dependentTactics_.push_back(pDependentTactic);
    }

    void CityBuildingTactic::update(const Player& player, const CityDataPtr& pCityData)
    {
        projection_ = getProjectedOutput(player, pCityData->clone(), player.getAnalysis()->getBuildingInfo(buildingType_), 50);
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