#pragma once

#include "./utils.h"
#include "./unit_mission.h"

class CvUnitAI;

namespace AltAI
{
    bool doLandUnitExplore(CvUnitAI* pUnit);
    bool doCoastalUnitExplore(CvUnitAI* pUnit);
    XYCoords getLandExploreRefPlot(const CvUnitAI* pUnit);
}