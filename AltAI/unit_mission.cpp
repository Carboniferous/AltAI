#include "AltAI.h"

#include "./player.h"
#include "./unit_mission.h"
#include "./unit.h"
#include "./unit_explore.h"
#include "./player_analysis.h"
#include "./worker_tactics.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./save_utils.h"
#include "./unit_log.h"

namespace AltAI
{
    UnitMissionPtr IUnitMission::factoryRead(FDataStreamBase* pStream)
    {
        UnitMissionPtr pUnitMission;

        int ID;
        pStream->Read(&ID);

        switch (ID)
        {
        case Unit::WorkerMission::ID:
            pUnitMission = UnitMissionPtr(new Unit::WorkerMission());
            break;
        case 1:  // LandExploreMission
            break;
        case 2: // WorkerMission
            pUnitMission = workerMissionFactoryHelper();
            break;
        default:            
            break;
        }

        pUnitMission->read(pStream);
        return pUnitMission;
    }

    void MilitaryMissionData::resetDynamicData()
    {
        ourAttackOdds = CombatGraph::Data();
        hostileAttackOdds = CombatGraph::Data();
        ourAttackers.clear();
        ourDefenders.clear();
        cityAttackOdds.clear();
        firstAttacker.reset();
        closestCity.reset();
        attackableUnits.clear();
        recalcOdds = false;
    }

    int MilitaryMissionData::getAssignedUnitCount(const CvPlayer* pPlayer, UnitTypes unitType) const
    {
        int count = 0;
        for (std::set<IDInfo>::const_iterator ci(assignedUnits.begin()), ciEnd(assignedUnits.end()); ci != ciEnd; ++ci)
        {
            const CvUnit* pUnit = pPlayer->getUnit(ci->iID);
            if (pUnit && pUnit->getUnitType() == unitType)
            {
                ++count;
            }
        }
        return count;
    }

    std::vector<IDInfo> MilitaryMissionData::updateRequiredUnits(const Player& player, std::set<IDInfo> availableUnits)
    {
        const CvPlot* pMissionTarget = NULL;
        std::vector<IDInfo> unitsToReassign;
        availableUnits.insert(assignedUnits.begin(), assignedUnits.end());

        std::vector<const CvUnit*> enemyStack;
        for (std::set<IDInfo>::const_iterator ti(targets.begin()), tiEnd(targets.end()); ti != tiEnd; ++ti)
        {
            const CvUnit* pTargetUnit = ::getUnit(*ti);
            if (pTargetUnit) // todo - store UnitData objects
            {
                enemyStack.push_back(pTargetUnit);
                pMissionTarget = pTargetUnit->plot();
            }
        }

        if (!enemyStack.empty() && pMissionTarget)
        {
            // todo - support storing list of UnitData objects as alternative to CvUnit pointers
            RequiredUnitStack requiredUnitStack = getRequiredUnits(player, pMissionTarget, enemyStack, availableUnits);
            requiredUnits = requiredUnitStack.unitsToBuild;

            for (size_t i = 0, count = requiredUnitStack.existingUnits.size(); i < count; ++i)
            {
                if (assignedUnits.find(requiredUnitStack.existingUnits[i]) == assignedUnits.end())
                {
                    unitsToReassign.push_back(requiredUnitStack.existingUnits[i]);
                }
            }
        }

        return unitsToReassign;
    }

    void MilitaryMissionData::assignUnit(const CvUnit* pUnit, bool updateRequiredUnits /* = true */)
    {
        if (assignedUnits.insert(pUnit->getIDInfo()).second)
        {
            if (updateRequiredUnits)
            {
                std::vector<RequiredUnitStack::UnitDataChoices>::iterator reqIter = std::remove_if(requiredUnits.begin(), requiredUnits.end(), UnitMatcher(pUnit));
                requiredUnits.erase(reqIter, requiredUnits.end());
            }
        }
    }

