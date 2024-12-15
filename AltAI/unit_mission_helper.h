#pragma once

#include "./utils.h"

#include "./unit_mission.h"
#include "./unit_tactics.h"

namespace AltAI
{
    struct MissionPlotProximity
    {
        MissionPlotProximity() : maxStepDistance(-1) {}
        MissionPlotProximity(int maxStepDistance_, XYCoords targetCoords_) : maxStepDistance(maxStepDistance_), targetCoords(targetCoords_) {}

        bool operator() (XYCoords coords) const
        {
            return stepDistance(coords.iX, coords.iY, targetCoords.iX, targetCoords.iY) <= maxStepDistance;
        }

        int maxStepDistance;
        XYCoords targetCoords;
    };

    struct UnitPlotProximity
    {
        UnitPlotProximity() : maxStepDistance(-1) {}
        UnitPlotProximity(int maxStepDistance_, XYCoords targetCoords_) : maxStepDistance(maxStepDistance_), targetCoords(targetCoords_) {}

        bool operator() (const CvUnit* pUnit) const
        {
            return stepDistance(pUnit->plot()->getX(), pUnit->plot()->getY(), targetCoords.iX, targetCoords.iY) <= maxStepDistance;
        }

        int maxStepDistance;
        XYCoords targetCoords;
    };

    struct MissionFinder
    {
        MissionFinder() : missionType(NO_MISSIONAI), domainType(NO_DOMAIN), requestedUnitType(NO_UNIT), subArea(-1) {}
        MissionAITypes missionType;
        IDInfo city, targetUnit, ourUnit;
        XYCoords targetCoords;
        MissionPlotProximity proximityData;
        UnitPlotProximity unitProximityData;
        DomainTypes domainType;
        UnitTypes requestedUnitType;
        int subArea;

        bool operator() (const MilitaryMissionDataPtr& pMission) const;
    };

    struct CanFoundP
    {
        bool operator () (IDInfo unit) const
        {
            const CvUnit* pUnit = ::getUnit(unit);
            return pUnit && pUnit->isFound();
        }
    };

    class Player;

    CombatGraph::Data getNearestCityAttackOdds(const Player& player, const MilitaryMissionDataPtr& pMission, const CvPlot* pTargetPlot);

    void getPromotionCombatValues(const Player& player, CvUnit* pUnit, const std::vector<PromotionTypes>& availablePromotions);

    void getOverlapAndNeighbourPlots(const PlotSet& theirPlots, const PlotSet& ourPlots, PlotSet& sharedPlots, PlotSet& neighbourPlots);

    std::pair<bool, XYCoords> checkAttackOdds(const CombatMoveData& moveData, XYCoords targetCoords);

    std::pair<bool, XYCoords> checkCityAttackOdds(const CombatMoveData& moveData, const std::map<IDInfo, CombatGraph::Data>& cityAttackOdds);

    void combineMoveData(const Player& player, MilitaryMissionData* pMission, CvUnit* pUnit, XYCoords stepCoords, 
        GroupCombatData& combatData, std::list<CombatMoveData>& noAttackMoveData, std::list<CombatMoveData>& attackMoveData);

    void logMoveToCoordsCombatData(std::ostream& os, const GroupCombatData& combatData, XYCoords moveToCoords);
    void logNoAttackMoveCombatData(std::ostream& os, const std::list<CombatMoveData>& noAttackMoveData);
}
