#pragma once

#include "./utils.h"

#include "../CvGameCoreDLL/CvUnitAI.h"

namespace AltAI
{
    struct UnitHistory
    {
        explicit UnitHistory(const CvUnitAI* pUnit);

        UnitTypes unitType;
        int unitID;

        bool operator < (const UnitHistory& other) const
        {
            return unitID < other.unitID;
        }
    };
}