    void MilitaryMissionData::unassignUnit(const CvUnit* pUnit, bool updateRequiredUnits)
    {
        if (assignedUnits.erase(pUnit->getIDInfo()) > 0 && updateRequiredUnits)
        {
            // todo - recalculate required units here
            requiredUnits.push_back(RequiredUnitStack::UnitDataChoices(1, UnitData(pUnit)));
        }
    }

    std::set<CvSelectionGroup*, CvSelectionGroupOrderF> MilitaryMissionData::getAssignedGroups() const
    {
        std::set<CvSelectionGroup*, CvSelectionGroupOrderF> groups;
        for (std::set<IDInfo>::const_iterator assignedIter(assignedUnits.begin()), assignedEndIter(assignedUnits.end());
            assignedIter != assignedEndIter; ++assignedIter)
        {
            const CvUnit* pAssignedUnit = ::getUnit(*assignedIter);
            if (pAssignedUnit)
            {
                groups.insert(pAssignedUnit->getGroup());
            }
        }
        return groups;
    }

    std::vector<const CvUnit*> MilitaryMissionData::getAssignedUnits(bool includeCanOnlyDefend) const
    {
        std::vector<const CvUnit*> plotUnits;
        for (std::set<IDInfo>::const_iterator unitsIter(assignedUnits.begin()), unitsEndIter(assignedUnits.end());
            unitsIter != unitsEndIter; ++unitsIter)
        {
            const CvUnit* pAssignedUnit = ::getUnit(*unitsIter);
            if (pAssignedUnit && pAssignedUnit->canFight())
            {
                if (includeCanOnlyDefend || (!pAssignedUnit->isOnlyDefensive() && (!pAssignedUnit->isMadeAttack() || pAssignedUnit->isBlitz())))
                {
                    plotUnits.push_back(pAssignedUnit);
                }
            }
        }
        return plotUnits;
    }

    std::vector<const CvUnit*> MilitaryMissionData::getHostileUnits(TeamTypes ourTeamId, bool includeCanOnlyDefend) const
    {
        std::vector<const CvUnit*> plotUnits;
        if (missionType == MISSIONAI_COUNTER)
        {
            for (std::set<IDInfo>::const_iterator unitsIter(targets.begin()), unitsEndIter(targets.end());
                unitsIter != unitsEndIter; ++unitsIter)
            {
                const CvUnit* pHostileUnit = ::getUnit(*unitsIter);
                if (pHostileUnit && pHostileUnit->plot()->isVisible(ourTeamId, false) && pHostileUnit->canFight())
                {
                    if (includeCanOnlyDefend || (!pHostileUnit->isOnlyDefensive() && (!pHostileUnit->isMadeAttack() || pHostileUnit->isBlitz())))
                    {
                        plotUnits.push_back(pHostileUnit);
                    }
                }
            }
        }
        return plotUnits;
    }

    void MilitaryMissionData::updateReachablePlots(const Player& player, bool ourUnits, bool useMaxMoves, bool canAttack)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = UnitLog::getLog(*player.getCvPlayer())->getStream();
#endif
        PlotSet& reachablePlots(ourUnits ? ourReachablePlots : hostilesReachablePlots);
        ReachablePlotDetails& reachablePlotDetails(ourUnits ? ourReachablePlotDetails : hostileReachablePlotDetails);
        reachablePlots.clear();
        reachablePlotDetails.clear();

