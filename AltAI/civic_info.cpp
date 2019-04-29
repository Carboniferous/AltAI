#include "AltAI.h"

#include "./civic_info.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        struct CivicInfoRequestData
        {
            CivicInfoRequestData(CivicTypes civicType_ = NO_CIVIC, PlayerTypes playerType_ = NO_PLAYER) : civicType(civicType_), playerType(playerType_)
            {
            }
            CivicTypes civicType;
            PlayerTypes playerType;
        };

        void getBuildingNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
            {
                BuildingTypes buildingType = getPlayerVersion(requestData.playerType, (BuildingClassTypes)i);
                if (buildingType != NO_BUILDING)
                {
                    int happy = civicInfo.getBuildingHappinessChanges((BuildingClassTypes)i);
                    int health = civicInfo.getBuildingHealthChanges((BuildingClassTypes)i);
                    if (happy != 0 || health != 0)
                    {
                        baseNode.nodes.push_back(CivicInfo::BuildingNode(buildingType, happy, health));
                    }
                }
            }

            for (int i = 0, count = gGlobals.getNumSpecialBuildingInfos(); i < count; ++i)
            {
                if (civicInfo.isSpecialBuildingNotRequired(i))
                {
                    baseNode.nodes.push_back(CivicInfo::BuildingNode(NO_BUILDING, 0, 0, (SpecialBuildingTypes)i));
                }
            }
        }

        void getImprovementNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            std::vector<std::pair<ImprovementTypes, PlotYield> > improvementTypeAndYieldModifiers;

            for (int i = 0, count = gGlobals.getNumImprovementInfos(); i < count; ++i)
            {
                PlotYield plotYield;
                for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
                {
                    plotYield[yieldType] = civicInfo.getImprovementYieldChanges((ImprovementTypes)i, yieldType);
                }

                if (!isEmpty(plotYield))
                {
                    improvementTypeAndYieldModifiers.push_back(std::make_pair((ImprovementTypes)i, plotYield));
                }
            }

            std::vector<std::pair<FeatureTypes, int> > featureTypeAndHappyChanges;

            for (int i = 0, count = gGlobals.getNumFeatureInfos(); i < count; ++i)
            {
                int happyChange = civicInfo.getFeatureHappinessChanges((FeatureTypes)i);
                if (happyChange != 0)
                {
                    featureTypeAndHappyChanges.push_back(std::make_pair((FeatureTypes)i, happyChange));
                }
            }

            int workerSpeedModifier = civicInfo.getWorkerSpeedModifier();

            int improvementUpgradeRateModifier = civicInfo.getImprovementUpgradeRateModifier();

            if (!improvementTypeAndYieldModifiers.empty() || !featureTypeAndHappyChanges.empty() || workerSpeedModifier != 0 || improvementUpgradeRateModifier != 0)
            {
                baseNode.nodes.push_back(CivicInfo::ImprovementNode(improvementTypeAndYieldModifiers, featureTypeAndHappyChanges,
                    workerSpeedModifier, improvementUpgradeRateModifier));
            }
        }

        void getUnitNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            int unitFreeExperience = civicInfo.getFreeExperience();
            int stateReligionFreeExperience = civicInfo.getStateReligionFreeExperience();
            int conscriptCount = civicInfo.getMaxConscript();

            if (unitFreeExperience > 0 || stateReligionFreeExperience > 0 || conscriptCount > 0)
            {
                baseNode.nodes.push_back(CivicInfo::UnitNode(unitFreeExperience, stateReligionFreeExperience, conscriptCount));
            }
        }

        void getYieldNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            int unitProductionModifier = civicInfo.getMilitaryProductionModifier();
            int stateReligionBuildingProductionModifier = civicInfo.getStateReligionBuildingProductionModifier();
            int stateReligionUnitProductionModifier = civicInfo.getStateReligionUnitProductionModifier();
            YieldModifier yieldModifier = civicInfo.getYieldModifierArray(), capitalYieldModifier = civicInfo.getCapitalYieldModifierArray();

            if (unitProductionModifier != 0 || stateReligionBuildingProductionModifier != 0 || stateReligionUnitProductionModifier != 0 ||
                !isEmpty(yieldModifier) || !isEmpty(capitalYieldModifier))
            {
                baseNode.nodes.push_back(CivicInfo::YieldNode(unitProductionModifier, stateReligionBuildingProductionModifier,
                    stateReligionUnitProductionModifier, yieldModifier, capitalYieldModifier));
            }
        }

        void getCommerceNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            CommerceModifier commerceModifier = civicInfo.getCommerceModifierArray();

            Commerce extraSpecialistCommerce = civicInfo.getSpecialistExtraCommerceArray();

            std::vector<SpecialistTypes> validSpecialists;

            for (int i = 0, count = gGlobals.getNumSpecialistInfos(); i < count; ++i)
            {
                if (civicInfo.isSpecialistValid(i))
                {
                    validSpecialists.push_back((SpecialistTypes)i);
                }
            }

            int freeSpecialists = civicInfo.getFreeSpecialist();

            if (!isEmpty(commerceModifier) || !isEmpty(extraSpecialistCommerce) || !validSpecialists.empty() || freeSpecialists > 0)
            {
                baseNode.nodes.push_back(CivicInfo::CommerceNode(commerceModifier, extraSpecialistCommerce, validSpecialists, freeSpecialists));
            }
        }

        void getMaintenanceNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            int distanceModifier = civicInfo.getDistanceMaintenanceModifier();
            int numCitiesModifier = civicInfo.getNumCitiesMaintenanceModifier();
            int corporationModifier = civicInfo.getCorporationMaintenanceModifier();
            int freeUnitsPopulationPercent = civicInfo.getFreeUnitsPopulationPercent();
            int extraGoldPerMilitaryUnit = civicInfo.getGoldPerMilitaryUnit();

            if (distanceModifier != 0 || numCitiesModifier != 0 || corporationModifier != 0 ||
                freeUnitsPopulationPercent != 0 || extraGoldPerMilitaryUnit != 0)
            {
                baseNode.nodes.push_back(CivicInfo::MaintenanceNode(distanceModifier, numCitiesModifier, corporationModifier,
                    freeUnitsPopulationPercent, extraGoldPerMilitaryUnit));
            }
        }

        void getHurryNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumHurryInfos(); i < count; ++i)
            {
                if (civicInfo.isHurry(i))
                {
                    baseNode.nodes.push_back(CivicInfo::HurryNode((HurryTypes)i));
                }
            }
        }

        void getTradeNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            bool noForeignTrade = civicInfo.isNoForeignTrade(), noForeignCorporations = civicInfo.isNoForeignCorporations();
            int extraTradeRoutes = civicInfo.getTradeRoutes();

            if (noForeignTrade || noForeignCorporations || extraTradeRoutes != 0)
            {
                baseNode.nodes.push_back(CivicInfo::TradeNode(noForeignTrade, noForeignCorporations, extraTradeRoutes));
            }
        }

        void getHappyNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            int largestCityHappy = civicInfo.getLargestCityHappiness();
            int happyPerUnit = civicInfo.getHappyPerMilitaryUnit();

            if (largestCityHappy > 0 || happyPerUnit > 0)
            {
                baseNode.nodes.push_back(CivicInfo::HappyNode(largestCityHappy, happyPerUnit));
            }
        }

        void getMiscEffectNode(CivicInfo::BaseNode& baseNode, const CvCivicInfo& civicInfo, const CivicInfoRequestData& requestData)
        {
            int warWearinessModifier = civicInfo.getWarWearinessModifier();
            int civicPercentAnger = civicInfo.getCivicPercentAnger();
            int stateReligionGreatPeopleRateModifier = civicInfo.getStateReligionGreatPeopleRateModifier();

            bool noNonStateReligionSpread = civicInfo.isNoNonStateReligionSpread(), ignoreCorporations = civicInfo.isNoCorporations();

            if (warWearinessModifier != 0 || civicPercentAnger != 0 || stateReligionGreatPeopleRateModifier != 0 || noNonStateReligionSpread || ignoreCorporations)
            {
                baseNode.nodes.push_back(CivicInfo::MiscEffectNode(warWearinessModifier, civicPercentAnger, 
                    stateReligionGreatPeopleRateModifier, noNonStateReligionSpread, ignoreCorporations));
            }
        }

        CivicInfo::BaseNode getBaseNode(const CivicInfoRequestData& requestData)
        {
            CivicInfo::BaseNode node;

            // todo - check player specific features (e.g. buildings) are only added for given player
            const CvCivicInfo& civicInfo = gGlobals.getCivicInfo(requestData.civicType);

            node.health = civicInfo.getExtraHealth();
            node.prereqTech = (TechTypes)civicInfo.getTechPrereq();

            getBuildingNode(node, civicInfo, requestData);
            getImprovementNode(node, civicInfo, requestData);
            getUnitNode(node, civicInfo, requestData);
            getYieldNode(node, civicInfo, requestData);
            getCommerceNode(node, civicInfo, requestData);
            getMaintenanceNode(node, civicInfo, requestData);
            getHurryNode(node, civicInfo, requestData);
            getTradeNode(node, civicInfo, requestData);
            getHappyNode(node, civicInfo, requestData);
            getMiscEffectNode(node, civicInfo, requestData);

            return node;
        }
    }

    CivicInfo::CivicInfo(CivicTypes civicType, PlayerTypes playerType) : civicType_(civicType), playerType_(playerType)
    {
        init_();
    }

    void CivicInfo::init_()
    {
        infoNode_ = getBaseNode(CivicInfoRequestData(civicType_, playerType_));
    }

    const CivicInfo::CivicInfoNode& CivicInfo::getInfo() const
    {
        return infoNode_;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::NullNode& node)
    {
        return os << "(Empty Node) ";
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::BuildingNode& node)
    {
        if (node.buildingType != NO_BUILDING)
        {
            os << " building: " << gGlobals.getBuildingInfo(node.buildingType).getType() << " gives: ";

            if (node.health != 0)
            {
                os << " " << node.health << " health ";

                if (node.happy != 0)
                {
                    os << ", ";
                }
            }

            if (node.happy != 0)
            {
                os << " " << node.happy << " happy ";
            }

            if (node.specialBuildingTypeNotReqd != NO_SPECIALBUILDING)
            {
                os << ", ";
            }
        }

        if (node.specialBuildingTypeNotReqd != NO_SPECIALBUILDING)
        {
            os << gGlobals.getSpecialBuildingInfo(node.specialBuildingTypeNotReqd).getType() << " not required ";
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::ImprovementNode& node)
    {
        for (size_t i = 0, count = node.improvementTypeAndYieldModifiers.size(); i < count; ++i)
        {
            if (i < 0)
            {
                os << ", ";
            }
            else
            {
                os << " ";
            }
            os << gGlobals.getImprovementInfo(node.improvementTypeAndYieldModifiers[i].first).getType() 
               << " yield increase = " << node.improvementTypeAndYieldModifiers[i].second;
        }

        for (size_t i = 0, count = node.featureTypeAndHappyChanges.size(); i < count; ++i)
        {
            if (i < 0)
            {
                os << ", ";
            }
            else
            {
                os << " ";
            }
            os << gGlobals.getFeatureInfo(node.featureTypeAndHappyChanges[i].first).getType()
               << " gives " << node.featureTypeAndHappyChanges[i].second << " happy ";
        }

        if (node.workerSpeedModifier != 0)
        {
            os << " worker speed increase =  " << node.workerSpeedModifier;
        }

        if (node.improvementUpgradeRateModifier != 0)
        {
            os << " improvments upgrade rate modifier = " << node.improvementUpgradeRateModifier;
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::UnitNode& node)
    {
        if (node.freeExperience > 0)
        {
            os << " gives " << node.freeExperience << " free experience ";
        }
        
        if (node.stateReligionFreeExperience > 0)
        {
            os << " gives " << node.stateReligionFreeExperience << " free experience with state religion ";
        }

        if (node.conscriptCount > 0)
        {
            os << " allows conscription of " << node.conscriptCount << " units per turn ";
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::YieldNode& node)
    {
        if (node.unitProductionModifier != 0)
        {
            os << " unit production modifier = " << node.unitProductionModifier;
        }

        if (node.stateReligionBuildingProductionModifier != 0)
        {
            os << " building production modifier with state religion = " << node.stateReligionBuildingProductionModifier;
        }

        if (node.stateReligionUnitProductionModifier != 0)
        {
            os  << " unit production modifier with state religion = " << node.stateReligionUnitProductionModifier;
        }

        if (!isEmpty(node.yieldModifier))
        {
            os << " yield modifier = " << node.yieldModifier;
        }

        if (!isEmpty(node.capitalYieldModifier))
        {
            os << " capital yield modifier = " << node.capitalYieldModifier;
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::CommerceNode& node)
    {
        if (!isEmpty(node.commerceModifier))
        {
            os << " commerce modifier = " << node.commerceModifier;
        }

        if (!isEmpty(node.extraSpecialistCommerce))
        {
            os << " extra specialist commerce = " << node.extraSpecialistCommerce;
        }

        for (size_t i = 0, count = node.validSpecialists.size(); i < count; ++i)
        {
            if (i > 0)
            {
                os << ", ";
            }
            os << " unlimited " << gGlobals.getSpecialistInfo(node.validSpecialists[i]).getType() << " specialists ";
        }

        if (node.freeSpecialists > 0)
        {
            os << " gives: " << node.freeSpecialists << " free specialists ";
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::MaintenanceNode& node)
    {
        if (node.distanceModifier != 0)
        {
            os << " city distance maintenance modifier = " << node.distanceModifier;
        }
        if (node.numCitiesModifier != 0)
        {
            os << " no. cities maintenance modifier = " << node.numCitiesModifier;
        }
        if (node.corporationModifier != 0)
        {
            os << " corporation maintenance modifier = " << node.corporationModifier;
        }
        if (node.freeUnitsPopulationPercent != 0)
        {
            os << " free units as pop modifier = " << node.freeUnitsPopulationPercent;
        }
        if (node.extraGoldPerMilitaryUnit != 0)
        {
            os << " extra gold cost per military unit = " << node.extraGoldPerMilitaryUnit;
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::HurryNode& node)
    {
        return os << " allows " << gGlobals.getHurryInfo(node.hurryType).getType();
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::TradeNode& node)
    {
        if (node.noForeignTrade)
        {
            os << " no foreign trade ";

            if (node.noForeignCorporations)
            {
                os << ", ";
            }
        }

        if (node.noForeignCorporations)
        {
            os << " no foreign corporations ";

            if (node.extraTradeRoutes != 0)
            {
                os << ", ";
            }
        }

        if (node.extraTradeRoutes != 0)
        {
            os << " " << node.extraTradeRoutes << " extra trade route(s) ";
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::HappyNode& node)
    {
        if (node.largestCityHappy > 0)
        {
            os << " largest cities have: " << node.largestCityHappy << " extra happy";

            if (node.happyPerUnit != 0)
            {
                os << ", ";
            }
        }

        if (node.happyPerUnit != 0)
        {
            os << " gives " << node.happyPerUnit << " happy per military unit ";
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::MiscEffectNode& node)
    {
        if (node.warWearinessModifier != 0)
        {
            os << " " << node.warWearinessModifier << " war weariness modifier ";
        }

        if (node.civicPercentAnger != 0)
        {
            os << " " << node.civicPercentAnger << " anger for civs not running this civic ";
        }

        if (node.stateReligionGreatPeopleRateModifier != 0)
        {
            os << " " << node.stateReligionGreatPeopleRateModifier << " great people production modifier ";
        }

        if (node.noNonStateReligionSpread)
        {
            os << " no spread of non-state religion ";
        }

        if (node.ignoreCorporations)
        {
            os << " no corporations ";
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const CivicInfo::BaseNode& node)
    {
        if (node.health > 0)
        {
            os << " extra healthiness = " << node.health;
        }

        if (node.prereqTech != NO_TECH)
        {
            os << " requires tech: " << gGlobals.getTechInfo(node.prereqTech).getType();
        }
        
        for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
        {
            if (i > 0)
            {
                os << ", ";
            }
            os << node.nodes[i];
        }
        return os;
    }
}