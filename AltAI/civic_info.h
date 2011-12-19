#pragma once

#include "./utils.h"

namespace AltAI
{
    // todo: various maintenance costs not included, great general rate changes and possibly other civic effects not used in standard game
    class CivicInfo
    {
    public:
        CivicInfo(CivicTypes techType, PlayerTypes playerType);

        CivicTypes getCivicType() const
        {
            return civicType_;
        }

        struct NullNode
        {
            NullNode() {}
        };

        struct BaseNode;

        struct BuildingNode
        {
            BuildingNode(BuildingTypes buildingType_ = NO_BUILDING, int happy_ = 0, int health_ = 0, SpecialBuildingTypes specialBuildingTypeNotReqd_ = NO_SPECIALBUILDING)
                : buildingType(buildingType_), happy(happy_), health(health_), specialBuildingTypeNotReqd(specialBuildingTypeNotReqd_)
            {
            }
            BuildingTypes buildingType;
            int happy, health;
            SpecialBuildingTypes specialBuildingTypeNotReqd;
        };

        struct ImprovementNode
        {
            ImprovementNode(const std::vector<std::pair<ImprovementTypes, PlotYield> >& improvementTypeAndYieldModifiers_ = std::vector<std::pair<ImprovementTypes, PlotYield> >(),
                const std::vector<std::pair<FeatureTypes, int> >& featureTypeAndHappyChanges_ = std::vector<std::pair<FeatureTypes, int> >(),
                int workerSpeedModifier_ = 0, int improvementUpgradeRateModifier_ = 0)
                : improvementTypeAndYieldModifiers(improvementTypeAndYieldModifiers_), featureTypeAndHappyChanges(featureTypeAndHappyChanges_), 
                  workerSpeedModifier(workerSpeedModifier_), improvementUpgradeRateModifier(improvementUpgradeRateModifier_)
            {
            }
            std::vector<std::pair<ImprovementTypes, PlotYield> > improvementTypeAndYieldModifiers;
            std::vector<std::pair<FeatureTypes, int> > featureTypeAndHappyChanges;
            int workerSpeedModifier, improvementUpgradeRateModifier;
        };

        struct UnitNode
        {
            UnitNode(int freeExperience_ = 0, int stateReligionFreeExperience_ = 0, int conscriptCount_ = 0)
                : freeExperience(freeExperience_), stateReligionFreeExperience(stateReligionFreeExperience_), conscriptCount(conscriptCount_)
            {
            }
            int freeExperience, stateReligionFreeExperience;
            int conscriptCount;  // todo calc scaled value for map size
        };

        struct YieldNode
        {
            YieldNode(int unitProductionModifier_ = 0, int stateReligionBuildingProductionModifier_ = 0, int stateReligionUnitProductionModifier_ = 0,
                      YieldModifier yieldModifier_ = YieldModifier(), YieldModifier capitalYieldModifier_ = YieldModifier())
                : unitProductionModifier(unitProductionModifier_), stateReligionBuildingProductionModifier(stateReligionBuildingProductionModifier_),
                  stateReligionUnitProductionModifier(stateReligionUnitProductionModifier_), yieldModifier(yieldModifier_), capitalYieldModifier(capitalYieldModifier_)
            {
            }
            int unitProductionModifier, stateReligionBuildingProductionModifier, stateReligionUnitProductionModifier;
            YieldModifier yieldModifier, capitalYieldModifier;
        };

        struct CommerceNode
        {
            CommerceNode(CommerceModifier commerceModifier_, Commerce extraSpecialistCommerce_ = Commerce(),
                         const std::vector<SpecialistTypes>& validSpecialists_ = std::vector<SpecialistTypes>(), int freeSpecialists_ = 0)
                : commerceModifier(commerceModifier_), extraSpecialistCommerce(extraSpecialistCommerce_), validSpecialists(validSpecialists_), freeSpecialists(freeSpecialists_)
            {
            }
            CommerceModifier commerceModifier;
            Commerce extraSpecialistCommerce;
            std::vector<SpecialistTypes> validSpecialists;  // unlimited specialist types
            int freeSpecialists;
        };

        struct MaintenanceNode
        {
            MaintenanceNode(int distanceModifier_ = 0, int numCitiesModifier_ = 0, int corporationModifier_ = 0,
                            int freeUnitsPopulationPercent_ = 0, int extraGoldPerMilitaryUnit_ = 0)
                : distanceModifier(distanceModifier_), numCitiesModifier(numCitiesModifier_), corporationModifier(corporationModifier_),
                  freeUnitsPopulationPercent(freeUnitsPopulationPercent_), extraGoldPerMilitaryUnit(extraGoldPerMilitaryUnit_)
            {
            }
            int distanceModifier, numCitiesModifier, corporationModifier;
            int freeUnitsPopulationPercent, extraGoldPerMilitaryUnit;
            // todo maintenance related changes which are not used in standard game
        };

        struct HurryNode
        {
            HurryNode(HurryTypes hurryType_ = NO_HURRY) : hurryType(hurryType_) {}
            HurryTypes hurryType;
        };

        struct TradeNode
        {
            TradeNode(bool noForeignTrade_ = false, bool noForeignCorporations_ = false, int extraTradeRoutes_ = 0)
                : noForeignTrade(noForeignTrade_), noForeignCorporations(noForeignCorporations_), extraTradeRoutes(extraTradeRoutes_)
            {
            }
            bool noForeignTrade, noForeignCorporations;
            int extraTradeRoutes;
        };

        struct HappyNode
        {
            HappyNode(int largestCityHappy_ = 0, int happyPerUnit_ = 0)
                : largestCityHappy(largestCityHappy_), happyPerUnit(happyPerUnit_)
            {
            }

            int largestCityHappy;
            int happyPerUnit;
        };

        struct MiscEffectNode
        {
            MiscEffectNode(int warWearinessModifier_ = 0, int civicPercentAnger_ = 0, int stateReligionGreatPeopleRateModifier_ = 0,
                bool noNonStateReligionSpread_ = false, bool ignoreCorporations_ = false)
                : warWearinessModifier(warWearinessModifier_), civicPercentAnger(civicPercentAnger_), stateReligionGreatPeopleRateModifier(stateReligionGreatPeopleRateModifier_),
                  noNonStateReligionSpread(noNonStateReligionSpread_), ignoreCorporations(ignoreCorporations_)
            {
            }

            int warWearinessModifier, civicPercentAnger, stateReligionGreatPeopleRateModifier;
            bool noNonStateReligionSpread, ignoreCorporations;
        };

        typedef boost::variant<NullNode, boost::recursive_wrapper<BaseNode>, BuildingNode, ImprovementNode, UnitNode, YieldNode,
            CommerceNode, MaintenanceNode, HurryNode, TradeNode, HappyNode, MiscEffectNode> CivicInfoNode;

        struct BaseNode
        {
            BaseNode() : health(0)
            {
            }
            int health;
            std::vector<CivicInfoNode> nodes;
        };

        const CivicInfoNode& getInfo() const;

    private:
        void init_();

        CivicTypes civicType_;
        PlayerTypes playerType_;
        CivicInfoNode infoNode_;
    };

    std::ostream& operator << (std::ostream& os, const CivicInfo::NullNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::BaseNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::BuildingNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::ImprovementNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::UnitNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::YieldNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::CommerceNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::MaintenanceNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::HurryNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::TradeNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::HappyNode& node);
    std::ostream& operator << (std::ostream& os, const CivicInfo::MiscEffectNode& node);
}