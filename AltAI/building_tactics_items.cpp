#include "AltAI.h"

#include "./building_tactics_items.h"
#include "./tactic_selection_data.h"
#include "./city_data.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./city_tactics.h"
#include "./city_unit_tactics.h"
#include "./building_info_visitors.h"
#include "./building_helper.h"
#include "./modifiers_helper.h"
#include "./tech_info_visitors.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./helper_fns.h"
#include "./save_utils.h"

namespace AltAI
{
    void EconomicBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityBuildingTactics->getCity());
        City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity);
        CityDataPtr pCityData = pCityBuildingTactics->getCityData();

        if (pCityBuildingTactics->getComparisonFlag() != ICityBuildingTactics::No_Comparison)
        {
            const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

            if (!ladder.buildings.empty())  // should have a comparison ladder
            {
                EconomicBuildingValue economicValue;

                economicValue.buildingType = pCityBuildingTactics->getBuildingType();
                economicValue.city = pCityBuildingTactics->getCity();
                economicValue.output = ladder.getOutput() - city.getBaseOutputProjection().getOutput();
                economicValue.nTurns = ladder.buildings[0].first;
                /*int estimatedTurns = city.getBaseOutputProjection().getExpectedTurnBuilt(pCityBuildingTactics->getBuildingCost() - pCity->getBuildingProduction(pCityBuildingTactics->getBuildingType()), 
                    pCityData->getModifiersHelper()->getBuildingProductionModifier(*pCityData, pCityBuildingTactics->getBuildingType()),
                    pCityData->getModifiersHelper()->getTotalYieldModifier(*pCityData)[YIELD_PRODUCTION]);*/

                /*if (estimatedTurns != economicValue.nTurns)
                {
#ifdef ALTAI_DEBUG
                    const CvPlayer& player = CvPlayerAI::getPlayer(pCity->getOwner());
                    std::ostream& os = CivLog::getLog(player)->getStream();
                    os << "\nMismatched turn projections for building: " << gGlobals.getBuildingInfo(economicValue.buildingType).getType()
                        << " p1 = " << economicValue.nTurns << ", p2 = " << estimatedTurns;
#endif
                }*/

                selectionData.economicBuildings.insert(economicValue);
            }
        }
    }

    void EconomicBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tGeneral economic building";
#endif
    }

    void EconomicBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void EconomicBuildingTactic::read(FDataStreamBase* pStream)
    {
    }


    void FoodBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
    }

    void FoodBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tFood building";
#endif
    }

    void FoodBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void FoodBuildingTactic::read(FDataStreamBase* pStream)
    {
    }


    void HappyBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
    }

    void HappyBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHappy building";
#endif
    }

    void HappyBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void HappyBuildingTactic::read(FDataStreamBase* pStream)
    {
    }


    void HealthBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
    }

    void HealthBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tHealth building";
#endif
    }

    void HealthBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void HealthBuildingTactic::read(FDataStreamBase* pStream)
    {
    }


    void GoldBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
    }

    void GoldBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tGold building";
#endif
    }

    void GoldBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void GoldBuildingTactic::read(FDataStreamBase* pStream)
    {
    }


    void ScienceBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
    }

    void ScienceBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tScience building";
#endif
    }

    void ScienceBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void ScienceBuildingTactic::read(FDataStreamBase* pStream)
    {
    }


    void CultureBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityBuildingTactics->getCity());
        City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity);
        CityDataPtr pCityData = pCityBuildingTactics->getCityData();

        int estimatedTurns = city.getBaseOutputProjection().getExpectedTurnBuilt(
            pCityBuildingTactics->getBuildingCost() - 
                pCity->getBuildingProduction(pCityBuildingTactics->getBuildingType()), 
            pCityData->getModifiersHelper()->getBuildingProductionModifier(*pCityData, pCityBuildingTactics->getBuildingType()),
            pCityData->getModifiersHelper()->getTotalYieldModifier(*pCityData)[YIELD_PRODUCTION]);

        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        if (estimatedTurns >= 0)
        {
            CultureBuildingValue cultureValue;

            cultureValue.buildingType = pCityBuildingTactics->getBuildingType();
            cultureValue.city = pCityBuildingTactics->getCity();
            cultureValue.output = ladder.getOutput() - city.getBaseOutputProjection().getOutput();
            cultureValue.nTurns = estimatedTurns;

            bool needCulture = pCity->getCultureLevel() == 1 && pCity->getCommerceRate(COMMERCE_CULTURE) == 0;
            bool culturePressure = pCityData->getNumUncontrolledPlots(true) > 0;

            if (needCulture && !isLimitedWonderClass(getBuildingClass(cultureValue.buildingType)))
            {
                selectionData.smallCultureBuildings.insert(cultureValue);
            }
            else if (culturePressure)
            {
                selectionData.smallCultureBuildings.insert(cultureValue);
            }
            else
            {
                selectionData.largeCultureBuildings.insert(cultureValue);
            }
        }
    }

    void CultureBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCulture building";
