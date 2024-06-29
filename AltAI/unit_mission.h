#pragma once

#include "./utils.h"
#include "./unit_tactics.h"

namespace AltAI
{
    class IUnitMission;
    typedef boost::shared_ptr<IUnitMission> UnitMissionPtr;

    class IUnitMission
    {
    public:
        virtual ~IUnitMission() = 0 {}

        virtual void update() = 0;

        virtual const CvUnitAI* getUnit() const = 0;
        virtual CvPlot* getTarget() const = 0;
        virtual bool isComplete() const = 0;
        virtual CvCity* getTargetCity() const = 0;
        virtual UnitAITypes getAIType() const = 0;

        // save/load functions
        virtual void write(FDataStreamBase*) const = 0;
        virtual void read(FDataStreamBase*) = 0;

        static UnitMissionPtr factoryRead(FDataStreamBase*);
    };

    struct HasTarget
    {
        explicit HasTarget(const CvPlot* pPlot_) : pPlot(pPlot_)
        {
        }

        bool operator() (const UnitMissionPtr& pMission) const
        {
            return pMission->getTarget() == pPlot;
        }

        const CvPlot* pPlot;
    };

    struct IsCloserThan
    {
        IsCloserThan(const CvPlot* pPlot_, const int minDistance_) : pPlot(pPlot_), minDistance(minDistance_)
        {
        }

        bool operator() (const UnitMissionPtr& pMission) const
        {
            return stepDistance(pMission->getTarget()->getX(), pMission->getTarget()->getY(), pPlot->getX(), pPlot->getY()) < minDistance;
        }

        const CvPlot* pPlot;
        const int minDistance;
    };

    struct LandWorkerPred
    {
        explicit LandWorkerPred(const CvArea* pArea) : areaID(pArea->getID())
        {
        }

        bool operator() (const UnitMissionPtr& pMission) const
        {
            return pMission->getAIType() == UNITAI_WORKER && 
                pMission->getTarget() && 
                pMission->getTarget()->area()->getID() == areaID;
        }

        const int areaID;
    };

    class IUnitsMission
    {
    public:
        virtual ~IUnitsMission() = 0 {}
        virtual bool doUnitMission(CvUnitAI* pUnit) = 0;
        virtual void update(Player& player) = 0;
    };

    typedef boost::shared_ptr<IUnitsMission> IUnitsMissionPtr;

    struct MilitaryMissionData
    {
        MilitaryMissionData() : targetCoords(-1, -1), missionType(NO_MISSIONAI), recalcOdds(false) {}
        explicit MilitaryMissionData(MissionAITypes missionType_) : targetCoords(-1, -1), missionType(missionType_) {}

        std::set<IDInfo> targetUnits, specialTargets;
        IDInfo targetCity;
        XYCoords targetCoords;
        std::set<IDInfo> assignedUnits;
        std::vector<RequiredUnitStack::UnitDataChoices> requiredUnits;

        PlotSet ourReachablePlots, hostilesReachablePlots;
        typedef std::map<const CvPlot*, std::list<IDInfo>, CvPlotOrderF> ReachablePlotDetails;
        ReachablePlotDetails ourReachablePlotDetails, hostileReachablePlotDetails;

        MissionAITypes missionType;

        IUnitsMissionPtr pUnitsMission;

        // dynamic turn data
        CombatGraph::Data ourAttackOdds, hostileAttackOdds;
        std::map<IDInfo, CombatGraph::Data> cityAttackOdds;
        std::vector<UnitData> ourAttackers, ourDefenders;
        IDInfo firstAttacker, closestCity;
        std::set<IDInfo> attackableUnits;
        std::pair<IDInfo, XYCoords> nextAttack, pushedAttack;
        bool recalcOdds;

        void resetDynamicData();

        int getRequiredUnitCount(UnitTypes unitType) const
        {
            return std::count_if(requiredUnits.begin(), requiredUnits.end(), UnitTypeInList(unitType));
        }

        int getAssignedUnitCount(const CvPlayer* pPlayer, UnitTypes unitType) const;

        std::vector<IDInfo> updateRequiredUnits(const Player& player, const std::set<IDInfo>& reserveUnits);

        void assignUnit(const CvUnit* pUnit, bool updateRequiredUnits = true);
        void unassignUnit(const CvUnit* pUnit, bool updateRequiredUnits);

        template <typename Pred> 
            std::vector<IDInfo> getOurMatchingUnits(const Pred& p) const
        {
            std::vector<IDInfo> matchedUnits;
            for (std::set<IDInfo>::const_iterator ci(assignedUnits.begin()), ciEnd(assignedUnits.end()); ci != ciEnd; ++ci)
            {
                if (p(*ci))
                {
                    matchedUnits.push_back(*ci);
                }
            }
            return matchedUnits;
        }

        std::set<CvSelectionGroup*, CvSelectionGroupOrderF> getAssignedGroups() const;
        std::vector<const CvUnit*> getAssignedUnits(bool includeCanOnlyDefend) const;
        std::vector<const CvUnit*> getHostileUnits(TeamTypes ourTeamId, bool includeCanOnlyDefend) const;

        void updateReachablePlots(const Player& player, bool ourUnits, bool useMaxMoves, bool canAttack);

        std::set<XYCoords> getReachablePlots(IDInfo unit) const;

        PlotSet getTargetPlots() const;

        const CvPlot* getTargetPlot() const;
        const CvCity* getTargetCity() const;

        void debug(std::ostream& os) const;
        void debugReachablePlots(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);
    };

    typedef boost::shared_ptr<MilitaryMissionData> MilitaryMissionDataPtr;

    class ReserveMission : public IUnitsMission
    {
    public:
        virtual ~ReserveMission() {}
        virtual bool doUnitMission(CvUnitAI* pUnit);
        virtual void update(Player& player);

    private:

    };

    class GuardBonusesMission : public IUnitsMission
    {
    public:
        explicit GuardBonusesMission(int subArea);
        virtual ~GuardBonusesMission() {}
        virtual bool doUnitMission(CvUnitAI* pUnit);
        virtual void update(Player& player);

    private:
        int subArea_;
        std::map<XYCoords, BonusTypes> bonusMap_;
    };

}
