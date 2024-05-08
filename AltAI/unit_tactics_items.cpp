#include "AltAI.h"

#include "./unit_tactics_items.h"
#include "./unit_tactics.h"
#include "./city_unit_tactics.h"
#include "./tech_tactics_items.h"
#include "./building_tactics_deps.h"
#include "./tactic_selection_data.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./settler_manager.h"
#include "./tictacs.h"
#include "./city_tactics.h"
#include "./map_analysis.h"
#include "./unit_analysis.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./iters.h"
#include "./unit_info_visitors.h"
#include "./tech_info_visitors.h"
#include "./modifiers_helper.h"
#include "./culture_helper.h"
#include "./unit_helper.h"
#include "./helper_fns.h"
#include "./civ_log.h"
#include "./save_utils.h"
#include "./error_log.h"


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

        std::pair<TotalOutput, std::vector<PlotImprovementData> > getImprovements(const Player& player, City& city)
        {
            std::vector<PlotImprovementData> improvements;
            TotalOutput improvementsDelta;
            const CvCity* pCity = city.getCvCity();

            if (pCity->getCultureLevel() > 1)
            {
                improvements = city.getCityImprovementManager()->getImprovements();
                improvementsDelta = city.getCityImprovementManager()->getImprovementsDelta();
            }
            else  // simulate improvements if we may not yet control all city plots we could (borders not expanded)
            {
                CityImprovementManager improvementManager(pCity->getIDInfo(), true);  // include unclaimed plots
                //ErrorLog::getLog(CvPlayerAI::getPlayer(player.getPlayerID()))->getStream() << "\nnew CityImprovementManager at: " << &improvementManager << " (stack)";
                TotalOutputWeights outputWeights = city.getPlotAssignmentSettings().outputWeights;

                improvementManager.simulateImprovements(city.getCityData(), 0, __FUNCTION__);
                improvements = improvementManager.getImprovements();
                improvementsDelta = improvementManager.getImprovementsDelta();
            }

            return std::make_pair(improvementsDelta, improvements);
        }

        void applyCombatTactic(const CityUnitTacticsPtr& pCityUnitTactics, 
            std::set<UnitTacticValue>& unitSelectionData, 
            IDInfo cityID, 
            const UnitData::CombatDetails& combatDetails,
            bool isAttacker)
        {
            CvCity* pCity = getCity(cityID);
            if (!pCity)
            {
                return;
            }

            Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
            City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity);
            UnitTypes unitType = pCityUnitTactics->getUnitType();
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

            int estimatedTurns = city.getBaseOutputProjection().getExpectedTurnBuilt(player.getCvPlayer()->getProductionNeeded(unitType), 
                city.getCityData()->getModifiersHelper()->getUnitProductionModifier(unitType),
                city.getCityData()->getModifiersHelper()->getTotalYieldModifier(*city.getCityData())[YIELD_PRODUCTION]);

            //const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

            //if (!ladder.units.empty())
            if (estimatedTurns >= 0)
            {
                UnitTacticValue tacticValue;
                tacticValue.unitType = unitType;
                tacticValue.city = cityID;
                
                //tacticValue.nTurns = ladder.units[0].turns;
                tacticValue.nTurns = estimatedTurns;
                //tacticValue.unitAnalysisValue = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis()->getUnitAnalysis()->getCityAttackUnitValue(pCityUnitTactics->getUnitType(), promotions_.size());
                
                std::vector<UnitTypes> combatUnits, possibleCombatUnits;
                boost::tie(combatUnits, possibleCombatUnits) = getActualAndPossibleCombatUnits(player, pCity, (DomainTypes)unitInfo.getDomainType());

                const int experience = pCityUnitTactics->getCityData()->getUnitHelper()->getUnitFreeExperience(unitType);
                const int level = player.getAnalysis()->getUnitLevel(experience);
                tacticValue.level = level;

                UnitData ourUnit(pCityUnitTactics->getUnitType());
                // todo - get any free promotions from city data unit helper
                player.getAnalysis()->getUnitAnalysis()->promote(ourUnit, combatDetails, isAttacker, level, Promotions());
                std::vector<int> odds = player.getAnalysis()->getUnitAnalysis()->getOdds(ourUnit, possibleCombatUnits, 0, combatDetails, isAttacker);
#ifdef ALTAI_DEBUG
                //if (unitInfo.getDomainType() != DOMAIN_LAND)
                {
                    std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
                    os << "\n" << unitInfo.getType();
                    ourUnit.debugPromotions(os);
                    os << "\n\t " << (isAttacker ? "attack" : "defence") << " odds =";
                    for (size_t i = 0, count = odds.size(); i < count; ++i)
                    {
                        if (odds[i] > 0)  // don't bother logging stuff with zero odds - just clutters up the log file
                        {
                            os << (i > 0 ? ", " : " ") << odds[i] << " v. " << gGlobals.getUnitInfo(possibleCombatUnits[i]).getType();
                        }
                    }
                }

                /*if (estimatedTurns != tacticValue.nTurns)
                {
                    const CvPlayer& player = CvPlayerAI::getPlayer(pCity->getOwner());
                    std::ostream& os = CivLog::getLog(player)->getStream();
                    os << "\nMismatched turn projections for unit: " << gGlobals.getUnitInfo(unitType).getType()
                        << " p1 = " << tacticValue.nTurns << ", p2 = " << estimatedTurns;
                }*/
#endif

                UnitValueHelper unitValueHelper;
                UnitValueHelper::MapT unitValueData;
                unitValueHelper.addMapEntry(unitValueData, pCityUnitTactics->getUnitType(), possibleCombatUnits, odds);

                tacticValue.unitAnalysisValue = unitValueHelper.getValue(unitValueData[pCityUnitTactics->getUnitType()]);

                if (tacticValue.unitAnalysisValue > 0)
                {
#ifdef ALTAI_DEBUG
                    std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
                    os << " value: " << tacticValue.unitAnalysisValue << ", exp = " << experience << ", level = " << level;
#endif
                    unitSelectionData.insert(tacticValue);
                }
            }
        }
    }

    CityDefenceUnitTactic::CityDefenceUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void CityDefenceUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        UnitData::CombatDetails combatDetails;
        combatDetails.flags = UnitData::CombatDetails::CityAttack;
        applyCombatTactic(pCityUnitTactics, selectionData.cityDefenceUnits, pCityUnitTactics->getCity(), combatDetails, false);
    }

    std::vector<XYCoords> CityDefenceUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        // just return this city for now
        const CvCity* pCity = getCity(city);
        return std::vector<XYCoords>(1, pCity->plot()->getCoords());
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

    ThisCityDefenceUnitTactic::ThisCityDefenceUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void ThisCityDefenceUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        IDInfo cityInfo = pCityUnitTactics->getCity();
        const CvCity* pCity = getCity(cityInfo);
        if (!pCity)
        {
            return;
        }
        
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(cityInfo.eOwner);
        const City& city = player.getCity(cityInfo.iID);

        UnitData::CombatDetails combatDetails;
        combatDetails.flags = UnitData::CombatDetails::CityAttack;
        combatDetails.plotIsHills = pCity->plot()->isHills();
        combatDetails.plotTerrain = pCity->plot()->getTerrainType();
        combatDetails.cultureDefence = gGlobals.getCultureLevelInfo(pCityUnitTactics->getCityData()->getCultureHelper()->getCultureLevel()).getCityDefenseModifier();
        combatDetails.buildingDefence = pCityUnitTactics->getCityData()->getUnitHelper()->getBuildingDefence();

        applyCombatTactic(pCityUnitTactics, selectionData.thisCityDefenceUnits, pCityUnitTactics->getCity(), combatDetails, false);
    }

    std::vector<XYCoords> ThisCityDefenceUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        // just return this city for now
        const CvCity* pCity = getCity(city);
        return std::vector<XYCoords>(1, pCity->plot()->getCoords());
    }

    void ThisCityDefenceUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tThis City defence unit";
        debugPromotions(os, promotions_);
