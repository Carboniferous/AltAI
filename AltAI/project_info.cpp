#include "AltAI.h"

#include "./project_info.h"

namespace AltAI
{
    namespace
    {
        struct ProjectInfoRequestData
        {
            explicit ProjectInfoRequestData(ProjectTypes projectType_ = NO_PROJECT, PlayerTypes playerType_ = NO_PLAYER) : projectType(projectType_), playerType(playerType_)
            {
            }
            ProjectTypes projectType;
            PlayerTypes playerType;
        };

        void getMiscNode(ProjectInfo::BaseNode& baseNode, const CvProjectInfo& projectInfo, const ProjectInfoRequestData& requestData)
        {
            ProjectInfo::MiscNode node;
            node.allowsNukes = projectInfo.isAllowsNukes();
            node.nukeInterceptionProb = projectInfo.getNukeInterception();
            node.specialBuilding = (SpecialBuildingTypes)projectInfo.getEveryoneSpecialBuilding();
            node.specialUnit = (SpecialUnitTypes)projectInfo.getEveryoneSpecialUnit();
            node.techShareCount = projectInfo.getTechShare();

            if (node.allowsNukes || node.nukeInterceptionProb != 0 || node.specialBuilding != NO_SPECIALBUILDING || node.specialUnit != NO_SPECIALUNIT || node.techShareCount > 0)
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getVictoryNode(ProjectInfo::BaseNode& baseNode, const CvProjectInfo& projectInfo, const ProjectInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumVictoryInfos(); i < count; ++i)
            {
                ProjectInfo::VictoryNode node;
                node.minNeededCount = projectInfo.getVictoryMinThreshold(i);
                node.neededCount = projectInfo.getVictoryThreshold(i);
                node.victoryDelayPercent = projectInfo.getVictoryDelayPercent();

                // delay percent is not victory type specific - presumably if a project is required for more than one victory type, all types are delayed equally
                if (node.minNeededCount > 0 || node.neededCount > 0)
                {
                    node.victoryType = (VictoryTypes)i;
                    baseNode.nodes.push_back(node);
                }
            }
        }

        void getPrereqNodes(ProjectInfo::BaseNode& baseNode, const CvProjectInfo& projectInfo, const ProjectInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumProjectInfos(); i < count; ++i)
            {
                int neededCount = projectInfo.getProjectsNeeded(i);
                if (neededCount > 0)
                {
                    ProjectInfo::PrereqNode node;
                    node.count = neededCount;
                    node.projectType = (ProjectTypes)i;
                    baseNode.nodes.push_back(node);
                }
            }
        }

        void getBonusNodes(ProjectInfo::BaseNode& baseNode, const CvProjectInfo& projectInfo, const ProjectInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumBonusInfos(); i < count; ++i)
            {
                int modifier = projectInfo.getBonusProductionModifier(i);
                if (modifier > 0)
                {
                    ProjectInfo::BonusNode node;
                    node.productionModifier = modifier;
                    node.bonusType = (BonusTypes)i;
                    baseNode.nodes.push_back(node);
                }
            }
        }

        ProjectInfo::BaseNode getBaseNode(const ProjectInfoRequestData& requestData)
        {
            ProjectInfo::BaseNode node;
            const CvProjectInfo& projectInfo = gGlobals.getProjectInfo(requestData.projectType);

            node.cost = projectInfo.getProductionCost();
            node.prereqTech = (TechTypes)projectInfo.getTechPrereq();
            node.anyonePrereqProject = (ProjectTypes)projectInfo.getAnyoneProjectPrereq();

            getPrereqNodes(node, projectInfo, requestData);
            getVictoryNode(node, projectInfo, requestData);
            getBonusNodes(node, projectInfo, requestData);
            getMiscNode(node, projectInfo, requestData);

            return node;
        }
    }

    ProjectInfo::ProjectInfo(ProjectTypes projectType) : projectType_(projectType)
    {
        init_();
    }

    const ProjectInfo::ProjectInfoNode& ProjectInfo::getInfo() const
    {
        return infoNode_;
    }

    void ProjectInfo::init_()
    {
        infoNode_ = getBaseNode(ProjectInfoRequestData(projectType_));
    }
}