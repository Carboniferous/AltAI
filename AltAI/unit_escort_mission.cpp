#include "AltAI.h"

#include "./unit_escort_mission.h"
#include "./unit_mission_helper.h"
#include "./helper_fns.h"
#include "./iters.h"
#include "./unit_log.h"
#include "./civ_log.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./worker_tactics.h"
#include "./military_tactics.h"
#include "./settler_manager.h"

namespace AltAI
{
    MilitaryMissionDataPtr SettlerEscortMission::createMission(int subArea)
    {
        MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_ESCORT));
        pMission->pUnitsMission = IUnitsMissionPtr(new SettlerEscortMission(subArea));
        return pMission;
    }

    SettlerEscortMission::SettlerEscortMission(int subArea) : subArea_(subArea)
    {
    }

    bool SettlerEscortMission::doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission)
    {
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner());
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();

#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
        const int currentTurn = gGlobals.getGame().getGameTurn();
#endif
        const CvPlot* pUnitPlot = pUnit->plot();

        std::vector<IDInfo> settlerUnits = pMission->getOurMatchingUnits(CanFoundP());
        const bool haveSettler = !settlerUnits.empty(), isSettler = pUnit->isFound();

        if (haveSettler)
        {
            CvUnit* pSettlerUnit = ::getUnit(settlerUnits[0]);

            if (!isSettler) // then we're an escort unit
            {
                const CvPlot* pSettlerPlot = pSettlerUnit->plot();
                // at plot with settler - join its group
                if (pUnitPlot == pSettlerPlot && pUnit->getGroup() != pSettlerUnit->getGroup())
                {
#ifdef ALTAI_DEBUG
                    os << "\nUnit: " << pUnit->getIDInfo() << " joined group: " << pSettlerUnit->getGroupID();
#endif
                    pUnit->joinGroup(pSettlerUnit->getGroup());
                    return true;
                }
                // not at settler's plot, but in same sub area - try and move towards settler
                else if (pUnitPlot->getSubArea() == pSettlerPlot->getSubArea())
                {
                    const CvPlot* pMovePlot = getNextMovePlot(player, pUnit->getGroup(), pSettlerUnit->plot());
                    CvUnit* pSettlerGroupHeadUnit = pSettlerUnit->getGroup()->getHeadUnit();
                    if (pMovePlot && pSettlerGroupHeadUnit)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " pushed settler escort mission move to: " << pMovePlot->getCoords();
#endif
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pSettlerGroupHeadUnit->getOwner(), pSettlerGroupHeadUnit->getID(),
                            MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY, false, false, MISSIONAI_GROUP, (CvPlot*)pMovePlot, pSettlerGroupHeadUnit, __FUNCTION__);
                        return true;
                    }
                }
                else // todo - find transport
                {
                    return false;
                }
            }
            else
            {
                // assume just one settler per mission for now at least                            
                bool haveCurrentTarget = !isEmpty(pMission->targetCoords);
                const CvPlot* pTargetPlot;
                if (haveCurrentTarget)
                {
                    pTargetPlot = gGlobals.getMap().plot(pMission->targetCoords.iX, pMission->targetCoords.iY);
                }
                CvPlot* pDestPlot = player.getBestPlot(pSettlerUnit, 
                    haveCurrentTarget ? pTargetPlot->getSubArea() : pSettlerUnit->plot()->getSubArea());
                if (pDestPlot && pUnit->plot() == pDestPlot &&
                    !pUnit->canFound(pDestPlot))  // don't want to push mission if we can't actually found here - otherwise risk getting stuck in a loop - need to find a new destination
                {
                    // may want to mark this plot, as it's a plot we can't found at, but we can't see the offending city blocking it
                    pDestPlot = player.getSettlerManager()->getBestPlot(pSettlerUnit->plot()->getSubArea(), std::vector<CvPlot*>(1, pDestPlot));
#ifdef ALTAI_DEBUG
                    os << "\nUnit: " << pUnit->getIDInfo() << " reselecting target plot to: " << (pDestPlot ? pDestPlot->getCoords() : XYCoords());
#endif
                }

                bool mayNeedEscort = player.getCvPlayer()->getNumCities() > 0 && !pMission->requiredUnits.empty();
                if (pMission->assignedUnits.size() > 1)
                {
                    std::set<CvSelectionGroup*, CvSelectionGroupOrderF> assignedGroups = pMission->getAssignedGroups();
                    mayNeedEscort = assignedGroups.size() > 1;
                }
                            
                if (pDestPlot)
                {
#ifdef ALTAI_DEBUG
                    os << "\nUnit: " << pUnit->getIDInfo() << " updating target plot to: " << pDestPlot->getCoords();
#endif                        
                    pMission->targetCoords = pDestPlot->getCoords();

                    // if at best plot - found if safe
                    // otherwise move towards best plot, if not owned area step await escort
                    if (pUnit->plot() == pDestPlot)
                    {                            
                        PlotSet moveToPlotSet;
                        moveToPlotSet.insert(pDestPlot);
                        std::set<IDInfo> hostileUnits = player.getAnalysis()->getMilitaryAnalysis()->getUnitsThreateningPlots(moveToPlotSet);
                        if (hostileUnits.empty())
                        {
                            pUnit->getGroup()->pushMission(MISSION_FOUND, -1, -1, 0, false, false, MISSIONAI_FOUND, 0, 0, __FUNCTION__);
                            return true;
                        }
                        else
                        {
                            const CvPlot* pMovePlot = getNextMovePlot(player, pUnit->getGroup(), pDestPlot);
                            if (pMovePlot != pDestPlot)
                            {
                                const std::map<XYCoords, std::list<IDInfo> >& enemyStacks = player.getAnalysis()->getMilitaryAnalysis()->getEnemyStacks();
                                std::map<XYCoords, std::list<IDInfo> >::const_iterator stackIter = enemyStacks.find(pMovePlot->getCoords());
                                if (stackIter != enemyStacks.end())
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nUnit: " << pUnit->getIDInfo() << " move plot has hostiles - pushed attack mission move to: " << pMovePlot->getCoords();
#endif
                                    pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMovePlot->getX(), pMovePlot->getY(), MOVE_IGNORE_DANGER, false, false, NO_MISSIONAI,
                                    (CvPlot*)pMovePlot, 0, __FUNCTION__);
                                    return true;
                                }
                                else
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nUnit: " << pUnit->getIDInfo() << " settle plot has hostiles - pushed mission move to: " << pMovePlot->getCoords();
#endif
                                    pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMovePlot->getX(), pMovePlot->getY(), MOVE_IGNORE_DANGER, false, false, NO_MISSIONAI,
                                        (CvPlot*)pMovePlot, 0, __FUNCTION__);
                                    return true;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (pDestPlot->getRevealedOwner(player.getTeamID(), false) == NO_PLAYER && mayNeedEscort)
                        {
                            if (pUnitPlot->isCity())
                            {
#ifdef ALTAI_DEBUG
                                os << "\nUnit: " << pUnit->getIDInfo() << " settle plot needs escort - pushed mission skip, settle plot = " << pDestPlot->getCoords();
#endif
                                pUnit->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                                return true;
                            }
                            else if (pUnitPlot->getOwner() != player.getPlayerID())
                            {
                                PlotSet moveToPlotSet;
                                moveToPlotSet.insert(pUnitPlot);
                                const CvPlot* pMovePlot = getNextMovePlot(player, pUnit->getGroup(), pDestPlot);
                                if (pMovePlot != pUnitPlot)
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nUnit: " << pUnit->getIDInfo() << " pushed escape mission move to: " << pMovePlot->getCoords();
#endif
                                    pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMovePlot->getX(), pMovePlot->getY(), MOVE_IGNORE_DANGER, false, false, NO_MISSIONAI,
                                        (CvPlot*)pMovePlot, 0, __FUNCTION__);
                                    return true;
                                }
                                else
                                {
#ifdef ALTAI_DEBUG
                                    os << "\nUnit: " << pUnit->getIDInfo() << " settle plot needs escort (outside city!) - pushed mission skip, settle plot = " << pDestPlot->getCoords();
#endif
                                    pUnit->getGroup()->pushMission(MISSION_SKIP, -1, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                                    return true;
                                }
                            }
                        }

                        const CvPlot* pMovePlot = getNextMovePlot(player, pUnit->getGroup(), pDestPlot);

                        if (pMovePlot)
                        {
#ifdef ALTAI_DEBUG
                            os << "\nUnit: " << pUnit->getIDInfo() << " pushed mission move to: " << pMovePlot->getCoords();
#endif
                            pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pMovePlot->getX(), pMovePlot->getY(), MOVE_IGNORE_DANGER, false, false, NO_MISSIONAI,
                                (CvPlot*)pMovePlot, 0, __FUNCTION__);
                            return true;
                        }
                    }                            
                }
            }
        }

        // check for other missions which needs escort, otherwise maybe add to reserve?
        return false;
    }

    void SettlerEscortMission::update(Player& player)
    {
    }

    void SettlerEscortMission::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(subArea_);
    }

    void SettlerEscortMission::read(FDataStreamBase* pStream)
    {
        pStream->Read(&subArea_);
    }

    MilitaryMissionDataPtr WorkerEscortMission::createMission(int subArea)
    {
        MilitaryMissionDataPtr pMission(new MilitaryMissionData(MISSIONAI_ESCORT_WORKER));
        pMission->pUnitsMission = IUnitsMissionPtr(new WorkerEscortMission(subArea));
        return pMission;
    }

    WorkerEscortMission::WorkerEscortMission(int subArea) : subArea_(subArea)
    {
    }

    bool WorkerEscortMission::doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission)
    {
        const Player& player = *gGlobals.getGame().getAltAI()->getPlayer(pUnit->getOwner());
        MilitaryAnalysisPtr pMilitaryAnalysis = player.getAnalysis()->getMilitaryAnalysis();

#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
        const CvPlot* pUnitPlot = pUnit->plot();

        if (!pMission->targetUnits.empty())
        {
            const CvUnit* pTargetUnit = ::getUnit(*pMission->targetUnits.begin());
            if (pTargetUnit)
            {
                const CvPlot* pTargetUnitPlot = pTargetUnit->plot();
                if (pUnitPlot == pTargetUnitPlot && pUnit->getGroup() != pTargetUnit->getGroup())
                {
#ifdef ALTAI_DEBUG
                    os << "\nUnit: " << pUnit->getIDInfo() << " joined group: " << pTargetUnit->getGroupID();
#endif
                    pUnit->joinGroup(pTargetUnit->getGroup());
                    player.getAnalysis()->getWorkerAnalysis()->setEscort(pTargetUnit->getIDInfo());
                    return true;
                }
                else
                {
                    const CvPlot* pMovePlot = getNextMovePlot(player, pTargetUnit->getGroup(), pTargetUnit->plot());
                    CvUnit* pTargetGroupHeadUnit = pTargetUnit->getGroup()->getHeadUnit();
                    if (pMovePlot && pTargetGroupHeadUnit)
                    {
#ifdef ALTAI_DEBUG
                        os << "\nUnit: " << pUnit->getIDInfo() << " pushed escort mission move to: " << pMovePlot->getCoords();
#endif
                        pUnit->getGroup()->pushMission(MISSION_MOVE_TO_UNIT, pTargetGroupHeadUnit->getOwner(), pTargetGroupHeadUnit->getID(),
                            MOVE_IGNORE_DANGER | MOVE_THROUGH_ENEMY, false, false, MISSIONAI_GROUP, (CvPlot*)pMovePlot, pTargetGroupHeadUnit, __FUNCTION__);
                        return true;
                    }
                }
            }
        }

        // maybe lost our target (hopefully because an escort is no longer needed, rather than it being killed)
        return false;
    }

    void WorkerEscortMission::update(Player& player)
    {
    }

    void WorkerEscortMission::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);
        pStream->Write(subArea_);
    }

    void WorkerEscortMission::read(FDataStreamBase* pStream)
    {
        pStream->Read(&subArea_);
    }
}