#endif
    }

    void ThisCityDefenceUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeSet(pStream , promotions_);
    }

    void ThisCityDefenceUnitTactic::read(FDataStreamBase* pStream)
    {
        readSet<PromotionTypes, int>(pStream, promotions_);
    }

    CityAttackUnitTactic::CityAttackUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void CityAttackUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        UnitData::CombatDetails combatDetails;
        combatDetails.flags = UnitData::CombatDetails::CityAttack;
        applyCombatTactic(pCityUnitTactics, selectionData.cityAttackUnits, pCityUnitTactics->getCity(), combatDetails, true);
    }

    std::vector<XYCoords> CityAttackUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        // just return this city for now
        const CvCity* pCity = getCity(city);
        return std::vector<XYCoords>(1, pCity->plot()->getCoords());
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

    void CollateralUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        UnitData::CombatDetails combatDetails;
        combatDetails.flags = UnitData::CombatDetails::CityAttack;
        applyCombatTactic(pCityUnitTactics, selectionData.collateralUnits, pCityUnitTactics->getCity(), combatDetails, true);
    }

    std::vector<XYCoords> CollateralUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        // just return this city for now
        const CvCity* pCity = getCity(city);
        return std::vector<XYCoords>(1, pCity->plot()->getCoords());
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

    void FieldDefenceUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        UnitData::CombatDetails combatDetails;
        applyCombatTactic(pCityUnitTactics, selectionData.fieldDefenceUnits, pCityUnitTactics->getCity(), combatDetails, false);
    }

    std::vector<XYCoords> FieldDefenceUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        // just return this city for now
        const CvCity* pCity = getCity(city);
        return std::vector<XYCoords>(1, pCity->plot()->getCoords());
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

    void FieldAttackUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        UnitData::CombatDetails combatDetails;
        applyCombatTactic(pCityUnitTactics, selectionData.fieldAttackUnits, pCityUnitTactics->getCity(), combatDetails, true);
    }

    std::vector<XYCoords> FieldAttackUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        // just return this city for now
        const CvCity* pCity = getCity(city);
        return std::vector<XYCoords>(1, pCity->plot()->getCoords());
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


    void BuildCityUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        IDInfo cityInfo = pCityUnitTactics->getCity();
        Player& player = *gGlobals.getGame().getAltAI()->getPlayer(cityInfo.eOwner);
        City& city = gGlobals.getGame().getAltAI()->getPlayer(cityInfo.eOwner)->getCity(cityInfo.iID);
        UnitTypes unitType = pCityUnitTactics->getUnitType();
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        int estimatedTurns = city.getBaseOutputProjection().getExpectedTurnBuilt(player.getCvPlayer()->getProductionNeeded(unitType), 
            city.getCityData()->getModifiersHelper()->getUnitProductionModifier(unitType),
            city.getCityData()->getModifiersHelper()->getTotalYieldModifier(*city.getCityData())[YIELD_PRODUCTION]);

        if (!ladder.units.empty())
        //if (estimatedTurns >= 0)
        {
            SettlerUnitValue buildValue;
            buildValue.unitType = pCityUnitTactics->getUnitType();
            buildValue.nTurns = ladder.units[0].turns;
            //buildValue.nTurns = estimatedTurns;

            if (gGlobals.getUnitInfo(pCityUnitTactics->getUnitType()).isFoodProduction())
            {
                const ProjectionLadder& ladder = pCityUnitTactics->getProjection();
                buildValue.lostOutput = city.getBaseOutputProjection().getOutput() - ladder.getOutput();
            }

//#ifdef ALTAI_DEBUG
//            if (estimatedTurns != buildValue.nTurns)
//            {
//                const CvPlayer& player = CvPlayerAI::getPlayer(cityInfo.eOwner);
//                std::ostream& os = CivLog::getLog(player)->getStream();
//                os << "\nMismatched turn projections for unit: " << gGlobals.getUnitInfo(unitType).getType()
//                    << " p1 = " << buildValue.nTurns << ", p2 = " << estimatedTurns;
//            }
//#endif

            selectionData.settlerUnits.insert(std::make_pair(buildValue.unitType, buildValue));
        }
    }

    std::vector<XYCoords> BuildCityUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {        
        const CvCity* pCity = getCity(city);
        const int subAreaID = pCity->plot()->getSubArea();

        std::vector<CvPlot*> ignorePlots;
        std::vector<XYCoords> possibleCoords;
        CvPlot* pBestPlot = player.getSettlerManager()->getBestPlot(subAreaID, ignorePlots);
        if (pBestPlot)
        {
            possibleCoords.push_back(pBestPlot->getCoords());
        }

        return possibleCoords;
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

    void BuildImprovementsUnitTactic::applyBuilds_(WorkerUnitValue& unitValue, IDInfo city, const std::vector<PlotImprovementData>& improvements, 
        TotalOutput totalDelta, const std::vector<TechTypes>& impTechs)
    {
        int buildCount = 0;
        // scale totalDelta by number of builds - for tech driven builds where we only have the total delta output
        if (!isEmpty(totalDelta))
        {
            for (size_t i = 0, count = improvements.size(); i < count; ++i)
            {
                if (improvements[i].isSelectedAndNotBuilt())
                {
                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(improvements[i].improvement);

                    if (std::find(buildTypes_.begin(), buildTypes_.end(), buildType) != buildTypes_.end())
                    {
                        ++buildCount;
                    }
                }
            }
        }
        if (buildCount > 0)
        {
            totalDelta /= buildCount;
        }

        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            if (improvements[i].isSelectedAndNotBuilt())
            {
                BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(improvements[i].improvement);

                if (std::find(buildTypes_.begin(), buildTypes_.end(), buildType) != buildTypes_.end())
                {
                    unitValue.addBuild(buildType, boost::make_tuple(improvements[i].coords, city, totalDelta, impTechs));
                }
            }
        }
    }

    void BuildImprovementsUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();
        const CvCity* pCity = getCity(pCityUnitTactics->getCity());

        if (!pCity)
        {
#ifdef ALTAI_DEBUG  // TODO: check why this may happen...
        //    os << "\nBuildImprovementsUnitTactic::apply - called with NULL city";
#endif
            return;
        }

        IDInfo cityInfo = pCityUnitTactics->getCity();
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        boost::shared_ptr<PlayerTactics> pPlayerTactics = pPlayer->getAnalysis()->getPlayerTactics();
        Player& player = *gGlobals.getGame().getAltAI()->getPlayer(cityInfo.eOwner);
        City& city = gGlobals.getGame().getAltAI()->getPlayer(cityInfo.eOwner)->getCity(cityInfo.iID);

        int estimatedTurns = city.getBaseOutputProjection().getExpectedTurnBuilt(pPlayer->getCvPlayer()->getProductionNeeded(pCityUnitTactics->getUnitType()), 
            city.getCityData()->getModifiersHelper()->getUnitProductionModifier(pCityUnitTactics->getUnitType()),
            city.getCityData()->getModifiersHelper()->getTotalYieldModifier(*city.getCityData())[YIELD_PRODUCTION]);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pPlayer->getCvPlayer())->getStream();
