#include "AltAI.h"

#include "./unit_tactics_items.h"
#include "./unit_tactics.h"
#include "./tactic_selection_data.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./city_tactics.h"
#include "./map_analysis.h"
#include "./unit_analysis.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./iters.h"
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

        std::vector<CityImprovementManager::PlotImprovementData> getImprovements(const Player& player, const City& city)
        {
            std::vector<CityImprovementManager::PlotImprovementData> improvements;
            const CvCity* pCity = city.getCvCity();

            if (pCity->getCultureLevel() > 1)
            {
                boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();
                const CityImprovementManager& improvementManager = pMapAnalysis->getImprovementManager(pCity->getIDInfo());
                improvements = improvementManager.getImprovements();
            }
            else  // simulate improvements if we may not yet control all city plots we could (borders not expanded)
            {
                CityImprovementManager improvementManager(pCity->getIDInfo(), true);  // include unclaimed plots
                TotalOutputWeights outputWeights = city.getPlotAssignmentSettings().outputWeights;

                improvementManager.simulateImprovements(outputWeights, __FUNCTION__);
                improvements = improvementManager.getImprovements();
            }

            return improvements;
        }
    }

    CityDefenceUnitTactic::CityDefenceUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void CityDefenceUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityUnitTactics->getCity());
        if (!pCity)
        {
            return;
        }

        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        const City& city = player.getCity(pCity);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nUnit tactics (defend city) for city: " << narrow(pCity->getName());
