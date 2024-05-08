#pragma once
#include "./utils.h"

namespace AltAI
{
    class UnitInfo
    {
    public:
        UnitInfo(UnitTypes unitType, PlayerTypes playerType);

        UnitTypes getUnitType() const
        {
            return unitType_;
        }

        struct NullNode
        {
            NullNode() {}
        };

        struct BaseNode;

        struct UpgradeNode
        {
            std::vector<UnitClassTypes> upgrades;
        };

        struct CombatNode
        {
            CombatNode() : strength(0), firstStrikes(0), firstStrikeChances(0), moves(0), collateralDamageReduction(0) {}
            int strength, firstStrikes, firstStrikeChances, moves, collateralDamageReduction;
            std::vector<UnitTypes> collateralImmunity;
            std::vector<UnitClassTypes> flankedTypes;
        };

        struct CollateralNode
        {
            CollateralNode() : damage(0), maxDamage(0), maxUnits(0) {}
            int damage, maxDamage, maxUnits; 
        };

        struct CityCombatNode
        {
            CityCombatNode() : extraDefence(0), extraAttack(0) {}
            int extraDefence, extraAttack;
        };

        struct AirCombatNode
        {
            AirCombatNode() : strength(0), range(0) {}
            int strength, range;
        };

        struct CombatBonusNode
        {
            std::vector<std::pair<UnitTypes, int> > bonuses, attackBonuses, defenceBonuses;
        };

        struct CargoNode
        {
            CargoNode() : cargoDomain(NO_DOMAIN), capacity(0) {}
            DomainTypes cargoDomain;
            std::vector<UnitTypes> cargoUnits;
            int capacity;
        };

        struct PromotionsNode
        {
            struct Promotion
            {
                Promotion() :
                    promotionType(NO_PROMOTION), orPromotion1(NO_PROMOTION), orPromotion2(NO_PROMOTION),
                    andPromotion(NO_PROMOTION), techType(NO_TECH), level(-1)
                {
                }

                explicit Promotion(PromotionTypes promotionType_, TechTypes techType_ = NO_TECH) : promotionType(promotionType_), 
                    orPromotion1(NO_PROMOTION), orPromotion2(NO_PROMOTION), andPromotion(NO_PROMOTION), techType(techType_), level(-1)
                {
                }

                PromotionTypes promotionType, orPromotion1, orPromotion2, andPromotion;
                
                TechTypes techType;
                int level;

                bool operator < (const Promotion& other) const
                {
                    return promotionType < other.promotionType;
                }
            };

            std::set<Promotion> promotions;
        };

        struct BuildNode
        {
            std::vector<BuildTypes> buildTypes;
        };

        struct ReligionNode
        {
            ReligionNode() : prereqReligion(NO_RELIGION) {}
            ReligionTypes prereqReligion;
            std::vector<std::pair<ReligionTypes, int> > religionSpreads;
        };

        struct MiscAbilityNode
        {   
            struct DiscoverTech
            {
                DiscoverTech() : baseDiscover(0), multiplier(0)
                {
                }
                int baseDiscover, multiplier;
            };

            struct SpecialBuildings
            {
                SpecialBuildings()
                {
                }
                std::vector<BuildingTypes> buildings;
            };

            struct CreateGreatWork
            {
                CreateGreatWork() : culture(0)
                {
                }
                int culture;
            };

            struct TradeMission
            {
                TradeMission() : baseGold(0), multiplier(0)
                {
                }
                int baseGold, multiplier;
            };

            struct SpyMission
            {
                SpyMission() : espionagePoints(0)
                {
                }
                int espionagePoints;
            };

            struct HurryBuilding
            {
                HurryBuilding() : baseHurry(0), multiplier(0)
                {
                }
                int baseHurry, multiplier;
            };

            MiscAbilityNode() : canFoundCity(false), betterHutResults(false), canDiscoverTech(false), canBuildSpecialBuilding(false),
                canCreateGreatWork(false), canDoTradeMission(false), canDoEspionageMission(false), canHurryBuilding(false)
            {
            }

            bool canFoundCity, betterHutResults, canDiscoverTech, canBuildSpecialBuilding, canCreateGreatWork, canDoTradeMission, canDoEspionageMission, canHurryBuilding;
            DiscoverTech discoverTech;
            SpecialBuildings specialBuildings;
            CreateGreatWork createGreatWork;
            TradeMission tradeMission;
            SpyMission spyMission;
            HurryBuilding hurryBuilding;
            std::vector<SpecialistTypes> settledSpecialists;
        };

        struct CorporationNode
        {
            CorporationNode() : prereqCorporation(NO_CORPORATION) {}
            CorporationTypes prereqCorporation;
            std::vector<std::pair<CorporationTypes, int> > corporationSpreads;
        };

        typedef boost::variant<NullNode, boost::recursive_wrapper<BaseNode>, UpgradeNode, CombatNode, 
            CollateralNode, CityCombatNode, AirCombatNode, CombatBonusNode, CargoNode, PromotionsNode,
            BuildNode, ReligionNode, CorporationNode, MiscAbilityNode> UnitInfoNode;

        struct BaseNode
        {
            explicit BaseNode(UnitTypes unitType_)
                : unitType(unitType_), domainType(NO_DOMAIN), cost(0), minAreaSize(-1), requireAreaBonuses(false),
                  prereqBuildingType(NO_BUILDING), specialUnitType(NO_SPECIALUNIT) {}

            UnitTypes unitType;
            DomainTypes domainType;

            int cost, minAreaSize;
            bool requireAreaBonuses;
            std::vector<BonusTypes> andBonusTypes, orBonusTypes;
            std::vector<TechTypes> techTypes;
            BuildingTypes prereqBuildingType;
            SpecialUnitTypes specialUnitType;
            std::vector<UnitInfoNode> nodes;
        };

        const UnitInfoNode& getInfo() const;

    private:
        void init_();

        UnitTypes unitType_;
        PlayerTypes playerType_;
        UnitInfoNode infoNode_;
    };
}