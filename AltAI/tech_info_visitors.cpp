#include "AltAI.h"

#include "./tech_info_visitors.h"
#include "./tech_info.h"
#include "./tech_info_streams.h"
#include "./city_data.h"
#include "./utils.h"
#include "./happy_helper.h"
#include "./health_helper.h"
#include "./visitor_utils.h"
#include "./iters.h"
#include "./game.h"
#include "./player.h"
#include "./helper_fns.h"
#include "./player_analysis.h"
#include "./civ_helper.h"
#include "./trade_route_helper.h"

namespace AltAI
{
    namespace
    {
        // update CityData with changes resulting from gaining give techs
        class CityTechsUpdater : public boost::static_visitor<>
        {
        public:
            explicit CityTechsUpdater(CityData& data) : data_(data)
            {
            }

            template <typename T>
                result_type operator() (const T&) const
            {
            }

            void operator() (const TechInfo::BaseNode& node) const
            {
                // add any base happy/health
                if (node.happy != 0)
                {
                    data_.getHappyHelper()->changePlayerHappiness(node.happy);
                    data_.changeWorkingPopulation();
                }

                if (node.health != 0)
                {
                    data_.getHealthHelper()->changePlayerHealthiness(node.health);
                }

                for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
                {
                    boost::apply_visitor(*this, node.nodes[i]);
                }
            }

            void operator() (const TechInfo::ImprovementNode& node) const
            {
                if (!isEmpty(node.modifier))
                {
                    for (PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        if (iter->isActualPlot() && iter->improvementType == node.improvementType)
                        {
                            iter->plotYield += node.modifier;
                        }
                    }
                }
            }

            void operator() (const TechInfo::TradeNode& node) const
            {
                if (node.extraTradeRoutes != 0)
                {
                    data_.getTradeRouteHelper()->changeNumRoutes(node.extraTradeRoutes);
                }
            }

            void operator() (const TechInfo::MiscEffectNode& node) const
            {
                if (node.enablesWaterWork)
                {
                    for (PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        if (iter->isActualPlot())
                        {
                            int xCoord = iter->coords.iX, yCoord = iter->coords.iY;
                            const CvPlot* pPlot = gGlobals.getMap().plot(xCoord, yCoord);
                            if (pPlot->isWater())
                            {
                                iter->ableToWork = true;
                            }
                        }
                    }
                }
            }

        private:
            CityData& data_;
        };
    }