#endif
        // can we build the relevant worker unit?
        if (pCityUnitTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None) && estimatedTurns >= 0)
        {
            std::vector<PlotImprovementData> improvements;
            TotalOutput improvementsDelta;
            boost::tie(improvementsDelta, improvements) = getImprovements(*pPlayer, city);

            WorkerUnitValue unitValue;
            unitValue.unitType = pCityUnitTactics->getUnitType();
            unitValue.nTurns = estimatedTurns;

            if (gGlobals.getUnitInfo(pCityUnitTactics->getUnitType()).isFoodProduction())
            {
                unitValue.lostOutput = city.getBaseOutputProjection().getOutput() - ladder.getOutput();
            }

            applyBuilds_(unitValue, pCity->getIDInfo(), improvements, improvementsDelta);

            CityIter iter(*pPlayer->getCvPlayer());

            while (CvCity* pCity = iter())
            {
                // already done ourselves above
                if (pCity->getIDInfo() == pCityUnitTactics->getCity())
                {
                    continue;
                }
                City& city = pPlayer->getCity(pCity);
                std::vector<PlotImprovementData> otherCityImprovements;
                TotalOutput otherCityImprovementsDelta;
                boost::tie(otherCityImprovementsDelta, otherCityImprovements) = getImprovements(*pPlayer, city);

                applyBuilds_(unitValue, pCity->getIDInfo(), otherCityImprovements, otherCityImprovementsDelta);
#ifdef ALTAI_DEBUG
                os << "\nApplying imp base builds for city: " << safeGetCityName(pCity);
                unitValue.debug(os);
#endif
            }

            //if (unitValue.buildsMap.empty())
            //{
                // add builds for techs we might research in the time it takes to construct the unit
                // use information in PlayerTactics city improvements tactics map
                PlayerTactics::CityImprovementTacticsMap::const_iterator improvementTacticsIter = pPlayerTactics->cityImprovementTacticsMap_.find(pCity->getIDInfo());
                if (improvementTacticsIter != pPlayerTactics->cityImprovementTacticsMap_.end())
                {
                    for (PlayerTactics::CityImprovementTacticsList::const_iterator cityImprovementsTacticsIter(improvementTacticsIter->second.begin()),
                        cityImprovementsTacticsEndIter(improvementTacticsIter->second.end()); cityImprovementsTacticsIter != cityImprovementsTacticsEndIter; ++cityImprovementsTacticsIter)
                    {
                        const std::vector<ResearchTechDependencyPtr>& techDeps = (*cityImprovementsTacticsIter)->getTechDependencies();
                        std::vector<TechTypes> impTechs;
                        for (size_t techIndex = 0, techCount = techDeps.size(); techIndex < techCount; ++techIndex)
                        {
                            impTechs.push_back(techDeps[techIndex]->getResearchTech());
                        }
                        TotalOutput delta = (*cityImprovementsTacticsIter)->getProjection().getOutput() - (*cityImprovementsTacticsIter)->getBaseProjection().getOutput();
                        // todo: check tech cost
                        applyBuilds_(unitValue, pCity->getIDInfo(), (*cityImprovementsTacticsIter)->getImprovements(), delta, impTechs);
#ifdef ALTAI_DEBUG
                        os << "\nApplying imp builds for city: " << safeGetCityName(pCity);

                        for (size_t i = 0, count = impTechs.size(); i < count; ++i)
                        {
                            if (i == 0) os << " imp tech(s): "; else os << ", ";
                            os << gGlobals.getTechInfo(impTechs[i]).getType();
                        }
                        unitValue.debug(os);
#endif
                    }
                }
            //}

            // todo: check this works for builds of improvements we don't have the tech for, but can see the resource (e.g. whales)
            if (hasConsumedBuilds_)
            {
                std::vector<int> accessibleSubAreas = pPlayer->getAnalysis()->getMapAnalysis()->
                    getAccessibleSubAreas((DomainTypes)gGlobals.getUnitInfo(pCityUnitTactics->getUnitType()).getDomainType());

                for (size_t subAreaIndex = 0, subAreaCount = accessibleSubAreas.size(); subAreaIndex < subAreaCount; ++subAreaIndex)
                {
                    for (size_t buildTypeIndex = 0, count = buildTypes_.size(); buildTypeIndex < count; ++buildTypeIndex)
                    {
                        std::vector<BonusTypes> bonusTypes = GameDataAnalysis::getBonusTypesForBuildType(buildTypes_[buildTypeIndex]);

                        if (!bonusTypes.empty())
                        {
                            std::vector<CvPlot*> resourcePlots = pPlayer->getAnalysis()->getMapAnalysis()->
                                getResourcePlots(accessibleSubAreas[subAreaIndex], bonusTypes, pPlayer->getPlayerID());

                            for (size_t j = 0, resourceCount = resourcePlots.size(); j < resourceCount; ++j)
                            {
                                // todo - drive improvement check from bonus type's required imp
                                if (resourcePlots[j]->getImprovementType() == NO_IMPROVEMENT && !resourcePlots[j]->isWithinTeamCityRadius(pPlayer->getTeamID()))
                                {
                                    unitValue.addNonCityBuild(buildTypes_[buildTypeIndex], 
                                        boost::make_tuple(resourcePlots[j]->getCoords(), IDInfo(), TotalOutput(), std::vector<TechTypes>()));
                                }
                            }
                        }
                    }
                }
            }

            selectionData.workerUnits.insert(std::make_pair(unitValue.unitType, unitValue));
        }
    }

    std::vector<XYCoords> BuildImprovementsUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        std::vector<PlotImprovementData> improvements;
        TotalOutput improvementsDelta;
        boost::tie(improvementsDelta, improvements) = getImprovements(player, player.getCity(city.iID));
        std::vector<XYCoords> targetCoords;

        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            if (improvements[i].isSelectedAndNotBuilt())
            {
                BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(improvements[i].improvement);

                if (std::find(buildTypes_.begin(), buildTypes_.end(), buildType) != buildTypes_.end())
                {
                    targetCoords.push_back(improvements[i].coords);
                }
            }
        }

        return targetCoords;
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

    void SeaAttackUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        UnitData::CombatDetails combatDetails;
        applyCombatTactic(pCityUnitTactics, selectionData.seaCombatUnits, pCityUnitTactics->getCity(), combatDetails, true);
    }

    std::vector<XYCoords> SeaAttackUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        // just return this city for now
        const CvCity* pCity = getCity(city);
        return std::vector<XYCoords>(1, pCity->plot()->getCoords());
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


    ScoutUnitTactic::ScoutUnitTactic(const Promotions& promotions) : promotions_(promotions)
    {
    }

    void ScoutUnitTactic::apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityUnitTactics->getCity());
        if (!pCity)
        {
            return;
        }

        Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());
        City& city = player.getCity(pCity);

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\nUnit tactics (scout) for city: " << narrow(pCity->getName());
#endif
        int estimatedTurns = city.getBaseOutputProjection().getExpectedTurnBuilt(player.getCvPlayer()->getProductionNeeded(pCityUnitTactics->getUnitType()), 
                city.getCityData()->getModifiersHelper()->getUnitProductionModifier(pCityUnitTactics->getUnitType()),
                city.getCityData()->getModifiersHelper()->getTotalYieldModifier(*city.getCityData())[YIELD_PRODUCTION]);

        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        //if (!ladder.units.empty())
        if (estimatedTurns >= 0)
        {
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(pCityUnitTactics->getUnitType());

            // we count explorers in the the city tactics check for building scouts, so we must only select those here
            // otherwise, we build scouts which we don't then count.
            if (!unitInfo.getUnitAIType(UNITAI_EXPLORE))
            {
                return;
            }

            UnitTacticValue scoutValue;
            scoutValue.unitType = pCityUnitTactics->getUnitType();
            scoutValue.nTurns = estimatedTurns;
            scoutValue.moves = unitInfo.getMoves();
            const int experience = pCityUnitTactics->getCityData()->getUnitHelper()->getUnitFreeExperience(scoutValue.unitType);
            scoutValue.level = player.getAnalysis()->getUnitLevel(experience);
            
            // treat scout units as combat for now
            std::vector<UnitTypes> combatUnits, possibleCombatUnits;
            boost::tie(combatUnits, possibleCombatUnits) = getActualAndPossibleCombatUnits(player, pCity, (DomainTypes)unitInfo.getDomainType());

            scoutValue.unitAnalysisValue = unitInfo.getCombat() * scoutValue.moves * scoutValue.moves;
            if (scoutValue.unitAnalysisValue > 0)
            {
#ifdef ALTAI_DEBUG
                os << "\nAdding scout unit: " << unitInfo.getType() << " with value: " << scoutValue.unitAnalysisValue;
#endif
                selectionData.scoutUnits.insert(scoutValue);
            }
        }
    }

    std::vector<XYCoords> ScoutUnitTactic::getPossibleTargets(Player& player, IDInfo city)
    {
        // just return this city for now
        const CvCity* pCity = getCity(city);
        return std::vector<XYCoords>(1, pCity->plot()->getCoords());
    }

    void ScoutUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tScout unit";
        debugPromotions(os, promotions_);