#endif
        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        if (!ladder.units.empty())
        {
            UnitTacticValue defenceValue;
            defenceValue.unitType = pCityUnitTactics->getUnitType();
            defenceValue.nTurns = ladder.units[0].first;
            //defenceValue.unitAnalysisValue = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getUnitAnalysis()->getCityDefenceUnitValue(pCityUnitTactics->getUnitType());

            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(pCityUnitTactics->getUnitType());
            std::vector<UnitTypes> combatUnits, possibleCombatUnits;
            boost::tie(combatUnits, possibleCombatUnits) = getActualAndPossibleCombatUnits(player, pCity, (DomainTypes)unitInfo.getDomainType());

            if (std::find(combatUnits.begin(), combatUnits.end(), pCityUnitTactics->getUnitType()) != combatUnits.end())
            {
                std::vector<int> odds = player.getAnalysis()->getUnitAnalysis()->getOdds(pCityUnitTactics->getUnitType(), possibleCombatUnits, 1, 1, UnitData::CityAttack, false);

                UnitValueHelper unitValueHelper;
                UnitValueHelper::MapT cityDefenceUnitData;
                unitValueHelper.addMapEntry(cityDefenceUnitData, pCityUnitTactics->getUnitType(), possibleCombatUnits, odds);

                defenceValue.unitAnalysisValue = unitValueHelper.getValue(cityDefenceUnitData[pCityUnitTactics->getUnitType()]);
#ifdef ALTAI_DEBUG
                os << "\nAdding city defence unit: " << unitInfo.getType() << " with value: " << defenceValue.unitAnalysisValue;
#endif
                selectionData.cityDefenceUnits.insert(defenceValue);
            }
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
        if (!pCity)
        {
            return;
        }

        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nUnit tactics (attack city) for city: " << narrow(pCity->getName());
#endif
        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        if (!ladder.units.empty())
        {
            UnitTacticValue attackValue;
            attackValue.unitType = pCityUnitTactics->getUnitType();
            attackValue.nTurns = ladder.units[0].first;
            //attackValue.unitAnalysisValue = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getUnitAnalysis()->getCityAttackUnitValue(pCityUnitTactics->getUnitType(), promotions_.size());

            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(pCityUnitTactics->getUnitType());
            std::vector<UnitTypes> combatUnits, possibleCombatUnits;
            boost::tie(combatUnits, possibleCombatUnits) = getActualAndPossibleCombatUnits(player, pCity, (DomainTypes)unitInfo.getDomainType());

            if (std::find(combatUnits.begin(), combatUnits.end(), pCityUnitTactics->getUnitType()) != combatUnits.end())
            {
                std::vector<int> odds = player.getAnalysis()->getUnitAnalysis()->getOdds(pCityUnitTactics->getUnitType(), possibleCombatUnits, 1, 1, UnitData::CityAttack, true);

                UnitValueHelper unitValueHelper;
                UnitValueHelper::MapT cityAttackUnitData;
                unitValueHelper.addMapEntry(cityAttackUnitData, pCityUnitTactics->getUnitType(), possibleCombatUnits, odds);

                attackValue.unitAnalysisValue = unitValueHelper.getValue(cityAttackUnitData[pCityUnitTactics->getUnitType()]);
#ifdef ALTAI_DEBUG
                os << "\nAdding city attack unit: " << unitInfo.getType() << " with value: " << attackValue.unitAnalysisValue;
#endif
                selectionData.cityAttackUnits.insert(attackValue);
            }
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


    CollateralUnitTactic::CollateralUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
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


    FieldDefenceUnitTactic::FieldDefenceUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
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
        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        if (!ladder.units.empty())
        {
            SettlerUnitValue buildValue;
            buildValue.unitType = pCityUnitTactics->getUnitType();
            buildValue.nTurns = ladder.units[0].first;

            selectionData.settlerUnits.insert(std::make_pair(buildValue.unitType, buildValue));
        }
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
        : buildTypes_(buildTypes), hasConsumedBuilds_(false)
    {
        for (size_t i = 0, count = buildTypes_.size(); i < count; ++i)
        {
            if (gGlobals.getBuildInfo(buildTypes_[i]).isKill())
            {
                hasConsumedBuilds_ = true;
                break;
            }
        }
    }

    void BuildImprovementsUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
#ifdef ALTAI_DEBUG
        //std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
#endif
        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();
        const CvCity* pCity = getCity(pCityUnitTactics->getCity());

        if (!pCity)
        {
#ifdef ALTAI_DEBUG  // TODO: check why this may happen...
        //    os << "\nBuildImprovementsUnitTactic::apply - called with NULL city";
#endif
            return;
        }

        boost::shared_ptr<Player> pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        boost::shared_ptr<PlayerTactics> pPlayerTactics = pPlayer->getAnalysis()->getPlayerTactics();

        const City& city = pPlayer->getCity(pCity);

        if (pCityUnitTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None) && !ladder.units.empty())
        {
            std::vector<CityImprovementManager::PlotImprovementData> improvements(getImprovements(*pPlayer, city));

            WorkerUnitValue unitValue;
            unitValue.unitType = pCityUnitTactics->getUnitType();
            unitValue.nTurns = ladder.units[0].first;

            for (size_t i = 0, count = improvements.size(); i < count; ++i)
            {
                if (boost::get<5>(improvements[i]) == CityImprovementManager::Not_Built)
                {
#ifdef ALTAI_DEBUG
                    //os << "\nBuildImprovementsUnitTactic::apply - imp: ";
                    //CityImprovementManager::logImprovement(os, improvements[i]);
#endif
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(boost::get<2>(improvements[i]));

                    if (std::find(buildTypes_.begin(), buildTypes_.end(), buildType) != buildTypes_.end())
                    {
#ifdef ALTAI_DEBUG
                        //os << "\nAdding build: " << gGlobals.getBuildInfo(buildType).getType();
#endif
                        //unitValue.addBuild(buildType, boost::make_tuple(boost::get<0>(improvements[i]), boost::get<4>(improvements[i]), std::vector<TechTypes>()));
                    }
                }
            }

            CityIter iter(*pPlayer->getCvPlayer());

            while (CvCity* pCity = iter())
            {
                // already done ourselves above
                if (pCity->getIDInfo() == pCityUnitTactics->getCity())
                {
                    continue;
                }
                const City& city = pPlayer->getCity(pCity);

                std::vector<CityImprovementManager::PlotImprovementData> improvements(getImprovements(*pPlayer, city));

                for (size_t i = 0, count = improvements.size(); i < count; ++i)
                {
                    if (boost::get<5>(improvements[i]) == CityImprovementManager::Not_Built)
                    {
#ifdef ALTAI_DEBUG
                        //os << "\nBuildImprovementsUnitTactic::apply - imp: ";
                        //CityImprovementManager::logImprovement(os, improvements[i]);
#endif
                        BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(boost::get<2>(improvements[i]));

                        if (std::find(buildTypes_.begin(), buildTypes_.end(), buildType) != buildTypes_.end())
                        {
#ifdef ALTAI_DEBUG
                            //os << "\nAdding build: " << gGlobals.getBuildInfo(buildType).getType();
#endif
                            unitValue.addBuild(buildType, boost::make_tuple(boost::get<0>(improvements[i]), boost::get<4>(improvements[i]), std::vector<TechTypes>()));
                        }
                    }
                }
            }

            if (hasConsumedBuilds_)
            {
                std::vector<int> accessibleSubAreas = pPlayer->getAnalysis()->getMapAnalysis()->
                    getAccessibleSubAreas((DomainTypes)gGlobals.getUnitInfo(pCityUnitTactics->getUnitType()).getDomainType());

                for (size_t subAreaIndex = 0, subAreaCount = accessibleSubAreas.size(); subAreaIndex < subAreaCount; ++subAreaIndex)
                {
                    for (size_t buildTypeIndex = 0, count = buildTypes_.size(); buildTypeIndex < count; ++buildTypeIndex)
                    {
                        std::vector<BonusTypes> bonusTypes = GameDataAnalysis::getBonusTypesForBuildType(buildTypes_[buildTypeIndex]);

                        std::vector<CvPlot*> resourcePlots = pPlayer->getAnalysis()->getMapAnalysis()->
                            getResourcePlots(bonusTypes, accessibleSubAreas[subAreaIndex]);

                        for (size_t j = 0, resourceCount = resourcePlots.size(); j < resourceCount; ++j)
                        {
                            // todo - drive improvement check from bonus type's required imp
                            if (resourcePlots[j]->getImprovementType() == NO_IMPROVEMENT && !resourcePlots[j]->isWithinTeamCityRadius(pPlayer->getTeamID()))
                            {
                                unitValue.addNonCityBuild(buildTypes_[buildTypeIndex], 
                                    boost::make_tuple(XYCoords(resourcePlots[j]->getX(), resourcePlots[j]->getY()), TotalOutput(), std::vector<TechTypes>()));
                            }
                        }
                    }
                }
            }

            selectionData.workerUnits.insert(std::make_pair(unitValue.unitType, unitValue));
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


    SeaAttackUnitTactic::SeaAttackUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void SeaAttackUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void SeaAttackUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tSea attack unit";
        debugPromotions(os, promotions_);
#endif
    }

    void SeaAttackUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeSet(pStream , promotions_);
    }

    void SeaAttackUnitTactic::read(FDataStreamBase* pStream)
    {
        readSet<PromotionTypes, int>(pStream, promotions_);
    }

    BuildSpecialBuildingUnitTactic::BuildSpecialBuildingUnitTactic(BuildingTypes buildingType)
        : buildingType_(buildingType)
    {
    }

    void BuildSpecialBuildingUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tBuild special building: " << (buildingType_ == NO_BUILDING ? "NONE" : gGlobals.getBuildingInfo(buildingType_).getType());
#endif
    }

    void BuildSpecialBuildingUnitTactic::apply(const IUnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void BuildSpecialBuildingUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(buildingType_);
    }

    void BuildSpecialBuildingUnitTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&buildingType_);
    }

    void DiscoverTechUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tDiscover tech unit";
#endif
    }

    void DiscoverTechUnitTactic::apply(const IUnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void DiscoverTechUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void DiscoverTechUnitTactic::read(FDataStreamBase* pStream)
    {
    }

    void CreateGreatWorkUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCreate great work unit";
#endif
    }

    void CreateGreatWorkUnitTactic::apply(const IUnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void CreateGreatWorkUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void CreateGreatWorkUnitTactic::read(FDataStreamBase* pStream)
    {
    }

    void TradeMissionUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tUndertake trade mission unit tactic";
#endif
    }

    void TradeMissionUnitTactic::apply(const IUnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void TradeMissionUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void TradeMissionUnitTactic::read(FDataStreamBase* pStream)
    {
    }
}
