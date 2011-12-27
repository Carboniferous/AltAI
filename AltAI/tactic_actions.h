#pragma once

#include "./utils.h"
#include "./city_improvements.h"

namespace AltAI
{
    const FlavorTypes AI_FLAVOR_MILITARY = (FlavorTypes)0;
    const FlavorTypes AI_FLAVOR_RELIGION = (FlavorTypes)1;
    const FlavorTypes AI_FLAVOR_PRODUCTION = (FlavorTypes)2;
    const FlavorTypes AI_FLAVOR_GOLD = (FlavorTypes)3;
    const FlavorTypes AI_FLAVOR_SCIENCE = (FlavorTypes)4;
    const FlavorTypes AI_FLAVOR_CULTURE = (FlavorTypes)5;
    const FlavorTypes AI_FLAVOR_GROWTH = (FlavorTypes)6;

    struct EconomicFlags
    {
        enum Flags
        {
            None = 0, Output_Food = (1 << 0), Output_Production = (1 << 1), Output_Commerce = (1 << 2), Output_Gold = (1 << 3), Output_Research = (1 << 4),
            Output_Culture = (1 << 5), Output_Espionage = (1 << 6), Output_Happy = (1 << 7), Output_Health = (1 << 8), Output_Maintenance_Reduction = (1 << 9),
            Output_Settler = (1 << 10), Num_Output_FlagTypes = 11
        };

        EconomicFlags() : flags(None) {}

        int flags;
    };

    struct MilitaryFlags
    {
        enum Flags
        {
            None = 0, Output_Production = (1 << 0), Output_Experience = (1 << 1), Output_Attack = (1 << 2), Output_Defence = (1 << 3), Output_City_Attack = (1 << 4),
            Output_City_Defence = (1 << 5), Output_UnitCombat_Counter = (1 << 6), Output_UnitClass_Counter = (1 << 7),
            Output_Collateral = (1 << 8), Output_Make_Unit_Available = (1 << 9), Output_Extra_Mobility = (1 << 10),
            Output_Combat_Unit = (1 << 11), Output_Unit_Transport = (1 << 12), Output_Explore = (1 << 13), Num_Output_FlagTypes = 14
        };

        MilitaryFlags() : flags(None) {}

        int flags;
    };

    struct BuildingFlags
    {
        enum Flags
        {
            None = 0, Building_National_Wonder = (1 << 0), Building_World_Wonder = (1 << 1)
        };

        BuildingFlags() : flags(None) {}

        int flags;
    };

    struct WorkerFlags
    {
        enum Flags
        {
            None = 0, Faster_Workers = (1 << 0), New_Improvements = (1 << 1), Better_Improvements = (1 << 2), Remove_Features = (1 << 3)
        };

        WorkerFlags() : flags(None) {}

        int flags;
    };

    struct TechFlags
    {
        enum Flags
        {
            None = 0, Found_Religion = (1 << 0), Free_Tech = (1 << 2), Free_GP = (1 << 3), Flexible_Commerce = (1 << 4), Trade_Routes = (1 << 5)
        };

        TechFlags() : flags(None) {}

        int flags;
    };

    struct DiplomacyFlags
    {
        enum Flags
        {
            None = 0
        };

        DiplomacyFlags() : flags(None) {}

        int flags;
    };

    struct WorkerTechCityData
    {
        std::map<FeatureTypes, int> removableFeatureCounts;
        std::vector<CityImprovementManager::PlotImprovementData> newImprovements;
    };

    struct ResearchTech
    {
        ResearchTech() : techType(NO_TECH), targetTechType(NO_TECH), depth(-1), techFlags(0), economicFlags(0), militaryFlags(0), workerFlags(0) {}

        explicit ResearchTech(TechTypes techType_, int depth_ = 1)
            : techType(techType_), targetTechType(techType_), depth(depth_),
              techFlags(0), economicFlags(0), militaryFlags(0), workerFlags(0)
        {
        }

        TechTypes techType, targetTechType;
        int depth;
        int techFlags, economicFlags, militaryFlags, workerFlags;
        std::vector<BuildingTypes> possibleBuildings;
        std::vector<UnitTypes> possibleUnits;
        std::vector<CivicTypes> possibleCivics;
        std::vector<BonusTypes> possibleBonuses;
        std::vector<ImprovementTypes> possibleImprovements;
        std::vector<FeatureTypes> possibleRemovableFeatures;
        std::vector<ProcessTypes> possibleProcesses;

