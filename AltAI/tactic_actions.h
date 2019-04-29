#pragma once

#include "./utils.h"

namespace AltAI
{
    const FlavorTypes AI_FLAVOR_MILITARY = (FlavorTypes)0;
    const FlavorTypes AI_FLAVOR_RELIGION = (FlavorTypes)1;
    const FlavorTypes AI_FLAVOR_PRODUCTION = (FlavorTypes)2;
    const FlavorTypes AI_FLAVOR_GOLD = (FlavorTypes)3;
    const FlavorTypes AI_FLAVOR_SCIENCE = (FlavorTypes)4;
    const FlavorTypes AI_FLAVOR_CULTURE = (FlavorTypes)5;
    const FlavorTypes AI_FLAVOR_GROWTH = (FlavorTypes)6;


    struct ResearchTech
    {
        ResearchTech() : techType(NO_TECH), targetTechType(NO_TECH), depth(-1) /*, techFlags(0), economicFlags(0), militaryFlags(0), workerFlags(0), victoryFlags(0) */ {}

        explicit ResearchTech(TechTypes techType_, int depth_ = 1)
            : techType(techType_), targetTechType(techType_), depth(depth_)
        {
        }

        TechTypes techType, targetTechType;
        int depth;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        void debug(std::ostream& os) const;
    };

	class IUnitEventGenerator;
	typedef boost::shared_ptr<IUnitEventGenerator> IUnitEventGeneratorPtr;

    struct ConstructItem
    {
        ConstructItem()
            : buildingType(NO_BUILDING), unitType(NO_UNIT), projectType(NO_PROJECT), improvementType(NO_IMPROVEMENT), processType(NO_PROCESS)
        {}

        explicit ConstructItem(BuildingTypes buildingType_)
            : buildingType(buildingType_), unitType(NO_UNIT), projectType(NO_PROJECT), improvementType(NO_IMPROVEMENT), processType(NO_PROCESS)
        {}

        explicit ConstructItem(UnitTypes unitType_)
            : buildingType(NO_BUILDING), unitType(unitType_), projectType(NO_PROJECT), improvementType(NO_IMPROVEMENT), processType(NO_PROCESS)
        {}

        explicit ConstructItem(ProjectTypes projectType_)
            : buildingType(NO_BUILDING), unitType(NO_UNIT), projectType(projectType_), improvementType(NO_IMPROVEMENT), processType(NO_PROCESS)
        {}

        explicit ConstructItem(ImprovementTypes improvementType_)
            : buildingType(NO_BUILDING), unitType(NO_UNIT), projectType(NO_PROJECT), improvementType(improvementType_), processType(NO_PROCESS)
        {}

        explicit ConstructItem(ProcessTypes processType_)
            : buildingType(NO_BUILDING), unitType(NO_UNIT), projectType(NO_PROJECT), improvementType(NO_IMPROVEMENT), processType(processType_)
        {}

        BuildingTypes buildingType;
        UnitTypes unitType;
        ProjectTypes projectType;
        ImprovementTypes improvementType;
        ProcessTypes processType;

        XYCoords buildTarget;
        IUnitEventGeneratorPtr pUnitEventGenerator;

        void debug(std::ostream& os) const;

        // guarantee is that only one field is set in constructItem_
        boost::tuple<UnitTypes, BuildingTypes, ProcessTypes, ProjectTypes> getConstructItem()
        {
            return boost::make_tuple(unitType, buildingType, processType, projectType);
        }

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);
    };
}