    // does tech give access to new buildings or obsolete old ones?
    // todo - expand this to answer other questions about tech (e.g. does it give access to other build processes/new units, etc...)
    // idea to indicate what simulations need to be recalculated when a tech is obtained
    class TechBuildingVisitor : public boost::static_visitor<bool>
    {
    public:
        TechBuildingVisitor()
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        bool operator() (const TechInfo::BaseNode& node) const
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                if (boost::apply_visitor(*this, node.nodes[i]))
                {
                    return true;
                }
            }
            return false;
        }

        bool operator() (const TechInfo::BuildingNode&) const
        {
            return true;
        }

    private:
    };

    // does tech give access to new improvements or obsolete old ones or does it change improvement yields?
    class TechImprovementVisitor : public boost::static_visitor<void>
    {
    public:
        TechImprovementVisitor() : affectsImprovements_(false)
        {
        }

        template <typename T>
            result_type operator() (const T&)
        {
        }

        void operator() (const TechInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        void operator() (const TechInfo::ImprovementNode& node)
        {
            if (node.improvementType != NO_IMPROVEMENT)
            {
                improvementTypes_.push_back(node.improvementType);
                affectsImprovements_ =  true;
            }
            if (node.removeFeatureType != NO_FEATURE)
            {
                removeFeatureTypes_.push_back(node.removeFeatureType);
                affectsImprovements_ =  true;
            }
        }

        void operator() (const TechInfo::BonusNode&)
        {
            affectsImprovements_ =  true;
        }

        void operator() (const TechInfo::RouteNode&)
        {
            affectsImprovements_ = true;
        }

        void operator() (const TechInfo::MiscEffectNode& node)
        {
            if (node.ignoreIrrigation || node.carriesIrrigation)
            {
                affectsImprovements_ = true;
            }
        }

        bool affectsImprovements() const
        {
            return affectsImprovements_;
        }

        const std::vector<ImprovementTypes>& getAffectedImprovementTypes() const
        {
            return improvementTypes_;
        }

        const std::vector<FeatureTypes>& getAffectedFeatureTypes() const
        {
            return removeFeatureTypes_;
        }

    private:
        bool affectsImprovements_;
        std::vector<ImprovementTypes> improvementTypes_;
        std::vector<FeatureTypes> removeFeatureTypes_;
    };

    // does tech give access to new improvements or route types or bonus types? (i.e. new stuff for workers to do)
    // not quite the same as TechImprovementVisitor as this also identifies techs which passively affect existing improvements
    class TechWorkerVisitor : public boost::static_visitor<bool>
    {
    public:
        TechWorkerVisitor()
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        bool operator() (const TechInfo::BaseNode& node) const
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                if (boost::apply_visitor(*this, node.nodes[i]))
                {
                    return true;
                }
            }
            return false;
        }

        bool operator() (const TechInfo::ImprovementNode& node) const
        {
            return node.improvementType != NO_IMPROVEMENT && node.allowsImprovement || node.removeFeatureType != NO_FEATURE;
        }

        bool operator() (const TechInfo::BonusNode& node) const
        {
            return node.revealBonus != NO_BONUS;
        }

        bool operator() (const TechInfo::RouteNode& node) const
        {
            return node.routeType != NO_ROUTE;
        }

        bool operator() (const TechInfo::MiscEffectNode& node) const
        {
            return node.ignoreIrrigation || node.carriesIrrigation;
        }

    private:
    };

    class TechRevealBonusVisitor : public boost::static_visitor<std::vector<BonusTypes> >
    {
    public:
        TechRevealBonusVisitor()
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return result_type();
        }

        result_type operator() (const TechInfo::BaseNode& node) const
        {
            result_type result;
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                result_type nodeResult(boost::apply_visitor(*this, node.nodes[i]));
                if (!nodeResult.empty())
                {
                    std::copy(nodeResult.begin(), nodeResult.end(), std::back_inserter(result));;
                }
            }
            return result;
        }

        result_type operator() (const TechInfo::BonusNode& node) const
        {
            return node.revealBonus != NO_BONUS ? result_type(1, node.revealBonus) : result_type();
        }

    private:
    };

    class TechTradeBonusVisitor : public boost::static_visitor<std::vector<BonusTypes> >
    {
    public:
        TechTradeBonusVisitor()
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return result_type();
        }

        result_type operator() (const TechInfo::BaseNode& node) const
        {
            result_type result;
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                result_type nodeResult(boost::apply_visitor(*this, node.nodes[i]));
                if (!nodeResult.empty())
                {
                    std::copy(nodeResult.begin(), nodeResult.end(), std::back_inserter(result));;
                }
            }
            return result;
        }

        result_type operator() (const TechInfo::BonusNode& node) const
        {
            return node.tradeBonus != NO_BONUS ? result_type(1, node.tradeBonus) : result_type();
        }

    private:
    };

    // todo - check bonus access?
    class TechRouteVisitor : public boost::static_visitor<std::vector<RouteTypes> >
    {
    public:
        TechRouteVisitor()
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return result_type();
        }

        result_type operator() (const TechInfo::BaseNode& node) const
        {
            result_type result;
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                result_type nodeResult(boost::apply_visitor(*this, node.nodes[i]));
                if (!nodeResult.empty())
                {
                    std::copy(nodeResult.begin(), nodeResult.end(), std::back_inserter(result));
                }
            }
            return result;
        }

        result_type operator() (const TechInfo::RouteNode& node) const
        {
            return node.routeType != NO_ROUTE ? result_type(1, node.routeType) : result_type();
        }

    private:
    };

    class TechBuildingsVisitor : public boost::static_visitor<std::vector<BuildingTypes> >
    {
    public:
        explicit TechBuildingsVisitor(bool obsoletes) : obsoletes_(obsoletes)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return result_type();
        }

        result_type operator() (const TechInfo::BaseNode& node) const
        {
            result_type result;
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                result_type nodeResult(boost::apply_visitor(*this, node.nodes[i]));
                if (!nodeResult.empty())
                {
                    std::copy(nodeResult.begin(), nodeResult.end(), std::back_inserter(result));
                }
            }
            return result;
        }

        result_type operator() (const TechInfo::BuildingNode& node) const
        {
            return node.buildingType != NO_BUILDING && node.obsoletes == obsoletes_ ? result_type(1, node.buildingType) : result_type();
        }

    private:
        bool obsoletes_;
    };

    
    class TechUnitsVisitor : public boost::static_visitor<std::vector<UnitTypes> >
    {
    public:
        explicit TechUnitsVisitor(bool obsoletes) : obsoletes_(obsoletes)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return result_type();
        }

        result_type operator() (const TechInfo::BaseNode& node) const
        {
            result_type result;
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                result_type nodeResult(boost::apply_visitor(*this, node.nodes[i]));
                if (!nodeResult.empty())
                {
                    std::copy(nodeResult.begin(), nodeResult.end(), std::back_inserter(result));
                }
            }
            return result;
        }

        result_type operator() (const TechInfo::UnitNode& node) const
        {
            return node.unitType != NO_UNIT ? result_type(1, node.unitType) : result_type();
        }

    private:
        bool obsoletes_;
    };

    class TechProcessesVisitor : public boost::static_visitor<std::vector<ProcessTypes> >
    {
    public:
        template <typename T>
            result_type operator() (const T&) const
        {
            return result_type();
        }

        result_type operator() (const TechInfo::BaseNode& node) const
        {
            result_type result;
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                result_type nodeResult(boost::apply_visitor(*this, node.nodes[i]));
                if (!nodeResult.empty())
                {
                    std::copy(nodeResult.begin(), nodeResult.end(), std::back_inserter(result));
                }
            }
            return result;
        }

        result_type operator() (const TechInfo::ProcessNode& node) const
        {
            return result_type(1, node.processType);
        }
    };

    // This and its companion PushTechResearchVisitor are based on pushResearch and findPathLength in CvPlayer
    // they allow simulation of research paths without actually pushing the research for real
    // we use CivHelper to track researched techs and pushed techs
    // kudos to the original author - this is definitely non-trivial!
    class TechResearchDepthVisitor : public boost::static_visitor<int>
    {
    public:
        explicit TechResearchDepthVisitor(PlayerTypes playerType)
        {
            pPlayer_ = gGlobals.getGame().getAltAI()->getPlayer(playerType);
            pCivHelper_ = pPlayer_->getCivHelper();
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return result_type();
        }

        result_type operator() (const TechInfo::BaseNode& node) const
        {
            int depth = 0;

            if (pCivHelper_->hasTech(node.tech) || pCivHelper_->isResearchingTech(node.tech))
            {
                return depth;
            }

            for (size_t i = 0, count = node.andTechs.size(); i < count; ++i)
            {
                if (!pCivHelper_->hasTech(node.andTechs[i]) && pCivHelper_->isResearchingTech(node.tech))
                {
                    depth += boost::apply_visitor(*this, pPlayer_->getAnalysis()->getTechInfo(node.andTechs[i])->getInfo());
                }
            }

            TechTypes orTech = NO_TECH;
            int minOrDepth = MAX_INT;

            for (size_t i = 0, count = node.orTechs.size(); i < count; ++i)
            {
                int orDepth = boost::apply_visitor(*this, pPlayer_->getAnalysis()->getTechInfo(node.orTechs[i])->getInfo());

                if (orDepth < minOrDepth)
                {
                    minOrDepth = orDepth;
                    orTech = node.orTechs[i];
                }
            }

            if (orTech != NO_TECH)
            {
                depth += minOrDepth;
            }

            return 1 + depth;
        }

    private:
        PlayerPtr pPlayer_;
        boost::shared_ptr<CivHelper> pCivHelper_;
    };

    // TODO: Doesn't handle case of unresearchable techs
    class PushTechResearchVisitor : public boost::static_visitor<bool>
    {
    public:
        explicit PushTechResearchVisitor(PlayerTypes playerType)
        {
            pPlayer_ = gGlobals.getGame().getAltAI()->getPlayer(playerType);
            pCivHelper_ = pPlayer_->getCivHelper();
        }

        template <typename T>
            result_type operator() (const T&)
        {
            return false;
        }

        result_type operator() (const TechInfo::BaseNode& node)
        {
            if (pCivHelper_->hasTech(node.tech) || pCivHelper_->isResearchingTech(node.tech))
            {
                return true;
            }

            for (size_t i = 0, count = node.andTechs.size(); i < count; ++i)
            {
                if (!boost::apply_visitor(*this, pPlayer_->getAnalysis()->getTechInfo(node.andTechs[i])->getInfo()))
                {
                    return false;  // will only be triggered if add unresearchable check
                }
            }

            TechTypes orTech = NO_TECH;
            int minOrDepth = MAX_INT;
            bool haveOrTech = false;

            for (size_t i = 0, count = node.orTechs.size(); i < count; ++i)
            {
                if (pCivHelper_->hasTech(node.orTechs[i]))
                {
                    haveOrTech = true;
                    minOrDepth = 0;
                    orTech = node.orTechs[i];
                    break;
                }
                else
                {
                    int orDepth = boost::apply_visitor(TechResearchDepthVisitor(pPlayer_->getPlayerID()), pPlayer_->getAnalysis()->getTechInfo(node.orTechs[i])->getInfo());

                    if (orDepth < minOrDepth)
                    {
                        minOrDepth = orDepth;
                        orTech = node.orTechs[i];
                    }
                }
            }

            if (orTech != NO_TECH)
            {
                if (!boost::apply_visitor(*this, pPlayer_->getAnalysis()->getTechInfo(orTech)->getInfo()))
                {
                    return false;  // will only be triggered if add unresearchable check
                }
            }

            pCivHelper_->pushResearchTech(node.tech);
            return true;
        }

    private:
        PlayerPtr pPlayer_;
        boost::shared_ptr<CivHelper> pCivHelper_;
    };

    boost::shared_ptr<TechInfo> makeTechInfo(TechTypes techType, PlayerTypes playerType)
    {
         return boost::shared_ptr<TechInfo>(new TechInfo(techType, playerType));
    }

    void streamTechInfo(std::ostream& os, const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        os << pTechInfo->getInfo();
    }

    void updateRequestData(const CvCity* pCity, CityData& data, const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        data.getCivHelper()->addTech(pTechInfo->getTechType());
        boost::apply_visitor(CityTechsUpdater(data), pTechInfo->getInfo());
        data.recalcOutputs();
    }

    bool techAffectsBuildings(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechBuildingVisitor(), pTechInfo->getInfo());
    }

    bool techAffectsImprovements(const boost::shared_ptr<TechInfo>& pTechInfo, 
        std::vector<ImprovementTypes>& affectedImprovements, std::vector<FeatureTypes>& removeFeatureTypes)
    {
        TechImprovementVisitor visitor;
        boost::apply_visitor(visitor, pTechInfo->getInfo());
        affectedImprovements = visitor.getAffectedImprovementTypes();
        removeFeatureTypes = visitor.getAffectedFeatureTypes();
        return visitor.affectsImprovements();
    }

    bool techIsWorkerTech(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechWorkerVisitor(), pTechInfo->getInfo());
    }

    std::vector<BonusTypes> getRevealedBonuses(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechRevealBonusVisitor(), pTechInfo->getInfo());
    }

    namespace
    {
        std::list<TechTypes> calculateResearchTechs(TechTypes techType, PlayerTypes playerType)
        {
            std::list<TechTypes> techs;

            if (techType != NO_TECH)
            {
                PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType);
                boost::shared_ptr<CivHelper> pCivHelper = pPlayer->getCivHelper();

                PushTechResearchVisitor visitor(playerType);
                bool canResearch = boost::apply_visitor(visitor, pPlayer->getAnalysis()->getTechInfo(techType)->getInfo());

                if (canResearch)
                {
                    techs = pCivHelper->getResearchTechs();
                    pCivHelper->clearResearchTechs();
                }
            }

            return techs;
        }
    }

    int calculateTechResearchDepth(TechTypes techType, PlayerTypes playerType)
    {
        return calculateResearchTechs(techType, playerType).size();
    }

    int calculateTechResearchCost(TechTypes techType, PlayerTypes playerType)
    {
        std::list<TechTypes> techs = calculateResearchTechs(techType, playerType);

        int cost = 0;
            
        for (std::list<TechTypes>::const_iterator ci(techs.begin()), ciEnd(techs.end()); ci != ciEnd; ++ci)
        {
            cost += CvTeamAI::getTeam(CvPlayerAI::getPlayer(playerType).getTeam()).getResearchLeft(*ci);
        }

        return cost;
    }

    std::list<TechTypes> pushTechAndPrereqs(TechTypes techType, const Player& player)
    {
        std::list<TechTypes> pushedTechs;

        if (techType != NO_TECH)
        {
            boost::shared_ptr<CivHelper> pCivHelper = player.getCivHelper();

            PushTechResearchVisitor visitor(player.getPlayerID());
            bool canResearch = boost::apply_visitor(visitor, player.getAnalysis()->getTechInfo(techType)->getInfo());

            if (canResearch)
            {
                pushedTechs = pCivHelper->getResearchTechs();
                pCivHelper->clearResearchTechs();
            }
        }

        for (std::list<TechTypes>::const_iterator ci(pushedTechs.begin()), ciEnd(pushedTechs.end()); ci != ciEnd; ++ci)
        {
            player.getCivHelper()->addTech(*ci);
        }

        return pushedTechs;
    }

    std::vector<BonusTypes> getWorkableBonuses(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechTradeBonusVisitor(), pTechInfo->getInfo());
    }

    std::vector<RouteTypes> availableRoutes(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechRouteVisitor(), pTechInfo->getInfo());
    }

    std::vector<TechTypes> getOrTechs(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::get<TechInfo::BaseNode>(pTechInfo->getInfo()).orTechs;
    }

    std::vector<BuildingTypes> getPossibleBuildings(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechBuildingsVisitor(false), pTechInfo->getInfo());
    }

    std::vector<BuildingTypes> getObsoletedBuildings(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechBuildingsVisitor(true), pTechInfo->getInfo());
    }

    std::vector<ProcessTypes> getPossibleProcesses(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechProcessesVisitor(), pTechInfo->getInfo());
    }

    std::vector<UnitTypes> getPossibleUnits(const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        return boost::apply_visitor(TechUnitsVisitor(false), pTechInfo->getInfo());
    }
}