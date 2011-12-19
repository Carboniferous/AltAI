#pragma once

#include "./utils.h"

namespace AltAI
{
    class BuildingInfo
    {
    public:
        explicit BuildingInfo(BuildingTypes buildingType);
        BuildingInfo(BuildingTypes buildingType, PlayerTypes playerType);

        BuildingTypes getBuildingType() const
        {
            return buildingType_;
        }

        struct NullNode
        {
            NullNode() {}
        };

        struct IsRiver
        {
        };

        struct MinArea
        {
            MinArea(bool isWater_, int minimumSize_) : isWater(isWater_), minimumSize(minimumSize_) {}
            bool isWater;
            int minimumSize;
        };

        struct BuildOrCondition;

        typedef boost::variant<NullNode, IsRiver, MinArea, boost::recursive_wrapper<BuildOrCondition> > BuildCondition;

        struct BuildOrCondition
        {
            std::vector<BuildCondition> conditions;
        };

        struct BaseNode;

        struct YieldNode
        {
            YieldNode() : plotCond(NULL), global(false) {}
            PlotYield yield;
            YieldModifier modifier;
            CvPlotFnPtr plotCond;
            bool global;
        };

        struct SpecialistNode
        {
            SpecialistNode() {}
            std::vector<std::pair<SpecialistTypes, PlotYield> > specialistTypesAndYields;
            Commerce extraCommerce;
        };

        struct CommerceNode
        {
            CommerceNode() : global(false) {}
            Commerce commerce, obsoleteSafeCommerce, stateReligionCommerce;
            CommerceModifier modifier;
            bool global;
        };

        struct TradeNode
        {
            explicit TradeNode(int extraTradeRoutes_ = 0, int extraCoastalTradeRoutes_ = 0, int extraGlobalTradeRoutes_ = 0,
                int tradeRouteModifier_ = 0, int foreignTradeRouteModifier_ = 0)
                : extraTradeRoutes(extraTradeRoutes_), extraCoastalTradeRoutes(extraCoastalTradeRoutes_), extraGlobalTradeRoutes(extraGlobalTradeRoutes_),
                  tradeRouteModifier(tradeRouteModifier_), foreignTradeRouteModifier(foreignTradeRouteModifier_) 
            {
            }

            int extraTradeRoutes;
            int extraCoastalTradeRoutes;
            int extraGlobalTradeRoutes;
            int tradeRouteModifier;
            int foreignTradeRouteModifier;
        };

        struct UnitExpNode
        {
            UnitExpNode() : freeExperience(0), globalFreeExperience(0), freePromotion(NO_PROMOTION) {}
            int freeExperience, globalFreeExperience;
            std::vector<std::pair<DomainTypes, int> > domainFreeExperience, domainProductionModifier;
            std::vector<std::pair<UnitCombatTypes, int> > combatTypeFreeExperience;
            PromotionTypes freePromotion;
        };

        struct SpecialistSlotNode
        {
            SpecialistSlotNode() {}
            std::vector<std::pair<SpecialistTypes, int> > specialistTypes;
            std::vector<std::pair<SpecialistTypes, int> > freeSpecialistTypes;
            std::vector<std::pair<ImprovementTypes, int> > improvementFreeSpecialists;
        };

        struct BonusNode
        {
            BonusNode() : bonusType(NO_BONUS), happy(0), health(0), prodModifier(0) {}
            BonusTypes bonusType;
            YieldModifier yieldModifier;
            int happy, health;
            int prodModifier;
        };

        struct FreeBonusNode
        {
            FreeBonusNode() : freeBonuses(std::make_pair(NO_BONUS, 0)) {}
            std::pair<BonusTypes, int> freeBonuses;
        };

        struct RemoveBonusNode
        {
            RemoveBonusNode() : bonusType(NO_BONUS) {}
            BonusTypes bonusType;
        };

        struct CityDefenceNode
        {
            CityDefenceNode() : defenceBonus(0), globalDefenceBonus(0), bombardRateModifier(0), espionageDefence(0) {}
            int defenceBonus, globalDefenceBonus, bombardRateModifier, espionageDefence;
        };

        //struct ReligionNode
        //{
        //    bool canSpread;
        //    ReligionTypes religionType_;
        //};

        struct MiscEffectNode
        {
            MiscEffectNode() : cityMaintenanceModifierChange(0), foodKeptPercent(0), hurryAngerModifier(0),
                noUnhealthinessFromBuildings(false), noUnhealthinessFromPopulation(false) {}
            int cityMaintenanceModifierChange;
            int foodKeptPercent;
            int hurryAngerModifier;
            bool noUnhealthinessFromBuildings, noUnhealthinessFromPopulation;
        };

        typedef boost::variant<NullNode, boost::recursive_wrapper<BaseNode>, YieldNode, CommerceNode, TradeNode, BonusNode, FreeBonusNode, 
            RemoveBonusNode, SpecialistNode, UnitExpNode, CityDefenceNode, SpecialistSlotNode, MiscEffectNode> BuildingInfoNode;

        struct BaseNode
        {
            BaseNode() : cost(0), productionModifier(0), happy(0), health(0) {}
            int cost, productionModifier;
            int happy, health;
            std::vector<TechTypes> techs;
            std::vector<BuildCondition> buildConditions;
            std::vector<BuildingInfoNode> nodes;
        };

        const BuildingInfoNode& getInfo() const;

    private:
        void init_();

        BuildingTypes buildingType_;
        PlayerTypes playerType_;
        BuildingInfoNode infoNode_;
    };
}