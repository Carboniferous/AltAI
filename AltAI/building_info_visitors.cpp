#include "./utils.h"
#include "./building_info_visitors.h"
#include "./buildings_info_streams.h"
#include "./resource_info_visitors.h"
#include "./visitor_utils.h"
#include "./hurry_helper.h"
#include "./happy_helper.h"
#include "./health_helper.h"
#include "./trade_route_helper.h"
#include "./religion_helper.h"
#include "./bonus_helper.h"
#include "./specialist_helper.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./player_analysis.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./civ_helper.h"
#include "./civ_log.h" 

namespace AltAI
{
    namespace
    {
        // update CityData with changes resulting from construction of supplied BuildingInfo nodes
        class CityOutputUpdater : public boost::static_visitor<>
        {
        public:
            CityOutputUpdater(const CvCity* pCity, CityData& data) : pCity_(pCity), data_(data)
            {
                pPlayer_ = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner()); 
            }

            template <typename T>
                result_type operator() (const T&) const
            {
            }

            void operator() (const BuildingInfo::BaseNode& node) const
            {
                // add any base happy/health
                if (node.happy > 0)
                {
                    data_.happyHelper->changeBuildingGoodHappiness(node.happy);
                }
                else if (node.happy < 0)
                {
                    data_.happyHelper->changeBuildingBadHappiness(node.happy);
                }
                data_.changeWorkingPopulation();

                if (node.health > 0)
                {
                    data_.healthHelper->changeBuildingGoodHealthiness(node.health);
                }
                else if (node.health < 0)
                {
                    data_.healthHelper->changeBuildingBadHealthiness(node.health);
                }

                for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
                {
                    boost::apply_visitor(*this, node.nodes[i]);
                }
            }

            void operator() (const BuildingInfo::SpecialistNode& node) const
            {
                // find 'plots' which represent specialists and update their outputs
                for (size_t i = 0, count = node.specialistTypesAndYields.size(); i < count; ++i)
                {
                    for (PlotDataListIter iter(data_.plotOutputs.begin()), endIter(data_.plotOutputs.end()); iter != endIter; ++iter)
                    {
                        // TODO - double check everything is in correct units here
                        if (!iter->isActualPlot() && (SpecialistTypes)iter->coords.iY == node.specialistTypesAndYields[i].first)
                        {
                            iter->plotYield += node.specialistTypesAndYields[i].second;
                            iter->commerce += node.extraCommerce;

                            TotalOutput specialistOutput(makeOutput(iter->plotYield, iter->commerce, data_.getYieldModifier(), data_.getCommerceModifier(), data_.getCommercePercent()));
                            iter->output = specialistOutput;
                            iter->actualOutput = specialistOutput;
                        }
                    }
                }
            }

            void operator() (const BuildingInfo::SpecialistSlotNode& node) const
            {
                const CvPlayer& player = CvPlayerAI::getPlayer(pCity_->getOwner());

                // add any 'plots' for new specialist slots (if we aren't maxed out already for that type)
                for (size_t i = 0, count = node.specialistTypes.size(); i < count; ++i)
                {
                    int specialistCount = data_.getNumPossibleSpecialists(node.specialistTypes[i].first);

                    // todo - should this total population?
                    if (specialistCount < data_.workingPopulation)  // if this is also changed by this building (e.g. HG)? - OK as long as add pop code updates specs
                    {
                        data_.addSpecialistSlots(node.specialistTypes[i].first, std::min<int>(node.specialistTypes[i].second, data_.workingPopulation - specialistCount));
                    }
                }

                // add free specialists
                for (size_t i = 0, count = node.freeSpecialistTypes.size(); i < count; ++i)
                {
                    data_.specialistHelper->changeFreeSpecialistCount(node.freeSpecialistTypes[i].first, node.freeSpecialistTypes[i].second);
                }

                // update city data to include any free specialists added for given improvement types (National Park + Forest Preserves)
                for (size_t i = 0, count = node.improvementFreeSpecialists.size(); i < count; ++i)
                {
                    data_.changeFreeSpecialistCountPerImprovement(node.improvementFreeSpecialists[i].first, node.improvementFreeSpecialists[i].second);
                }
            }