#endif
    }

    void CultureBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(baseCommerce);
        pStream->Write(baseGlobalCommerce);
    }

    void CultureBuildingTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read(&baseCommerce);
        pStream->Read(&baseGlobalCommerce);
    }


    void EspionageBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
    }

    void EspionageBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tEspionage building";
#endif
    }

    void EspionageBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void EspionageBuildingTactic::read(FDataStreamBase* pStream)
    {
    }


    void SpecialistBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
    }
       
    void SpecialistBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tSpecialist building";
#endif
    }  

    void SpecialistBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void SpecialistBuildingTactic::read(FDataStreamBase* pStream)
    {
    }


    GovCenterTactic::GovCenterTactic(bool isNewCenter) : isNewCenter_(isNewCenter)
    {
    }

    void GovCenterTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        // TODO - for now to stop pointless palace moves
        if (!isNewCenter_)
        {            
            selectionData.exclusions.insert(pCityBuildingTactics->getBuildingType());
        }
    }

    void GovCenterTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tGov center tactic " << (isNewCenter_ ? " (new center) " : " (move existing center) ");
#endif
    }

    void GovCenterTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(isNewCenter_);
    }

    void GovCenterTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read(&isNewCenter_);
    }


    UnitExperienceTactic::UnitExperienceTactic(int freeExperience_, int globalFreeExperience_, 
        const std::vector<std::pair<DomainTypes, int> >& domainFreeExperience_,
        const std::vector<std::pair<UnitCombatTypes, int> >& combatTypeFreeExperience_,
        PromotionTypes freePromotion_)
        : freeExperience(freeExperience_), globalFreeExperience(globalFreeExperience_), freePromotion(freePromotion_)
    {
        for (size_t i = 0, count = domainFreeExperience_.size(); i < count; ++i)
        {
            domainFreeExperience.insert(domainFreeExperience_[i]);
        }

        for (size_t i = 0, count = combatTypeFreeExperience_.size(); i < count; ++i)
        {
            combatTypeFreeExperience.insert(combatTypeFreeExperience_[i]);
        }
    }

    void UnitExperienceTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {        
        const CvCity* pCity = getCity(pCityBuildingTactics->getCity());
        City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity);
        CityDataPtr pCityData = pCityBuildingTactics->getCityData();
        Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());

        int estimatedTurns = city.getBaseOutputProjection().getExpectedTurnBuilt(pCityBuildingTactics->getBuildingCost() - 
                pCity->getBuildingProduction(pCityBuildingTactics->getBuildingType()), 
            pCityData->getModifiersHelper()->getBuildingProductionModifier(*pCityData, pCityBuildingTactics->getBuildingType()),
            pCityData->getModifiersHelper()->getTotalYieldModifier(*pCityData)[YIELD_PRODUCTION]);

        if (estimatedTurns >= 0)  // building built
        {
            MilitaryBuildingValue buildingValue(pCity->getIDInfo());
            buildingValue.freeExperience = freeExperience;
            buildingValue.globalFreeExperience = globalFreeExperience;
            buildingValue.domainFreeExperience = domainFreeExperience;
            buildingValue.combatTypeFreeExperience = combatTypeFreeExperience;
            buildingValue.freePromotion = freePromotion;

            buildingValue.buildingType = pCityBuildingTactics->getBuildingType();
            buildingValue.nTurns = estimatedTurns;

            CityDataPtr pCopyCityData = pCityData->clone();
            updateRequestData(*pCopyCityData, player.getAnalysis()->getBuildingInfo(pCityBuildingTactics->getBuildingType()));

            TacticSelectionData buildingUnitsData;
            boost::shared_ptr<PlayerTactics> pPlayerTactics = player.getAnalysis()->getPlayerTactics();

            for (PlayerTactics::UnitTacticsMap::const_iterator iter(pPlayerTactics->unitTacticsMap_.begin()),
                endIter(pPlayerTactics->unitTacticsMap_.end()); iter != endIter; ++iter)
            {
                if (gGlobals.getUnitInfo(iter->first).getCombat() <= 0)
                {
                    continue;
                }
                // todo - means we ignore any units with a new tech if this is a tech tactic selection calling
                if (!iter->second->areTechDependenciesSatisfied(player))
                {
                    continue;
                }

                CityUnitTacticsPtr pCityUnitTactics = iter->second->getCityTactics(pCityBuildingTactics->getCity());
                if (pCityUnitTactics && pCityUnitTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                {
                    
                    pCityUnitTactics->update(player, pCopyCityData);
                    pCityUnitTactics->apply(buildingUnitsData, IDependentTactic::Ignore_None);
                }
            }

            buildingValue.cityAttackUnits = buildingUnitsData.cityAttackUnits;
            buildingValue.cityDefenceUnits = buildingUnitsData.cityDefenceUnits;
            buildingValue.collateralUnits = buildingUnitsData.collateralUnits;

            selectionData.militaryBuildings.insert(buildingValue);        
        }
    }

    void UnitExperienceTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tUnit experience building";
