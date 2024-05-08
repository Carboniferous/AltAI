#pragma once

#include "./utils.h"
#include "./unit_history.h"
#include "./unit_mission.h"

#include <stack>

class CvUnit;

namespace AltAI
{
    class IUnitEventGenerator;
    typedef boost::shared_ptr<IUnitEventGenerator> IUnitEventGeneratorPtr;
    class MapAnalysis;

    struct UnitAction
    {
        XYCoords targetPlot;
    };

    // few utility functions for debugging stacks of units
    void debugUnit(std::ostream& os, const CvUnit* pUnit);
    void debugUnitVector(std::ostream& os, const std::vector<const CvUnit*>& units);
    void debugUnitIDInfoVector(std::ostream& os, const std::vector<IDInfo>& units);
    void debugUnitSet(std::ostream& os, const std::set<IDInfo>& units);

    class Unit
    {
    public:
        enum MissionFlags
        {
            None = 0, Ok = (1 << 0), CantStart = (1 << 1), Failed = (1 << 2), PlotDanger = (1 << 3), Hold = (1 << 4), NoMissions = (1 << 5)
        };

        struct WorkerMission : public IUnitMission
        {
            WorkerMission();
            explicit WorkerMission(const CvUnitAI* pUnit);
            WorkerMission(IDInfo unit_, IDInfo city_, XYCoords targetCoords_, MissionTypes missionType_, BuildTypes buildType_, int iFlags_, int length_, XYCoords startCoords_);
            WorkerMission(const CvUnitAI* pUnit, const CvCity* pCity, const CvPlot* pTargetPlot, MissionTypes missionType_, BuildTypes buildType_, int iFlags_, int length_, XYCoords startCoords_);

            IDInfo city;
            XYCoords startCoords, targetCoords;
            MissionTypes missionType;
            BuildTypes buildType;
            int iFlags, missionFlags;
            int length;
            IDInfo unit;

            virtual const CvUnitAI* getUnit() const;
            virtual CvPlot* getTarget() const;
            virtual bool isComplete() const;
            virtual CvCity* getTargetCity() const;
            virtual UnitAITypes getAIType() const;

            virtual void update();

            bool isWorkerMissionAt(XYCoords coords) const;
            bool isWorkerMissionFor(const CvCity* pCity) const;
            bool canStartMission() const;
            bool isBorderMission(const boost::shared_ptr<MapAnalysis>& pMapAnalysis) const;

            void debug(std::ostream& os) const;

            virtual void write(FDataStreamBase* pStream) const;
            virtual void read(FDataStreamBase* pStream);

            static const int ID = 0;
        };

        explicit Unit(CvUnitAI* pUnit);

        bool operator < (const Unit& other) const;

        UnitTypes getUnitType() const
        {
            return pUnit_->getUnitType();
        }

        const CvUnitAI* getUnit() const
        {
            return pUnit_;
        }

        const UnitAction& getAction() const
        {
            return action_;
        }

        void setAction(const UnitAction& action)
        {
            action_ = action;
        }

        void pushWorkerMission(size_t turn, const CvCity* pCity, const CvPlot* pTargetPlot, MissionTypes missionType, BuildTypes buildType, int iFlags);
        //void updateMission();
        void updateMission(const CvPlot* pOldPlot, const CvPlot* pNewPlot);
        void clearMission(size_t turn);

        void pushMission(const UnitMissionPtr& pMission);

        bool hasWorkerMissionAt(XYCoords targetCoords) const;
        bool hasBuildOrRouteMission() const;
        bool isWorkerMissionFor(const CvCity* pCity) const;
        const std::vector<WorkerMission>& getWorkerMissions() const;
        std::vector<WorkerMission>& getWorkerMissions();

        const std::vector<UnitMissionPtr>& getMissions() const;

        const IUnitEventGeneratorPtr& getUnitEventGenerator() const
        {
            return pUnitEventGenerator;
        }

        void setUnitEventGenerator(const IUnitEventGeneratorPtr& pUnitEventGenerator_)
        {
            pUnitEventGenerator = pUnitEventGenerator_;
        }

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        CvUnitAI* pUnit_;
        UnitAction action_;
        IUnitEventGeneratorPtr pUnitEventGenerator;
        UnitHistory unitHistory_;
        std::vector<WorkerMission> workerMissions_;
        std::vector<UnitMissionPtr> missions_;
        size_t missionLength_;
    };
}

