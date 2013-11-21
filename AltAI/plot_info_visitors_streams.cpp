#include "AltAI.h"

#include "./plot_info_visitors_streams.h"

namespace AltAI
{
    class ShortDescVisitor : public boost::static_visitor<std::string>
    {
    public:
        ShortDescVisitor(PlayerTypes playerType) : playerType_(playerType)
        {
        }

        template <typename NodeType>
            result_type operator() (const NodeType& node) const
        {
            return result_type();
        }

        result_type operator() (const PlotInfo::BaseNode& node) const
        {
            std::ostringstream oss;

            if (node.terrainType != NO_TERRAIN)
            {
                oss << terrainChars[node.terrainType];
            }
            else
            {
                oss << " ";
            }

            if (node.featureType != NO_FEATURE)
            {
                oss << featureChars[node.featureType];
            }
            else
            {
                oss << " ";
            }

            if (node.plotType != PLOT_LAND)
            {
                oss << plotChars[node.plotType];
            }
            else
            {
                oss << " ";
            }

            return oss.str();
        }

    private:
        PlayerTypes playerType_;

        static std::vector<char> plotChars;
        // rest of these need to be updated if xml is changed
        static std::vector<char> terrainChars;
        static std::vector<char> featureChars;
        //static std::vector<char> bonusChars;
    };

    std::vector<char> ShortDescVisitor::plotChars = boost::assign::list_of('P')('H')(0)('O');  // Peak, Hill, (Land), Ocean
    // grass, plains, desert, tundra, snow, coast, ocean, peak, hill
    std::vector<char> ShortDescVisitor::terrainChars = boost::assign::list_of('G')('P')('D')('T')('S')('C')('O')('M')('H');
    // ice, jungle, flood plains, forest, fallout
    std::vector<char> ShortDescVisitor::featureChars = boost::assign::list_of('I')('J')('O')('L')('F')('R');

    std::string getPlotDebugString(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType)
    {
        return boost::apply_visitor(ShortDescVisitor(playerType), node);
    }

    std::ostream& operator << (std::ostream& os, const PlotInfo::NullNode& node)
    {
        return os;
    }

    std::ostream& operator << (std::ostream& os, const PlotInfo::BaseNode& node)
    {
        os << "BaseYield = " << node.yield;
        if (!node.bonusYield.isNone())
        {
            os << " Bonus = " << node.bonusYield;
        }
        os << node.featureRemovedNode;

        for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
        {
            os << node.improvementNodes[i];
        }

        if (node.tech != NO_TECH)
        {
            const CvTechInfo& techInfo = gGlobals.getTechInfo(node.tech);
            os << " Tech = " << techInfo.getType();
        }

        if (node.featureRemoveTech != NO_TECH)
        {
            const CvTechInfo& techInfo = gGlobals.getTechInfo(node.featureRemoveTech);
            os << " Tech = " << techInfo.getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const PlotInfo::ImprovementNode& node)
    {
        if (node.improvementType != NO_IMPROVEMENT)
        {
            const CvImprovementInfo& info = gGlobals.getImprovementInfo(node.improvementType);
            os << "\n\t(" << info.getType() << ")";
        }

        os << " BaseYield = " << node.yield;
        if (!node.bonusYield.isNone())
        {
            os << " Bonus = " << node.bonusYield;
        }

        for (size_t i = 0, count = node.buildConditions.size(); i < count; ++i)
        {
            if (i > 0)
            {
                os << " and ";
            }
            os << node.buildConditions[i];
        }

        for (size_t i = 0, count = node.techYields.size(); i < count; ++i)
        {
            const CvTechInfo& techInfo = gGlobals.getTechInfo(node.techYields[i].first);
            os << " Tech = " << techInfo.getType() << " " << node.techYields[i].second;
        }

        for (size_t i = 0, count = node.civicYields.size(); i < count; ++i)
        {
            const CvCivicInfo& civicInfo = gGlobals.getCivicInfo(node.civicYields[i].first);
            os << " Civic = " << civicInfo.getType() << " " << node.civicYields[i].second;
        }

        for (size_t i = 0, count = node.routeYields.size(); i < count; ++i)
        {
            const CvRouteInfo& routeInfo = gGlobals.getRouteInfo(node.routeYields[i].second.first);
            os << " Route = " << routeInfo.getType() << " " << node.routeYields[i].second.second;
        }

        if (!node.upgradeNode.empty())
        {
            os << node.upgradeNode[0];
        }
        
        return os;
    }

    std::ostream& operator << (std::ostream& os, const PlotInfo::UpgradeNode& node)
    {
        if (node.improvementType != NO_IMPROVEMENT)
        {
            const CvImprovementInfo& info = gGlobals.getImprovementInfo(node.improvementType);
            os << "\n\tUpgrades to (" << info.getType() << ")";
        }

        os << " BaseYield = " << node.yield;
        if (!node.bonusYield.isNone())
        {
            os << " Bonus = " << node.bonusYield;
        }

        for (size_t i = 0, count = node.buildConditions.size(); i < count; ++i)
        {
            if (i > 0)
            {
                os << " and ";
            }
            os << node.buildConditions[i];
        }

        for (size_t i = 0, count = node.techYields.size(); i < count; ++i)
        {
            const CvTechInfo& techInfo = gGlobals.getTechInfo(node.techYields[i].first);
            os << " Tech = " << techInfo.getType() << " " << node.techYields[i].second;
        }

        for (size_t i = 0, count = node.civicYields.size(); i < count; ++i)
        {
            const CvCivicInfo& civicInfo = gGlobals.getCivicInfo(node.civicYields[i].first);
            os << " Civic = " << civicInfo.getType() << " " << node.civicYields[i].second;
        }

        if (!node.upgradeNode.empty())
        {
            os << node.upgradeNode[0];
        }
        
        return os;
    }

    std::ostream& operator << (std::ostream& os, const PlotInfo::FeatureRemovedNode& node)
    {
        os << " (No Feature) BaseYield = " << node.yield;
        if (!node.bonusYield.isNone())
        {
            os << " Bonus = " << node.bonusYield;
        }

        for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
        {
            os << node.improvementNodes[i];
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const PlotInfo::HasTech& node)
    {
        return os << " requires tech: " << gGlobals.getTechInfo(node.techType).getType();
    }

    std::ostream& operator << (std::ostream& os, const PlotInfo::BuildOrCondition& node)
    {
        for (size_t i = 0, count = node.conditions.size(); i < count; ++i)
        {
            if (i > 0)
            {
                os << " or ";
            }
            os << node.conditions[i];
        }
        return os;
    }
}