#pragma once

#include "./utils.h"

#include "./unit_mission.h"
#include "./unit_tactics.h"

namespace AltAI
{
    class Player;

    class SettlerEscortMission : public IUnitsMission
    {
    public:
        static MilitaryMissionDataPtr createMission(int subArea);
        SettlerEscortMission() : subArea_(-1) {}
        explicit SettlerEscortMission(int subArea);
        virtual ~SettlerEscortMission() {}
        virtual bool doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission);
        virtual void update(Player& player);
        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IUnitsMission::SettlerEscortMissionID;
    private:        
        int subArea_;
    };

    class WorkerEscortMission : public IUnitsMission
    {
    public:
        static MilitaryMissionDataPtr createMission(int subArea);
        WorkerEscortMission() : subArea_(-1) {}
        explicit WorkerEscortMission(int subArea);
        virtual ~WorkerEscortMission() {}
        virtual bool doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission);
        virtual void update(Player& player);
        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IUnitsMission::WorkerEscortMissionID;
    private:        
        int subArea_;
    };
}