            void operator() (const BuildingInfo::YieldNode& node) const
            {
                // TODO - check whether yield modifiers can be conditional on plots too (only store one in CityData)
                data_.changeYieldModifier(node.modifier);

                if (!node.plotCond)  // applies to building itself
                {
                    data_.cityPlotOutput.plotYield += node.yield;
                }
                else
                {
                    int xCoord = data_.cityPlotOutput.coords.iX, yCoord = data_.cityPlotOutput.coords.iY;
                    CvPlot* pPlot = gGlobals.getMap().plot(xCoord, yCoord);

                    if ((pPlot->*(node.plotCond))())
                    {
                        data_.cityPlotOutput.plotYield += node.yield;
                    }

                    // update plot yields
                    for (PlotDataListIter iter(data_.plotOutputs.begin()), endIter(data_.plotOutputs.end()); iter != endIter; ++iter)
                    {
                        if (iter->isActualPlot())
                        {
                            xCoord = iter->coords.iX, yCoord = iter->coords.iY;
                            pPlot = gGlobals.getMap().plot(xCoord, yCoord);
                            if ((pPlot->*(node.plotCond))())
                            {
                                iter->plotYield += node.yield;
                            }
                        }
                    }
                }
            }

            void operator() (const BuildingInfo::CommerceNode& node) const
            {
                // update city commerce and/or commerce modifier
                data_.cityPlotOutput.commerce += 100 * node.commerce;
                data_.cityPlotOutput.commerce += 100 * node.obsoleteSafeCommerce;
                data_.changeCommerceModifier(node.modifier);
            }

            void operator() (const BuildingInfo::TradeNode& node) const
            {
                if (node.extraTradeRoutes != 0)
                {
                    data_.tradeRouteHelper->changeNumRoutes(node.extraTradeRoutes);
                }

                if (node.extraCoastalTradeRoutes != 0 && data_.pCity->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
                {
                    data_.tradeRouteHelper->changeNumRoutes(node.extraCoastalTradeRoutes);
                }

                if (node.extraGlobalTradeRoutes != 0)
                {
                    data_.tradeRouteHelper->changeNumRoutes(node.extraGlobalTradeRoutes);
                }

                if (node.tradeRouteModifier != 0)
                {
                    data_.tradeRouteHelper->changeTradeRouteModifier(node.tradeRouteModifier);
                }

                if (node.foreignTradeRouteModifier != 0)
                {
                    data_.tradeRouteHelper->changeForeignTradeRouteModifier(node.foreignTradeRouteModifier);
                }
            }

            void operator() (const BuildingInfo::BonusNode& node) const
            {
                // check for increased health/happy from bonus
                if (data_.bonusHelper->getNumBonuses(node.bonusType) > 0)
                {
                    if (node.happy > 0)
                    {
                        data_.happyHelper->changeBonusGoodHappiness(node.happy);
                    }
                    else if (node.happy < 0)
                    {
                        data_.happyHelper->changeBonusBadHappiness(node.happy);
                    }
                    data_.changeWorkingPopulation();

                    if (node.health > 0)
                    {
                        data_.healthHelper->changeBonusGoodHealthiness(node.health);
                    }
                    else if (node.health < 0)
                    {
                        data_.healthHelper->changeBonusBadHealthiness(node.health);
                    }

                    data_.changeYieldModifier(node.yieldModifier);
                }
            }

            void operator() (const BuildingInfo::FreeBonusNode& node) const
            {
                data_.bonusHelper->changeNumBonuses(node.freeBonuses.first, node.freeBonuses.second);
                updateCityData(data_, pPlayer_->getAnalysis()->getResourceInfo(node.freeBonuses.first), true); // process new bonus
            }

            void operator() (const BuildingInfo::RemoveBonusNode& node) const
            {
                data_.bonusHelper->allowOrDenyBonus(node.bonusType, false);
                
                updateCityData(data_, pPlayer_->getAnalysis()->getResourceInfo(node.bonusType), false);
            }

            void operator() (const BuildingInfo::PowerNode& node) const
            {
                // todo - get health change - use healthhelper
            }

            void operator() (const BuildingInfo::MiscEffectNode& node) const
            {
                if (node.cityMaintenanceModifierChange != 0)
                {
                    data_.maintenanceHelper->changeModifier(node.cityMaintenanceModifierChange);
                }

                if (node.foodKeptPercent != 0)
                {
                    data_.foodKeptPercent += node.foodKeptPercent;
                }

                if (node.hurryAngerModifier != 0)
                {
                    data_.happyHelper->getHurryHelper().changeModifier(node.hurryAngerModifier);
                }

                if (node.noUnhealthinessFromBuildings)
                {
                    data_.healthHelper->setNoUnhealthinessFromBuildings();
                }

                if (node.noUnhealthinessFromPopulation)
                {
                    data_.healthHelper->setNoUnhealthinessFromPopulation();
                }
            }

        private:
            const CvCity* pCity_;
            boost::shared_ptr<Player> pPlayer_;
            CityData& data_;
        };
    }


