#include "AltAI.h"
#include "./iters.h"

namespace AltAI
{
    void debugSelectionGroup(const CvSelectionGroup* pGroup, std::ostream& os)
    {
        UnitGroupIter unitIter(pGroup);
        const CvUnit* pUnit = NULL;
        os << "(";
        bool first = true;
        while (pUnit = unitIter())
        {
            if (!first) os << ", ";
            first = false;
            os << gGlobals.getUnitInfo(pUnit->getUnitType()).getType() << " (" << pUnit->getID() << ")";
        }
        os << ") ";
    }
}