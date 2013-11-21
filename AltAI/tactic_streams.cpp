#include "AltAI.h"

#include "./tactic_actions.h"
#include "./tactic_streams.h"

namespace AltAI
{
    /*void streamEconomicFlags(std::ostream& os, int flags)
    {
        if (flags & EconomicFlags::Output_Food)
        {
            os << " Output_Food, ";
        }
        if (flags & EconomicFlags::Output_Production)
        {
            os << " Output_Production, ";
        }
        if (flags & EconomicFlags::Output_Commerce)
        {
            os << " Output_Commerce, ";
        }
        if (flags & EconomicFlags::Output_Gold)
        {
            os << " Output_Gold, ";
        }
        if (flags & EconomicFlags::Output_Research)
        {
            os << " Output_Research, ";
        }
        if (flags & EconomicFlags::Output_Culture)
        {
            os << " Output_Culture, ";
        }
        if (flags & EconomicFlags::Output_Espionage)
        {
            os << " Output_Espionage, ";
        }
        if (flags & EconomicFlags::Output_Happy)
        {
            os << " Output_Happy, ";
        }
        if (flags & EconomicFlags::Output_Health)
        {
            os << " Output_Health, ";
        }
        if (flags & EconomicFlags::Output_Maintenance_Reduction)
        {
            os << " Output_Maintenance_Reduction, ";
        }
        if (flags & EconomicFlags::Output_Settler)
        {
            os << " Output_Settler, ";
        }
    }

    void streamMilitaryFlags(std::ostream& os, int flags)
    {
        if (flags & MilitaryFlags::Output_Production)
        {
            os << " Output_Production ";
        }
        if (flags & MilitaryFlags::Output_Experience)
        {
            os << " Output_Experience ";
        }
        if (flags & MilitaryFlags::Output_Attack)
        {
            os << " Output_Attack ";
        }
        if (flags & MilitaryFlags::Output_Defence)
        {
            os << " Output_Defence ";
        }
        if (flags & MilitaryFlags::Output_City_Attack)
        {
            os << " Output_City_Attack ";
        }
        if (flags & MilitaryFlags::Output_City_Defence)
        {
            os << " Output_City_Defence ";
        }
        if (flags & MilitaryFlags::Output_UnitCombat_Counter)
        {
            os << " Output_UnitCombat_Counter ";
        }
        if (flags & MilitaryFlags::Output_UnitClass_Counter)
        {
            os << " Output_UnitClass_Counter ";
        }
        if (flags & MilitaryFlags::Output_Collateral)
        {
            os << " Output_Collateral ";
        }
        if (flags & MilitaryFlags::Output_Make_Unit_Available)
        {
            os << " Output_Make_Unit_Available ";
        }
        if (flags & MilitaryFlags::Output_Extra_Mobility)
        {
            os << " Output_Extra_Mobility ";
        }
        if (flags & MilitaryFlags::Output_Combat_Unit)
        {
            os << " Output_Combat_Unit ";
        }
        if (flags & MilitaryFlags::Output_Unit_Transport)
        {
            os << " Output_Unit_Transport ";
        }
        if (flags & MilitaryFlags::Output_Explore)
        {
            os << " Output_Explore, ";
        }
    }

    void streamTechFlags(std::ostream& os, int flags)
    {
        if (flags & TechFlags::Found_Religion)
        {
            os << " Found_Religion, ";
        }
        if (flags & TechFlags::Free_Tech)
        {
            os << " Free_Tech, ";
        }
        if (flags & TechFlags::Free_GP)
        {
            os << " Free_GP, ";
        }
        if (flags & TechFlags::Flexible_Commerce)
        {
            os << " Flexible_Commerce, ";
        }
        if (flags & TechFlags::Trade_Routes)
        {
            os << " Trade_Routes, ";
        }
    }

    void streamWorkerFlags(std::ostream& os, int flags)
    {
        if (flags & WorkerFlags::Faster_Workers)
        {
            os << " Faster_Workers, ";
        }
        if (flags & WorkerFlags::New_Improvements)
        {
            os << " New_Improvements, ";
        }
        if (flags & WorkerFlags::Better_Improvements)
        {
            os << " Better_Improvements, ";
        }
        if (flags & WorkerFlags::Remove_Features)
        {
            os << " Remove_Features, ";
        }
    }

    void streamVictoryFlags(std::ostream& os, int flags)
    {
        if (flags & VictoryFlags::Component_Project)
        {
            os << " Component_Project, ";
        }
        if (flags & VictoryFlags::Prereq_Project)
        {
            os << " Prereq_Project, ";
        }
    }*/