#endif
    }

    void ScoutUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        writeSet(pStream , promotions_);
    }

    void ScoutUnitTactic::read(FDataStreamBase* pStream)
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

    void BuildSpecialBuildingUnitTactic::apply(const UnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
        Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pUnitTactics->getPlayerType());
        const PlayerTypes playerType = player.getPlayerID();
        std::map<int, ICityBuildingTacticsPtr> cityBuildingTactics = player.getAnalysis()->getPlayerTactics()->getCitySpecialBuildingTactics(buildingType_);

        CityIter iter(CvPlayerAI::getPlayer(pUnitTactics->getPlayerType()));
        CvCity* pCity;
        while (pCity = iter())
        {
            if (!pCity->canConstruct(buildingType_, false, false, true))
            {
                continue;
            }

            const int cityID = pCity->getID();
            std::map<int, ICityBuildingTacticsPtr>::iterator cityTacticIter = cityBuildingTactics.find(cityID);

            if (cityTacticIter != cityBuildingTactics.end())
            {
                City& city = gGlobals.getGame().getAltAI()->getPlayer(playerType)->getCity(cityID);

                cityTacticIter->second->update(player, city.getCityData()->clone());
                cityTacticIter->second->apply(selectionData);
            }
        }
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

    void DiscoverTechUnitTactic::apply(const UnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
        TechTypes discoverTech = getDiscoveryTech(pUnitTactics->getUnitType(), pUnitTactics->getPlayerType());

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(pUnitTactics->getPlayerType()))->getStream();
        os << "\n\tDiscoverTechUnitTactic::apply - can discover tech = " << (discoverTech == NO_TECH ? " none " : gGlobals.getTechInfo(discoverTech).getType());
