#include "AltAI.h"

#include "./tech_tactics_items.h"
#include "./tactic_selection_data.h"
#include "./gamedata_analysis.h"
#include "./city_tactics.h"
#include "./city_data.h"
#include "./building_tactics_deps.h"
#include "./tech_info_visitors.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./civ_helper.h"
#include "./civ_log.h"

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
        CityDataPtr pSimulationCityData(new CityData(pCityData->getCity(), plotData_, true));

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->apply(pSimulationCityData);
        }
        
        std::vector<IProjectionEventPtr> events;
        events.push_back(IProjectionEventPtr(new ProjectionPopulationEvent()));

        projection_ = getProjectedOutput(player, pSimulationCityData, 50, events);

        for (size_t i = 0, count = dependentTactics_.size(); i < count; ++i)
        {
            dependentTactics_[i]->remove(pSimulationCityData);
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

    const std::vector<ResearchTechDependencyPtr>& CityImprovementTactics::getTechDependencies() const
    {
        return dependentTactics_;
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

    void FreeTechTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tFree tech tactic";
#endif
    }

    void FreeTechTactic::apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData)
    {
        const TechTypes techType = pTechTactics->getTechType();

        if (gGlobals.getGame().countKnownTechNumTeams(techType) == 0)
        {
            selectionData.getFreeTech = true;

            const PlayerTypes playerType = pTechTactics->getPlayer();
            boost::shared_ptr<Player> pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType);
            const CvPlayer& player = CvPlayerAI::getPlayer(playerType);

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(player)->getStream();
            os << "\nChecking free tech data for tech: " << gGlobals.getTechInfo(techType).getType();
#endif

            std::list<TechTypes> prereqTechs = pushTechAndPrereqs(techType, *pPlayer);
            pPlayer->getAnalysis()->recalcTechDepths();

            std::vector<TechTypes> techs = pPlayer->getAnalysis()->getTechsWithDepth(1);

            int maxCost = 0;
            for (size_t i = 0, count = techs.size(); i < count; ++i)
            {
                const int thisCost = calculateTechResearchCost(techs[i], playerType);
                maxCost = std::max<int>(maxCost, thisCost);
#ifdef ALTAI_DEBUG
                os << "\n\tTech: " << gGlobals.getTechInfo(techs[i]).getType() << " has depth = 1 and research cost: " << thisCost;
#endif
            }

            for (std::list<TechTypes>::const_iterator ci(prereqTechs.begin()), ciEnd(prereqTechs.end()); ci != ciEnd; ++ci)
            {
                pPlayer->getCivHelper()->removeTech(*ci);
            }
            pPlayer->getCivHelper()->removeTech(techType);
            pPlayer->getAnalysis()->recalcTechDepths();

            selectionData.freeTechValue = maxCost;
        }
    }

    void FreeTechTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void FreeTechTactic::read(FDataStreamBase* pStream)
    {
    }

    void FoundReligionTechTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tFound religion tech tactic";
#endif
    }

    void FoundReligionTechTactic::apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData)
    {
        const TechTypes techType = pTechTactics->getTechType();

        if (gGlobals.getGame().countKnownTechNumTeams(techType) == 0)
        {
        }
    }

    void FoundReligionTechTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void FoundReligionTechTactic::read(FDataStreamBase* pStream)
    {
    }
}