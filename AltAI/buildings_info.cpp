#include "AltAI.h"

#include "./buildings_info.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        struct BuildingInfoRequestData
        {
            BuildingInfoRequestData() : buildingType(NO_BUILDING), playerType(NO_PLAYER) {}
            BuildingTypes buildingType;
            PlayerTypes playerType;
        };

        void getYieldNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            // TODO check we can't miss any combinations of arrays here which are permitted in the xml
            PlotYield yieldChange = buildingInfo.getYieldChangeArray();
            YieldModifier yieldModifier = buildingInfo.getYieldModifierArray();
            YieldModifier powerYieldModifier = buildingInfo.getPowerYieldModifierArray();
            int militaryProductionModifier = buildingInfo.getMilitaryProductionModifier();

            if (!isEmpty(yieldChange) || !isEmpty(yieldModifier) || !isEmpty(powerYieldModifier) || militaryProductionModifier != 0)
            {
                BuildingInfo::YieldNode node;
                node.yield = yieldChange;
                node.modifier = yieldModifier;
                node.powerModifier = powerYieldModifier;
                node.militaryProductionModifier = militaryProductionModifier;
                baseNode.nodes.push_back(node);
            }

            yieldChange = buildingInfo.getSeaPlotYieldChangeArray();
            if (!isEmpty(yieldChange))
            {
                BuildingInfo::YieldNode node;
                node.yield = yieldChange;
                node.plotCond = &CvPlot::isWater;
                baseNode.nodes.push_back(node);
            }

            yieldChange = buildingInfo.getGlobalSeaPlotYieldChangeArray();
            if (!isEmpty(yieldChange))
            {
                BuildingInfo::YieldNode node;
                node.yield = yieldChange;
                node.plotCond = &CvPlot::isWater;
                node.global = true;
                baseNode.nodes.push_back(node);
            }

            yieldChange = buildingInfo.getRiverPlotYieldChangeArray();
            if (!isEmpty(yieldChange))
            {
                BuildingInfo::YieldNode node;
                node.yield = yieldChange;
                node.plotCond = &CvPlot::isRiver;
                baseNode.nodes.push_back(node);
            }
        }

        void getCommerceNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            Commerce commerce = buildingInfo.getCommerceChangeArray();
            CommerceModifier modifier = buildingInfo.getCommerceModifierArray();
            Commerce obsoleteSafeCommerce = buildingInfo.getObsoleteSafeCommerceChangeArray();
            Commerce stateReligionCommerce = buildingInfo.getStateReligionCommerceArray();

            if (!isEmpty(commerce) || !isEmpty(obsoleteSafeCommerce) || 
                !isEmpty(stateReligionCommerce) || !isEmpty(modifier))
            {
                BuildingInfo::CommerceNode node;
                node.commerce = commerce;
                node.obsoleteSafeCommerce = obsoleteSafeCommerce;
                node.stateReligionCommerce = stateReligionCommerce;
                if (!isEmpty(node.stateReligionCommerce))
                {
                    node.global = true;
                }
                node.modifier = modifier;
                baseNode.nodes.push_back(node);
            }

            modifier = buildingInfo.getGlobalCommerceModifierArray();
            if (modifier != CommerceModifier())
            {
                BuildingInfo::CommerceNode node;
                node.modifier = modifier;
                node.global = true;
                baseNode.nodes.push_back(node);
            }
        }

        void getTradeNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            int extraTradeRoutes = buildingInfo.getTradeRoutes();
            int extraCoastalTradeRoutes = buildingInfo.getCoastalTradeRoutes();
            int extraGlobalTradeRoutes = buildingInfo.getGlobalTradeRoutes();  // nothing has this in standard game
            int tradeRouteModifier = buildingInfo.getTradeRouteModifier();
            int foreignTradeRouteModifier = buildingInfo.getForeignTradeRouteModifier();

            if (extraTradeRoutes != 0 || extraCoastalTradeRoutes != 0 || extraGlobalTradeRoutes != 0 || tradeRouteModifier != 0 || foreignTradeRouteModifier != 0)
            {
                BuildingInfo::TradeNode node(extraTradeRoutes, extraCoastalTradeRoutes, extraGlobalTradeRoutes, tradeRouteModifier, foreignTradeRouteModifier);
                baseNode.nodes.push_back(node);
            }
        }

        void getSpecialistNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            BuildingInfo::SpecialistNode node;

            node.extraCommerce = buildingInfo.getSpecialistExtraCommerceArray();

            for (int i = 0, count = gGlobals.getNumSpecialistInfos(); i < count; ++i)
            {
                PlotYield yield = buildingInfo.getSpecialistYieldChangeArray(i);
                if (!isEmpty(yield))
                {
                    node.specialistTypesAndYields.push_back(std::make_pair((SpecialistTypes)i, yield));
                }
            }

            node.generatedGPP = std::make_pair(buildingInfo.getGreatPeopleRateChange(), (UnitClassTypes)buildingInfo.getGreatPeopleUnitClass());
            node.cityGPPRateModifier = buildingInfo.getGreatPeopleRateModifier();
            node.playerGPPRateModifier = buildingInfo.getGlobalGreatPeopleRateModifier();

            if (!isEmpty(node.extraCommerce) || !node.specialistTypesAndYields.empty() ||
                node.generatedGPP.first != 0 || node.cityGPPRateModifier != 0 || node.playerGPPRateModifier != 0)
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getSpecialistSlotNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            BuildingInfo::SpecialistSlotNode node;

            for (int i = 0, count = gGlobals.getNumSpecialistInfos(); i < count; ++i)
            {
                int specCount = buildingInfo.getSpecialistCount(i);
                if (specCount > 0)
                {
                    node.specialistTypes.push_back(std::make_pair((SpecialistTypes)i, specCount));
                }

                int freeSpecCount = buildingInfo.getFreeSpecialistCount(i);
                if (freeSpecCount > 0)
                {
                    node.freeSpecialistTypes.push_back(std::make_pair((SpecialistTypes)i, freeSpecCount));
                }
            }

            for (int i = 0, count = gGlobals.getNumImprovementInfos(); i < count; ++i)
            {
                int specCount = buildingInfo.getImprovementFreeSpecialist((ImprovementTypes)i);
                if (specCount > 0)
                {
                    node.improvementFreeSpecialists.push_back(std::make_pair((ImprovementTypes)i, specCount));
                }
            }
            
            if (!node.specialistTypes.empty() || !node.freeSpecialistTypes.empty() || !node.improvementFreeSpecialists.empty())
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getPowerNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            if (buildingInfo.isPower() || buildingInfo.getPowerBonus() != NO_BONUS || buildingInfo.isAreaCleanPower())
            {
                BuildingInfo::PowerNode node;
                node.bonusType = (BonusTypes)buildingInfo.getPowerBonus();
                node.isDirty = buildingInfo.isDirtyPower();
                node.areaCleanPower = buildingInfo.isAreaCleanPower();

                baseNode.nodes.push_back(node);
            }
        }

        void getUnitExpNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            BuildingInfo::UnitExpNode node;

            node.freeExperience = buildingInfo.getFreeExperience();
            node.globalFreeExperience = buildingInfo.getGlobalFreeExperience();

            for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
            {
                int unitCombatFreeExperience = buildingInfo.getUnitCombatFreeExperience(i);
                if (unitCombatFreeExperience != 0)
                {
                    node.combatTypeFreeExperience.push_back(std::make_pair((UnitCombatTypes)i, unitCombatFreeExperience));
                }
            }

            for (int i = 0; i < NUM_DOMAIN_TYPES; ++i)
            {
                int domainFreeExperience = buildingInfo.getDomainFreeExperience(i);
                if (domainFreeExperience != 0)
                {
                    node.domainFreeExperience.push_back(std::make_pair((DomainTypes)i, domainFreeExperience));
                }
                int domainProductionModifier = buildingInfo.getDomainProductionModifier(i);
                if (domainProductionModifier != 0)
                {
                    node.domainProductionModifier.push_back(std::make_pair((DomainTypes)i, domainProductionModifier));
                }
            }

            node.freePromotion = (PromotionTypes)buildingInfo.getFreePromotion();

            if (!isEmpty(node))
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getCityDefenceNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            BuildingInfo::CityDefenceNode node;

            node.defenceBonus = buildingInfo.getDefenseModifier();
            node.globalDefenceBonus = buildingInfo.getAllCityDefenseModifier();
            node.bombardRateModifier = buildingInfo.getBombardDefenseModifier();
            node.espionageDefence = buildingInfo.getEspionageDefenseModifier();
            node.airDefenceModifier = buildingInfo.getAirModifier();
            node.nukeDefenceModifier = buildingInfo.getNukeModifier();
            node.extraAirUnitCapacity = buildingInfo.getAirUnitCapacity();
            node.extraAirLiftCount = buildingInfo.getAirlift();

            if (!isEmpty(node))
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getBonusNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            for (int i = 0, count = gGlobals.getNumBonusInfos(); i < count; ++i)
            {
                BuildingInfo::BonusNode node;
                node.prodModifier = buildingInfo.getBonusProductionModifier(i);
                node.happy = buildingInfo.getBonusHappinessChanges(i);
                node.health = buildingInfo.getBonusHealthChanges(i);

                node.yieldModifier = buildingInfo.getBonusYieldModifierArray(i);

                if (!isEmpty(node))
                {
                    node.bonusType = (BonusTypes)i;
                    baseNode.nodes.push_back(node);
                }
            }

            BonusTypes freeBonus = (BonusTypes)buildingInfo.getFreeBonus();
            if (freeBonus != NO_BONUS)
            {
                BuildingInfo::BonusNode node;
                int numFreeBonuses = buildingInfo.getNumFreeBonuses();
                if (numFreeBonuses == -1)
                {
                    numFreeBonuses = gGlobals.getWorldInfo(gGlobals.getMap().getWorldSize()).getNumFreeBuildingBonuses();
                }
                node.bonusType = freeBonus;
                node.freeBonusCount = numFreeBonuses;
                baseNode.nodes.push_back(node);
            }

            BonusTypes noBonus = (BonusTypes)buildingInfo.getNoBonus();
            if (noBonus != NO_BONUS)
            {
                BuildingInfo::BonusNode node;
                node.bonusType = noBonus;
                node.isRemoved = true;
                baseNode.nodes.push_back(node);
            }
        }

        void getMiscEffectNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo, const BuildingInfoRequestData& requestData)
        {
            BuildingInfo::MiscEffectNode node;

            node.cityMaintenanceModifierChange = buildingInfo.getMaintenanceModifier();
            node.foodKeptPercent = buildingInfo.getFoodKept();
            node.hurryAngerModifier = buildingInfo.getHurryAngerModifier();
            node.workerSpeedModifier = buildingInfo.getWorkerSpeedModifier();
            node.noUnhealthinessFromBuildings = buildingInfo.isBuildingOnlyHealthy();
            node.noUnhealthinessFromPopulation = buildingInfo.isNoUnhealthyPopulation();
            node.noUnhappiness = buildingInfo.isNoUnhappiness();
            node.startsGoldenAge = buildingInfo.isGoldenAge();
            node.globalPopChange = buildingInfo.getGlobalPopulationChange();
            node.makesCityCapital = buildingInfo.isCapital();
            node.isGovernmentCenter = buildingInfo.isGovernmentCenter();
            node.nFreeTechs = buildingInfo.getFreeTechs();

            BuildingClassTypes freeBuildingClass = (BuildingClassTypes)buildingInfo.getFreeBuildingClass();
            if (freeBuildingClass != NO_BUILDINGCLASS)
            {
                node.freeBuildingType = getPlayerVersion(requestData.playerType, freeBuildingClass);
            }

            CivicOptionTypes civicOptionType = (CivicOptionTypes)buildingInfo.getCivicOption();
            if (civicOptionType != NO_CIVICOPTION)
            {
                for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
                {
                    const CvCivicInfo& civicInfo = gGlobals.getCivicInfo((CivicTypes)i);
                    if ((CivicOptionTypes)civicInfo.getCivicOptionType() == civicOptionType)
                    {
                        node.civicTypes.push_back((CivicTypes)i);
                    }
                }
            }

            if (!isEmpty(node))
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getReligionNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            BuildingInfo::ReligionNode node;

            node.prereqReligion = (ReligionTypes)buildingInfo.getPrereqReligion();
            node.religionType = (ReligionTypes)buildingInfo.getReligionType();

            ReligionTypes globalReligionType = (ReligionTypes)buildingInfo.getGlobalReligionCommerce();

            if (globalReligionType != NO_RELIGION)  // shrine
            {
                node.globalReligionType = globalReligionType;
                node.globalCommerceChange = gGlobals.getReligionInfo(globalReligionType).getGlobalReligionCommerceArray();
            }

            if (node.prereqReligion != NO_RELIGION || node.religionType != NO_RELIGION || node.globalReligionType != NO_RELIGION)
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getHurryNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            BuildingInfo::HurryNode node;
            node.hurryAngerModifier = buildingInfo.getHurryAngerModifier();
            node.globalHurryCostModifier = buildingInfo.getGlobalHurryModifier();

            if (node.hurryAngerModifier != 0 || node.globalHurryCostModifier != 0)
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getAreaEffectNode(BuildingInfo::BaseNode& baseNode, const CvBuildingInfo& buildingInfo)
        {
            BuildingInfo::AreaEffectNode node;

            node.areaHealth = buildingInfo.getAreaHealth();
            node.globalHealth = buildingInfo.getGlobalHealth();
            node.areaHappy = buildingInfo.getAreaHappiness();
            node.globalHappy = buildingInfo.getGlobalHappiness();

            if (node.areaHealth != 0 || node.globalHealth != 0 || node.areaHappy != 0 ||
                node.globalHappy != 0)
            {
                baseNode.nodes.push_back(node);
            }
        }

        BuildingInfo::BaseNode getBaseNode(const BuildingInfoRequestData& requestData)
        {
            BuildingInfo::BaseNode node;

            const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(requestData.buildingType);

            node.cost = buildingInfo.getProductionCost();
            const CvPlayer& player = CvPlayerAI::getPlayer(requestData.playerType);
            node.productionModifier = player.getProductionModifier(requestData.buildingType);
            node.hurryCostModifier = buildingInfo.getHurryCostModifier();

            node.happy = buildingInfo.getHappiness();
            node.health = buildingInfo.getHealth();

            TechTypes andTech = (TechTypes)buildingInfo.getPrereqAndTech();

            if (andTech != NO_TECH)
            {
                node.techs.push_back(andTech);
            }

            for (int andIndex = 0, andCount = gGlobals.getNUM_BUILDING_AND_TECH_PREREQS(); andIndex < andCount; ++andIndex)
            {
                TechTypes andTech = (TechTypes)buildingInfo.getPrereqAndTechs(andIndex);
                if (andTech != NO_TECH)
                {
                    node.techs.push_back(andTech);
                }
            }

            SpecialBuildingTypes specialBuildingType = (SpecialBuildingTypes)buildingInfo.getSpecialBuildingType();

            if (specialBuildingType != NO_SPECIALBUILDING)
            {
                TechTypes prereqTech = (TechTypes)gGlobals.getSpecialBuildingInfo(specialBuildingType).getTechPrereq();
                if (prereqTech != NO_TECH)
                {
                    node.techs.push_back(prereqTech);
                }
            }

            int minimumAreaSize = buildingInfo.getMinAreaSize();

            if (buildingInfo.isWater())
            {
                if (buildingInfo.isRiver())
                {
                    BuildingInfo::BuildOrCondition orConditions;
                    orConditions.conditions.push_back(BuildingInfo::IsRiver());
                    orConditions.conditions.push_back(BuildingInfo::MinArea(true, minimumAreaSize));

                    node.buildConditions.push_back(orConditions);
                }
                else
                {
                    node.buildConditions.push_back(BuildingInfo::MinArea(true, minimumAreaSize));
                }
            }
            else if (buildingInfo.isRiver())
            {
                node.buildConditions.push_back(BuildingInfo::IsRiver());
            }

            if (minimumAreaSize > 0 && !buildingInfo.isWater())
            {
                node.buildConditions.push_back(BuildingInfo::MinArea(false, minimumAreaSize));
            }

            BuildingInfo::RequiredBuildings requiredBuildings;
            for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
            {
                if (buildingInfo.isBuildingClassNeededInCity(i))
                {
                    requiredBuildings.cityBuildings.push_back(getPlayerVersion(requestData.playerType, (BuildingClassTypes)i));
                }

                int requiredCount = buildingInfo.getPrereqNumOfBuildingClass(i);
                if (requiredCount > 0)
                {
                    requiredCount *= std::max<int>(0, (gGlobals.getWorldInfo(gGlobals.getMap().getWorldSize()).getBuildingClassPrereqModifier() + 100));
                    requiredCount /= 100;

                    requiredBuildings.buildingCounts.push_back(std::make_pair(getPlayerVersion(requestData.playerType, (BuildingClassTypes)i), requiredCount));
                }
            }

            if (!requiredBuildings.buildingCounts.empty() || !requiredBuildings.cityBuildings.empty())
            {
                node.buildConditions.push_back(requiredBuildings);
            }

            getYieldNode(node, buildingInfo);
            getCommerceNode(node, buildingInfo);
            getTradeNode(node, buildingInfo);
            getBonusNode(node, buildingInfo);
            getPowerNode(node, buildingInfo);
            getSpecialistNode(node, buildingInfo);
            getSpecialistSlotNode(node, buildingInfo);
            getMiscEffectNode(node, buildingInfo, requestData);
            getCityDefenceNode(node, buildingInfo);
            getUnitExpNode(node, buildingInfo);
            getReligionNode(node, buildingInfo);
            getHurryNode(node, buildingInfo);
            getAreaEffectNode(node, buildingInfo);

            return node;
        }
    }

    bool BuildingInfo::BonusNode::operator == (const BuildingInfo::BonusNode& other) const
    {
        return bonusType == other.bonusType &&
            yieldModifier == other.yieldModifier &&
            happy == other.happy &&
            health == other.health &&
            prodModifier == other.prodModifier &&
            freeBonusCount == other.freeBonusCount &&
            isRemoved == other.isRemoved;
    }

    bool BuildingInfo::UnitExpNode::operator == (const BuildingInfo::UnitExpNode& other) const
    {
        bool areEqual = freeExperience == other.freeExperience && 
            globalFreeExperience == other.globalFreeExperience && 
            freePromotion == other.freePromotion &&
            domainFreeExperience.size() == other.domainFreeExperience.size() &&
            domainProductionModifier.size() == other.domainProductionModifier.size() &&
            combatTypeFreeExperience.size() == other.combatTypeFreeExperience.size();

        if (areEqual)
        {
            for (size_t i = 0, count = domainFreeExperience.size(); i < count; ++i)
            {
                if (std::find(other.domainFreeExperience.begin(), other.domainFreeExperience.end(), domainFreeExperience[i]) == other.domainFreeExperience.end())
                {
                    areEqual = false;
                    break;
                }
            }
        }

        if (areEqual)
        {
            for (size_t i = 0, count = domainProductionModifier.size(); i < count; ++i)
            {
                if (std::find(other.domainProductionModifier.begin(), other.domainProductionModifier.end(), domainProductionModifier[i]) == other.domainProductionModifier.end())
                {
                    areEqual = false;
                    break;
                }
            }
        }

        if (areEqual)
        {
            for (size_t i = 0, count = combatTypeFreeExperience.size(); i < count; ++i)
            {
                if (std::find(other.combatTypeFreeExperience.begin(), other.combatTypeFreeExperience.end(), combatTypeFreeExperience[i]) == other.combatTypeFreeExperience.end())
                {
                    areEqual = false;
                    break;
                }
            }
        }
        return areEqual;
    }

    bool BuildingInfo::YieldNode::operator == (const BuildingInfo::YieldNode& other) const
    {
        return yield == other.yield &&
            modifier == other.modifier &&
            powerModifier == other.powerModifier &&
            plotCond == other.plotCond &&
            militaryProductionModifier == other.militaryProductionModifier &&
            global == other.global;
    }

    bool BuildingInfo::CityDefenceNode::operator == (const BuildingInfo::CityDefenceNode& other) const
    {
        return defenceBonus == other.defenceBonus &&
            globalDefenceBonus == other.globalDefenceBonus &&
            bombardRateModifier == other.bombardRateModifier &&
            espionageDefence == other.espionageDefence &&
            airDefenceModifier == other.airDefenceModifier &&
            nukeDefenceModifier == other.nukeDefenceModifier &&
            extraAirUnitCapacity == other.extraAirUnitCapacity &&
            extraAirLiftCount == other.extraAirLiftCount;
    }

    bool BuildingInfo::MiscEffectNode::operator == (const BuildingInfo::MiscEffectNode& other) const
    {
        bool areEqual = cityMaintenanceModifierChange == other.cityMaintenanceModifierChange &&
            foodKeptPercent == other.foodKeptPercent &&
            hurryAngerModifier == other.hurryAngerModifier &&
            workerSpeedModifier == other.workerSpeedModifier &&
            globalPopChange == other.globalPopChange &&
            nFreeTechs == other.nFreeTechs &&
            noUnhealthinessFromBuildings == other.noUnhealthinessFromBuildings &&
            noUnhealthinessFromPopulation == other.noUnhealthinessFromPopulation &&
            noUnhappiness == other.noUnhappiness &&
            startsGoldenAge == other.startsGoldenAge &&
            makesCityCapital == other.makesCityCapital &&
            isGovernmentCenter == other.isGovernmentCenter &&
            freeBuildingType == other.freeBuildingType &&
            civicTypes.size() == other.civicTypes.size();

        if (areEqual)
        {
            for (size_t i = 0, count = civicTypes.size(); i < count; ++i)
            {
                if (std::find(other.civicTypes.begin(), other.civicTypes.end(), civicTypes[i]) == other.civicTypes.end())
                {
                    areEqual = false;
                    break;
                }
            }
        }

        return areEqual;
    }

    BuildingInfo::BuildingInfo(BuildingTypes buildingType) : buildingType_(buildingType), playerType_(NO_PLAYER)
    {
        init_();
    }

    BuildingInfo::BuildingInfo(BuildingTypes buildingType, PlayerTypes playerType) : buildingType_(buildingType), playerType_(playerType)
    {
        init_();
    }

    void BuildingInfo::init_()
    {
        BuildingInfoRequestData requestData;
        requestData.buildingType = buildingType_;
        requestData.playerType = playerType_;

        infoNode_ = getBaseNode(requestData);
    }

    const BuildingInfo::BuildingInfoNode& BuildingInfo::getInfo() const
    {
        return infoNode_;
    }
}