    boost::shared_ptr<BuildingInfo> makeBuildingInfo(BuildingTypes buildingType, PlayerTypes playerType)
    {
        return boost::shared_ptr<BuildingInfo>(new BuildingInfo(buildingType, playerType));
    }

    void streamBuildingInfo(std::ostream& os, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        os << pBuildingInfo->getInfo();
    }

    std::vector<TechTypes> getRequiredTechs(const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        const BuildingInfo::BaseNode& node = boost::get<BuildingInfo::BaseNode>(pBuildingInfo->getInfo());
        return node.techs;
    }

    // todo - make this just return the 'bad' bits in a complete BuildingInfo::BuildingInfoNode structure
    // currently returns all nodes which have any bad bit, plus the top level BaseNode
    class BadBuildingNodeFinder : public boost::static_visitor<BuildingInfo::BuildingInfoNode>
    {
    public:
        template <typename T>
            result_type operator() (const T& node) const
        {
            return BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::YieldNode& node) const
        {
            return YieldAndCommerceFunctor<LessThanZero>()(node.modifier) || 
                YieldAndCommerceFunctor<LessThanZero>()(node.yield) ? node : BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::CommerceNode& node) const
        {
            return YieldAndCommerceFunctor<LessThanZero>()(node.commerce) || 
                YieldAndCommerceFunctor<LessThanZero>()(node.modifier) ||
                YieldAndCommerceFunctor<LessThanZero>()(node.obsoleteSafeCommerce) ||
                YieldAndCommerceFunctor<LessThanZero>()(node.stateReligionCommerce)
                ? node : BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::BonusNode& node) const
        {
            return node.happy < 0 || node.health < 0 || node.prodModifier  < 0 ||
                YieldAndCommerceFunctor<LessThanZero>()(node.yieldModifier)
                ? node : BuildingInfo::BuildingInfoNode(BuildingInfo::NullNode());
        }

        result_type operator() (const BuildingInfo::BaseNode& node) const
        {
            BuildingInfo::BaseNode badBaseNode;
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                BuildingInfo::BuildingInfoNode potentialBadNode(boost::apply_visitor(*this, node.nodes[i]));
                if (!boost::apply_visitor(IsVariantOfType<BuildingInfo::NullNode>(), potentialBadNode))
                {
                    badBaseNode.nodes.push_back(potentialBadNode);
                }
            }

            badBaseNode.happy = node.happy < 0 ? node.happy : 0;
            badBaseNode.happy = node.health < 0 ? node.health : 0;
            