        std::vector<const CvUnit*> plotUnits(ourUnits ? getAssignedUnits(true) : getHostileUnits(player.getTeamID(), true));
        if (!plotUnits.empty())
        {
            ReachablePlotsData reachablePlotsData;
            getReachablePlotsData(reachablePlotsData, player, plotUnits, useMaxMoves, canAttack);
            reachablePlots = reachablePlotsData.allReachablePlots;

            for (UnitMovementDataMap::const_iterator udIter(reachablePlotsData.unitMovementDataMap.begin()), udEndIter(reachablePlotsData.unitMovementDataMap.end()); udIter != udEndIter; ++udIter)
            {
                std::list<IDInfo>& unitsForPlot = reachablePlotDetails[udIter->first];
                for (std::map<const CvUnit*, int, CvUnitIDInfoOrderF>::const_iterator unitsIter(udIter->second.begin()), unitsEndIter(udIter->second.end()); unitsIter != unitsEndIter; ++unitsIter)
                {
                    unitsForPlot.push_back(unitsIter->first->getIDInfo());
                }
            }
        }
#ifdef ALTAI_DEBUG
        os << "\nCalculated reachable plots for mission: (" << this << ") for: " << (ourUnits ? "our" : "hostile") << " units";
#endif
    }

    std::set<XYCoords> MilitaryMissionData::getReachablePlots(IDInfo unit) const
    {
        std::set<XYCoords> plotsUnitCanReach;
        for (ReachablePlotDetails::const_iterator rIter(ourReachablePlotDetails.begin()), rEndIter(ourReachablePlotDetails.end()); rIter != rEndIter; ++rIter)
        {
            if (std::find(rIter->second.begin(), rIter->second.end(), unit) != rIter->second.end())
            {
                plotsUnitCanReach.insert(rIter->first->getCoords());
            }
        }
        return plotsUnitCanReach;
    }

    PlotSet MilitaryMissionData::getTargetPlots() const
    {
        PlotSet targetPlots;
        for (std::set<IDInfo>::const_iterator tIter(targets.begin()), tEndIter(targets.end()); tIter != tEndIter; ++tIter)
        {
            const CvUnit* pUnit = ::getUnit(*tIter);
            if (pUnit) targetPlots.insert(pUnit->plot());
        }
        return targetPlots;
    }

    const CvPlot* MilitaryMissionData::getTargetPlot() const
    {
        XYCoords targetCoords = plotTarget;
        const CvCity* pTargetCity = NULL;
        if (!targets.empty())
        {
            pTargetCity = ::getCity(*targets.begin());
            if (pTargetCity)
            {
                targetCoords = pTargetCity->plot()->getCoords();
            }
        }
        return targetCoords == XYCoords(-1, -1) ? NULL : gGlobals.getMap().plot(targetCoords);
    }

    void MilitaryMissionData::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nMilitary mission data: (" << this << ") unit ai = " << getMissionAIString(missionType);
        if (targets.empty())
        {
            os << " (no targets) ";
        }
        else
        {
            os << " targets: ";
            for (std::set<IDInfo>::const_iterator targetsIter(targets.begin()), targetsEndIter(targets.end()); targetsIter != targetsEndIter; ++targetsIter)
            {
                if (targetsIter != targets.begin()) os << ", ";

                if (missionType == MISSIONAI_GUARD_CITY || missionType == MISSIONAI_RESERVE)
                {
                    const CvCity* pCity = ::getCity(*targetsIter);
                    if (pCity)
                    {
                        os << "city = " << safeGetCityName(*targetsIter) << " at: " << pCity->plot()->getCoords();
                    }
                }
                else
                {
                    os << *targetsIter;
                    const CvUnit* pUnit = ::getUnit(*targetsIter);
                    if (pUnit)
                    {
                        if (!pUnit->isDelayedDeath())
                        {
                            os << " at: " << pUnit->plot()->getCoords();
                        }
                        else
                        {
                            os << " (zombie unit)";  // think this happens if unit is killed in combat but update not yet processed
                        }                            
                    }
                    else
                    {
                        os << " (not found)";
                    }
                }
            }
        }
        if (!assignedUnits.empty())
        {
            os << " assigned units: ";
            debugUnitSet(os, assignedUnits);
        }
        else
        {
            os << " (no assigned units) ";
        }
        if (!requiredUnits.empty())
        {
            os << " required units: ";
            debugUnitDataLists(os, requiredUnits);
        }
        debugReachablePlots(os);
