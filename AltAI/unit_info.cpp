#include "./unit_info.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        static const int MAX_PROMOTION_LEVEL_SEARCH_DEPTH = 10;

        typedef std::map<UnitCombatTypes, std::vector<UnitTypes> > CombatTypesMap;

        struct UnitInfoRequestData
        {
            UnitInfoRequestData(UnitTypes unitType_, PlayerTypes playerType_, const CombatTypesMap& combatTypesMap_)
                : unitType(unitType_), playerType(playerType_), combatTypesMap(combatTypesMap_)
            {
            }
            UnitTypes unitType;
            PlayerTypes playerType;
            CombatTypesMap combatTypesMap;
        };

        void getUpgradeNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::UpgradeNode node;

            for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
        	{
        		if (unitInfo.getUpgradeUnitClass(i))
                {
			        node.upgrades.push_back((UnitClassTypes)i);
                }
            }

            if (!node.upgrades.empty())
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getCombatNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::CombatNode node;

            node.strength = unitInfo.getCombat();
            node.firstStrikes = unitInfo.getFirstStrikes();
            node.firstStrikeChances = unitInfo.getChanceFirstStrikes();
            node.moves = unitInfo.getMoves();

            for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
            {
                int flankingDamage = unitInfo.getFlankingStrikeUnitClass(i);
                if (flankingDamage > 0)
                {
                    node.flankedTypes.push_back((UnitClassTypes)i);
                }
            }

            for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
        	{
                if (unitInfo.getUnitCombatCollateralImmune(i))
                {
                    CombatTypesMap::const_iterator ci(requestData.combatTypesMap.find((UnitCombatTypes)i));
                    if (ci != requestData.combatTypesMap.end())
                    {
                        std::copy(ci->second.begin(), ci->second.end(), std::back_inserter(node.collateralImmunity));
                    }
                }
            }

            if (node.strength > 0)  // not much use in combat otherwise
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getCityCombatNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::CityCombatNode node;

            node.extraDefence = unitInfo.getCityDefenseModifier();
            node.extraAttack = unitInfo.getCityAttackModifier();

            if (node.extraDefence > 0 || node.extraAttack > 0)
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getCollateralNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::CollateralNode node;

            node.damage = unitInfo.getCollateralDamage();
            node.maxDamage = unitInfo.getCollateralDamageLimit();
            node.maxUnits = unitInfo.getCollateralDamageMaxUnits();
            if (node.damage > 0 && node.maxDamage > 0 && node.maxUnits > 0)
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getAirCombatNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::AirCombatNode node;

            node.strength = unitInfo.getAirCombat();
            node.range = unitInfo.getAirRange();
            if (node.strength > 0)
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getCombatBonusNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::CombatBonusNode node;

            for (int i = 0, count = gGlobals.getNumUnitCombatInfos(); i < count; ++i)
		    {
                int unitCombatModifier = unitInfo.getUnitCombatModifier(i);
    		    if (unitCombatModifier > 0)
                {
                    CombatTypesMap::const_iterator ci(requestData.combatTypesMap.find((UnitCombatTypes)i));
                    if (ci != requestData.combatTypesMap.end())
                    {
                        for (size_t j = 0, count = ci->second.size(); j < count; ++j)
                        {
                            node.bonuses.push_back(std::make_pair(ci->second[j], unitCombatModifier));
                        }
                    }
                }
            }

            for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
		    {
                int attackMod = unitInfo.getUnitClassAttackModifier(i);
			    if (attackMod != 0)
                {
                    const CvUnitClassInfo& unitClassInfo = gGlobals.getUnitClassInfo((UnitClassTypes)i);
                    UnitTypes defaultUnitType = (UnitTypes)unitClassInfo.getDefaultUnitIndex();
                    if (defaultUnitType != NO_UNIT)
                    {
                        node.attackBonuses.push_back(std::make_pair(defaultUnitType, attackMod));
                    }
                }

                int defenceMod = unitInfo.getUnitClassDefenseModifier(i);
                if (defenceMod != 0)
                {
                    const CvUnitClassInfo& unitClassInfo = gGlobals.getUnitClassInfo((UnitClassTypes)i);
                    UnitTypes defaultUnitType = (UnitTypes)unitClassInfo.getDefaultUnitIndex();
                    if (defaultUnitType != NO_UNIT)
                    {
                        node.defenceBonuses.push_back(std::make_pair(defaultUnitType, defenceMod));
                    }
                }
            }

            if (!node.bonuses.empty() || !node.attackBonuses.empty() || !node.defenceBonuses.empty())
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getCargoNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::CargoNode node;

            DomainTypes cargoDomain = (DomainTypes)unitInfo.getDomainCargo();
            int capacity = unitInfo.getCargoSpace();
            
            if (cargoDomain != NO_DOMAIN && capacity > 0)
            {
                node.cargoDomain = cargoDomain;

                SpecialUnitTypes specialUnitType = (SpecialUnitTypes)unitInfo.getSpecialCargo();
                if (specialUnitType != NO_SPECIALUNIT)
                {
                    for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
                    {
                        UnitTypes cargoUnitType = getPlayerVersion(requestData.playerType, (UnitClassTypes)i);
                        if (cargoUnitType != NO_UNIT)
                        {
                            const CvUnitInfo& cargoUnitInfo = gGlobals.getUnitInfo(cargoUnitType);
                            SpecialUnitTypes cargoSpecialUnitType = (SpecialUnitTypes)cargoUnitInfo.getSpecialUnitType();
                            if (cargoSpecialUnitType == specialUnitType)
                            {
                                node.cargoUnits.push_back(cargoUnitType);
                            }
                        }
                    }
                }

                node.capacity = capacity;
                baseNode.nodes.push_back(node);
            }
        }

        void getBuildNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::BuildNode node;

            for (int i = 0, count = gGlobals.getNumBuildInfos(); i < count; ++i)
            {
                if (unitInfo.getBuilds(i))
                {
                    node.buildTypes.push_back((BuildTypes)i);
                }
            }

            if (!node.buildTypes.empty())
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getReligionNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::ReligionNode node;

            for (int i = 0, count = gGlobals.getNumReligionInfos(); i < count; ++i)
            {
                int baseValue = unitInfo.getReligionSpreads(i);
                if (baseValue > 0)
                {
                    node.religionSpreads.push_back(std::make_pair((ReligionTypes)i, baseValue));
                }
            }

            if (unitInfo.getPrereqReligion() != NO_RELIGION)
            {
                node.prereqReligion = (ReligionTypes)unitInfo.getPrereqReligion();
            }

            if (!node.religionSpreads.empty() || node.prereqReligion != NO_RELIGION)
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getCorporationNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::CorporationNode node;

            node.prereqCorporation = (CorporationTypes)unitInfo.getPrereqCorporation();

            for (int i = 0, count = gGlobals.getNumCorporationInfos(); i < count; ++i)
            {
                int prob = unitInfo.getCorporationSpreads(i);
                if (prob > 0)
                {
                    node.corporationSpreads.push_back(std::make_pair((CorporationTypes)i, prob));
                }
            }

            if (node.prereqCorporation != NO_CORPORATION || !node.corporationSpreads.empty())
            {
                baseNode.nodes.push_back(node);
            }
        }

        void getMiscNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::MiscAbilityNode node;

            node.canFoundCity = unitInfo.isFound();
            node.betterHutResults = unitInfo.isNoBadGoodies();

            if (node.canFoundCity || node.betterHutResults)
            {
                baseNode.nodes.push_back(node);
            }
        }

        bool isForbiddenForDefensiveUnits(const CvPromotionInfo& promotionInfo)
        {
		    return promotionInfo.getCityAttackPercent() != 0 ||
			       promotionInfo.getWithdrawalChange() != 0 ||
			       promotionInfo.getCollateralDamageChange() != 0 ||
			       promotionInfo.isBlitz() ||
			       promotionInfo.isAmphib() ||
			       promotionInfo.isRiver() ||
			       promotionInfo.getHillsAttackPercent() != 0;
        }

        typedef std::set<UnitInfo::PromotionsNode::Promotion> PromotionsSet;

        void getLevelPromotionsNode(UnitInfo::PromotionsNode& node, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData, int level)
        {
            UnitCombatTypes unitCombatType = (UnitCombatTypes)unitInfo.getUnitCombatType();

            if (unitCombatType == NO_UNITCOMBAT || level > MAX_PROMOTION_LEVEL_SEARCH_DEPTH)
            {
                return;
            }

            PromotionsSet promotions;

            for (int i = 0, count = gGlobals.getNumPromotionInfos(); i < count; ++i)
            {
                const CvPromotionInfo& promotionInfo = gGlobals.getPromotionInfo((PromotionTypes)i);
                UnitInfo::PromotionsNode::Promotion promotion((PromotionTypes)i);

                if (promotionInfo.getUnitCombat(unitCombatType) && !promotionInfo.isLeader() && 
                    (unitInfo.isOnlyDefensive() ? !isForbiddenForDefensiveUnits(promotionInfo) : true) &&
                    node.promotions.find(promotion) == node.promotions.end())
                {
                    promotion.andPromotion = (PromotionTypes)promotionInfo.getPrereqPromotion();

                    if (promotion.andPromotion == NO_PROMOTION || node.promotions.find(UnitInfo::PromotionsNode::Promotion(promotion.andPromotion)) != node.promotions.end())
                    {
                        promotion.orPromotion1 = (PromotionTypes)promotionInfo.getPrereqOrPromotion1();
                        promotion.orPromotion2 = (PromotionTypes)promotionInfo.getPrereqOrPromotion2();

                        bool hasPrereqs = promotion.orPromotion1 == NO_PROMOTION && promotion.orPromotion2 == NO_PROMOTION;
                        if (!hasPrereqs)
                        {
                            hasPrereqs = promotion.orPromotion1 != NO_PROMOTION &&
                                node.promotions.find(UnitInfo::PromotionsNode::Promotion(promotion.orPromotion1)) != node.promotions.end();
                        }
                        if (!hasPrereqs)
                        {
                            hasPrereqs = promotion.orPromotion2 != NO_PROMOTION &&
                                node.promotions.find(UnitInfo::PromotionsNode::Promotion(promotion.orPromotion2)) != node.promotions.end();
                        }

                        if (hasPrereqs)
                        {
                            promotion.techType = (TechTypes)promotionInfo.getTechPrereq();
                            promotion.level = level;
                            promotions.insert(promotion);
                        }
                    }
                }
            }

            node.promotions.insert(promotions.begin(), promotions.end());

            if (!promotions.empty())
            {
                getLevelPromotionsNode(node, unitInfo, requestData, level + 1);
            }
        }

        void getPromotionsNode(UnitInfo::BaseNode& baseNode, const CvUnitInfo& unitInfo, const UnitInfoRequestData& requestData)
        {
            UnitInfo::PromotionsNode node;

            const CvPlayerAI& player = CvPlayerAI::getPlayer(requestData.playerType);

            PromotionsSet freePromotions;
            for (int i = 0, count = gGlobals.getNumPromotionInfos(); i < count; ++i)
            {
                if (unitInfo.getFreePromotions(i))
                {
                    UnitInfo::PromotionsNode::Promotion promotion((PromotionTypes)i);
                    promotion.level = 0;
                    freePromotions.insert(promotion);
                }
            }

            for (int i = 0, count = gGlobals.getNumTraitInfos(); i < count; ++i)
            {
                if (player.hasTrait((TraitTypes)i))
                {
                    const CvTraitInfo& traitInfo = gGlobals.getTraitInfo((TraitTypes)i);

                    for (int j = 0, count = gGlobals.getNumPromotionInfos(); j < count; ++j)
                    {
                        if (traitInfo.isFreePromotion(j))
                        {
                            if (unitInfo.getUnitCombatType() != NO_UNITCOMBAT &&
                                traitInfo.isFreePromotionUnitCombat(unitInfo.getUnitCombatType()))
                            {
                                UnitInfo::PromotionsNode::Promotion promotion((PromotionTypes)j);
                                promotion.level = 0;

                                freePromotions.insert(promotion);
                            }
                        }
                    }
                }
            }

            node.promotions.insert(freePromotions.begin(), freePromotions.end());

            getLevelPromotionsNode(node, unitInfo, requestData, 1);

            baseNode.nodes.push_back(node);
        }

        UnitInfo::BaseNode getBaseNode(const UnitInfoRequestData& requestData)
        {
            UnitInfo::BaseNode node(requestData.unitType);
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(requestData.unitType);
            
            node.cost = CvPlayerAI::getPlayer(requestData.playerType).getProductionNeeded(requestData.unitType);

            node.domainType = (DomainTypes)unitInfo.getDomainType();

            BonusTypes bonusType = (BonusTypes)unitInfo.getPrereqAndBonus();
            if (bonusType != NO_BONUS)
            {
                node.andBonusTypes.push_back(bonusType);
            }

            for (int i = 0, count = gGlobals.getNUM_UNIT_PREREQ_OR_BONUSES(); i < count; ++i)
		    {
			    BonusTypes bonusType = (BonusTypes)unitInfo.getPrereqOrBonuses(i);
                if (bonusType != NO_BONUS)
                {
                    node.orBonusTypes.push_back(bonusType);
                }
    		}

            if (node.andBonusTypes.size() == 1 && node.orBonusTypes.size() == 1)
            {
                node.andBonusTypes.push_back(node.orBonusTypes[0]);
                node.orBonusTypes.clear();
            }

            TechTypes techType = (TechTypes)unitInfo.getPrereqAndTech();

            if (techType != NO_TECH)
            {
                node.techTypes.push_back(techType);
            }

            for (int i = 0, count = gGlobals.getNUM_UNIT_AND_TECH_PREREQS(); i < count; ++i)
            {
                techType = (TechTypes)unitInfo.getPrereqAndTechs(i);
                if (techType != NO_TECH)
                {
                    node.techTypes.push_back(techType);
                }
            }

            node.prereqBuildingType = (BuildingTypes)unitInfo.getPrereqBuilding();

            node.specialUnitType = (SpecialUnitTypes)unitInfo.getSpecialUnitType();

            getPromotionsNode(node, unitInfo, requestData);
            getCombatNode(node, unitInfo, requestData);
            getCollateralNode(node, unitInfo, requestData);
            getCityCombatNode(node, unitInfo, requestData);
            getAirCombatNode(node, unitInfo, requestData);
            getCombatBonusNode(node, unitInfo, requestData);
            getUpgradeNode(node, unitInfo, requestData);
            getCargoNode(node, unitInfo, requestData);
            getBuildNode(node, unitInfo, requestData);
            getReligionNode(node, unitInfo, requestData);
            getCorporationNode(node, unitInfo, requestData);
            getMiscNode(node, unitInfo, requestData);

            return node;
        }
    }

    UnitInfo::UnitInfo(UnitTypes unitType, PlayerTypes playerType) : unitType_(unitType), playerType_(playerType)
    {
        init_();
    }

    void UnitInfo::init_()
    {
        // TODO - move to helper_fns.h
        CombatTypesMap combatTypesMap;
        for (int i = 0, count = gGlobals.getNumUnitInfos(); i < count; ++i)
        {
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo((UnitTypes)i);
            UnitCombatTypes combatType = (UnitCombatTypes)unitInfo.getUnitCombatType();
            
            if (combatType != NO_UNITCOMBAT)
            {
                combatTypesMap[combatType].push_back((UnitTypes)i);
            }
        }

        infoNode_ = getBaseNode(UnitInfoRequestData(unitType_, playerType_, combatTypesMap));
    }

    const UnitInfo::UnitInfoNode& UnitInfo::getInfo() const
    {
        return infoNode_;
    }
}