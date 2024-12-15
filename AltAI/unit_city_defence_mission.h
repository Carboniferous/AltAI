#pragma once

#include "./utils.h"

#include "./unit_mission.h"
#include "./unit_tactics.h"

namespace AltAI
{
    class Player;

    class CityDefenceMission : public IUnitsMission
    {
    public:
        static MilitaryMissionDataPtr createMission();
        CityDefenceMission () {}
        virtual ~CityDefenceMission () {}
        virtual bool doUnitMission(CvUnitAI* pUnit, MilitaryMissionData* pMission);
        virtual void update(Player& player);
        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = IUnitsMission::CityDefenceMissionID;
    private:        
    };
}