#endif
    }

    void UnitExperienceTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        pStream->Write(freeExperience);
        pStream->Write(globalFreeExperience);

        writeMap<DomainTypes, int>(pStream, domainFreeExperience);
        writeMap<UnitCombatTypes, int>(pStream, combatTypeFreeExperience);

        pStream->Write(freePromotion);
    }

    void UnitExperienceTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read(&freeExperience);
        pStream->Read(&globalFreeExperience);

        readMap<DomainTypes, int, int, int>(pStream, domainFreeExperience);
        readMap<UnitCombatTypes, int, int, int>(pStream, combatTypeFreeExperience);

        pStream->Read((int*)&freePromotion);
    }


    void CityDefenceBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityBuildingTactics->getCity());
        City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity);
        CityDataPtr pCityData = pCityBuildingTactics->getCityData();
        Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner());

        int estimatedTurns = city.getBaseOutputProjection().getExpectedTurnBuilt(pCityBuildingTactics->getBuildingCost() - 
                pCity->getBuildingProduction(pCityBuildingTactics->getBuildingType()), 
            pCityData->getModifiersHelper()->getBuildingProductionModifier(*pCityData, pCityBuildingTactics->getBuildingType()),
            pCityData->getModifiersHelper()->getTotalYieldModifier(*pCityData)[YIELD_PRODUCTION]);

        if (estimatedTurns >= 0)
        {
            // todo - compare on defence modifiers, etc...
            MilitaryBuildingValue buildingValue(pCity->getIDInfo());
            buildingValue.cityDefence = cityDefence;
            buildingValue.globalCityDefence = globalCityDefence;
            buildingValue.bombardDefence = bombardDefence;

            buildingValue.buildingType = pCityBuildingTactics->getBuildingType();
            buildingValue.nTurns = estimatedTurns;

            CityDataPtr pCopyCityData = pCityData->clone();
            updateRequestData(*pCopyCityData, player.getAnalysis()->getBuildingInfo(pCityBuildingTactics->getBuildingType()));

            TacticSelectionData buildingUnitsData;
            boost::shared_ptr<PlayerTactics> pPlayerTactics = player.getAnalysis()->getPlayerTactics();

            for (PlayerTactics::UnitTacticsMap::const_iterator iter(pPlayerTactics->unitTacticsMap_.begin()),
                endIter(pPlayerTactics->unitTacticsMap_.end()); iter != endIter; ++iter)
            {
                if (gGlobals.getUnitInfo(iter->first).getCombat() <= 0)
                {
                    continue;
                }
                // todo - means we ignore any units with a new tech if this is a tech tactic selection calling
                if (!iter->second->areTechDependenciesSatisfied(player))
                {
                    continue;
                }

                CityUnitTacticsPtr pCityUnitTactics = iter->second->getCityTactics(pCityBuildingTactics->getCity());
                if (pCityUnitTactics && pCityUnitTactics->areDependenciesSatisfied(IDependentTactic::Ignore_None))
                {                    
                    pCityUnitTactics->update(player, pCopyCityData);
                    pCityUnitTactics->apply(buildingUnitsData, IDependentTactic::Ignore_None);
                }
            }

            buildingValue.thisCityDefenceUnits = buildingUnitsData.thisCityDefenceUnits;
            selectionData.militaryBuildings.insert(buildingValue);
        }
    }

    void CityDefenceBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCity Defence building";
