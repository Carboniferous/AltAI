#include "./tech_tactics_items.h"
#include "./gamedata_analysis.h"
#include "./city_tactics.h"
#include "./city_data.h"
#include "./building_tactics_deps.h"

namespace AltAI
{

    CityImprovementTactics::CityImprovementTactics(const std::vector<CityImprovementManager::PlotImprovementData>& plotData) : plotData_(plotData)
    {
    }

    void CityImprovementTactics::addTactic(const IWorkerBuildTacticPtr& pBuildTactic)
    {
        buildTactics_.push_back(pBuildTactic);
    }

    void CityImprovementTactics::addDependency(const ResearchTechDependencyPtr& pDependentTactic)
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
        std::vector<IProjectionEventPtr> events;
        events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));

        projection_ = getProjectedOutput(player, pSimulationCityData, 50, events);

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->remove(pCityData);
        }
    }

    void CityImprovementTactics::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& tacticSelectionData)
    {
        std::map<UnitTypes, WorkerUnitValue>::iterator iter = tacticSelectionData.workerUnits.find(pCityUnitTactics->getUnitType());

        if (iter != tacticSelectionData.workerUnits.end())
        {
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(pCityUnitTactics->getUnitType());

            for (size_t i = 0, count = plotData_.size(); i < count; ++i)
            {
                if (boost::get<5>(plotData_[i]) == CityImprovementManager::Not_Built)
                {
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(boost::get<2>(plotData_[i]));

                    if (unitInfo.getBuilds(buildType))
                    {
                        std::vector<TechTypes> techs;
                        for (size_t j = 0, prereqCount = dependentTactics_.size(); j < prereqCount; ++j)
                        {
                            techs.push_back(dependentTactics_[j]->getResearchTech());
                        }
                        iter->second.addBuild(buildType, boost::make_tuple(boost::get<0>(plotData_[i]), boost::get<4>(plotData_[i]), techs));
                    }
                }
            }
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

    void CityImprovementTactics::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        size_t depCount = dependentTactics_.size();
        pStream->Write(depCount);
        for (size_t i = 0; i < depCount; ++i)
        {
            dependentTactics_[i]->write(pStream);
        }

        pStream->Write(buildTactics_.size());
        for (std::list<IWorkerBuildTacticPtr>::const_iterator ci(buildTactics_.begin()), ciEnd(buildTactics_.end()); ci != ciEnd; ++ci)
        {
            (*ci)->write(pStream);
        }

        CityImprovementManager::writeImprovements(pStream, plotData_);

        projection_.write(pStream);
    }

    void CityImprovementTactics::read(FDataStreamBase* pStream)
    {
        size_t depCount;
        pStream->Read(&depCount);
        for (size_t i = 0; i < depCount; ++i)
        {
            ResearchTechDependencyPtr pDependentTactic(new ResearchTechDependency());

            int ID;
            pStream->Read(&ID);  // this should always be 0

            pDependentTactic->read(pStream);
            dependentTactics_.push_back(pDependentTactic);
        }

        size_t buildTacticCount;
        pStream->Read(&buildTacticCount);
        for (size_t i = 0; i < buildTacticCount; ++i)
        {
            buildTactics_.push_back(IWorkerBuildTactic::factoryRead(pStream));
        }

        CityImprovementManager::readImprovements(pStream, plotData_);

        projection_.read(pStream);
    }


    void EconomicImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tEconomic improvement";
#endif
    }

    void EconomicImprovementTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void EconomicImprovementTactic::read(FDataStreamBase* pStream)
    {
    }


    void RemoveFeatureTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tRemove feature";
#endif
    }

    void RemoveFeatureTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void RemoveFeatureTactic::read(FDataStreamBase* pStream)
    {
    }


    void ProvidesResourceTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tProvides resource";
#endif
    }

    void ProvidesResourceTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void ProvidesResourceTactic::read(FDataStreamBase* pStream)
    {
    }


    void HappyImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHappy improvement";
#endif
    }

    void HappyImprovementTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void HappyImprovementTactic::read(FDataStreamBase* pStream)
    {
    }


    void HealthImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHealth improvement";
#endif
    }

    void HealthImprovementTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void HealthImprovementTactic::read(FDataStreamBase* pStream)
    {
    }


    void MilitaryImprovementTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tMilitary improvement";
#endif
    }

    void MilitaryImprovementTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void MilitaryImprovementTactic::read(FDataStreamBase* pStream)
    {
    }
}