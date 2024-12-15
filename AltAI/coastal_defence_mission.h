#pragma once

#include "./utils.h"

#include "./unit_mission.h"
#include "./unit_tactics.h"

namespace AltAI
{
    class Player;

    class CoastalDefenceMission : public IUnitsMission
    {
    public:
        static MilitaryMissionDataPtr createMission(int subArea);
        CoastalDefenceMission() : subArea_(-1) {}
        explicit CoastalDefenceMission(int subArea);
        virtual ~CoastalDefenceMission() {}

        virtual bool doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission);
        virtual void update(Player& player);
        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IUnitsMission::CoastalDefenceMissionID;

    private:
        int subArea_;
        std::set<IDInfo> holdingUnits_;
    };
}
