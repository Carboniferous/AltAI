#include "./unit_tactics_items.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./city_tactics.h"
#include "./map_analysis.h"
#include "./unit_analysis.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./civ_log.h"
#include "./save_utils.h"


namespace AltAI
{
    namespace
    {
        void debugPromotions(std::ostream& os, const Promotions& promotions)
        {
            if (!promotions.empty())
            {
                os << " promotions = ";
            }
            for (Promotions::const_iterator ci(promotions.begin()), ciEnd(promotions.end()); ci != ciEnd; ++ci)
            {
                os << gGlobals.getPromotionInfo(*ci).getType() << ", ";
            }
        }
    }

    CityDefenceUnitTactic::CityDefenceUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void CityDefenceUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityUnitTactics->getCity());
        const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity->getID());

        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        if (pCityUnitTactics->getDependencies().empty() && !ladder.units.empty())
        {
            UnitTacticValue defenceValue;
            defenceValue.unitType = pCityUnitTactics->getUnitType();
            defenceValue.nTurns = ladder.units[0].first;
            defenceValue.unitAnalysisValue = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getUnitAnalysis()->getCityDefenceUnitValue(pCityUnitTactics->getUnitType());

            selectionData.cityDefenceUnits.insert(defenceValue);
        }
    }

    void CityDefenceUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCity defence unit";
        debugPromotions(os, promotions_);
#endif
    }

    void CityDefenceUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeSet(pStream , promotions_);
    }

    void CityDefenceUnitTactic::read(FDataStreamBase* pStream)
    {
        readSet<PromotionTypes, int>(pStream, promotions_);
    }


    CityAttackUnitTactic::CityAttackUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void CityAttackUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityUnitTactics->getCity());
        const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity->getID());

        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        if (pCityUnitTactics->getDependencies().empty() && !ladder.units.empty())
        {
            UnitTacticValue attackValue;
            attackValue.unitType = pCityUnitTactics->getUnitType();
            attackValue.nTurns = ladder.units[0].first;
            attackValue.unitAnalysisValue = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getUnitAnalysis()->getCityAttackUnitValue(pCityUnitTactics->getUnitType(), promotions_.size());

            selectionData.cityAttackUnits.insert(attackValue);
        }
    }

    void CityAttackUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCity attack unit";
        debugPromotions(os, promotions_);
#endif
    }

    void CityAttackUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeSet(pStream , promotions_);
    }

    void CityAttackUnitTactic::read(FDataStreamBase* pStream)
    {
        readSet<PromotionTypes, int>(pStream, promotions_);
    }


    void CollateralUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void CollateralUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCollateral unit";
        debugPromotions(os, promotions_);
#endif
    }

    void CollateralUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeSet(pStream , promotions_);
    }

    void CollateralUnitTactic::read(FDataStreamBase* pStream)
    {
        readSet<PromotionTypes, int>(pStream, promotions_);
    }


    void FieldDefenceUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void FieldDefenceUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tField defence unit";
        debugPromotions(os, promotions_);
#endif
    }

    void FieldDefenceUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeSet(pStream , promotions_);
    }

    void FieldDefenceUnitTactic::read(FDataStreamBase* pStream)
    {
        readSet<PromotionTypes, int>(pStream, promotions_);
    }


    FieldAttackUnitTactic::FieldAttackUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void FieldAttackUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void FieldAttackUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tField attack unit";
        debugPromotions(os, promotions_);
#endif
    }

    void FieldAttackUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeSet(pStream , promotions_);
    }

    void FieldAttackUnitTactic::read(FDataStreamBase* pStream)
    {
        readSet<PromotionTypes, int>(pStream, promotions_);
    }


    void BuildCityUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void BuildCityUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tBuild city unit";
#endif
    }

    void BuildCityUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void BuildCityUnitTactic::read(FDataStreamBase* pStream)
    {
    }


    BuildImprovementsUnitTactic::BuildImprovementsUnitTactic(const std::vector<BuildTypes>& buildTypes)
        : buildTypes_(buildTypes)
    {
    }

    void BuildImprovementsUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        if (pCityUnitTactics->getDependencies().empty() && !ladder.units.empty())
        {
            const CvCity* pCity = getCity(pCityUnitTactics->getCity());

            boost::shared_ptr<Player> pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
            boost::shared_ptr<PlayerTactics> pPlayerTactics = pPlayer->getAnalysis()->getPlayerTactics();
            const City& city = pPlayer->getCity(pCity->getID());

            std::vector<CityImprovementManager::PlotImprovementData> improvements;

            if (pCity->getCultureLevel() > 1)
            {
                boost::shared_ptr<MapAnalysis> pMapAnalysis = pPlayer->getAnalysis()->getMapAnalysis();
                const CityImprovementManager& improvementManager = pMapAnalysis->getImprovementManager(pCity->getIDInfo());
                improvements = improvementManager.getImprovements();
            }
            else  // simulate improvements if we may not yet control all city plots we could (borders not expanded)
            {
                CityImprovementManager improvementManager(city.getCvCity()->getIDInfo(), true);  // include unclaimed plots
                TotalOutputWeights outputWeights = city.getPlotAssignmentSettings().outputWeights;

                improvementManager.simulateImprovements(outputWeights, __FUNCTION__);
                improvements = improvementManager.getImprovements();
            }

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
            os << "\nImp count = " << improvements.size();
#endif            

            WorkerUnitValue unitValue;
            unitValue.unitType = pCityUnitTactics->getUnitType();
            unitValue.nTurns = ladder.units[0].first;

            for (size_t i = 0, count = improvements.size(); i < count; ++i)
            {
                if (boost::get<5>(improvements[i]) == CityImprovementManager::Not_Built)
                {
#ifdef ALTAI_DEBUG
                    os << "\nBuildImprovementsUnitTactic::apply - imp: ";
                    CityImprovementManager::logImprovement(os, improvements[i]);
#endif
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(boost::get<2>(improvements[i]));

                    if (std::find(buildTypes_.begin(), buildTypes_.end(), buildType) != buildTypes_.end())
                    {
                        unitValue.addBuild(buildType, boost::make_tuple(boost::get<0>(improvements[i]), boost::get<4>(improvements[i]), std::vector<TechTypes>()));
                    }
                }
            }

            selectionData.workerUnits.insert(std::make_pair(unitValue.unitType, unitValue));

            // city improvement tactics
            PlayerTactics::CityImprovementTacticsMap::iterator impIter = pPlayerTactics->cityImprovementTacticsMap_.find(city.getCvCity()->getIDInfo());
            if (impIter != pPlayerTactics->cityImprovementTacticsMap_.end())
            {
                for (PlayerTactics::CityImprovementTacticsList::iterator listIter(impIter->second.begin()), endListIter(impIter->second.end()); listIter != endListIter; ++listIter)
                {
                    (*listIter)->apply(pCityUnitTactics, selectionData);
                }
            }
        }
    }

    void BuildImprovementsUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tBuild improvements unit";
#endif
    }

    void BuildImprovementsUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        writeVector<BuildTypes>(pStream, buildTypes_);
    }

    void BuildImprovementsUnitTactic::read(FDataStreamBase* pStream)
    {
        readVector<BuildTypes, int>(pStream, buildTypes_);
    }
}