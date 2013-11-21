#include "AltAI.h"

#include "./plot_info_visitors.h"
#include "./plot_info_visitors_streams.h"
#include "./tech_info_visitors.h"
#include "./gamedata_analysis.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./civ_helper.h"
#include "./specialist_helper.h"
#include "./helper_fns.h"
#include "./city_data.h"
#include "./city_log.h"
#include "./civ_log.h"
#include "./error_log.h"
#include "./game.h"
#include "./city.h"

namespace AltAI
{
    namespace
    {
        YieldValueFunctor makeSYieldP()
        {
            return YieldValueFunctor(OutputUtils<PlotYield>::getDefaultWeights());
        }

        PlotYield getExtraYield(const PlotYield yield, PlayerTypes playerType)
        {
            PlotYield newPlotYield(yield);
            const CvPlayer& player = CvPlayerAI::getPlayer(playerType);
            static const int extraYield = gGlobals.getDefineINT("EXTRA_YIELD");

            for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
            {
                int yieldThreshold = player.getExtraYieldThreshold((YieldTypes)yieldType);
                if (yieldThreshold > 0 && yield[yieldType] >= yieldThreshold)
                {
                    newPlotYield[yieldType] += extraYield;
                }
            }
            return newPlotYield;
        }

        PlotYield getExtraGoldenAgeYield(const PlotYield plotYield)
        {
            PlotYield newPlotYield(plotYield);
            for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
            {
                if (plotYield[yieldType] >= gGlobals.getYieldInfo((YieldTypes)yieldType).getGoldenAgeYieldThreshold())
			    {
				    newPlotYield[yieldType] += gGlobals.getYieldInfo((YieldTypes)yieldType).getGoldenAgeYield();
                }
			}
            return newPlotYield;
        }
    }

    class ImprovementBuildCondVisitor : public boost::static_visitor<bool>
    {
    public:
        ImprovementBuildCondVisitor(PlayerTypes playerType, int lookaheadDepth) : playerType_(playerType), lookaheadDepth_(lookaheadDepth)
        {
            civHelper_ = gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getCivHelper();
            pAnalysis_ = gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getAnalysis();
        }

        bool operator() (const PlotInfo::NullNode& node) const
        {
            return true;
        }

        bool operator() (const PlotInfo::HasTech& node) const
        {
            return civHelper_->hasTech(node.techType) || 
                (lookaheadDepth_ == 0 ? false : pAnalysis_->getTechResearchDepth(node.techType) <= lookaheadDepth_);
        }

        bool operator() (const PlotInfo::BuildOrCondition& node) const
        {
            for (size_t i = 0, count = node.conditions.size(); i < count; ++i)
            {
                if (boost::apply_visitor(*this, node.conditions[i]))
                {
                    return true;
                }
            }
            return false;
        }

    private:
        PlayerTypes playerType_;
        boost::shared_ptr<CivHelper> civHelper_;
        boost::shared_ptr<PlayerAnalysis> pAnalysis_;
        int lookaheadDepth_;
    };

    bool meetsBuildConditions(const std::vector<PlotInfo::BuildCondition>& conditions, PlayerTypes playerType, int lookaheadDepth)
    {
        for (size_t i = 0, count = conditions.size(); i < count; ++i)
        {
            if (!boost::apply_visitor(ImprovementBuildCondVisitor(playerType, lookaheadDepth), conditions[i]))
            {
                return false;
            }
        }
        return true;
    }

    class BasicMaxYieldFinder : public boost::static_visitor<std::pair<PlotYield, ImprovementTypes> >
    {
    public:
        BasicMaxYieldFinder(PlayerTypes playerType, int lookaheadDepth) : playerType_(playerType), lookaheadDepth_(lookaheadDepth), valueF_(makeSYieldP())
        {
            pAnalysis_ = gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getAnalysis();
        }

        result_type operator() (const PlotInfo::NullNode& node) const
        {
            return std::make_pair(PlotYield(), NO_IMPROVEMENT);
        }

        result_type operator() (const PlotInfo::BaseNode& node) const
        {
            if (node.isImpassable || pAnalysis_->getTechResearchDepth(node.tech) > lookaheadDepth_)
            {
                return std::make_pair(PlotYield(), NO_IMPROVEMENT);
            }

            PlotYield maxImprovedYield;
            ImprovementTypes bestImprovement = NO_IMPROVEMENT;
            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                PlotYield improvementYield;
                ImprovementTypes improvementType;
                boost::tie(improvementYield, improvementType) = (*this)(node.improvementNodes[i]);
                if (valueF_(improvementYield, maxImprovedYield))
                {
                    maxImprovedYield = improvementYield;
                    bestImprovement = improvementType;
                }
            }
            ImprovementTypes bestImprovementWithFeatureRemoved = NO_IMPROVEMENT;
            PlotYield maxFeatureRemovedYield;
            if (pAnalysis_->getTechResearchDepth(node.featureRemoveTech) <= lookaheadDepth_)
            {
                boost::tie(maxFeatureRemovedYield, bestImprovementWithFeatureRemoved) = boost::apply_visitor(*this, node.featureRemovedNode);
            }

