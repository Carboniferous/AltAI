#include "AltAI.h"

#include "./tactic_actions.h"
#include "./tactic_streams.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const ConstructItem& node)
    {
        bool hasNoType = true;
        if (node.buildingType != NO_BUILDING)
        {
            hasNoType = false;
            os << " want building = " << gGlobals.getBuildingInfo(node.buildingType).getType();
        }
        if (node.unitType != NO_UNIT)
        {
            hasNoType = false;
            os << " want unit = " << gGlobals.getUnitInfo(node.unitType).getType();
        }
        if (node.buildTarget != XYCoords())
        {
            os << " targeting plot: " << node.buildTarget;
        }
        if (node.improvementType != NO_IMPROVEMENT)
        {
            hasNoType = false;
            os << " want improvement = " << gGlobals.getImprovementInfo(node.improvementType).getType();
        }
        if (node.processType != NO_PROCESS)
        {
            hasNoType = false;
            os << " want process = " << gGlobals.getProcessInfo(node.processType).getType();
        }
        if (node.projectType != NO_PROJECT)
        {
            hasNoType = false;
            os << " want project = " << gGlobals.getProjectInfo(node.projectType).getType();
        }
        if (hasNoType)
        {
            os << " construct item is empty";
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const ResearchTech& node)
    {
        if (node.techType == NO_TECH)
        {
            return os << " ResearchTech with (NO_TECH)? ";
        }

        os << " can research: " << gGlobals.getTechInfo(node.techType).getType() << " (depth = " << node.depth << ")";
        if (node.targetTechType != NO_TECH)
        {
            os << " target tech = " << gGlobals.getTechInfo(node.targetTechType).getType();
        }

        return os;
    }
}