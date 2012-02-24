#include "./project_tactics_visitors.h"
#include "./project_info.h"
#include "./tactic_actions.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./city.h"


namespace AltAI
{
    class MakeProjectConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeProjectConditionsVisitor(const Player& player, ProjectTypes projectType, VictoryTypes victoryType)
            : player_(player), constructItem_(projectType), victoryType_(victoryType)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const ProjectInfo::BaseNode& node)
        {
            if (node.prereqTech != NO_TECH)
            {
                constructItem_.requiredTechs.push_back(node.prereqTech);
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        void operator() (const ProjectInfo::VictoryNode& node)
        {
            if (node.victoryType == victoryType_)
            {
                if (CvTeamAI::getTeam(player_.getTeamID()).getProjectCount(constructItem_.projectType) < node.neededCount)
                {
                    constructItem_.victoryFlags |= VictoryFlags::Component_Project;
                }
            }
        }

        void operator() (const ProjectInfo::PrereqNode& node)
        {
            if (CvTeamAI::getTeam(player_.getTeamID()).getProjectCount(node.projectType) < node.count)
            {
                MakeProjectConditionsVisitor visitor(player_, node.projectType, victoryType_);
                boost::apply_visitor(visitor, player_.getAnalysis()->getProjectInfo(node.projectType)->getInfo());
                visitor.constructItem_.victoryFlags |= VictoryFlags::Prereq_Project;
                constructItem_.prerequisites.push_back(visitor.constructItem_);
            }
        }

        void operator() (const ProjectInfo::BonusNode& node)
        {
            constructItem_.positiveBonuses.push_back(node.bonusType);
        }

        //void operator() (const ProjectInfo::MiscNode& node)
        //{
        //} 

        ConstructItem getConstructItem() const
        {
            return constructItem_.isEmpty() ? ConstructItem(NO_PROJECT): constructItem_;
        }

    private:
        const Player& player_;
        ConstructItem constructItem_;
        VictoryTypes victoryType_;
    };

    ConstructItem getProjectTactics(const Player& player, ProjectTypes projectType, VictoryTypes victoryType, const boost::shared_ptr<ProjectInfo>& pProjectInfo)
    {
        MakeProjectConditionsVisitor visitor(player, projectType, victoryType);
        boost::apply_visitor(visitor, pProjectInfo->getInfo());

        return visitor.getConstructItem();
    }
}