#include "AltAI.h"

#include "./unit_city_defence_mission.h"
#include "./unit_log.h"
#include "./game.h"
#include "./city.h"
#include "./player.h"
#include "./military_tactics.h"
#include "./player_analysis.h"
#include "./helper_fns.h"

namespace AltAI
{
    MilitaryMissionDataPtr CityDefenceMission::createMission()
    {
        MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_GUARD_CITY));
        pMission->pUnitsMission = IUnitsMissionPtr(new CityDefenceMission());
        return pMission;
    }

    bool CityDefenceMission::doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission)
    {
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner());
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
        bool isAttackMove = !isEmpty(pMission->nextAttack.first);
        // todo - maybe reassign unit to counter mission and use its code
        if (isAttackMove)
        {
            std::pair<IDInfo, XYCoords> attackData = pMission->nextAttack;
            pMission->nextAttack = std::make_pair(IDInfo(), XYCoords());  // clear next attacker so we don't loop

            if (pUnit->getGroupSize() > 1)
            {
#ifdef ALTAI_DEBUG
                os << "\nsplitting group for attack unit: " << pUnit->getIDInfo();
#endif
                pUnit->joinGroup(NULL);
            }
#ifdef ALTAI_DEBUG
                
            os << "\nMission: " << pMission << " turn = " << gGlobals.getGame().getGameTurn() << " Unit: " << pUnit->getIDInfo() << " pushed sirt defence mission attack move to: " << attackData.second;
#endif
            pMission->pushedAttack = attackData;  // store this as useful for updating combat data after combat is resolved
            pUnit->getGroup()->pushMission(MISSION_MOVE_TO, attackData.second.iX, attackData.second.iY, MOVE_IGNORE_DANGER, false, false, MISSIONAI_COUNTER, 0, 0, __FUNCTION__);
            return true;
        }

        const CvCity* pTargetCity = pMission->getTargetCity();
        if (pTargetCity)
        {                        
            if (pUnit->at(pTargetCity->getX(), pTargetCity->getY()))
            {
#ifdef ALTAI_DEBUG
                os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission skip for city (guard): " << safeGetCityName(pTargetCity);
#endif                       
                pUnit->getGroup()->pushMission(MISSION_SKIP, pUnit->plot()->getX(), pUnit->plot()->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
            }
            else
            {
#ifdef ALTAI_DEBUG
                os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission move to for city (guard): " << safeGetCityName(pTargetCity);
#endif
                pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pTargetCity->getX(), pTargetCity->getY(), MOVE_IGNORE_DANGER, false, false, MISSIONAI_GUARD_CITY,
                    (CvPlot*)pTargetCity->plot(), 0, __FUNCTION__);
            }
            return true;
        }
        return false;
    }

    void CityDefenceMission::update(Player& player)
    {
    }

    void CityDefenceMission::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
    }

    void CityDefenceMission::read(FDataStreamBase* pStream)
    {
    }
}
