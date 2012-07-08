#include "./building_tactics_items.h"
#include "./city_data.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./city_tactics.h"
#include "./civ_log.h"
#include "./save_utils.h"

namespace AltAI
{
    void EconomicBuildingTactic::apply(const ICityBuildingTacticsPtr& pCityBuildingTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityBuildingTactics->getCity());
        const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity->getID());

        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        if (pCityBuildingTactics->getDependencies().empty() && pCity->canConstruct(pCityBuildingTactics->getBuildingType(), true) && !ladder.buildings.empty())
        {
            EconomicBuildingValue economicValue;

            economicValue.buildingType = pCityBuildingTactics->getBuildingType();
            economicValue.output = ladder.getOutput() - city.getCurrentOutputProjection().getOutput();

            economicValue.nTurns = ladder.buildings[0].first;

            selectionData.economicBuildings.insert(economicValue);
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
        const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity->getID());

        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        if (pCityBuildingTactics->getDependencies().empty() && pCity->canConstruct(pCityBuildingTactics->getBuildingType(), true) && !ladder.buildings.empty())
        {
            CultureBuildingValue cultureValue;

            cultureValue.buildingType = pCityBuildingTactics->getBuildingType();
            cultureValue.output = ladder.getOutput() - city.getCurrentOutputProjection().getOutput();
            cultureValue.nTurns = ladder.buildings[0].first;

            selectionData.smallCultureBuildings.insert(cultureValue);
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
    }

    void CultureBuildingTactic::read(FDataStreamBase* pStream)
    {
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
        const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity->getID());

        const ProjectionLadder& ladder = pCityBuildingTactics->getProjection();

        if (pCityBuildingTactics->getDependencies().empty() && !ladder.buildings.empty())
        {
            MilitaryBuildingValue buildingValue;
            buildingValue.freeExperience = freeExperience;
            buildingValue.globalFreeExperience = globalFreeExperience;
            buildingValue.domainFreeExperience = domainFreeExperience;
            buildingValue.combatTypeFreeExperience = combatTypeFreeExperience;
            buildingValue.freePromotion = freePromotion;

            buildingValue.buildingType = pCityBuildingTactics->getBuildingType();
            buildingValue.nTurns = ladder.buildings[0].first;

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
}