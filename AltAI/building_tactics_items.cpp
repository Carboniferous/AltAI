#include "./building_tactics_items.h"
#include "./city_data.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./iters.h"

namespace AltAI
{
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
        // todo
    }

    void CityBuildingDependency::remove(const CityDataPtr& pCityData)
    {
        // todo
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
        // todo
    }

    void CivBuildingDependency::remove(const CityDataPtr& pCityData)
    {
        // todo
    }

    void CivBuildingDependency::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nDependent on civ building: " << gGlobals.getBuildingInfo(buildingType_).getType() << ", count = " << count_;
#endif
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

    void GoldBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tGold building";
#endif
    }

    void ScienceBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tScience building";
#endif
    }

    void CultureBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCulture building";
#endif
    }

    void EspionageBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tEspionage building";
#endif
    }

    void SpecialistBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tSpecialist building";
#endif
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
        for (std::list<ICityBuildingTacticsPtr>::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            (*iter)->addTactic(pBuildingTactic);
        }
    }

    void GlobalBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (std::list<ICityBuildingTacticsPtr>::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            (*iter)->addDependency(pDependentTactic);
        }
    }

    void GlobalBuildingTactic::update(const Player& player)
    {
        for (std::list<ICityBuildingTacticsPtr>::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
        }
    }

    void GlobalBuildingTactic::addCityTactic(const ICityBuildingTacticsPtr& pCityTactic)
    {
        cities_.push_back(pCityTactic);
    }

    BuildingTypes GlobalBuildingTactic::getBuildingType() const
    {
        return buildingType_;
    }

    void GlobalBuildingTactic::debug(std::ostream& os) const
    {
        for (std::list<ICityBuildingTacticsPtr>::const_iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            (*iter)->debug(os);
        }
    }

    NationalBuildingTactic::NationalBuildingTactic(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void NationalBuildingTactic::addTactic(const ICityBuildingTacticPtr& pBuildingTactic)
    {
        for (std::list<ICityBuildingTacticsPtr>::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            (*iter)->addTactic(pBuildingTactic);
        }
    }

    void NationalBuildingTactic::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        for (std::list<ICityBuildingTacticsPtr>::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            (*iter)->addDependency(pDependentTactic);
        }
    }

    void NationalBuildingTactic::update(const Player& player)
    {
        for (std::list<ICityBuildingTacticsPtr>::iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
        }
    }

    void NationalBuildingTactic::addCityTactic(const ICityBuildingTacticsPtr& pCityTactic)
    {
        cities_.push_back(pCityTactic);
    }

    BuildingTypes NationalBuildingTactic::getBuildingType() const
    {
        return buildingType_;
    }

    void NationalBuildingTactic::debug(std::ostream& os) const
    {
        for (std::list<ICityBuildingTacticsPtr>::const_iterator iter(cities_.begin()), endIter(cities_.end()); iter != endIter; ++iter)
        {
            (*iter)->debug(os);
        }
    }

}