#include "./tech_tactics_items.h"
#include "./city_data.h"

namespace AltAI
{

    CityImprovementTactics::CityImprovementTactics(const std::vector<CityImprovementManager::PlotImprovementData>& plotData) : plotData_(plotData)
    {
    }

    void CityImprovementTactics::addTactic(const IWorkerBuildTacticPtr& pBuildTactic)
    {
        buildTactics_.push_back(pBuildTactic);
    }

    void CityImprovementTactics::addDependency(const IDependentTacticPtr& pDependentTactic)
    {
        dependentTactics_.push_back(pDependentTactic);
    }

    void CityImprovementTactics::update(const Player& player, const CityDataPtr& pCityData)
    {
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->apply(pCityData);
        }

        CityDataPtr pSimulationCityData(new CityData(pCityData->getCity(), plotData_, true));
        projection_ = getProjectedOutput(player, pSimulationCityData, 50);

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->remove(pCityData);
        }
    }

    ProjectionLadder CityImprovementTactics::getProjection() const
    {
        return projection_;
    }

    void CityImprovementTactics::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nCity improvements: ";
        for (size_t i = 0, count = plotData_.size(); i < count; ++i)
        {
            CityImprovementManager::logImprovement(os, plotData_[i]);
        }

        for (std::list<IWorkerBuildTacticPtr>::const_iterator ci(buildTactics_.begin()), ciEnd(buildTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->debug(os);
        }
        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->debug(os);
        }
#endif
    }

    void EconomicImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tEconomic improvement";
#endif
    }

    void RemoveFeatureTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tRemove feature";
#endif
    }

    void ProvidesResourceTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tProvides resource";
#endif
    }

    void HappyImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHappy improvement";
#endif
    }

    void HealthImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHealth improvement";
#endif
    }

    void MilitaryImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tMilitary improvement";
#endif
    }
}