#endif
    }

    void CityDefenceBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        pStream->Write(cityDefence);
        pStream->Write(globalCityDefence);
        pStream->Write(bombardDefence);
    }

    void CityDefenceBuildingTactic::read(FDataStreamBase* pStream)
    {
        pStream->Read(&cityDefence);
        pStream->Read(&globalCityDefence);
        pStream->Read(&bombardDefence);
    }


    void FreeTechBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();
        if (ladder.buildings.empty())
        {
            return;
        }

        const PlayerTypes playerType = pCityBuildingTactics->getCity().eOwner;
        PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType);
        const CvPlayer& player = CvPlayerAI::getPlayer(playerType);
        const CvTeam& team = CvTeamAI::getTeam(player.getTeam());

#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(player)->getStream();
        os << "\nChecking free tech data for building: " << gGlobals.getBuildingInfo(pCityBuildingTactics->getBuildingType()).getType();
#endif
        TechTypes currentResearch = pPlayer->getCvPlayer()->getCurrentResearch();
        std::list<TechTypes> prereqTechs;

        if (currentResearch != NO_TECH)
        {
            int remainingTechCost = team.getResearchLeft(currentResearch);
            const int rate = player.calculateResearchRate(currentResearch);
            const int approxTurns = 1 + remainingTechCost / rate;
#ifdef ALTAI_DEBUG
            os << "\nBuilding takes: " << ladder.buildings[0].first << " turns";
            os << " current research = " << (currentResearch == NO_TECH ? " none " : gGlobals.getTechInfo(currentResearch).getType()) << " approx turns left = " << approxTurns
                << ", cost = " << remainingTechCost << ", rate = " << rate;
#endif
            // add techs in order to better calculate available set when building completed
            if (approxTurns <= ladder.buildings[0].first)
            {
                prereqTechs = pushTechAndPrereqs(currentResearch, *pPlayer);
                pPlayer->getAnalysis()->recalcTechDepths();
            }
        }

        std::vector<TechTypes> techs = pPlayer->getAnalysis()->getTechsWithDepth(1);

        int maxCost = 0;
        TechTypes possibleTechSelection = NO_TECH;
        for (size_t i = 0, count = techs.size(); i < count; ++i)
        {
            const int thisCost = calculateTechResearchCost(techs[i], playerType);
            if (thisCost > maxCost)
            {
                maxCost = thisCost;
                possibleTechSelection = techs[i];
            }
#ifdef ALTAI_DEBUG
            os << "\n\tTech: " << gGlobals.getTechInfo(techs[i]).getType() << " has depth = 1 and research cost: " << thisCost;
#endif
        }

        // remove techs we added temporarily
        for (std::list<TechTypes>::const_iterator ci(prereqTechs.begin()), ciEnd(prereqTechs.end()); ci != ciEnd; ++ci)
        {
            pPlayer->getCivHelper()->removeTech(*ci);
        }
        if (currentResearch != NO_TECH)
        {
            pPlayer->getCivHelper()->removeTech(currentResearch);
            pPlayer->getAnalysis()->recalcTechDepths();
        }        

        selectionData.possibleFreeTech = possibleTechSelection;
    }

    void FreeTechBuildingTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tFree tech building";
#endif
    }

    void FreeTechBuildingTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void FreeTechBuildingTactic::read(FDataStreamBase* pStream)
    {
    }

}