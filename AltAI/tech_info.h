#pragma once

#include "./utils.h"

namespace AltAI
{
    class TechInfo
    {
    public:
        TechInfo(TechTypes techType, PlayerTypes playerType);

        TechTypes getTechType() const
        {
            return techType_;
        }

        // todo - route speed increases (engineering)
        struct NullNode
        {
            NullNode() {}
        };

        struct BaseNode;

        struct BuildingNode
        {
            BuildingNode(BuildingTypes buildingType_ = NO_BUILDING, bool obsoletes_ = false)
                : buildingType(buildingType_), obsoletes(obsoletes_)
            {
            }
            BuildingTypes buildingType;
            bool obsoletes;
        };

        struct ImprovementNode
        {
            explicit ImprovementNode(ImprovementTypes improvementType_ = NO_IMPROVEMENT, bool allowsImprovement_ = false, 
                FeatureTypes removeFeatureType_ = NO_FEATURE, PlotYield modifier_ = PlotYield(), int workerSpeedModifier_ = 0)
                : improvementType(improvementType_), allowsImprovement(allowsImprovement_), removeFeatureType(removeFeatureType_),
                  modifier(modifier_), workerSpeedModifier(workerSpeedModifier_)
            {
            }
            ImprovementTypes improvementType;
            bool allowsImprovement;
            FeatureTypes removeFeatureType;
            PlotYield modifier;
            int workerSpeedModifier;
        };

        struct CivicNode
        {
            explicit CivicNode(CivicTypes civicType_ = NO_CIVIC) : civicType(civicType_) {}
            CivicTypes civicType;
        };

        struct UnitNode
        {
            explicit UnitNode(::UnitTypes unitType_ = NO_UNIT) : unitType(unitType_) {}
            ::UnitTypes unitType;
            std::vector<PromotionTypes> promotions;
            std::vector<std::pair<DomainTypes, int> > domainExtraMoves;
        };

        struct BonusNode
        {
            explicit BonusNode(BonusTypes revealBonus_ = NO_BONUS, BonusTypes tradeBonus_ = NO_BONUS, BonusTypes obsoleteBonus_ = NO_BONUS)
                : revealBonus(revealBonus_), tradeBonus(tradeBonus_), obsoleteBonus(obsoleteBonus_)
            {
            }
            BonusTypes revealBonus, tradeBonus, obsoleteBonus;
        };

        struct CommerceNode
        {
            explicit CommerceNode(CommerceTypes adjustableCommerceType_ = NO_COMMERCE) : adjustableCommerceType(adjustableCommerceType_) {}
            CommerceTypes adjustableCommerceType;
        };

        struct ProcessNode
        {
            ProcessNode(ProcessTypes processType_, CommerceModifier productionToCommerceModifier_)
                : processType(processType_), productionToCommerceModifier(productionToCommerceModifier_) {}
            ProcessTypes processType;
            CommerceModifier productionToCommerceModifier;
        };

        struct RouteNode
        {
            explicit RouteNode(RouteTypes routeType_ = NO_ROUTE) : routeType(routeType_) {}
            RouteTypes routeType;
        };

        struct TradeNode
        {
            explicit TradeNode(TerrainTypes terrainType_ = NO_TERRAIN, bool isRiverTrade_ = false, int extraTradeRoutes_ = 0)
                : terrainType(terrainType_), isRiverTrade(isRiverTrade_), extraTradeRoutes(extraTradeRoutes_)
            {
            }
            TerrainTypes terrainType;
            bool isRiverTrade;
            int extraTradeRoutes;
        };

        struct FirstToNode
        {
            explicit FirstToNode(int freeTechCount_ = 0, bool foundReligion_ = false, ReligionTypes defaultReligionType_ = NO_RELIGION,
                                 UnitClassTypes freeUnitClass_ = NO_UNITCLASS)
                : freeTechCount(freeTechCount_), foundReligion(foundReligion_), defaultReligionType(defaultReligionType_), freeUnitClass(freeUnitClass_)
            {
            }
            int freeTechCount;
            bool foundReligion;
            ReligionTypes defaultReligionType;
            UnitClassTypes freeUnitClass;
        };

        struct MiscEffectNode
        {
            explicit MiscEffectNode(bool enablesOpenBorders_ = false, bool enablesTechTrading_ = false, bool enablesGoldTrading_ = false, bool enablesMapTrading_ = false,
                bool enablesWaterWork_ = false, bool ignoreIrrigation_ = false, bool carriesIrrigation_ = false, bool extraWaterSight_ = false, bool centresMap_ = false,
                bool enablesBridgeBuilding_ = false, bool enablesDefensivePacts_ = false, bool enablesPermanentAlliances_ = false)
                : enablesOpenBorders(enablesOpenBorders_), enablesTechTrading(enablesTechTrading_), enablesGoldTrading(enablesGoldTrading_), enablesMapTrading(enablesMapTrading_),
                  enablesWaterWork(enablesWaterWork_), ignoreIrrigation(ignoreIrrigation_), carriesIrrigation(carriesIrrigation_),
                  extraWaterSight(extraWaterSight_), centresMap(centresMap_), enablesBridgeBuilding(enablesBridgeBuilding_),
                  enablesDefensivePacts(enablesDefensivePacts_), enablesPermanentAlliances(enablesPermanentAlliances_)
            {
            }

            bool enablesOpenBorders, enablesTechTrading, enablesGoldTrading, enablesMapTrading;
            bool enablesWaterWork, ignoreIrrigation, carriesIrrigation;
            bool extraWaterSight, centresMap;
            bool enablesBridgeBuilding;
            bool enablesDefensivePacts, enablesPermanentAlliances;
        };

        typedef boost::variant<NullNode, boost::recursive_wrapper<BaseNode>, BuildingNode, ImprovementNode, CivicNode, UnitNode, BonusNode,
            CommerceNode, ProcessNode, RouteNode, TradeNode, FirstToNode, MiscEffectNode> TechInfoNode;

        struct BaseNode
        {
            BaseNode() : tech(NO_TECH), happy(0), health(0)
            {
            }
            TechTypes tech;
            std::vector<TechTypes> orTechs, andTechs;
            int happy, health;
            std::vector<TechInfoNode> nodes;
        };

        const TechInfoNode& getInfo() const;

    private:
        void init_();

        TechTypes techType_;
        PlayerTypes playerType_;
        TechInfoNode infoNode_;
    };
}