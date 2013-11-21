#include "AltAI.h"

#include "./civic_info_visitors.h"
#include "./civic_info.h"
#include "./player_analysis.h"
#include "./city_data.h"
#include "./happy_helper.h"
#include "./health_helper.h"
#include "./modifiers_helper.h"
#include "./maintenance_helper.h"
#include "./trade_route_helper.h"
#include "./building_helper.h"
#include "./religion_helper.h"
#include "./civ_helper.h"
#include "./city.h"

namespace AltAI
{
    namespace
    {
        // update CityData with changes resulting from adopting civics
        class CityOutputUpdater : public boost::static_visitor<>
        {
        public:
            CityOutputUpdater(const CvCity* pCity, CityData& data, bool isAdding) : pCity_(pCity), data_(data), isAdding_(isAdding)
            {
            }

            template <typename T>
                result_type operator() (const T&) const
            {
            }

            void operator() (const CivicInfo::BaseNode& node) const
            {
                if (node.health != 0)
                {
                    data_.getHealthHelper()->changePlayerHealthiness(isAdding_ ? node.health : -node.health);
                }

                for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
                {
                    boost::apply_visitor(*this, node.nodes[i]);
                }
            }

            void operator() (const CivicInfo::BuildingNode& node) const
            {
                if (node.buildingType != NO_BUILDING)
                {
                    int numBuildings = data_.getBuildingsHelper()->getNumBuildings(node.buildingType);
                    if (numBuildings > 0)
                    {
                        if (node.happy > 0)
                        {
                            data_.getHappyHelper()->changeExtraBuildingGoodHappiness(isAdding_ ? numBuildings * node.happy : -numBuildings * node.happy);
                        }
                        else if (node.happy < 0)
                        {
                            data_.getHappyHelper()->changeExtraBuildingBadHappiness(isAdding_ ? numBuildings * node.happy : -numBuildings * node.happy);
                        }

                        if (node.health > 0)
                        {
                            data_.getHealthHelper()->changeExtraBuildingGoodHealthiness(isAdding_ ? numBuildings * node.health : -numBuildings * node.health);
                        }
                        else if (node.health < 0)
                        {
                            data_.getHealthHelper()->changeExtraBuildingBadHealthiness(isAdding_ ? numBuildings * node.health : -numBuildings * node.health);
                        }
                    }
                }

            }

            void operator() (const CivicInfo::ImprovementNode& node) const
            {
                for (size_t i = 0, count = node.improvementTypeAndYieldModifiers.size(); i < count; ++i)
                {
                    for (PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        if (iter->isActualPlot() && iter->improvementType == node.improvementTypeAndYieldModifiers[i].first)
                        {
                            if (isAdding_)
                            {
                                iter->plotYield += node.improvementTypeAndYieldModifiers[i].second;
                            }
                            else
                            {
                                iter->plotYield -= node.improvementTypeAndYieldModifiers[i].second;
                            }
                        }
                    }
                }

                for (size_t i = 0, count = node.featureTypeAndHappyChanges.size(); i < count; ++i)
                {
                    for (PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        if (iter->isActualPlot() && iter->featureType == node.featureTypeAndHappyChanges[i].first)
                        {
                            if (node.featureTypeAndHappyChanges[i].second > 0)
                            {
                                data_.getHappyHelper()->changeFeatureGoodHappiness(isAdding_ ? node.featureTypeAndHappyChanges[i].second : -node.featureTypeAndHappyChanges[i].second);
                            }
                            else if (node.featureTypeAndHappyChanges[i].second < 0)
                            {
                                data_.getHappyHelper()->changeFeatureBadHappiness(isAdding_ ? node.featureTypeAndHappyChanges[i].second : -node.featureTypeAndHappyChanges[i].second);
                            }
                        }
                    }
                }

                if (node.improvementUpgradeRateModifier > 0)
                {
                    for (PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        if (iter->isActualPlot() && !iter->upgradeData.upgrades.empty())
                        {
                            // todo - need upgrade rate code moved into CivHelper
                        }
                    }
                }
            }