#endif

        if (discoverTech != NO_TECH)
        {
            selectionData.possibleFreeTech = discoverTech;
            //selectionData.freeTechValue = calculateTechResearchCost(discoverTech, pUnitTactics->getPlayerType());
        }
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

    void CreateGreatWorkUnitTactic::apply(const UnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
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

    void TradeMissionUnitTactic::apply(const UnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void TradeMissionUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void TradeMissionUnitTactic::read(FDataStreamBase* pStream)
    {
    }


    JoinCityUnitTactic::JoinCityUnitTactic(SpecialistTypes specType)
        : specType_(specType)
    {
    }

    void JoinCityUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tJoin city: " << (specType_ == NO_SPECIALIST ? "NONE" : gGlobals.getSpecialistInfo(specType_).getType());
#endif
    }

    void JoinCityUnitTactic::apply(const UnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pUnitTactics->getPlayerType());
        const PlayerTypes playerType = player.getPlayerID();
        const int numSimTurns = player.getAnalysis()->getNumSimTurns();

        CityIter iter(CvPlayerAI::getPlayer(pUnitTactics->getPlayerType()));
        CvCity* pCity;
        while (pCity = iter())
        {
            const int cityID = pCity->getID();
            City& city = gGlobals.getGame().getAltAI()->getPlayer(playerType)->getCity(cityID);
            CityDataPtr pBaseCityData = city.getCityData()->clone();
            CityDataPtr pCityData = city.getCityData()->clone();

            std::vector<IProjectionEventPtr> events;
            ProjectionLadder base = getProjectedOutput(player, pBaseCityData, numSimTurns, events, ConstructItem(), __FUNCTION__, false);

            updateRequestData(*pCityData, specType_);
            events.clear();
            ProjectionLadder ladder = getProjectedOutput(player, pCityData, numSimTurns, events, ConstructItem(), __FUNCTION__, false);

            SettledSpecialistValue value;
            value.city = pCity->getIDInfo();
            value.output = ladder.getOutput() - base.getOutput();
            value.specType = specType_;

            selectionData.settledSpecialists.insert(value);
        }
    }

    void JoinCityUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(specType_);
    }
        
    void JoinCityUnitTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&specType_);
    }


    HurryBuildingUnitTactic::HurryBuildingUnitTactic(int baseHurry, int multiplier)
        : baseHurry_(baseHurry), multiplier_(multiplier)
    {
    }

    void HurryBuildingUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHurry building with unit tactic";
#endif
    }

    void HurryBuildingUnitTactic::apply(const UnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void HurryBuildingUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(baseHurry_);
        pStream->Write(multiplier_);
    }

    void HurryBuildingUnitTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read(&baseHurry_);
        pStream->Read(&multiplier_);
    }
}