        typedef std::map<IDInfo, boost::shared_ptr<WorkerTechCityData> > WorkerTechDataMap;
        WorkerTechDataMap workerTechDataMap;

        bool hasFlags() const
        {
            return techFlags || economicFlags || militaryFlags || workerFlags;
        }

        bool isEmpty() const
        {
            return !hasFlags() && possibleBuildings.empty() && possibleBonuses.empty() && possibleCivics.empty() && 
                possibleRemovableFeatures.empty() && possibleImprovements.empty() && possibleUnits.empty() && possibleProcesses.empty() &&
                workerTechDataMap.empty();
        }

        bool hasNewImprovements(IDInfo city) const;

        void merge(const ResearchTech& other);

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        void debug(std::ostream& os) const;
    };

    struct ResearchTechFinder
    {
        explicit ResearchTechFinder(const ResearchTech& researchTech_)
            : researchTech(researchTech_)
        {
        }

        bool operator() (const ResearchTech& other) const
        {
            return researchTech.techType == other.techType;
        }

        const ResearchTech& researchTech;
    };

    typedef std::list<ResearchTech> ResearchList;
    typedef ResearchList::const_iterator ResearchListConstIter;
    typedef ResearchList::iterator ResearchListIter;

    struct ConstructItem
    {
        ConstructItem()
            : buildingType(NO_BUILDING), unitType(NO_UNIT), improvementType(NO_IMPROVEMENT), processType(NO_PROCESS), economicFlags(0), militaryFlags(0), buildingFlags(0) {}

        explicit ConstructItem(BuildingTypes buildingType_)
            : buildingType(buildingType_), unitType(NO_UNIT), improvementType(NO_IMPROVEMENT), processType(NO_PROCESS), economicFlags(0), militaryFlags(0), buildingFlags(0) {}
        explicit ConstructItem(UnitTypes unitType_)
            : buildingType(NO_BUILDING), unitType(unitType_), improvementType(NO_IMPROVEMENT), processType(NO_PROCESS), economicFlags(0), militaryFlags(0), buildingFlags(0) {}
        explicit ConstructItem(ImprovementTypes improvementType_)
            : buildingType(NO_BUILDING), unitType(NO_UNIT), improvementType(improvementType_), processType(NO_PROCESS), economicFlags(0), militaryFlags(0), buildingFlags(0) {}
        explicit ConstructItem(ProcessTypes processType_)
            : buildingType(NO_BUILDING), unitType(NO_UNIT), improvementType(NO_IMPROVEMENT), processType(processType_), economicFlags(0), militaryFlags(0), buildingFlags(0) {}

        bool hasFlags() const
        {
            return economicFlags || militaryFlags || buildingFlags;
        }

        bool isEmpty() const
        {
            return !hasFlags() && possibleBuildTypes.empty();
        }

        void merge(const ConstructItem& other);

        BuildingTypes buildingType;
        UnitTypes unitType;
        ImprovementTypes improvementType;
        ProcessTypes processType;
        std::map<BuildTypes, int> possibleBuildTypes;
        std::vector<BonusTypes> positiveBonuses;
        std::vector<TechTypes> requiredTechs;
        // int pair - first int is value, second is either -1, or the unit class or unit combat type the value is for.
        // the value of the map key determines how to interpret the value
        typedef std::multimap<MilitaryFlags::Flags, std::pair<int, int> > MilitaryFlagValuesMap;
        MilitaryFlagValuesMap militaryFlagValuesMap;
        int economicFlags, militaryFlags, buildingFlags;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);
    };

    struct ConstructItemFinder
    {
        explicit ConstructItemFinder(const ConstructItem& constructItem_)
            :  constructItem(constructItem_)
        {
        }

        bool operator() (const ConstructItem& other) const
        {
            return constructItem.buildingType == other.buildingType && constructItem.unitType == other.unitType &&
                constructItem.improvementType == other.improvementType && constructItem.processType == other.processType;
        }

        const ConstructItem& constructItem;
    };

    typedef std::list<ConstructItem> ConstructList;
    typedef ConstructList::const_iterator ConstructListConstIter;
    typedef ConstructList::iterator ConstructListIter;
}