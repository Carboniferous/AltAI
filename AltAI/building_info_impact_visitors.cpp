#include "AltAI.h"

#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./civ_helper.h"
#include "./trade_route_helper.h"
#include "./bonus_helper.h"
#include "./religion_helper.h"
#include "./iters.h"
#include "./civ_log.h"
#include "./building_info_impact_visitors.h"
#include "./buildings_info.h"


namespace AltAI
{
    class ResourceInfo;
    std::pair<int, int> getResourceMilitaryUnitCount(const boost::shared_ptr<ResourceInfo>& pResourceInfo);

    namespace
    {
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
                if (!isEmpty(node.modifier) || !isEmpty(node.powerModifier))
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
                        int xCoord = data_.getCityPlotData().coords.iX, yCoord = data_.getCityPlotData().coords.iY;
                        CvPlot* pPlot = gGlobals.getMap().plot(xCoord, yCoord);

                        if ((pPlot->*(node.plotCond))())
                        {
                            return true;
                        }

                        for (PlotDataListConstIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
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
                    for (PlotDataListConstIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
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

                if (node.extraCoastalTradeRoutes != 0 && data_.getCity()->isCoastal(gGlobals.getMIN_WATER_SIZE_FOR_OCEAN()))
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

                if (node.foreignTradeRouteModifier != 0 && data_.getTradeRouteHelper()->couldHaveForeignRoutes())
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
                const CvPlayer& player = CvPlayerAI::getPlayer(data_.getOwner());

                // add any 'plots' for new specialist slots (if we aren't maxed out already for that type)
                for (size_t i = 0, count = node.specialistTypes.size(); i < count; ++i)
                {
                    int specialistCount = data_.getNumPossibleSpecialists(node.specialistTypes[i].first);

                    // todo - should this total population?
                    if (specialistCount < data_.getWorkingPopulation())  // if this is also changed by this building (e.g. HG)? - OK as long as add pop code updates specs
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
                if (data_.getBonusHelper()->getNumBonuses(node.bonusType) > 0)
                {
                    if (node.happy != 0 || node.health != 0)
                    {
                        return true;
                    }

                    if (!isEmpty(node.yieldModifier))
                    {
                        return true;
                    }

                    if (node.freeBonusCount > 0)
                    {
                        return true;
                    }

                    if (node.isRemoved)
                    {
                        return true;
                    }
                }
                return false;
            }

            result_type operator() (const BuildingInfo::ReligionNode& node) const
            {
                return node.prereqReligion != NO_RELIGION && data_.getReligionHelper()->getReligionCount(node.prereqReligion) > 0;
            }

            result_type operator() (const BuildingInfo::HurryNode& node) const            
            {
                return node.globalHurryCostModifier != 0 || node.hurryAngerModifier != 0;
            }

            result_type operator() (const BuildingInfo::AreaEffectNode& node) const
            {
                return true;
            }

            result_type operator() (const BuildingInfo::MiscEffectNode& node) const
            {
                return !isEmpty(node);
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

            result_type operator() (const BuildingInfo::PowerNode&) const
            {
                return true;
            }

            result_type operator() (const BuildingInfo::ReligionNode& node) const
            {
                return true;
            }

            result_type operator() (const BuildingInfo::HurryNode& node)
            {
                return true;
            }

            result_type operator() (const BuildingInfo::MiscEffectNode& node) const
            {
                return !isEmpty(node);
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

            result_type operator() (const BuildingInfo::YieldNode& node) const
            {
                return node.militaryProductionModifier != 0;
            }

            result_type operator() (const BuildingInfo::CityDefenceNode&) const
            {
                return true;
            }

            result_type operator() (const BuildingInfo::UnitExpNode&) const
            {
                return true;
            }
    
            result_type operator() (const BuildingInfo::BonusNode& node) const
            {
                std::pair<int, int> militaryUnitCounts;

                if (node.freeBonusCount > 0)
                {
                    PlayerPtr pPlayer = gGlobals.getGame().getAltAI()->getPlayer(playerType_);

                    std::pair<int, int> militaryUnitCounts = getResourceMilitaryUnitCount(pPlayer->getAnalysis()->getResourceInfo(node.bonusType));
                }

                return militaryUnitCounts.first > 0 || militaryUnitCounts.second > 0;
            }

        private:
            PlayerTypes playerType_;
        };
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
}