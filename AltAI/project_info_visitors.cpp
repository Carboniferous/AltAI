#include "./project_info_visitors.h"
#include "./project_info.h"
#include "./project_info_streams.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./civ_helper.h"

namespace AltAI
{
    boost::shared_ptr<ProjectInfo> makeProjectInfo(ProjectTypes projectType, PlayerTypes)
    {
        return boost::shared_ptr<ProjectInfo>(new ProjectInfo(projectType));
    }

    void streamProjectInfo(std::ostream& os, const boost::shared_ptr<ProjectInfo>& pProjectInfo)
    {
        os << pProjectInfo->getInfo();
    }

    TechTypes getRequiredTech(const boost::shared_ptr<ProjectInfo>& pProjectInfo)
    {
        const ProjectInfo::BaseNode& node = boost::get<ProjectInfo::BaseNode>(pProjectInfo->getInfo());
        return node.prereqTech;
    }

    class CouldConstructProjectVisitor : public boost::static_visitor<bool>
    {
    public:
        CouldConstructProjectVisitor(const Player& player, const City& city, int lookaheadDepth) : player_(player), city_(city), lookaheadDepth_(lookaheadDepth)
        {
            civHelper_ = player.getCivHelper();
            pAnalysis_ = player.getAnalysis();
        }

        template <typename T>
            bool operator() (const T&) const
        {
            return true;
        }

        bool operator() (const ProjectInfo::BaseNode& node) const
        {
            // if we don't we have the tech and its depth is deeper than our lookaheadDepth, return false
            if (!civHelper_->hasTech(node.prereqTech) &&
                (lookaheadDepth_ == 0 ? true : pAnalysis_->getTechResearchDepth(node.prereqTech) > lookaheadDepth_))
            {
                return false;
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                if (!boost::apply_visitor(*this, node.nodes[i]))
                {
                    return false;
                }
            }

            return true;
        }

        bool operator() (const ProjectInfo::PrereqNode& node) const
        {
            return CvTeamAI::getTeam(player_.getTeamID()).getProjectCount(node.projectType) >= node.count;
        }

    private:
        const Player& player_;
        const City& city_;
        int lookaheadDepth_;
        boost::shared_ptr<CivHelper> civHelper_;
        boost::shared_ptr<PlayerAnalysis> pAnalysis_;
    };

    bool couldConstructProject(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<ProjectInfo>& pProjectInfo)
    {
        return boost::apply_visitor(CouldConstructProjectVisitor(player, city, lookaheadDepth), pProjectInfo->getInfo());
    }
}