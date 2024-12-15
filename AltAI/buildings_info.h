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

        struct RequiredBuildings
        {
            std::vector<BuildingTypes> cityBuildings;
            std::vector<std::pair<BuildingTypes, int> > buildingCounts;
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

        struct IsHolyCity
        {
            explicit IsHolyCity(ReligionTypes religionType_) : religionType(religionType_) {}
            ReligionTypes religionType;
        };

        struct BuildOrCondition;

        typedef boost::variant<NullNode, RequiredBuildings, IsRiver, MinArea, IsHolyCity, boost::recursive_wrapper<BuildOrCondition> > BuildCondition;

        struct BuildOrCondition
        {
            std::vector<BuildCondition> conditions;
        };

        struct BaseNode;

        struct YieldNode
        {
            YieldNode() : plotCond(NULL), militaryProductionModifier(0), global(false) {}
            PlotYield yield;
            YieldModifier modifier, powerModifier;       
            CvPlotFnPtr plotCond;
            int militaryProductionModifier;
            bool global;

            bool operator == (const YieldNode& other) const;
        };

        struct SpecialistNode
        {
            SpecialistNode() : cityGPPRateModifier(0), playerGPPRateModifier(0) {} 
            std::vector<std::pair<SpecialistTypes, PlotYield> > specialistTypesAndYields;
            Commerce extraCommerce;
            std::pair<int, UnitClassTypes> generatedGPP;
            int cityGPPRateModifier, playerGPPRateModifier;
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

        struct PowerNode
        {
            PowerNode() : bonusType(NO_BONUS), isDirty(false), areaCleanPower(false) {}
            BonusTypes bonusType;
            bool isDirty, areaCleanPower;
        };

        struct UnitExpNode
        {
            UnitExpNode() : freeExperience(0), globalFreeExperience(0), freePromotion(NO_PROMOTION) {}
            int freeExperience, globalFreeExperience;
            std::vector<std::pair<DomainTypes, int> > domainFreeExperience, domainProductionModifier;
            std::vector<std::pair<UnitCombatTypes, int> > combatTypeFreeExperience;
            PromotionTypes freePromotion;

            bool operator == (const UnitExpNode& other) const;
        };

        struct UnitNode
        {
            UnitNode() : enabledUnitType(NO_UNIT) {}
            UnitTypes enabledUnitType;
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
            BonusNode() : bonusType(NO_BONUS), happy(0), health(0), prodModifier(0), freeBonusCount(0), isRemoved(false) {}
            BonusTypes bonusType;
            YieldModifier yieldModifier;
            int happy, health;
            int prodModifier;
            int freeBonusCount;
            bool isRemoved;

            bool operator == (const BonusNode& other) const;
        };

        struct CityDefenceNode
        {
            CityDefenceNode() : defenceBonus(0), globalDefenceBonus(0), bombardRateModifier(0),
                espionageDefence(0), airDefenceModifier(0), nukeDefenceModifier(0),
                extraAirUnitCapacity(0), extraAirLiftCount(0) {}
            int defenceBonus, globalDefenceBonus, bombardRateModifier, espionageDefence, airDefenceModifier, nukeDefenceModifier;
            int extraAirUnitCapacity, extraAirLiftCount;

            bool operator == (const CityDefenceNode& other) const;
        };

        struct ReligionNode
        {
            ReligionNode() : prereqReligion(NO_RELIGION), religionType(NO_RELIGION), globalReligionType(NO_RELIGION) {}
            ReligionTypes prereqReligion, religionType, globalReligionType;
            Commerce globalCommerceChange;
        };

        struct HurryNode
        {
            HurryNode() : hurryAngerModifier(0), globalHurryCostModifier(0) {}
            int hurryAngerModifier, globalHurryCostModifier;
        };

        struct AreaEffectNode
        {
            AreaEffectNode() : areaHealth(0), globalHealth(0), areaHappy(0), globalHappy(0) {}
            int areaHealth, globalHealth;
            int areaHappy, globalHappy;
        };

        struct MiscEffectNode
        {
            MiscEffectNode() : cityMaintenanceModifierChange(0), foodKeptPercent(0), hurryAngerModifier(0), 
                workerSpeedModifier(0), globalPopChange(0), nFreeTechs(0), 
                noUnhealthinessFromBuildings(false), noUnhealthinessFromPopulation(false), noUnhappiness(false),
                startsGoldenAge(false), makesCityCapital(false), isGovernmentCenter(false), freeBuildingType(NO_BUILDING)
            {}

            int cityMaintenanceModifierChange;
            int foodKeptPercent;
            int hurryAngerModifier;
            int workerSpeedModifier;
            int globalPopChange;
            int nFreeTechs;
            bool noUnhealthinessFromBuildings, noUnhealthinessFromPopulation, noUnhappiness, startsGoldenAge;
            bool makesCityCapital, isGovernmentCenter;
            BuildingTypes freeBuildingType;
            std::vector<CivicTypes> civicTypes;

            bool operator == (const MiscEffectNode& other) const;
        };

        typedef boost::variant<NullNode, boost::recursive_wrapper<BaseNode>, YieldNode, CommerceNode, 
            TradeNode, BonusNode, SpecialistNode, PowerNode, UnitExpNode, UnitNode, CityDefenceNode, SpecialistSlotNode,
            AreaEffectNode, ReligionNode, HurryNode, MiscEffectNode> BuildingInfoNode;

        struct BaseNode
        {
            BaseNode() : cost(0), productionModifier(0), hurryCostModifier(0), happy(0), health(0) {}
            int cost, productionModifier, hurryCostModifier;
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

    // todo - generalise to multiple conditions?
    struct ConditionalPlotYieldEnchancingBuilding
    {
        explicit ConditionalPlotYieldEnchancingBuilding(BuildingTypes buildingType_ = NO_BUILDING) : buildingType(buildingType_) {}
        BuildingTypes buildingType;
        std::vector<BuildingInfo::BuildCondition> buildConditions;
        std::vector<std::pair<CvPlotFnPtr, PlotYield> > conditionalYieldChanges;
    };
}