            return badBaseNode;
        }
    };

    BuildingInfo::BaseNode getBadNodes(const BuildingInfo::BuildingInfoNode& node)
    {
        return boost::get<BuildingInfo::BaseNode>(boost::apply_visitor(BadBuildingNodeFinder(), node));
    }

    class CommerceFinder : public boost::static_visitor<Commerce>
    {
    public:
        explicit CommerceFinder(const CvCity* pCity) : pCity_(pCity)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return Commerce();
        }

        result_type operator() (const BuildingInfo::CommerceNode& node)
        {
            PlotYield totalYield;
            CityPlotIter iter(pCity_);

            while (IterPlot pPlot = iter())
            {
                if (pPlot.valid() && pCity_->isWorkingPlot(pPlot))
                {
                    totalYield += pPlot->getYield();
                }
            }
            return node.commerce + node.obsoleteSafeCommerce;
        }

    private:
        const CvCity* pCity_;
    };

    class BuildingHasEconomicImpactVisitor : public boost::static_visitor<bool>
    {
    public:
        explicit BuildingHasEconomicImpactVisitor(const CityData& data) : data_(data)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        result_type operator() (const BuildingInfo::BaseNode& node) const
        {
            if (node.happy != 0 || node.health != 0)
            {
                return true;
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                if (boost::apply_visitor(*this, node.nodes[i]))
                {
                    return true;
                }
            }
            return false;
        }

        result_type operator() (const BuildingInfo::YieldNode& node) const
        {
            if (!isEmpty(node.modifier))
            {
                return true;
            }

            if (!isEmpty(node.yield))
            {
                if (!node.plotCond)  // applies to building itself
                {
                    return true;
                }
                else
                {
                    int xCoord = data_.cityPlotOutput.coords.iX, yCoord = data_.cityPlotOutput.coords.iY;
                    CvPlot* pPlot = gGlobals.getMap().plot(xCoord, yCoord);

                    if ((pPlot->*(node.plotCond))())
                    {
                        return true;
                    }

                    for (PlotDataListConstIter iter(data_.plotOutputs.begin()), endIter(data_.plotOutputs.end()); iter != endIter; ++iter)
                    {
                        if (iter->isActualPlot())
                        {
                            xCoord = iter->coords.iX, yCoord = iter->coords.iY;
                            pPlot = gGlobals.getMap().plot(xCoord, yCoord);
                            if ((pPlot->*(node.plotCond))())
                            {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }

        result_type operator() (const BuildingInfo::SpecialistNode& node) const
        {
            for (size_t i = 0, count = node.specialistTypesAndYields.size(); i < count; ++i)
            {
                for (PlotDataListConstIter iter(data_.plotOutputs.begin()), endIter(data_.plotOutputs.end()); iter != endIter; ++iter)
                {
                    if (!iter->isActualPlot() && (SpecialistTypes)iter->coords.iY == node.specialistTypesAndYields[i].first)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        result_type operator() (const BuildingInfo::CommerceNode& node) const
        {
            // update city commerce and/or commerce modifier

            if (!isEmpty(node.commerce) || !isEmpty(node.obsoleteSafeCommerce) || !isEmpty(node.modifier))
            {
                return true;
            }

            return false;
        }

        result_type operator() (const BuildingInfo::TradeNode& node) const
        {
            if (node.extraTradeRoutes != 0)
            {
                return true;
            }

            if (node.extraCoastalTradeRoutes != 0 && data_.pCity->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
            {
                return true;
            }

            if (node.extraGlobalTradeRoutes != 0)
            {
                return true;
            }

            if (node.tradeRouteModifier != 0)
            {
                return true;
            }

            if (node.foreignTradeRouteModifier != 0 && data_.tradeRouteHelper->couldHaveForeignRoutes())
            {
                return true;
            }

            return false;
        }

        result_type operator() (const BuildingInfo::PowerNode& node) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::SpecialistSlotNode& node) const
        {
            const CvPlayer& player = CvPlayerAI::getPlayer(data_.owner);

            // add any 'plots' for new specialist slots (if we aren't maxed out already for that type)
            for (size_t i = 0, count = node.specialistTypes.size(); i < count; ++i)
            {
                int specialistCount = data_.getNumPossibleSpecialists(node.specialistTypes[i].first);

                // todo - should this total population?
                if (specialistCount < data_.workingPopulation)  // if this is also changed by this building (e.g. HG)? - OK as long as add pop code updates specs
                {
                    return true;
                }
            }

            if (!node.freeSpecialistTypes.empty())
            {
                return true;
            }

            return false;
        }
    
        result_type operator() (const BuildingInfo::BonusNode& node) const
        {
            // check for increased health/happy from bonus
            if (data_.bonusHelper->getNumBonuses(node.bonusType) > 0)
            {
                if (node.happy != 0 || node.health != 0)
                {
                    return true;
                }

                if (!isEmpty(node.yieldModifier))
                {
                    return true;
                }
            }
            return false;
        }

        result_type operator() (const BuildingInfo::FreeBonusNode& node) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::RemoveBonusNode& node) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::MiscEffectNode& node) const
        {
            if (node.cityMaintenanceModifierChange != 0 || node.foodKeptPercent != 0 || node.hurryAngerModifier != 0 || 
                node.noUnhealthinessFromBuildings || node.noUnhealthinessFromPopulation)
            {
                return true;
            }

            return false;
        }

    private:
        const CityData& data_;
    };

    // very basic check - just for existance of 'economic' nodes (like extra spec slots)
    class BuildingHasPotentialEconomicImpactVisitor : public boost::static_visitor<bool>
    {
    public:
        BuildingHasPotentialEconomicImpactVisitor()
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        result_type operator() (const BuildingInfo::BaseNode& node) const
        {
            if (node.happy != 0 || node.health != 0)
            {
                return true;
            }

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                if (boost::apply_visitor(*this, node.nodes[i]))
                {
                    return true;
                }
            }
            return false;
        }

        result_type operator() (const BuildingInfo::YieldNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::SpecialistNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::CommerceNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::TradeNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::SpecialistSlotNode&) const
        {
            return true;
        }
    
        result_type operator() (const BuildingInfo::BonusNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::FreeBonusNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::RemoveBonusNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::PowerNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::MiscEffectNode& node) const
        {
            if (node.cityMaintenanceModifierChange != 0 || node.foodKeptPercent != 0 || node.hurryAngerModifier != 0 || 
                node.noUnhealthinessFromBuildings || node.noUnhealthinessFromPopulation)
            {
                return true;
            }

            return false;
        }
    };

    class BuildingHasPotentialMilitaryImpactVisitor : public boost::static_visitor<bool>
    {
    public:
        explicit BuildingHasPotentialMilitaryImpactVisitor(PlayerTypes playerType) : playerType_(playerType)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        result_type operator() (const BuildingInfo::BaseNode& node) const
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

        result_type operator() (const BuildingInfo::CityDefenceNode&) const
        {
            return true;
        }

        result_type operator() (const BuildingInfo::UnitExpNode&) const
        {
            return true;
        }
    
        result_type operator() (const BuildingInfo::FreeBonusNode& node) const
        {
            boost::shared_ptr<Player> pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);

            std::pair<int, int> militaryUnitCounts = getResourceMilitaryUnitCount(pPlayer->getAnalysis()->getResourceInfo(node.freeBonuses.first));

            return militaryUnitCounts.first > 0 || militaryUnitCounts.second > 0;
        }

    private:
        PlayerTypes playerType_;
    };

    class ConditionalBuildingHasPlotYieldChange : public boost::static_visitor<ConditionalPlotYieldEnchancingBuilding>
    {
    public:
        explicit ConditionalBuildingHasPlotYieldChange(BuildingTypes buildingType) : buildingType_(buildingType)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return result_type();
        }

        result_type operator() (const BuildingInfo::BaseNode& node) const
        {
            if (node.buildConditions.empty())
            {
                return result_type();
            }

            result_type result(buildingType_);
            result.buildConditions = node.buildConditions;

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                result_type nodeResult(boost::apply_visitor(*this, node.nodes[i]));
                if (!nodeResult.conditionalYieldChanges.empty())
                {
                    std::copy(nodeResult.conditionalYieldChanges.begin(), nodeResult.conditionalYieldChanges.end(), std::back_inserter(result.conditionalYieldChanges));
                }
            }
            return result.conditionalYieldChanges.empty() ? result_type() : result;
        }

        result_type operator() (const BuildingInfo::YieldNode& node) const
        {
            result_type nodeResult(buildingType_);
            if (node.plotCond)
            {
                nodeResult.conditionalYieldChanges.push_back(std::make_pair(node.plotCond, node.yield));
            }
            return nodeResult;
        }

    private:
        BuildingTypes buildingType_;
    };

    class PlotBuildCondVisitor : public boost::static_visitor<bool>
    {
    public:
        explicit PlotBuildCondVisitor(XYCoords coords) : coords_(coords)
        {
        }

        result_type operator() (const BuildingInfo::NullNode&) const
        {
            return false;
        }

        result_type operator() (const BuildingInfo::IsRiver& node) const
        {
            return gGlobals.getMap().plot(coords_.iX, coords_.iY)->isRiver();
        }

        result_type operator() (const BuildingInfo::MinArea& node) const
        {
            if (node.isWater)
            {
                return gGlobals.getMap().plot(coords_.iX, coords_.iY)->isCoastalLand(node.minimumSize);
            }
            else
            {
                return gGlobals.getMap().plot(coords_.iX, coords_.iY)->area()->getNumTiles() >= node.minimumSize;
            }
        }

        result_type operator() (const BuildingInfo::BuildOrCondition& node) const
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
        XYCoords coords_;
    };


    Commerce getCommerceValue(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        return boost::apply_visitor(CommerceFinder(pCity), pBuildingInfo->getInfo());
    }

    void updateRequestData(const CvCity* pCity, CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        boost::apply_visitor(CityOutputUpdater(pCity, data), pBuildingInfo->getInfo());
        data.recalcOutputs();
    }

    bool buildingHasEconomicImpact(const CityData& data, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        return boost::apply_visitor(BuildingHasEconomicImpactVisitor(data), pBuildingInfo->getInfo());
    }

    bool buildingHasPotentialEconomicImpact(const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        return boost::apply_visitor(BuildingHasPotentialEconomicImpactVisitor(), pBuildingInfo->getInfo());
    }

    bool buildingHasPotentialMilitaryImpact(PlayerTypes playerType, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        return boost::apply_visitor(BuildingHasPotentialMilitaryImpactVisitor(playerType), pBuildingInfo->getInfo());
    }

    std::vector<ConditionalPlotYieldEnchancingBuilding> getConditionalYieldEnchancingBuildings(PlayerTypes playerType)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(playerType);
        const boost::shared_ptr<PlayerAnalysis>& pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(playerType)->getAnalysis();

        std::vector<ConditionalPlotYieldEnchancingBuilding> buildings;

        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            BuildingTypes buildingType = (BuildingTypes)gGlobals.getCivilizationInfo(player.getCivilizationType()).getCivilizationBuildings(i);

            if (!isLimitedWonderClass(getBuildingClass(buildingType)))
            {
                boost::shared_ptr<BuildingInfo> pBuildingInfo = pPlayerAnalysis->getBuildingInfo(buildingType);
                if (!pBuildingInfo)
                {
#ifdef ALTAI_DEBUG
                    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
                    os << "\nSkipping conditional building: " << gGlobals.getBuildingInfo(buildingType).getType();
#endif
                    continue;
                }

                ConditionalPlotYieldEnchancingBuilding result(boost::apply_visitor(ConditionalBuildingHasPlotYieldChange(buildingType), pBuildingInfo->getInfo()));
                if (result.buildingType != NO_BUILDING)
                {
                    buildings.push_back(result);
                }
            }
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
            for (size_t i = 0, count = buildings.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                else os << "\nConditional Yield Enhancing buildings: ";
                os << gGlobals.getBuildingInfo(buildings[i].buildingType).getType();
            }
        }
#endif
        return buildings;
    }

    std::map<XYCoords, PlotYield> getExtraConditionalYield(const std::pair<XYCoords, std::map<int, std::set<XYCoords> > >& plotData,
        const std::vector<ConditionalPlotYieldEnchancingBuilding>& conditionalYieldEnchancingBuildings, PlayerTypes playerType, int lookaheadDepth)
    {
        std::map<XYCoords, PlotYield> extraYieldsMap;
        for (size_t i = 0, count = conditionalYieldEnchancingBuildings.size(); i < count; ++i)
        {
            bool isValid = true;

            const boost::shared_ptr<Player>& player = gGlobals.getGame().getAltAI()->getPlayer(playerType);
            const boost::shared_ptr<BuildingInfo>& pBuildingInfo = player->getAnalysis()->getBuildingInfo(conditionalYieldEnchancingBuildings[i].buildingType);
            const BuildingInfo::BaseNode& node = boost::get<BuildingInfo::BaseNode>(pBuildingInfo->getInfo());

            for (size_t j = 0, techCount = node.techs.size(); j < techCount; ++j)
            {
                const int depth = player->getTechResearchDepth(node.techs[j]);
                if (depth > lookaheadDepth)
                {
                    isValid = false;
                    break;
                }
            }

            if (!isValid)
            {
                continue;
            }

            for (size_t j = 0, count = conditionalYieldEnchancingBuildings[i].buildConditions.size(); j < count; ++j)
            {
                isValid = isValid && boost::apply_visitor(PlotBuildCondVisitor(plotData.first), conditionalYieldEnchancingBuildings[i].buildConditions[j]);
            }

            if (isValid)
            {
                for (size_t j = 0, count = conditionalYieldEnchancingBuildings[i].conditionalYieldChanges.size(); j < count; ++j)
                {
                    CvPlot* pPlot = gGlobals.getMap().plot(plotData.first.iX, plotData.first.iY);

                    if ((pPlot->*(conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].first))())
                    {
                        extraYieldsMap[plotData.first] += conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].second;
                    }

                    for (std::map<int, std::set<XYCoords> >::const_iterator ci(plotData.second.begin()), ciEnd(plotData.second.end()); ci != ciEnd; ++ci)
                    {
                        for (std::set<XYCoords>::const_iterator si(ci->second.begin()), siEnd(ci->second.end()); si != siEnd; ++si)
                        {
                            pPlot = gGlobals.getMap().plot(si->iX, si->iY);

                            if ((pPlot->*(conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].first))())
                            {
                                extraYieldsMap[*si] += conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].second;
                            }
                        }
                    }
                }
            }
        }

