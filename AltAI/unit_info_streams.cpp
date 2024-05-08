#include "AltAI.h"

#include "./unit_info_streams.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const UnitInfo::NullNode& node)
    {
        return os << "(Empty Node) ";
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::UpgradeNode& node)
    {
        os << "\n\t upgrades: ";
        for (size_t i = 0, count = node.upgrades.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getUnitClassInfo(node.upgrades[i]).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::CargoNode& node)
    {
        os << "\n\t cargo = " << node.capacity << " ";
        if (node.cargoUnits.empty())
        {
            os << gGlobals.getDomainInfo(node.cargoDomain).getType() << " units ";
        }
        else
        {
            for (size_t i = 0, count = node.cargoUnits.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << gGlobals.getUnitInfo(node.cargoUnits[i]).getType();
            }
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::CombatNode& node)
    {
        os << "\n\t strength = " << node.strength << " moves = " << node.moves;
        if (node.firstStrikes > 0)
        {
            os << ", first strikes = " << node.firstStrikes;
        }
        if (node.firstStrikeChances > 0)
        {
            os << ", first strike chances = " << node.firstStrikeChances;
        }
        for (size_t i = 0, count = node.collateralImmunity.size(); i < count; ++i)
        {
            if (i == 0) os << " immune to collateral from: ";
            else os << ", ";
            os << gGlobals.getUnitInfo(node.collateralImmunity[i]).getType();
        }
        for (size_t i = 0, count = node.flankedTypes.size(); i < count; ++i)
        {
            if (i == 0) os << " flanking damage to: ";
            else os << ", ";
            os << gGlobals.getUnitClassInfo(node.flankedTypes[i]).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::AirCombatNode& node)
    {
        return os << "\n\t strength = " << node.strength << ", range = " << node.range;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::CityCombatNode& node)
    {
        os << "\n\t";
        if (node.extraDefence > 0)
        {
            os << " extra city defence = " << node.extraDefence;
        }
        if (node.extraAttack > 0)
        {
            os << " extra city attack = " << node.extraAttack;
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::CombatBonusNode& node)
    {
        os << "\n\t";
        for (size_t i = 0, count = node.bonuses.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << " +" << node.bonuses[i].second << "% v. " << gGlobals.getUnitInfo(node.bonuses[i].first).getType();
        }

        for (size_t i = 0, count = node.attackBonuses.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << " +" << node.attackBonuses[i].second << "% attack v. " << gGlobals.getUnitInfo(node.attackBonuses[i].first).getType();
        }

        for (size_t i = 0, count = node.defenceBonuses.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << " +" << node.defenceBonuses[i].second << "% defence v. " << gGlobals.getUnitInfo(node.defenceBonuses[i].first).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::CollateralNode& node)
    {
        return os << "\n\t collateral damage = " << node.damage << ", max = " << node.maxDamage << ", units = " << node.maxUnits;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::PromotionsNode& node)
    {
        for (std::set<UnitInfo::PromotionsNode::Promotion>::const_iterator ci(node.promotions.begin()), ciEnd(node.promotions.end()); ci != ciEnd; ++ci)
        {
            os << "\n level = " << ci->level;
            os << "\n\t" << gGlobals.getPromotionInfo(ci->promotionType).getType();

            if (ci->techType != NO_TECH)
            {
                os << " tech = " << gGlobals.getTechInfo(ci->techType).getType();
            }

            bool andReq = ci->andPromotion != NO_PROMOTION;
            if (andReq)
            {
                os << " reqs: " << gGlobals.getPromotionInfo(ci->andPromotion).getType();
            }

            bool orReq1 = ci->orPromotion1 != NO_PROMOTION, orReq2 = ci->orPromotion2 != NO_PROMOTION;
            if (orReq1 || orReq2)
            {
                if (andReq)
                {
                    os << " and ";
                }
                else
                {
                    os << " reqs: ";
                }
                if (orReq1)
                {
                    os << gGlobals.getPromotionInfo(ci->orPromotion1).getType();
                }
                if (orReq2)
                {
                    os << " or " << gGlobals.getPromotionInfo(ci->orPromotion2).getType();
                }
            }
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::BuildNode& node)
    {
        for (size_t i = 0, count = node.buildTypes.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            else os << " can build: ";
            os << gGlobals.getBuildInfo(node.buildTypes[i]).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::ReligionNode& node)
    {
        os << "\n\t can spread: ";
        for (size_t i = 0, count = node.religionSpreads.size(); i < count; ++i)
        {
            os << gGlobals.getReligionInfo(node.religionSpreads[i].first).getType() << " prob = " << node.religionSpreads[i].second;
        }

        if (node.prereqReligion != NO_RELIGION)
        {
            os << " requires religion: " << gGlobals.getReligionInfo(node.prereqReligion).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::CorporationNode& node)
    {
        os << "\n\t can spread: ";
        for (size_t i = 0, count = node.corporationSpreads.size(); i < count; ++i)
        {
            os << gGlobals.getCorporationInfo(node.corporationSpreads[i].first).getType() << " prob = " << node.corporationSpreads[i].second;
        }

        if (node.prereqCorporation != NO_CORPORATION)
        {
            os << " requires corporation: " << gGlobals.getCorporationInfo(node.prereqCorporation).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::MiscAbilityNode& node)
    {
        if (node.canFoundCity)
        {
            os << " can found city ";
        }
        if (node.betterHutResults)
        {
            os << " only good hut results ";
        }
        if (node.canDiscoverTech)
        {
            os << " can research tech, base value = " << node.discoverTech.baseDiscover << ", multiplier = " << node.discoverTech.multiplier;
        }
        if (node.canBuildSpecialBuilding)
        {
            os << " can build: ";
            for (size_t i = 0, count = node.specialBuildings.buildings.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << gGlobals.getBuildingInfo(node.specialBuildings.buildings[i]).getType();
            }
        }
        if (node.canCreateGreatWork)
        {
            os << " can create great work, culture = " << node.createGreatWork.culture;
        }
        if (node.canDoTradeMission)
        {
            os << " can do trade mission, base value = " << node.tradeMission.baseGold << ", multiplier = " << node.tradeMission.multiplier;
        }
        if (node.canDoEspionageMission)
        {
            os << " can do spy mission, base value = " << node.spyMission.espionagePoints;
        }
        if (node.canHurryBuilding)
        {
            os << " can hurry building, base value = " <<  node.hurryBuilding.baseHurry << ", multiplier = " << node.hurryBuilding.multiplier;
        }
        for (size_t i = 0, count = node.settledSpecialists.size(); i < count; ++i)
        {
            os << " can create specialist: " << gGlobals.getSpecialistInfo(node.settledSpecialists[i]).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const UnitInfo::BaseNode& node)
    {
        os << " domain = " << gGlobals.getDomainInfo(node.domainType).getType();
        os << " cost = " << node.cost;

        if (node.minAreaSize != -1)
        {
            os << " min area size = " << node.minAreaSize;
        }
        if (node.requireAreaBonuses)
        {
            os << " requires bonuses in area to build";
        }

        for (size_t i = 0, count = node.andBonusTypes.size(); i < count; ++i)
        {
            if (i > 0) os << " and ";
            os << " requires& " << gGlobals.getBonusInfo(node.andBonusTypes[i]).getType();
        }

        for (size_t i = 0, count = node.orBonusTypes.size(); i < count; ++i)
        {
            if (i > 0) os << " or ";
            os << " requires| " << gGlobals.getBonusInfo(node.orBonusTypes[i]).getType();
        }

        for (size_t i = 0, count = node.techTypes.size(); i < count; ++i)
        {
            if (i == 0)
            {
                os << " requires: ";
            }
            else
            {
                os << ", ";
            }
            os << gGlobals.getTechInfo(node.techTypes[i]).getType();
        }

        if (node.prereqBuildingType != NO_BUILDING)
        {
            os << " requires: " << gGlobals.getBuildingInfo(node.prereqBuildingType).getType() << " ";
        }

        if (node.specialUnitType != NO_SPECIALUNIT)
        {
            os << " requires: " << gGlobals.getSpecialUnitInfo(node.specialUnitType).getType() << " ";
        }

        for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
        {
            os << node.nodes[i];
        }
        return os;
    }
}