    std::ostream& operator << (std::ostream& os, const ConstructItem& node)
    {
        if (node.buildingType != NO_BUILDING)
        {
            os << " want building = " << gGlobals.getBuildingInfo(node.buildingType).getType();
        }
        if (node.unitType != NO_UNIT)
        {
            os << " want unit = " << gGlobals.getUnitInfo(node.unitType).getType();
        }
        if (node.buildTarget != XYCoords())
        {
            os << " targeting plot: " << node.buildTarget;
        }
        if (node.improvementType != NO_IMPROVEMENT)
        {
            os << " want improvement = " << gGlobals.getImprovementInfo(node.improvementType).getType();
        }
        if (node.processType != NO_PROCESS)
        {
            os << " want process = " << gGlobals.getProcessInfo(node.processType).getType();
        }
        if (node.projectType != NO_PROJECT)
        {
            os << " want project = " << gGlobals.getProjectInfo(node.projectType).getType();
        }
        /*if (node.economicFlags)
        {
            streamEconomicFlags(os, node.economicFlags);
        }
        if (node.militaryFlags)
        {
            streamMilitaryFlags(os, node.militaryFlags);
        }
        if (node.victoryFlags)
        {
            streamVictoryFlags(os, node.victoryFlags);
        }

        bool first = true;
        for (std::map<BuildTypes, int>::const_iterator ci(node.possibleBuildTypes.begin()), ciEnd(node.possibleBuildTypes.end()); ci != ciEnd; ++ci)
        {
            if (first)
            {
                os << " possible builds: ";
                first = false;
            }
            else
            {
                os << ",";
            }

            if (ci->first != NO_BUILD)
            {
                os << gGlobals.getBuildInfo(ci->first).getType();
                if (ci->second > 0)
                {
                    os << " (count = " << ci->second << "), ";
                }
            }
        }
        for (size_t i = 0, count = node.religionTypes.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            else os << " religions: ";
            os << gGlobals.getReligionInfo(node.religionTypes[i]).getType() << " ";
        }
        for (size_t i = 0, count = node.positiveBonuses.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            else os << " positive bonuses: ";
            os << gGlobals.getBonusInfo(node.positiveBonuses[i]).getType() << " ";
        }
        for (size_t i = 0, count = node.requiredTechs.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getTechInfo(node.requiredTechs[i]).getType() << " ";
        }
        for (ConstructItem::MilitaryFlagValuesMap::const_iterator ci(node.militaryFlagValuesMap.begin()), ciEnd(node.militaryFlagValuesMap.end()); ci != ciEnd; ++ci)
        {
            streamMilitaryFlags(os, ci->first);
            os << "= " << ci->second.first;
            if (ci->first == MilitaryFlags::Output_UnitCombat_Counter && ci->second.second != NO_UNITCOMBAT)
            {
                os << " for: " << gGlobals.getUnitCombatInfo((UnitCombatTypes)ci->second.second).getType();
            }
            else if (ci->first == MilitaryFlags::Output_UnitClass_Counter && ci->second.second != NO_UNITCLASS)
            {
                os << " for: " << gGlobals.getUnitClassInfo((UnitClassTypes)ci->second.second).getType();
            }
        }
        for (std::vector<ConstructItem>::const_iterator ci(node.prerequisites.begin()), ciEnd(node.prerequisites.end()); ci != ciEnd; ++ci)
        {
            os << "\nPrerequisite: " << *ci;
        }*/
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
        /*if (node.techFlags)
        {
            streamTechFlags(os, node.techFlags);
        }
        if (node.economicFlags)
        {
            streamEconomicFlags(os, node.economicFlags);
        }
        if (node.militaryFlags)
        {
            streamMilitaryFlags(os, node.militaryFlags);
        }
        if (node.workerFlags)
        {
            streamWorkerFlags(os, node.workerFlags);
        }
        if (node.victoryFlags)
        {
            streamVictoryFlags(os, node.victoryFlags);
        }

        for (size_t i = 0, count = node.possibleBonuses.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getBonusInfo(node.possibleBonuses[i]).getType();
        }
        os << " ";

        for (size_t i = 0, count = node.possibleBuildings.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getBuildingInfo(node.possibleBuildings[i]).getType();
        }
        os << " ";

        for (size_t i = 0, count = node.possibleCivics.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getCivicInfo(node.possibleCivics[i]).getType();
        }
        os << " ";

        for (size_t i = 0, count = node.possibleUnits.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getUnitInfo(node.possibleUnits[i]).getType();
        }
        os << " ";

        for (size_t i = 0, count = node.possibleImprovements.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getImprovementInfo(node.possibleImprovements[i]).getType();
        }
        os << " ";

        for (size_t i = 0, count = node.possibleRemovableFeatures.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getFeatureInfo(node.possibleRemovableFeatures[i]).getType();
        }
        os << " ";

        for (size_t i = 0, count = node.possibleProcesses.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getProcessInfo(node.possibleProcesses[i]).getType();
        }
        os << " ";

        for (ResearchTech::WorkerTechDataMap::const_iterator ci(node.workerTechDataMap.begin()), ciEnd(node.workerTechDataMap.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity)
            {
                os << " city: " << narrow(pCity->getName()) << " ";
            }
            if (ci->second)
            {
                for (std::map<FeatureTypes, int>::const_iterator fi(ci->second->removableFeatureCounts.begin()), fiEnd(ci->second->removableFeatureCounts.end()); fi != fiEnd; ++fi)
                {
                    os << gGlobals.getFeatureInfo(fi->first).getType() << " feature count = " << fi->second;
                }
            }
        }*/

        return os;
    }

