#include "./unit_tactics_items.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./city_tactics.h"
#include "./civ_log.h"

#include "./unit_analysis.h"
#include "./player_analysis.h"

namespace AltAI
{
    void CityDefenceUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
        const CvCity* pCity = getCity(pCityUnitTactics->getCity());
        const City& city = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getCity(pCity->getID());

        const ProjectionLadder& ladder = pCityUnitTactics->getProjection();

        if (pCityUnitTactics->getDependencies().empty() && !ladder.units.empty())
        {
            CityDefenceUnitValue defenceValue;
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
#endif
    }

    void CityDefenceUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void CityDefenceUnitTactic::read(FDataStreamBase* pStream)
    {
    }


    void CityAttackUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void CityAttackUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCity attack unit";
#endif
    }

    void CityAttackUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void CityAttackUnitTactic::read(FDataStreamBase* pStream)
    {
    }


    void CollateralUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void CollateralUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tCollateral unit";
#endif
    }

    void CollateralUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void CollateralUnitTactic::read(FDataStreamBase* pStream)
    {
    }


    void FieldDefenceUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void FieldDefenceUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tField defence unit";
#endif
    }

    void FieldDefenceUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void FieldDefenceUnitTactic::read(FDataStreamBase* pStream)
    {
    }


    void FieldAttackUnitTactic::apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData)
    {
    }

    void FieldAttackUnitTactic::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\n\tField attack unit";
#endif
    }

    void FieldAttackUnitTactic::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void FieldAttackUnitTactic::read(FDataStreamBase* pStream)
    {
    }
}