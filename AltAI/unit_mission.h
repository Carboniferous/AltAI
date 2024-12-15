#pragma once

#include "./utils.h"
#include "./unit_tactics.h"

namespace AltAI
{
    class IUnitMission;
    typedef boost::shared_ptr<IUnitMission> UnitMissionPtr;

    struct MilitaryMissionData;

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

        enum UnitMissionTypes
        {
            WorkerMissionID = 0, LandExploreMissionID = 1
        };
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

    class IUnitsMission;
    typedef boost::shared_ptr<IUnitsMission> IUnitsMissionPtr;

    struct MilitaryMissionData
    {
        MilitaryMissionData() : targetCoords(), missionType(NO_MISSIONAI), recalcOdds(false) {}
        explicit MilitaryMissionData(MissionAITypes missionType_);

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
        std::vector<IDInfo> tooDistantAssignedUnits;
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

        void debug(std::ostream& os, bool includeReachablePlots = false) const;
        void debugReachablePlots(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);
    };

    typedef boost::shared_ptr<MilitaryMissionData> MilitaryMissionDataPtr;

    class IUnitsMission
    {
    public:
        virtual ~IUnitsMission() = 0 {}
        virtual bool doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission) = 0;
        virtual void update(Player& player) = 0;
        virtual void write(FDataStreamBase* pStream) const = 0;
        virtual void read(FDataStreamBase* pStream) = 0;

        static IUnitsMissionPtr factoryRead(FDataStreamBase*);

        enum MissionTypes
        {
            ReserveMissionID = 0, GuardBonusesMissionID = 1, CoastalDefenceMissionID = 2, UnitCounterMissionID = 3,
            SettlerEscortMissionID = 4, WorkerEscortMissionID = 5, CityDefenceMissionID = 6
        };
    };

    class ReserveMission : public IUnitsMission
    {
    public:
        static MilitaryMissionDataPtr createMission();
        virtual ~ReserveMission() {}
        virtual bool doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission);
        virtual void update(Player& player);
        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IUnitsMission::ReserveMissionID;
    private:
    };

    class GuardBonusesMission : public IUnitsMission
    {
    public:
        static MilitaryMissionDataPtr createMission(int subArea);
        GuardBonusesMission() :subArea_(-1) {}
        explicit GuardBonusesMission(int subArea);
        virtual ~GuardBonusesMission() {}
        virtual bool doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission);
        virtual void update(Player& player);
        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IUnitsMission::GuardBonusesMissionID;
    private:        
        int subArea_;
        std::map<XYCoords, BonusTypes> bonusMap_;
    };

}