            PlotYield baseYield = getExtraYield(node.yield + node.bonusYield, playerType_);
            if (valueF_(maxImprovedYield, maxFeatureRemovedYield))
            {
                return valueF_(baseYield, maxImprovedYield) ? std::make_pair(baseYield, NO_IMPROVEMENT) : std::make_pair(maxImprovedYield, bestImprovement);
            }
            else
            {
                return valueF_(baseYield, maxFeatureRemovedYield) ? std::make_pair(baseYield, NO_IMPROVEMENT) : std::make_pair(maxFeatureRemovedYield, bestImprovementWithFeatureRemoved);
            }
        }

        result_type operator() (const PlotInfo::FeatureRemovedNode& node) const
        {
            PlotYield maxImprovedYield;
            ImprovementTypes bestImprovement = NO_IMPROVEMENT;
            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                PlotYield improvementYield;
                ImprovementTypes improvementType;
                boost::tie(improvementYield, improvementType) = (*this)(node.improvementNodes[i]);
                if (valueF_(improvementYield, maxImprovedYield))
                {
                    maxImprovedYield = improvementYield;
                    bestImprovement = improvementType;
                }
            }
            PlotYield baseYield = getExtraYield(node.yield + node.bonusYield, playerType_);
            return valueF_(baseYield, maxImprovedYield) ? std::make_pair(baseYield, NO_IMPROVEMENT) : std::make_pair(maxImprovedYield, bestImprovement);
        }

        template <typename NodeType>
            result_type operator() (const NodeType& node) const  // ImprovementNode and UpgradeNode
        {
            if (!meetsBuildConditions(node.buildConditions, playerType_, lookaheadDepth_))
            {
                return std::make_pair(PlotYield(), NO_IMPROVEMENT);
            }

            PlotYield totalTechYield;
            for (size_t i = 0, count = node.techYields.size(); i < count; ++i)
            {
                if (pAnalysis_->getTechResearchDepth(node.techYields[i].first) <= lookaheadDepth_)
                {
                    totalTechYield += node.techYields[i].second;
                }
            }

            PlotYield totalRouteYield;
            for (size_t i = 0, count = node.routeYields.size(); i < count; ++i)
            {
                if (pAnalysis_->getTechResearchDepth(node.routeYields[i].first) <= lookaheadDepth_)
                {
                    totalRouteYield += node.routeYields[i].second.second;
                }
            }

            typedef std::map<CivicOptionTypes, PlotYield> CivicYieldsMap;
            CivicYieldsMap civicYieldsMap;
            for (size_t i = 0, count = node.civicYields.size(); i < count; ++i)
            {
                const CvCivicInfo& civicInfo = gGlobals.getCivicInfo(node.civicYields[i].first);

                // TODO - handle civics available via wonders/votes, etc...
                TechTypes civicTech = (TechTypes)civicInfo.getTechPrereq();
                if (pAnalysis_->getTechResearchDepth(civicTech) > lookaheadDepth_)
                {
                    continue;
                }

                CivicOptionTypes civicOptionType = (CivicOptionTypes)civicInfo.getCivicOptionType();
                CivicYieldsMap::iterator iter = civicYieldsMap.find(civicOptionType);
                if (iter == civicYieldsMap.end())
                {
                    iter = civicYieldsMap.insert(std::make_pair(civicOptionType, node.civicYields[i].second)).first;
                }
                else
                {
                    iter->second = PlotUtils::compareOutput(iter->second, node.civicYields[i].second, makeSYieldP());
                }
            }

            PlotYield maxCivicsYield;
            for (CivicYieldsMap::const_iterator ci(civicYieldsMap.begin()), ciEnd(civicYieldsMap.end()); ci != ciEnd; ++ci)
            {
                maxCivicsYield += ci->second;
            }

            PlotYield ourYield = getExtraYield(node.yield + node.bonusYield + totalTechYield + totalRouteYield + maxCivicsYield, playerType_);
            if (!node.upgradeNode.empty())
            {
                PlotYield upgradeYield;
                ImprovementTypes upgradeImprovementType;
                boost::tie(upgradeYield, upgradeImprovementType) = (*this)(node.upgradeNode[0]);
                return valueF_(ourYield, upgradeYield) ? std::make_pair(ourYield, node.improvementType) : std::make_pair(upgradeYield, upgradeImprovementType);
            }
            else
            {
                return std::make_pair(ourYield, node.improvementType);
            }
        }

    private:
        PlayerTypes playerType_;
        boost::shared_ptr<PlayerAnalysis> pAnalysis_;
        int lookaheadDepth_;
        YieldValueFunctor valueF_;
    };

    std::pair<PlotYield, ImprovementTypes> getMaxYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, int lookaheadDepth)
    {
        return boost::apply_visitor(BasicMaxYieldFinder(playerType, lookaheadDepth), node);
    }


    class YieldsFinder : public boost::static_visitor<std::vector<std::pair<PlotYield, ImprovementTypes> > >
    {
    public:
        YieldsFinder(PlayerTypes playerType, int lookaheadDepth)
            : playerType_(playerType), lookaheadDepth_(lookaheadDepth)
        {
            civHelper_ = gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getCivHelper();
            pAnalysis_ = gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getAnalysis();
        }

        result_type operator() (const PlotInfo::NullNode& node) const
        {
            return result_type();
        }

        result_type operator() (const PlotInfo::BaseNode& node) const
        {
            if (node.isImpassable || !techIsAvailable_(node.tech))
            {
                return result_type();
            }

            result_type results(1, std::make_pair(getExtraYield(node.yield + node.bonusYield, playerType_), NO_IMPROVEMENT));

            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                result_type improvementResults((*this)(node.improvementNodes[i]));
                std::copy(improvementResults.begin(), improvementResults.end(), std::back_inserter(results));
            }

            if (techIsAvailable_(node.featureRemoveTech))
            {
                result_type featureResults(boost::apply_visitor(*this, node.featureRemovedNode));
                std::copy(featureResults.begin(), featureResults.end(), std::back_inserter(results));
            }

            return results;
        }

        result_type operator() (const PlotInfo::FeatureRemovedNode& node) const
        {
            result_type results(1, std::make_pair(getExtraYield(node.yield + node.bonusYield, playerType_), NO_IMPROVEMENT));

            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                result_type improvementResults((*this)(node.improvementNodes[i]));
                std::copy(improvementResults.begin(), improvementResults.end(), std::back_inserter(results));
            }

            return results;
        }

        template <typename NodeType>
            result_type operator() (const NodeType& node) const  // ImprovementNode and UpgradeNode
        {
            if (!meetsBuildConditions(node.buildConditions, playerType_, lookaheadDepth_))
            {
                return result_type();
            }

            PlotYield totalTechYield;
            for (size_t i = 0, count = node.techYields.size(); i < count; ++i)
            {
                if (techIsAvailable_(node.techYields[i].first))
                {
                    totalTechYield += node.techYields[i].second;
                }
            }

            PlotYield totalRouteYield;
            for (size_t i = 0, count = node.routeYields.size(); i < count; ++i)
            {
                if (techIsAvailable_(node.routeYields[i].first))
                {
                    totalRouteYield += node.routeYields[i].second.second;
                }
            }

            // ignore civic effects - just want to construct list of possible improvements
            result_type results(1, std::make_pair(getExtraYield(node.yield + node.bonusYield + totalTechYield + totalRouteYield, playerType_), node.improvementType));

            if (!node.upgradeNode.empty())
            {
                result_type upgradeResults((*this)(node.upgradeNode[0]));
                for (size_t i = 0, count = upgradeResults.size(); i < count; ++i)
                {
                    if (isStrictlyGreater(upgradeResults[i].first, results[0].first))
                    {
                        results[0] = upgradeResults[i];
                    }
                    else
                    {
                        results.push_back(upgradeResults[i]);
                    }
                }
            }

            return results;
        }

    private:

        bool techIsAvailable_(TechTypes techType) const
        {
            return civHelper_->hasTech(techType) || 
                (lookaheadDepth_ == 0 ? false : pAnalysis_->getTechResearchDepth(techType) <= lookaheadDepth_);
        }

        PlayerTypes playerType_;
        boost::shared_ptr<CivHelper> civHelper_;
        boost::shared_ptr<PlayerAnalysis> pAnalysis_;
        const int lookaheadDepth_;
    };

    std::vector<std::pair<PlotYield, ImprovementTypes> > getYields(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, int lookaheadDepth)
    {
        return boost::apply_visitor(YieldsFinder(playerType, lookaheadDepth), node);
    }

    class HasYieldVisitor : public boost::static_visitor<bool>
    {
    public:
        HasYieldVisitor(PlayerTypes playerType) : playerType_(playerType)
        {
        }

        bool operator() (const PlotInfo::NullNode& node) const
        {
            return false;
        }

        bool operator() (const PlotInfo::BaseNode& node) const
        {
            if (node.isImpassable)
            {
                return false;
            }

            if (boost::apply_visitor(*this, node.featureRemovedNode))
            {
                return true;
            }

            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                if ((*this)(node.improvementNodes[i]))
                {
                    return true;
                }
            }

            return !isEmpty(node.yield + node.bonusYield);
        }

        bool operator() (const PlotInfo::FeatureRemovedNode& node) const
        {
            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                if ((*this)(node.improvementNodes[i]))
                {
                    return true;
                }
            }
            return !isEmpty(node.yield + node.bonusYield);
        }

        template <typename NodeType> // ImprovementNode and UpgradeNode
            bool operator() (const NodeType& node) const
        {
            if (!node.upgradeNode.empty())
            {
                if ((*this)(node.upgradeNode[0]))
                {
                    return true;
                }
            }

            PlotYield totalYield(node.yield + node.bonusYield);
            for (size_t i = 0, count = node.techYields.size(); i < count; ++i)
            {
                totalYield += node.techYields[i].second;
            }

            for (size_t i = 0, count = node.routeYields.size(); i < count; ++i)
            {
                totalYield += node.routeYields[i].second.second;
            }

            for (size_t i = 0, count = node.civicYields.size(); i < count; ++i)
            {
                totalYield += node.civicYields[i].second;
            }

            return !isEmpty(getExtraYield(totalYield, playerType_));
        }

    private:
        PlayerTypes playerType_;
    };

    bool hasPossibleYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType)
    {
        return boost::apply_visitor(HasYieldVisitor(playerType), node);
    }

    class YieldVisitor : public boost::static_visitor<PlotYield>
    {
    public:
        explicit YieldVisitor(PlayerTypes playerType, RouteTypes routeType = NO_ROUTE, bool isGoldenAge = false)
            : playerType_(playerType), routeType_(routeType), isGoldenAge_(isGoldenAge)
        {
        }

        PlotYield operator() (const PlotInfo::NullNode& node) const
        {
            return PlotYield();
        }

        PlotYield operator() (const PlotInfo::BaseNode& node) const
        {
            PlotYield plotYield = getExtraYield(node.yield + node.bonusYield, playerType_);
            return isGoldenAge_ ? getExtraGoldenAgeYield(plotYield) : plotYield;
        }

        PlotYield operator() (const PlotInfo::FeatureRemovedNode& node) const
        {
            PlotYield plotYield = getExtraYield(node.yield + node.bonusYield, playerType_);
            return isGoldenAge_ ? getExtraGoldenAgeYield(plotYield) : plotYield;
        }

        template <typename NodeType> // ImprovementNode and UpgradeNode
            PlotYield operator() (const NodeType& node) const
        {
            const CvPlayer& player = CvPlayerAI::getPlayer(playerType_);

            boost::shared_ptr<CivHelper> civHelper = gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getCivHelper();

            PlotYield totalYield(node.yield + node.bonusYield);
            for (size_t i = 0, count = node.techYields.size(); i < count; ++i)
            {
                if (civHelper->hasTech(node.techYields[i].first))
                {
                    totalYield += node.techYields[i].second;
                }
            }

            for (size_t i = 0, count = node.routeYields.size(); i < count; ++i)
            {
                if (node.routeYields[i].second.first == routeType_ && (node.routeYields[i].first == NO_TECH || civHelper->hasTech(node.routeYields[i].first)))
                {
                    // TODO - check have bonus
                    totalYield += node.routeYields[i].second.second;
                    break;
                }
            }

            for (size_t i = 0, count = node.civicYields.size(); i < count; ++i)
            {
                if (civHelper->isInCivic(node.civicYields[i].first))
                {
                    totalYield += node.civicYields[i].second;
                }
            }

            PlotYield plotYield = getExtraYield(totalYield, playerType_);
            return isGoldenAge_ ? getExtraGoldenAgeYield(plotYield) : plotYield;
        }

    private:
        PlayerTypes playerType_;
        RouteTypes routeType_;
        bool isGoldenAge_;
    };

    class CalcYieldVisitor : public boost::static_visitor<std::pair<bool, PlotYield> >
    {
    public:
        CalcYieldVisitor(PlayerTypes playerType, ImprovementTypes improvementType, FeatureTypes featureType, RouteTypes routeType, bool isGoldenAge)
            : playerType_(playerType), improvementType_(improvementType), featureType_(featureType), routeType_(routeType), isGoldenAge_(isGoldenAge)
        {
        }

        std::pair<bool, PlotYield> operator() (const PlotInfo::NullNode& node) const
        {
            return std::make_pair(false, PlotYield());
        }

        std::pair<bool, PlotYield> operator() (const PlotInfo::BaseNode& node) const
        {
            if (node.featureType != featureType_)
            {
                return boost::apply_visitor(*this, node.featureRemovedNode);
            }

            if (improvementType_ == NO_IMPROVEMENT)
            {
                return std::make_pair(true, YieldVisitor(playerType_, routeType_, isGoldenAge_)(node));
            }

            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                result_type result = (*this)(node.improvementNodes[i]);
                if (result.first)
                {
                    return result;
                }
            }
            return std::make_pair(false, PlotYield());
        }

        template <typename NodeType>
            std::pair<bool, PlotYield> operator() (const NodeType& node) const
        {
            if (node.improvementType == improvementType_)  // found improvement
            {
                return std::make_pair(true, YieldVisitor(playerType_, routeType_, isGoldenAge_)(node));
            }
            else if (!node.upgradeNode.empty())
            {
                return (*this)(node.upgradeNode[0]);  // check upgraded node for improvement type
            }
            else
            {
                return std::make_pair(false, PlotYield());  // not found?
            }
        }

        std::pair<bool, PlotYield> operator() (const PlotInfo::FeatureRemovedNode& node) const
        {
            if (improvementType_ == NO_IMPROVEMENT)
            {
                return std::make_pair(true, YieldVisitor(playerType_, routeType_, isGoldenAge_)(node));
            }

            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                result_type result = (*this)(node.improvementNodes[i]);
                if (result.first)
                {
                    return result;
                }
            }
            return std::make_pair(false, PlotYield());  // not found?;
        }

    private:
        PlayerTypes playerType_;
        ImprovementTypes improvementType_;
        FeatureTypes featureType_;
        RouteTypes routeType_;
        bool isGoldenAge_;
    };

    PlotYield getYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, ImprovementTypes improvementType, FeatureTypes featureType, RouteTypes routeType, bool isGoldenAge)
    {
        return boost::apply_visitor(CalcYieldVisitor(playerType, improvementType, featureType, routeType, isGoldenAge), node).second;
    }

    
    std::vector<std::pair<RouteTypes, PlotYield> > getRouteYieldChanges(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, ImprovementTypes improvementType, FeatureTypes featureType)
    {
        std::vector<std::pair<RouteTypes, PlotYield> > routeYieldChanges;
        PlotYield baseYield = boost::apply_visitor(CalcYieldVisitor(playerType, improvementType, featureType, NO_ROUTE, false), node).second;

        for (int i = 0, count = gGlobals.getNumRouteInfos(); i < count; ++i)
        {
            routeYieldChanges.push_back(std::make_pair((RouteTypes)i, boost::apply_visitor(CalcYieldVisitor(playerType, improvementType, featureType, (RouteTypes)i, false), node).second - baseYield));
        }
        return routeYieldChanges;
    }

    class UpgradeYieldChangeFinder : public boost::static_visitor<std::pair<bool, PlotYield> >
    {
    public:
        UpgradeYieldChangeFinder(PlayerTypes playerType, ImprovementTypes improvementType) : playerType_(playerType), improvementType_(improvementType)
        {
        }

        std::pair<bool, PlotYield> operator() (const PlotInfo::NullNode& node) const
        {
            return std::make_pair(false, PlotYield());
        }

        std::pair<bool, PlotYield> operator() (const PlotInfo::BaseNode& node) const
        {
            std::pair<bool, PlotYield> result(boost::apply_visitor(*this, node.featureRemovedNode));

            if (!result.first)
            {
                for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
                {
                    result = (*this)(node.improvementNodes[i]);
                    if (result.first)
                    {
                        break;
                    }
                }
            }
            return result;
        }

        template <typename NodeType>
            std::pair<bool, PlotYield> operator() (const NodeType& node) const
        {
            if (!node.upgradeNode.empty())
            {
                if (node.improvementType == improvementType_)  // found improvement - compare with upgraded one
                {
                    return std::make_pair(true, YieldVisitor(playerType_)(node.upgradeNode[0]) - YieldVisitor(playerType_)(node));
                }
                else
                {
                    return (*this)(node.upgradeNode[0]);  // check upgraded node for improvement type
                }
            }
            else
            {
                return std::make_pair(false, PlotYield());  // no upgrade
            }
        }

        std::pair<bool, PlotYield> operator() (const PlotInfo::FeatureRemovedNode& node) const
        {
            std::pair<bool, PlotYield> result;

            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                result = (*this)(node.improvementNodes[i]);
                if (result.first)
                {
                    break;
                }
            }
            return result;
        }

    private:
        PlayerTypes playerType_;
        ImprovementTypes improvementType_;
    };

    AvailableImprovements::AvailableImprovements(FeatureTypes featureType_) : featureType(featureType_)
    {
    }

    std::vector<boost::tuple<FeatureTypes, ImprovementTypes, PlotYield> > AvailableImprovements::getAsList() const
    {
        std::vector<boost::tuple<FeatureTypes, ImprovementTypes, PlotYield> > list;
        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            list.push_back(boost::make_tuple(NO_FEATURE, improvements[i].improvementType, improvements[i].plotYield));
        }
        for (size_t i = 0, count = removeFeatureImprovements.size(); i < count; ++i)
        {
            list.push_back(boost::make_tuple(featureType, removeFeatureImprovements[i].improvementType, removeFeatureImprovements[i].plotYield));
        }
        return list;
    }

    std::vector<std::pair<FeatureTypes, ImprovementTypes> > AvailableImprovements::getAsListNoYields() const
    {
        std::vector<std::pair<FeatureTypes, ImprovementTypes> > list;
        for (size_t i = 0, count = improvements.size(); i < count; ++i)
        {
            list.push_back(std::make_pair(NO_FEATURE, improvements[i].improvementType));
        }
        for (size_t i = 0, count = removeFeatureImprovements.size(); i < count; ++i)
        {
            list.push_back(std::make_pair(featureType, removeFeatureImprovements[i].improvementType));
        }
        return list;
    }

    bool AvailableImprovements::empty() const
    {
        return improvements.empty() && removeFeatureImprovements.empty();
    }
    
    class AvailableImprovementsFinder : public boost::static_visitor<AvailableImprovements::ListT>
    {
    public:
        explicit AvailableImprovementsFinder(PlayerTypes playerType, FeatureTypes featureType) : playerType_(playerType), availableImprovements_(featureType)
        {
        }

        result_type operator() (const PlotInfo::NullNode&)
        {
            return result_type();
        }

        result_type operator() (const PlotInfo::UpgradeNode&)
        {
            return result_type();
        }

        result_type operator() (const PlotInfo::BaseNode& node)
        {
            result_type results;

            if (node.tech == NO_TECH || gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getCivHelper()->hasTech(node.tech))
            {
                if (node.featureRemoveTech == NO_TECH || gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getCivHelper()->hasTech(node.featureRemoveTech))
                {
                    // improvements which require the plot's feature to be removed
                    availableImprovements_.removeFeatureImprovements = boost::apply_visitor(*this, node.featureRemovedNode);
                    std::copy(availableImprovements_.removeFeatureImprovements.begin(), availableImprovements_.removeFeatureImprovements.end(), std::back_inserter(results));
                }

                for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
                {
                    result_type improvements = (*this)(node.improvementNodes[i]);
                    std::copy(improvements.begin(), improvements.end(), std::back_inserter(availableImprovements_.improvements));
                }
                std::copy(availableImprovements_.improvements.begin(), availableImprovements_.improvements.end(), std::back_inserter(results));
            }
            return results;
        }

        result_type operator() (const PlotInfo::ImprovementNode& node)
        {
            result_type results;

            if (meetsBuildConditions(node.buildConditions, playerType_, 0))
            {
                results.push_back(AvailableImprovements::ImprovementData(node.improvementType, YieldVisitor(playerType_)(node)));
            }
            return results;
        }

        result_type operator() (const PlotInfo::FeatureRemovedNode& node)
        {
            result_type results;

            for (size_t i = 0, count = node.improvementNodes.size(); i < count; ++i)
            {
                result_type improvementResults((*this)(node.improvementNodes[i]));
                std::copy(improvementResults.begin(), improvementResults.end(), std::back_inserter(results));
            }
            return results;
        }

        const AvailableImprovements& getAvailableImprovements() const { return availableImprovements_; }

    private:
        AvailableImprovements availableImprovements_;
        PlayerTypes playerType_;
    };

    struct ImprovementFinder
    {
        explicit ImprovementFinder(ImprovementTypes improvementType_) : improvementType(improvementType_)
        {
        }

        bool operator() (const PlotInfo::ImprovementNode& node) const
        {
            return node.improvementType == improvementType;
        }

        ImprovementTypes improvementType;
    };

    namespace 
    {
        class CityOutputUpdater : public boost::static_visitor<>
        {
        public:
            CityOutputUpdater(CityData& data, PlotData& plotData, FeatureTypes featureType, RouteTypes routeType, ImprovementTypes improvementType, const PlotInfo::PlotInfoNode& plotInfoNode)
                : data_(data), plotData_(plotData), featureType_(featureType), routeType_(routeType), improvementType_(improvementType), plotInfoNode_(plotInfoNode)
            {
                playerType_ = data_.getCity()->getOwner();
                isGoldenAge_ = data_.getGoldenAgeTurns() > 0;
            }

            void operator() (const PlotInfo::NullNode&) const
            {
            }

            void operator() (const PlotInfo::UpgradeNode& node) const
            {
                const int upgradeRate = CvPlayerAI::getPlayer(playerType_).getImprovementUpgradeRate();
                const int timeHorizon = gGlobals.getGame().getAltAI()->getPlayer(playerType_)->getAnalysis()->getTimeHorizon();

                plotData_.upgradeData = PlotData::UpgradeData(timeHorizon, getUpgradedImprovementsData(plotInfoNode_, playerType_, improvementType_, 
                    getActualUpgradeTurns(gGlobals.getGame().getImprovementUpgradeTime(improvementType_), upgradeRate), timeHorizon, upgradeRate));

                plotData_.output += plotData_.upgradeData.getExtraOutput(makeYield(100, 100, data_.getCommerceYieldModifier()), makeCommerce(100, 100, 100, 100), data_.getCommercePercent());
            }

            void operator() (const PlotInfo::BaseNode& node) const
            {
                if (featureType_ != NO_FEATURE)
                {
                    FAssertMsg(node.featureType == featureType_, "Inconsistent feature types in CityOutputUpdater?");
                    boost::apply_visitor(*this, node.featureRemovedNode);
                }
                else
                {
                    std::vector<PlotInfo::ImprovementNode>::const_iterator ci = std::find_if(node.improvementNodes.begin(), node.improvementNodes.end(), ImprovementFinder(improvementType_));
                    if (ci != node.improvementNodes.end())
                    {
                        (*this)(*ci);
                    }
                }
            }

            // todo - bonus access changes
            void operator() (const PlotInfo::ImprovementNode& node) const
            {
                plotData_.plotYield = YieldVisitor(playerType_, routeType_, isGoldenAge_)(node);
                plotData_.actualOutput = plotData_.output = makeOutput(plotData_.plotYield, makeYield(100, 100, data_.getCommerceYieldModifier()), makeCommerce(100, 100, 100, 100), data_.getCommercePercent());

                if (!node.upgradeNode.empty())
                {
                    (*this)(node.upgradeNode[0]);
                }

                int freeSpecCount = data_.getSpecialistHelper()->getFreeSpecialistCountPerImprovement(node.improvementType);
                if (freeSpecCount > 0)
                {
                    data_.changeImprovementFreeSpecialistSlotCount(freeSpecCount);
                }
            }

            void operator() (const PlotInfo::FeatureRemovedNode& node) const
            {
                std::vector<PlotInfo::ImprovementNode>::const_iterator ci = std::find_if(node.improvementNodes.begin(), node.improvementNodes.end(), ImprovementFinder(improvementType_));
                if (ci != node.improvementNodes.end())
                {
                    return (*this)(*ci);
                }
            }

        private:
            CityData& data_;
            PlotData& plotData_;
            PlayerTypes playerType_;
            FeatureTypes featureType_;
            ImprovementTypes improvementType_;
            RouteTypes routeType_;
            bool isGoldenAge_;
            const PlotInfo::PlotInfoNode& plotInfoNode_;
        };
    }

    void updateCityOutputData(CityData& data, PlotData& plotData, FeatureTypes featureType, RouteTypes routeType, ImprovementTypes improvementType)
    {
        //const boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(data.owner)->getAnalysis()->getMapAnalysis();

        //const PlotInfo::PlotInfoNode& node = pMapAnalysis->getPlotInfoNode(gGlobals.getMap().plot(plotData.coords.iX, plotData.coords.iY));
        PlotInfo plotInfo(gGlobals.getMap().plot(plotData.coords.iX, plotData.coords.iY), data.getCity()->getOwner());

#ifdef ALTAI_DEBUG
        // debug
        std::ostream& os = CityLog::getLog(data.getCity())->getStream();
        os << "\nPlot: " << plotData.coords << " applying improvement: " << gGlobals.getImprovementInfo(improvementType).getType();
        if (featureType != NO_FEATURE)
        {
            os << " feature = " << gGlobals.getFeatureInfo(featureType).getType();
        }
        os << " plot output before = " << plotData.actualOutput;
#endif

        boost::apply_visitor(CityOutputUpdater(data, plotData, featureType, routeType, improvementType, plotInfo.getInfo()), plotInfo.getInfo());

#ifdef ALTAI_DEBUG
        os << " after = " << plotData.actualOutput;
#endif
    }

    PlotsAndImprovements getPossibleImprovements(const CityData& data, bool ignoreExisting)
    {
        //const boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(data.owner)->getAnalysis()->getMapAnalysis();
        PlotsAndImprovements results;

        for (PlotDataListConstIter iter(data.getPlotOutputs().begin()), endIter(data.getPlotOutputs().end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot() && iter->controlled && (ignoreExisting || iter->improvementType == NO_IMPROVEMENT))
            {
                PlotInfo plotInfo(gGlobals.getMap().plot(iter->coords.iX, iter->coords.iY), data.getCity()->getOwner());
                //const PlotInfo::PlotInfoNode& node = pMapAnalysis->getPlotInfoNode(gGlobals.getMap().plot(iter->coords.iX, iter->coords.iY));

                AvailableImprovementsFinder improvementsFinder(data.getCity()->getOwner(), iter->featureType);
                boost::apply_visitor(improvementsFinder, plotInfo.getInfo());

                AvailableImprovements improvements(improvementsFinder.getAvailableImprovements());

                if (!improvements.empty())
                {
                    results.push_back(std::make_pair(iter->coords, improvements.getAsListNoYields()));
                }
            }
        }
        return results;
    }

    std::vector<std::pair<XYCoords, boost::tuple<FeatureTypes, ImprovementTypes, PlotYield> > > getBestImprovements(const CityData& data, YieldValueFunctor valueF, bool ignoreExisting)
    {
        const boost::shared_ptr<MapAnalysis> pMapAnalysis = gGlobals.getGame().getAltAI()->getPlayer(data.getOwner())->getAnalysis()->getMapAnalysis();
        std::vector<std::pair<XYCoords, boost::tuple<FeatureTypes, ImprovementTypes, PlotYield> > > results;

        for (PlotDataListConstIter iter(data.getPlotOutputs().begin()), endIter(data.getPlotOutputs().end()); iter != endIter; ++iter)
        {
            if (iter->isActualPlot() && iter->controlled && (ignoreExisting || iter->improvementType == NO_IMPROVEMENT))
            {
                PlotInfo plotInfo(gGlobals.getMap().plot(iter->coords.iX, iter->coords.iY), data.getCity()->getOwner());

                AvailableImprovementsFinder improvementsFinder(data.getCity()->getOwner(), iter->featureType);
                boost::apply_visitor(improvementsFinder, plotInfo.getInfo());

                AvailableImprovements improvements(improvementsFinder.getAvailableImprovements());

                if (!improvements.empty())
                {
                    boost::tuple<FeatureTypes, ImprovementTypes, PlotYield> bestImprovementAndYield;
                    for (size_t j = 0, count = improvements.improvements.size(); j < count; ++j)
                    {
                        if (valueF(improvements.improvements[j].plotYield) > valueF(boost::get<2>(bestImprovementAndYield)))
                        {
                            bestImprovementAndYield = boost::make_tuple(NO_FEATURE, improvements.improvements[j].improvementType, improvements.improvements[j].plotYield);
                        }
                    }
                    for (size_t j = 0, count = improvements.removeFeatureImprovements.size(); j < count; ++j)
                    {
                        if (valueF(improvements.removeFeatureImprovements[j].plotYield) > valueF(boost::get<2>(bestImprovementAndYield)))
                        {
                            bestImprovementAndYield = boost::make_tuple(improvements.featureType, improvements.removeFeatureImprovements[j].improvementType, improvements.removeFeatureImprovements[j].plotYield);
                        }
                    }

                    results.push_back(std::make_pair(iter->coords, bestImprovementAndYield));
                }
            }
        }
        return results;
    }

    PlotYield getUpgradedImprovementYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, ImprovementTypes improvementType)
    {
        return boost::apply_visitor(UpgradeYieldChangeFinder(playerType, improvementType), node).second;
    }

    std::list<PlotData::UpgradeData::Upgrade> 
        getUpgradedImprovementsData(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType, ImprovementTypes improvementType, int turnsUntilUpgrade, int timeHorizon, int upgradeRate)
    {
        std::list<PlotData::UpgradeData::Upgrade> upgrades;

        int remainingTurns = timeHorizon - turnsUntilUpgrade;
        ImprovementTypes upgradeType = NO_IMPROVEMENT;

        while ((upgradeType = (ImprovementTypes)gGlobals.getImprovementInfo(improvementType).getImprovementUpgrade()) != NO_IMPROVEMENT)
        {
            upgrades.push_back(PlotData::UpgradeData::Upgrade(upgradeType, remainingTurns, boost::apply_visitor(UpgradeYieldChangeFinder(playerType, improvementType), node).second));

            turnsUntilUpgrade = getActualUpgradeTurns(gGlobals.getGame().getImprovementUpgradeTime(improvementType), upgradeRate);
            remainingTurns -= turnsUntilUpgrade;
            improvementType = upgradeType;
        }
        return upgrades;
    }

    class CityPlotYieldVisitor : public boost::static_visitor<PlotYield>
    {
    public:
        CityPlotYieldVisitor(PlayerTypes playerType) : playerType_(playerType)
        {
        }

        template <typename NodeType>
            PlotYield operator() (const NodeType& node) const
        {
            return PlotYield();
        }

        PlotYield operator() (const PlotInfo::BaseNode& node) const
        {
            return node.featureType == NO_FEATURE ? getExtraYield(node.yield + node.bonusYield, playerType_) : boost::apply_visitor(*this, node.featureRemovedNode);
        }

        PlotYield operator() (const PlotInfo::FeatureRemovedNode& node) const
        {
            return getExtraYield(node.yield + node.bonusYield, playerType_);
        }

    private:
        PlayerTypes playerType_;
    };

    PlotYield getPlotCityYield(const PlotInfo::PlotInfoNode& node, PlayerTypes playerType)
    {
        PlotYield baseYield = boost::apply_visitor(CityPlotYieldVisitor(playerType), node);
        for (int i = 0; i < NUM_YIELD_TYPES; ++i)
        {
            // todo - does financial apply here? yes, I think?
            baseYield[i] = std::max<int>(baseYield[i], gGlobals.getYieldInfo((YieldTypes)i).getMinCity());
        }

#ifdef ALTAI_DEBUG
        //// debug
        //{
        //    if (isStrictlyGreater(baseYield, makeYield(2, 1, 1)))
        //    {
        //        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
        //        os << "\ncity plot yield = " << baseYield << "\n";
        //    }
        //}
#endif
        return baseYield;
    }
}