    /*
    std::ostream& operator << (std::ostream& os, EconomicFlags economicFlags)
    {
        os << "EconomicFlags: ";

        if (economicFlags.flags)
        {
            os << " flags = ";
            streamEconomicFlags(os, economicFlags.flags);
        }
        
        return os;
    }

    std::ostream& operator << (std::ostream& os, MilitaryFlags militaryFlags)
    {
        os << "MilitaryFlags: ";

        if (militaryFlags.flags)
        {
            os << " flags = ";
            streamEconomicFlags(os, militaryFlags.flags);
        }
        
        return os;
    }

    std::ostream& operator << (std::ostream& os, TechFlags techFlags)
    {
        os << "TechFlags: ";

        if (techFlags.flags)
        {
            os << " flags = ";
            streamTechFlags(os, techFlags.flags);
        }
        
        return os;
    }

    std::ostream& operator << (std::ostream& os, WorkerFlags workerFlags)
    {
        os << "WorkerFlags: ";

        if (workerFlags.flags)
        {
            os << " flags = ";
            streamWorkerFlags(os, workerFlags.flags);
        }
        
        return os;
    }

    std::ostream& operator << (std::ostream& os, VictoryFlags victoryFlags)
    {
        os << "VictoryFlags: ";

        if (victoryFlags.flags)
        {
            os << " flags = ";
            streamVictoryFlags(os, victoryFlags.flags);
        }
        
        return os;
    }*/
}