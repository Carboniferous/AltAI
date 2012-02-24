#include "./project_info_streams.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const ProjectInfo::NullNode& node)
    {
        return os << "(Empty Node) ";
    }

    std::ostream& operator << (std::ostream& os, const ProjectInfo::VictoryNode& node)
    {
        os << "\n\tvictory prereqs for: " << gGlobals.getVictoryInfo(node.victoryType).getType();

        if (node.neededCount > 0)
        {
            os << " reqd. = " << node.neededCount;
        }

        if (node.minNeededCount > 0)
        {
            os << " min. reqd. = " << node.minNeededCount;
        }
        
        if (node.victoryDelayPercent > 0)
        {
            os << " delay % = " << node.victoryDelayPercent;
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const ProjectInfo::BonusNode& node)
    {
        return os << "\n\t+" << node.productionModifier << "% production with: " << gGlobals.getBonusInfo(node.bonusType).getType();
    }

    std::ostream& operator << (std::ostream& os, const ProjectInfo::PrereqNode& node)
    {
        os << "\n\trequires: " << node.count << " " << gGlobals.getProjectInfo(node.projectType).getType();
        return os;
    }

    std::ostream& operator << (std::ostream& os, const ProjectInfo::MiscNode& node)
    {
        os << "\n\t";

        if (node.allowsNukes)
        {
            os << " allows nuclear weapons ";
        }

        if (node.nukeInterceptionProb > 0)
        {
            os << node.nukeInterceptionProb << "% prob. of intercepting nuclear weapons ";
        }

        if (node.specialBuilding != NO_SPECIALBUILDING)
        {
            os << " can construct " << gGlobals.getSpecialBuildingInfo(node.specialBuilding).getType();
        }

        if (node.specialUnit != NO_SPECIALUNIT)
        {
            os << " can train " << gGlobals.getSpecialUnitInfo(node.specialUnit).getType();
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const ProjectInfo::BaseNode& node)
    {
        os << "\ncost = " << node.cost;

        if (node.prereqTech != NO_TECH)
        {
            os << " requires " << gGlobals.getTechInfo(node.prereqTech).getType();
        }

        if (node.anyonePrereqProject != NO_PROJECT)
        {
            os << " requires " << gGlobals.getProjectInfo(node.anyonePrereqProject).getType() << " to have been built ";
        }

        for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
        {
            os << node.nodes[i];
        }
        return os;
    }
}