            void operator() (const CivicInfo::YieldNode& node) const
            {
                bool recalcAllOutputs = false;

                if (!isEmpty(node.yieldModifier))
                {
                    recalcAllOutputs = true;
                    if (node.yieldModifier[YIELD_COMMERCE] != 0)
                    {
                        data_.changeCommerceYieldModifier(isAdding_ ? node.yieldModifier[YIELD_COMMERCE] : -node.yieldModifier[YIELD_COMMERCE]);
                    }
                    data_.getModifiersHelper()->changePlayerYieldModifier(isAdding_ ? node.yieldModifier : -node.yieldModifier);
                }

                if (!isEmpty(node.capitalYieldModifier) && data_.getCity()->isCapital())
                {
                    recalcAllOutputs = true;
                    if (node.capitalYieldModifier[YIELD_COMMERCE] != 0)
                    {
                        data_.changeCommerceYieldModifier(isAdding_ ? node.capitalYieldModifier[YIELD_COMMERCE] : -node.capitalYieldModifier[YIELD_COMMERCE]);
                    }
                    data_.getModifiersHelper()->changeCapitalYieldModifier(isAdding_ ? node.capitalYieldModifier : -node.capitalYieldModifier);
                }

                if (!isEmpty(node.stateReligionBuildingProductionModifier))
                {
                    ReligionTypes stateReligion = data_.getReligionHelper()->getStateReligion();
                    if (stateReligion != NO_RELIGION && data_.getReligionHelper()->isHasReligion(stateReligion))
                    {
                        data_.getModifiersHelper()->changeStateReligionBuildingProductionModifier(
                            isAdding_ ? node.stateReligionBuildingProductionModifier : -node.stateReligionBuildingProductionModifier);
                    }
                }

                if (recalcAllOutputs)
                {
                    data_.recalcOutputs();
                }
            }

            void operator() (const CivicInfo::CommerceNode& node) const
            {
                bool recalcAllOutputs = false;

                if (!isEmpty(node.commerceModifier))
                {
                    recalcAllOutputs = true;
                    data_.getModifiersHelper()->changePlayerCommerceModifier(isAdding_ ? node.commerceModifier : -node.commerceModifier);
                }

                if (!isEmpty(node.extraSpecialistCommerce))
                {
                    for (PlotDataListIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                    {
                        if (!iter->isActualPlot())
                        {
                            if (isAdding_)
                            {
                                iter->commerce += node.extraSpecialistCommerce;
                            }
                            else
                            {
                                iter->commerce -= node.extraSpecialistCommerce;
                            }

                            TotalOutput specialistOutput(makeOutput(iter->plotYield, iter->commerce, makeYield(100, 100, data_.getCommerceYieldModifier()), makeCommerce(100, 100, 100, 100), data_.getCommercePercent()));
                            iter->actualOutput = iter->output = specialistOutput;
                        }
                    }

                    for (PlotDataListIter iter(data_.getFreeSpecOutputs().begin()), endIter(data_.getFreeSpecOutputs().end()); iter != endIter; ++iter)
                    {
                        if (isAdding_)
                        {
                            iter->commerce += node.extraSpecialistCommerce;
                        }
                        else
                        {
                            iter->commerce -= node.extraSpecialistCommerce;
                        }

                        TotalOutput specialistOutput(makeOutput(iter->plotYield, iter->commerce, makeYield(100, 100, data_.getCommerceYieldModifier()), makeCommerce(100, 100, 100, 100), data_.getCommercePercent()));
                        iter->output = specialistOutput;
                        iter->actualOutput = specialistOutput;
                    }
                }

                for (size_t i = 0, count = node.validSpecialists.size(); i < count; ++i)
                {
                    if (isAdding_)
                    {
                        int currentSpecCount = data_.getNumPossibleSpecialists(node.validSpecialists[i]);
                        if (currentSpecCount < data_.getPopulation())
                        {
                            data_.addSpecialistSlots(node.validSpecialists[i], data_.getPopulation() - currentSpecCount);
                        }
                    }
                    else  // TODO - need to know if spec count is unlimited
                    {
                    }
                }

                data_.changePlayerFreeSpecialistSlotCount(isAdding_ ? node.freeSpecialists : -node.freeSpecialists);

                if (recalcAllOutputs)
                {
                    data_.recalcOutputs();
                }
            }

            void operator() (const CivicInfo::MaintenanceNode& node) const
            {
                if (node.distanceModifier != 0)
                {
                    data_.getMaintenanceHelper()->changeDistanceModifier(isAdding_ ? node.distanceModifier : -node.distanceModifier);
                }
                
                if (node.numCitiesModifier != 0)
                {
                    data_.getMaintenanceHelper()->changeNumCitiesModifier(isAdding_ ? node.numCitiesModifier : -node.numCitiesModifier);
                }
                
                if (node.corporationModifier != 0)
                {
                    // todo
                }

                if (node.freeUnitsPopulationPercent != 0)
                {
                    // todo (civ level)
                }
                
                if (node.extraGoldPerMilitaryUnit != 0)
                {
                    // todo (civ level)
                }
            }

            void operator() (const CivicInfo::TradeNode& node) const
            {
                if (node.extraTradeRoutes > 0)
                {
                    data_.getTradeRouteHelper()->changeNumRoutes(isAdding_ ? node.extraTradeRoutes : -node.extraTradeRoutes);
                }

                data_.getTradeRouteHelper()->setAllowForeignTradeRoutes(isAdding_ ? !node.noForeignTrade : node.noForeignTrade);
            }

            void operator() (const CivicInfo::HappyNode& node) const
            {
                if (node.largestCityHappy != 0)
                {
                    data_.getHappyHelper()->changeLargestCityHappiness(isAdding_ ? node.largestCityHappy : -node.largestCityHappy);
                }

                data_.getHappyHelper()->setMilitaryHappiness(node.happyPerUnit);
            }

            void operator() (const CivicInfo::MiscEffectNode& node) const
            {
                // todo
            }

        private:
            const CvCity* pCity_;
            CityData& data_;
            bool isAdding_;
        };
    }