#ifdef ALTAI_DEBUG  // need to pass player in
        /*{
            if (!extraYieldsMap.empty())
            {
                PlotYield totalExtraYield;
                for (std::map<XYCoords, PlotYield>::const_iterator ci(extraYieldsMap.begin()), ciEnd(extraYieldsMap.end()); ci != ciEnd; ++ci)
                {
                    totalExtraYield += ci->second;
                }

                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
                os << "\nextra plotyield = " << totalExtraYield;
            }
        }*/
#endif

        return extraYieldsMap;
    }

    PlotYield getExtraConditionalYield(XYCoords cityCoords, XYCoords plotCoords, const std::vector<ConditionalPlotYieldEnchancingBuilding>& conditionalYieldEnchancingBuildings)
    {
        PlotYield extraYield;
        for (size_t i = 0, count = conditionalYieldEnchancingBuildings.size(); i < count; ++i)
        {
            bool isValid = true;
            for (size_t j = 0, count = conditionalYieldEnchancingBuildings[i].buildConditions.size(); j < count; ++j)
            {
                isValid = isValid && boost::apply_visitor(PlotBuildCondVisitor(cityCoords), conditionalYieldEnchancingBuildings[i].buildConditions[j]);
            }

            if (isValid)
            {
                for (size_t j = 0, count = conditionalYieldEnchancingBuildings[i].conditionalYieldChanges.size(); j < count; ++j)
                {
                    CvPlot* pPlot = gGlobals.getMap().plot(plotCoords.iX, plotCoords.iY);

                    if ((pPlot->*(conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].first))())
                    {
                        extraYield += conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].second;
                    }
                }
            }
        }

        return extraYield;
    }

    class CouldConstructBuildingVisitor : public boost::static_visitor<bool>
    {
    public:
        CouldConstructBuildingVisitor(const Player& player, const City& city, int lookaheadDepth) : player_(player), city_(city), lookaheadDepth_(lookaheadDepth)
        {
            civHelper_ = player.getCivHelper();
            pAnalysis_ = player.getAnalysis();
        }

        template <typename T>
            bool operator() (const T&) const
        {
            return true;
        }

        bool operator() (const BuildingInfo::BaseNode& node) const
        {
            for (size_t i = 0, count = node.techs.size(); i < count; ++i)
            {
                // if we don't we have the tech and its depth is deeper than our lookaheadDepth, return false
                if (!civHelper_->hasTech(node.techs[i]) &&
                    (lookaheadDepth_ == 0 ? true : pAnalysis_->getTechResearchDepth(node.techs[i]) > lookaheadDepth_))
                {
                    return false;
                }
            }

            PlotBuildCondVisitor plotConditionsVisitor(city_.getCoords());
            for (size_t i = 0, count = node.buildConditions.size(); i < count; ++i)
            {
                if (!boost::apply_visitor(plotConditionsVisitor, node.buildConditions[i]))
                {
                    return false;
                }
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

        bool operator() (const BuildingInfo::BonusNode& node) const
        {
            // todo - add any required bonus check here
            return true;
        }

    private:
        const Player& player_;
        const City& city_;
        int lookaheadDepth_;
        boost::shared_ptr<CivHelper> civHelper_;
        boost::shared_ptr<PlayerAnalysis> pAnalysis_;
    };

    bool couldConstructBuilding(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        return boost::apply_visitor(CouldConstructBuildingVisitor(player, city, lookaheadDepth), pBuildingInfo->getInfo());
    }
}