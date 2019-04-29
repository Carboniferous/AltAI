#pragma once

#include "./utils.h"
#include "./unit_tactics.h"
#include "./unit_history.h"
#include "./unit_mission.h"

#include <stack>

class CvUnit;

namespace AltAI
{
    class IUnitEventGenerator;
    typedef boost::shared_ptr<IUnitEventGenerator> IUnitEventGeneratorPtr;

    class Unit
    {
    public:
        struct Mission : public IUnitMission
        {
            explicit Mission(const CvUnitAI* pUnit);
            Mission(const CvUnitAI* pUnit, const CvCity* pCity, const CvPlot* pTargetPlot, MissionTypes missionType_, BuildTypes buildType_, int length_, XYCoords startCoords_);

            IDInfo city;
            XYCoords startCoords, targetCoords;
            MissionTypes missionType;
            BuildTypes buildType;
            int length;
            const CvUnitAI* pUnit_;

            virtual const CvUnitAI* getUnit() const;
            virtual CvPlot* getTarget() const;
            virtual bool isComplete() const;
            virtual CvCity* getTargetCity() const;
            virtual UnitAITypes getAIType() const;

            virtual void update();

            bool isWorkerMissionAt(XYCoords coords) const;
            bool isWorkerMissionFor(const CvCity* pCity) const;

            void debug(std::ostream& os) const;

            void write(FDataStreamBase* pStream) const;
            void read(FDataStreamBase* pStream);
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

        void pushWorkerMission(size_t turn, const CvCity* pCity, const CvPlot* pTargetPlot, MissionTypes missionType, BuildTypes buildType);
        void updateMission();
        void updateMission(const CvPlot* pOldPlot, const CvPlot* pNewPlot);
        void clearMission(size_t turn);

        void pushMission(const UnitMissionPtr& pMission);

        bool hasWorkerMissionAt(const CvPlot* pTargetPlot) const;
        bool hasBuildOrRouteMission() const;
        bool isWorkerMissionFor(const CvCity* pCity) const;
        const std::vector<Mission>& getWorkerMissions() const;

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
        std::vector<Mission> workerMissions_;
        std::vector<UnitMissionPtr> missions_;
        size_t missionLength_;
    };
}

