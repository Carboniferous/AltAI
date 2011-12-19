#include "./utils.h"
#include "./unit.h"

namespace AltAI
{
    Unit::Unit(CvUnitAI* pUnit) : pUnit_(pUnit)
    {
        
    }

    bool Unit::operator < (const Unit& other) const
    {
        return pUnit_->getID() < other.pUnit_->getID();
    }
}