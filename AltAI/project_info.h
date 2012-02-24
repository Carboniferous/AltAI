#pragma once

#include "./utils.h"

namespace AltAI
{
    class ProjectInfo
    {
    public:
        explicit ProjectInfo(ProjectTypes projectType);

        ProjectTypes getProjectType() const
        {
            return projectType_;
        }

        struct NullNode
        {
            NullNode() {}
        };

        struct VictoryNode
        {
            VictoryNode() : victoryType(NO_VICTORY), minNeededCount(0), neededCount(0), victoryDelayPercent(0)
            {
            }
            VictoryTypes victoryType;
            int minNeededCount, neededCount;
            int victoryDelayPercent;
        };

        struct BonusNode
        {
            BonusNode() : bonusType(NO_BONUS), productionModifier(0)
            {
            }
            BonusTypes bonusType;
            int productionModifier;
        };

        struct PrereqNode
        {
            PrereqNode() : projectType(NO_PROJECT), count(0)
            {
            }
            ProjectTypes projectType;
            int count;
        };

        struct MiscNode
        {
            MiscNode() : allowsNukes(false), nukeInterceptionProb(0), specialBuilding(NO_SPECIALBUILDING), specialUnit(NO_SPECIALUNIT), techShareCount(-1)
            {
            }

            bool allowsNukes;
            int nukeInterceptionProb;
            SpecialBuildingTypes specialBuilding;
            SpecialUnitTypes specialUnit;
            int techShareCount;
        };

        struct BaseNode;

        typedef boost::variant<NullNode, PrereqNode, VictoryNode, BonusNode, MiscNode, boost::recursive_wrapper<BaseNode> > ProjectInfoNode;

        struct BaseNode
        {
            BaseNode() : cost(-1), prereqTech(NO_TECH), anyonePrereqProject(NO_PROJECT)
            {
            }

            int cost;
            TechTypes prereqTech;
            ProjectTypes anyonePrereqProject;

            std::vector<ProjectInfoNode> nodes;
        };

        const ProjectInfoNode& getInfo() const;

    private:
        void init_();

        ProjectTypes projectType_;
        ProjectInfoNode infoNode_;
    };
}