#endif
    }

    void MilitaryMissionData::debugReachablePlots(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG                
        os << "\n\t hostiles reachable plots: (count = " << hostilesReachablePlots.size() << ") ";
        bool first = true;
        for (PlotSet::const_iterator ci(hostilesReachablePlots.begin()), ciEnd(hostilesReachablePlots.end()); ci != ciEnd; ++ci)
        {
            os << (first ? "" : ", ") << (*ci)->getCoords();
            if (first) first = false;
        }
                
        os << "\n\t our reachable plots: (count = " << ourReachablePlots.size() << ") ";
        first = true;
        for (PlotSet::const_iterator ci(ourReachablePlots.begin()), ciEnd(ourReachablePlots.end()); ci != ciEnd; ++ci)
        {
            os << (first ? "" : ", ") << (*ci)->getCoords();
            if (first) first = false;
        }
#endif
    }

    void MilitaryMissionData::write(FDataStreamBase* pStream) const
    {
        writeComplexSet(pStream, targets);
        writeComplexSet(pStream, specialTargets);
        plotTarget.write(pStream);
        writeComplexSet(pStream, assignedUnits);

        pStream->Write(requiredUnits.size());
        for (size_t i = 0, count = requiredUnits.size(); i < count; ++i)
        {
            writeComplexList(pStream, requiredUnits[i]);
        }

        pStream->Write(ourReachablePlots.size());
        for (PlotSet::const_iterator ci(ourReachablePlots.begin()), ciEnd(ourReachablePlots.end()); ci != ciEnd; ++ci)
        {
            (*ci)->getCoords().write(pStream);
        }

        pStream->Write(hostilesReachablePlots.size());
        for (PlotSet::const_iterator ci(hostilesReachablePlots.begin()), ciEnd(hostilesReachablePlots.end()); ci != ciEnd; ++ci)
        {
            (*ci)->getCoords().write(pStream);
        }

        closestCity.write(pStream);
        pStream->Write(missionType);
    }

    void MilitaryMissionData::read(FDataStreamBase* pStream)
    {
        readComplexSet(pStream, targets);
        readComplexSet(pStream, specialTargets);
        plotTarget.read(pStream);
        readComplexSet(pStream, assignedUnits);

        size_t count = 0;
        requiredUnits.clear();
        pStream->Read(&count);
        for (size_t i = 0; i < count; ++i)
        {
            std::list<UnitData> unitChoiceList;
            readComplexList(pStream, unitChoiceList);
            requiredUnits.push_back(unitChoiceList);
        }
                                
        pStream->Read(&count);
        ourReachablePlots.clear();
        for (size_t i = 0; i < count; ++i)
        {
            XYCoords coords;
            coords.read(pStream);
            ourReachablePlots.insert(gGlobals.getMap().plot(coords.iX, coords.iY));
        }

        pStream->Read(&count);
        hostilesReachablePlots.clear();
        for (size_t i = 0; i < count; ++i)
        {
            XYCoords coords;
            coords.read(pStream);
            hostilesReachablePlots.insert(gGlobals.getMap().plot(coords.iX, coords.iY));
        }

        closestCity.read(pStream);
        pStream->Read((int*)&missionType);
    }


    bool ReserveMission::doUnitMission(CvUnitAI* pUnit)
    {
        return true;
    }

    void ReserveMission::update(Player& player)
    {
    }

    GuardBonusesMission::GuardBonusesMission(int subArea) : subArea_(subArea)
    {
    }

    bool GuardBonusesMission::doUnitMission(CvUnitAI* pUnit)
    {
        return true;
    }

    void GuardBonusesMission::update(Player& player)
    {
        const std::list<const CvPlot*>& plots = player.getAnalysis()->getMapDelta()->getNewBonusPlots();
        for (std::list<const CvPlot*>::const_iterator iter(plots.begin()), endIter(plots.end()); iter != endIter; ++iter)
        {
            if ((*iter)->getSubArea() == subArea_)
            {
            }
        }
    }
}