    class CivicHasEconomicImpactVisitor : public boost::static_visitor<bool>
    {
    public:
        explicit CivicHasEconomicImpactVisitor(const CityData& data) : data_(data)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        result_type operator() (const CivicInfo::BaseNode& node) const
        {
            if (node.health != 0)
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

        result_type operator() (const CivicInfo::BuildingNode& node) const
        {
            if (node.buildingType != NO_BUILDING)
            {
                int numBuildings = data_.getBuildingsHelper()->getNumBuildings(node.buildingType);
                if ((node.happy > 0 || node.health > 0) && numBuildings > 0)
                {
                    return true;
                }
            }

            if (node.specialBuildingTypeNotReqd != NO_SPECIALBUILDING)
            {
                // todo - implement getNumSpecBuildings
                return true;
            }

            return false;
        }

        result_type operator() (const CivicInfo::ImprovementNode& node) const
        {
            for (size_t i = 0, count = node.improvementTypeAndYieldModifiers.size(); i < count; ++i)
            {
                for (PlotDataListConstIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                {
                    if (iter->isActualPlot() && iter->improvementType == node.improvementTypeAndYieldModifiers[i].first)
                    {
                        return true;
                    }
                }
            }

            for (size_t i = 0, count = node.featureTypeAndHappyChanges.size(); i < count; ++i)
            {
                for (PlotDataListConstIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                {
                    if (iter->isActualPlot() && iter->featureType == node.featureTypeAndHappyChanges[i].first)
                    {
                        return true;
                    }
                }
            }

            if (node.improvementUpgradeRateModifier > 0)
            {
                for (PlotDataListConstIter iter(data_.getPlotOutputs().begin()), endIter(data_.getPlotOutputs().end()); iter != endIter; ++iter)
                {
                    if (iter->isActualPlot() && !iter->upgradeData.upgrades.empty())
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        result_type operator() (const CivicInfo::YieldNode& node) const
        {
            if (!isEmpty(node.yieldModifier))
            {
                return true;
            }

            if (!isEmpty(node.capitalYieldModifier) && data_.getCity()->isCapital())
            {
                return true;
            }
            return false;
        }

        result_type operator() (const CivicInfo::CommerceNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::MaintenanceNode& node) const
        {
            if (node.distanceModifier != 0 || node.numCitiesModifier != 0 || node.extraGoldPerMilitaryUnit != 0 || node.freeUnitsPopulationPercent != 0)
            {
                return true;
            }

            if (node.corporationModifier)
            {
                return true;  // todo - check we have any corps
            }

            return false;
        }

        result_type operator() (const CivicInfo::HurryNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::TradeNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::HappyNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::MiscEffectNode& node) const
        {
            return false;  // todo (true in some cases for civicPercentAnger and stateReligionGreatPeopleRateModifier and warWearinessModifier)
        }

    private:
        const CityData& data_;
    };

    class CivicHasPotentialEconomicImpactVisitor : public boost::static_visitor<bool>
    {
    public:
        explicit CivicHasPotentialEconomicImpactVisitor(PlayerTypes playerType) : playerType_(playerType)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        result_type operator() (const CivicInfo::BaseNode& node) const
        {
            if (node.health != 0)
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

        result_type operator() (const CivicInfo::BuildingNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::ImprovementNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::YieldNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::CommerceNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::MaintenanceNode& node) const
        {
            if (node.corporationModifier)
            {
                return true;  // todo - check we have any corps
            }

            return true;
        }

        result_type operator() (const CivicInfo::HurryNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::TradeNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::HappyNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::MiscEffectNode& node) const
        {
            return false;  // todo (true in some cases for civicPercentAnger and stateReligionGreatPeopleRateModifier and warWearinessModifier)
        }

    private:
        PlayerTypes playerType_;
    };

    class CivicHasPotentialMilitaryImpactVisitor : public boost::static_visitor<bool>
    {
    public:
        explicit CivicHasPotentialMilitaryImpactVisitor(PlayerTypes playerType) : playerType_(playerType)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
            return false;
        }

        result_type operator() (const CivicInfo::BaseNode& node) const
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

        // hurrying is useful for building units too
        result_type operator() (const CivicInfo::HurryNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::UnitNode& node) const
        {
            return true;
        }

        result_type operator() (const CivicInfo::MiscEffectNode& node) const
        {
            return node.warWearinessModifier != 0;
        }

    private:
        PlayerTypes playerType_;
    };

    class UpdateSpecialBuildingNotRequiredCountVisitor : public boost::static_visitor<void>
    {
    public:
        explicit UpdateSpecialBuildingNotRequiredCountVisitor(std::vector<int>& specialBuildingNotRequiredCounts, bool isAdding)
            : specialBuildingNotRequiredCounts_(specialBuildingNotRequiredCounts), isAdding_(isAdding)
        {
        }

        template <typename T>
            result_type operator() (const T&) const
        {
        }

        result_type operator() (const CivicInfo::BaseNode& node) const
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        result_type operator() (const CivicInfo::BuildingNode& node) const
        {
            if (isAdding_)
            {
                ++specialBuildingNotRequiredCounts_[node.specialBuildingTypeNotReqd];
            }
            else
            {
                --specialBuildingNotRequiredCounts_[node.specialBuildingTypeNotReqd];
            }
        }

    private:
        std::vector<int>& specialBuildingNotRequiredCounts_;
        bool isAdding_;
    };

    boost::shared_ptr<CivicInfo> makeCivicInfo(CivicTypes civicType, PlayerTypes playerType)
    {
        return boost::shared_ptr<CivicInfo>(new CivicInfo(civicType, playerType));
    }

    void streamCivicInfo(std::ostream& os, const boost::shared_ptr<CivicInfo>& pCivicInfo)
    {
        os << pCivicInfo->getInfo();
    }

    void updateRequestData(const CvCity* pCity, CityData& data, const boost::shared_ptr<PlayerAnalysis>& pPlayerAnalysis, CivicTypes newCivic)
    {
        CivicTypes currentCivic = data.getCivHelper()->currentCivic((CivicOptionTypes)gGlobals.getCivicInfo(newCivic).getCivicOptionType());

        if (currentCivic != newCivic)
        {
            boost::apply_visitor(CityOutputUpdater(pCity, data, false), pPlayerAnalysis->getCivicInfo(currentCivic)->getInfo());
            data.getCivHelper()->adoptCivic(newCivic);
            boost::apply_visitor(CityOutputUpdater(pCity, data, true), pPlayerAnalysis->getCivicInfo(newCivic)->getInfo());
            data.recalcOutputs();
        }
    }

    bool civicHasEconomicImpact(const CityData& data, const boost::shared_ptr<CivicInfo>& pCivicInfo)
    {
        return boost::apply_visitor(CivicHasEconomicImpactVisitor(data), pCivicInfo->getInfo());
    }

    bool civicHasPotentialEconomicImpact(PlayerTypes playerType, const boost::shared_ptr<CivicInfo>& pCivicInfo)
    {
        return boost::apply_visitor(CivicHasPotentialEconomicImpactVisitor(playerType), pCivicInfo->getInfo());
    }

    bool civicHasPotentialMilitaryImpact(PlayerTypes playerType, const boost::shared_ptr<CivicInfo>& pCivicInfo)
    {
        return boost::apply_visitor(CivicHasPotentialMilitaryImpactVisitor(playerType), pCivicInfo->getInfo());
    }

    void updateSpecialBuildingNotRequiredCount(std::vector<int>& specialBuildingNotRequiredCounts, const boost::shared_ptr<CivicInfo>& pCivicInfo, bool isAdding)
    {
        boost::apply_visitor(UpdateSpecialBuildingNotRequiredCountVisitor(specialBuildingNotRequiredCounts, isAdding), pCivicInfo->getInfo());
    }
}