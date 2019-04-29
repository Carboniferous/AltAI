#include "AltAI.h"

#include "./unit_history.h"

namespace AltAI
{
    UnitHistory::UnitHistory(const CvUnitAI* pUnit)
    {
        unitID = pUnit->getID();
        unitType = pUnit->getUnitType();
    }
}