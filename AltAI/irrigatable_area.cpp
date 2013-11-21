#include "AltAI.h"
#include "./irrigatable_area.h"

namespace AltAI
{
    int IrrigatableArea::nextID_(1);

    IrrigatableArea::IrrigatableArea(bool isIrrigatable, bool hasFreshWaterAccess, int subAreaID) : 
        isIrrigatable_(isIrrigatable), hasFreshWaterAccess_(hasFreshWaterAccess), ID_(nextID_++), subAreaID_(subAreaID), numTiles_(0)
    {
    }

    void IrrigatableArea::resetNextID()
    {
        nextID_ = 1;
    }
}