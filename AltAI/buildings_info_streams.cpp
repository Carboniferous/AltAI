#include "AltAI.h"

#include "./buildings_info_streams.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const BuildingInfo::NullNode& node)
    {
        return os << "(Empty Node) ";
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::RequiredBuildings& node)
    {
        for (size_t i = 0, count = node.buildingCounts.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            else os << " requires: ";
            os << node.buildingCounts[i].second << " " << gGlobals.getBuildingInfo(node.buildingCounts[i].first).getType();
        }

        for (size_t i = 0, count = node.cityBuildings.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            else os << " requires: ";
            os << gGlobals.getBuildingInfo(node.cityBuildings[i]).getType();
        }

        if (!node.cityBuildings.empty()) os << " in city ";

        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::IsRiver& node)
    {
        return os << "requires river ";
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::MinArea& node)
    {
        return os << (node.isWater ? "requires water, " : "") << " min area size = " << node.minimumSize;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::IsHolyCity& node)
    {
        return os << "requires city to have founded religion: " << gGlobals.getReligionInfo(node.religionType).getType();
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::BuildOrCondition& node)
    {
        for (size_t i = 0, count = node.conditions.size(); i < count; ++i)
        {
            if (i == 0) os << "\n\t";
            else os << " OR ";
            os << node.conditions[i];
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::BaseNode& node)
    {
        os << " cost = " << node.cost;
        for (size_t i = 0, count = node.buildConditions.size(); i < count; ++i)
        {
            if (i == 0) os << " build conditions:";
            else os << " AND ";
            os << node.buildConditions[i];
        }

        if (node.happy != 0)
        {
            os << " base happy = " << node.happy;
        }
        if (node.health != 0)
        {
            os << " base health = " << node.health;
        }

        if (node.productionModifier != 0)
        {
            ShowPos showpos(os);
            os << " production modifier = " << node.productionModifier << '%';
        }
        if (node.hurryCostModifier != 0)
        {
            ShowPos showpos(os);
            os << " hurry cost modifier change = " << node.hurryCostModifier;
        }

        for (size_t i = 0, count = node.techs.size(); i < count; ++i)
        {
            if (i == 0)
            {
                os << " requires: ";
            }
            else
            {
                os << ", ";
            }
            os << gGlobals.getTechInfo(node.techs[i]).getType();
        }

        for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
        {
            os << node.nodes[i];
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::YieldNode& node)
    {
        os << "\n\t" << (node.global ? "global" : "");
        if (!isEmpty(node.modifier))
        {
            os << " yield modifier = " << node.modifier;
        }
        if (!isEmpty(node.powerModifier))
        {
            os << " power yield modifier = " << node.powerModifier;
        }
        if (!isEmpty(node.yield))
        {
            os << " extra plot yield = " << node.yield;
        }
        if (node.militaryProductionModifier != 0)
        {
            os << " military production modifier = " << node.militaryProductionModifier;
        }
        if (node.plotCond == &CvPlot::isWater)
        {
            os << " for water plots";
        }
        else if (node.plotCond == &CvPlot::isRiver)
        {
            os << " for riverside plots";
        }
        else if (node.plotCond)
        {
            os << " (conditional)";
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::SpecialistNode& node)
    {
        os << "\n\t";
        if (!isEmpty(node.extraCommerce))
        {
            os << " extra specialist commerce = " << node.extraCommerce << " ";
        }
        for (size_t i = 0, count = node.specialistTypesAndYields.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << gGlobals.getSpecialistInfo(node.specialistTypesAndYields[i].first).getType() << " extra yield = " << node.specialistTypesAndYields[i].second;
        }
        if (node.generatedGPP.first != 0)
        {
            ShowPos showpos(os);
            os << node.generatedGPP.first << " " << (node.generatedGPP.second != NO_UNITCLASS ? gGlobals.getUnitClassInfo(node.generatedGPP.second).getType() : "NO_UNITCLASS") << " great people points";
        }
        if (node.cityGPPRateModifier != 0)
        {
            ShowPos showpos(os);
            os << " " << node.cityGPPRateModifier << "% city great people rate ";
        }
        if (node.playerGPPRateModifier != 0)
        {
            ShowPos showpos(os);
            os << " " << node.playerGPPRateModifier << "% civ great people rate ";
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::CommerceNode& node)
    {
        os << "\n\t" << (node.global ? "global" : "");
        if (!isEmpty(node.commerce))
        {
            os << " commerce = " << node.commerce;
        }
        if (!isEmpty(node.obsoleteSafeCommerce))
        {
            os << " obsolete safe commerce = " << node.obsoleteSafeCommerce;
        }
        if (!isEmpty(node.stateReligionCommerce))
        {
            os << " state religion commerce = " << node.stateReligionCommerce;
        }
        if (!isEmpty(node.modifier))
        {
            os << " commerce modifier = " << node.modifier;
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::TradeNode& node)
    {
        os << "\n\t ";
        if (node.extraTradeRoutes != 0)
        {
            ShowPos showpos(os);
            os << node.extraTradeRoutes << " extra trade route" << (node.extraTradeRoutes == 1 ? "" : "s");
        }
        if (node.extraCoastalTradeRoutes != 0)
        {
            ShowPos showpos(os);
            os << node.extraCoastalTradeRoutes << " extra coastal city trade route" << (node.extraCoastalTradeRoutes == 1 ? "" : "s");
        }
        if (node.extraGlobalTradeRoutes != 0)
        {
            ShowPos showpos(os);
            os << node.extraGlobalTradeRoutes << " extra trade route" << (node.extraGlobalTradeRoutes == 1 ? "" : "s") << " globally";
        }
        if (node.tradeRouteModifier != 0)
        {
            ShowPos showpos(os);
            os << " trade route modifier = " << node.tradeRouteModifier << '%';
        }
        if (node.foreignTradeRouteModifier != 0)
        {
            ShowPos showpos(os);
            os << " foreign trade route modifier = " << node.foreignTradeRouteModifier << '%';
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::PowerNode& node)
    {
        os << "\n\tprovides" << (node.isDirty ? " dirty" : " clean") << " power";
        if (node.areaCleanPower)
        {
            os << " for all cities in area ";
        }
        if (node.bonusType != NO_BONUS)
        {
            os << " requires: " << gGlobals.getBonusInfo(node.bonusType).getType();
        }        
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::UnitExpNode& node)
    {
        ShowPos showpos(os);
        os << "\n\t";
        if (node.freeExperience != 0)
        {
            os << " gives " << node.freeExperience << " free experience ";
        }
        if (node.globalFreeExperience != 0)
        {
            os << " gives " << node.globalFreeExperience << " free experience globally ";
        }
        for (size_t i = 0, count = node.domainFreeExperience.size(); i < count; ++i)
        {
            if (i > 0) os << ", ";
            os << " gives " << node.domainFreeExperience[i].second << " free experience to units with domain: " << gGlobals.getDomainInfo(node.domainFreeExperience[i].first).getType();
        }
        for (size_t i = 0, count = node.domainProductionModifier.size(); i < count; ++i)
        {
            os << " gives " << node.domainProductionModifier[i].second << " production modifier for units with domain: " << gGlobals.getDomainInfo(node.domainProductionModifier[i].first).getType();
        }
        for (size_t i = 0, count = node.combatTypeFreeExperience.size(); i < count; ++i)
        {
            os << " gives " << node.combatTypeFreeExperience[i].second << " free experience for units with combat type: " << gGlobals.getUnitCombatInfo(node.combatTypeFreeExperience[i].first).getType();
        }
        if (node.freePromotion != NO_PROMOTION)
        {
            os << " gives free promotion: " << gGlobals.getPromotionInfo(node.freePromotion).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::UnitNode& node)
    {
        os << "\n\t";
        if (node.enabledUnitType != NO_UNIT)
        {
            os << " enables: " << gGlobals.getUnitInfo(node.enabledUnitType).getType();
        }
		return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::SpecialistSlotNode& node)
    {
        ShowPos showpos(os);
        os << "\n\t";
        for (size_t i = 0, count = node.specialistTypes.size(); i < count; ++i)
        {
            os << node.specialistTypes[i].second << (node.specialistTypes[i].second == 1 ? " slot" :  " slots")
                << " for type: " << gGlobals.getSpecialistInfo(node.specialistTypes[i].first).getType();
        }
        for (size_t i = 0, count = node.freeSpecialistTypes.size(); i < count; ++i)
        {
            os << node.freeSpecialistTypes[i].second << " free specialists of type: " << gGlobals.getSpecialistInfo(node.freeSpecialistTypes[i].first).getType();
        }
        for (size_t i = 0, count = node.improvementFreeSpecialists.size(); i < count; ++i)
        {
            os << node.improvementFreeSpecialists[i].second << " free specialists for improvement: " << gGlobals.getImprovementInfo(node.improvementFreeSpecialists[i].first).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::BonusNode& node)
    {
        os << "\n\t";
        os << gGlobals.getBonusInfo(node.bonusType).getType();
        if (!isEmpty(node.yieldModifier))
        {
            os << " modifies yields: " << node.yieldModifier;
        }
        if (node.happy != 0)
        {
            ShowPos showpos(os);
            os << " happy change = " << node.happy;
        }
        if (node.health != 0)
        {
            ShowPos showpos(os);
            os << " health change = " << node.health;
        }
        if (node.prodModifier != 0)
        {
            ShowPos showpos(os);
            os << " production modifier = " << node.prodModifier << '%';
        }
        if (node.freeBonusCount != 0)
        {
            os << "\n\tgenerates " << node.freeBonusCount << " of resource";
        }
        if (node.isRemoved)
        {
            os << "\n\tremoves access to resource";
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::CityDefenceNode& node)
    {
        ShowPos showpos(os);
        os << "\n\t";
        if (node.defenceBonus != 0)
        {
            os << " city defence bonus = " << node.defenceBonus << '%';
        }
        if (node.globalDefenceBonus != 0)
        {
            os << " global city defence bonus = " << node.globalDefenceBonus << '%';
        }
        if (node.bombardRateModifier != 0)
        {
            os << " city bombard rate modifier = " << node.bombardRateModifier << '%';
        }
        if (node.espionageDefence != 0)
        {
            os << " city espionage defence bonus = " << node.espionageDefence << '%';
        }
        if (node.airDefenceModifier != 0)
        {
            os << " air defence modifier = " << node.airDefenceModifier << '%';
        }
        if (node.nukeDefenceModifier != 0)
        {
            os << " nuke damage modifier = " << node.nukeDefenceModifier << '%';
        }
        if (node.extraAirUnitCapacity != 0)
        {
            os << node.extraAirUnitCapacity << " air unit capacity";
        }
        if (node.extraAirLiftCount != 0)
        {
            os << " can airlift " << node.extraAirLiftCount << " extra unit" << (node.extraAirLiftCount == 1 ? "" : "s");
        }
        return os;

    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::MiscEffectNode& node)
    {
        os << "\n\t";
        if (node.cityMaintenanceModifierChange != 0)
        {
            ShowPos showpos(os);
            os << " city maintenance modifier change = " << node.cityMaintenanceModifierChange << '%';
        }
        if (node.foodKeptPercent != 0)
        {
            ShowPos showpos(os);
            os << " city food kept change = " << node.foodKeptPercent << '%';
        }
        if (node.hurryAngerModifier != 0)
        {
            ShowPos showpos(os);
            os << " hurry anger modifier change = " << node.hurryAngerModifier << '%';
        }        
        if (node.workerSpeedModifier != 0)
        {
            ShowPos showpos(os);
            os << " worker speed modifier change = " << node.workerSpeedModifier << '%';
        }
        if (node.globalPopChange != 0)
        {
            ShowPos showpos(os);
            os << " global pop change = " << node.globalPopChange;
        }
        if (node.nFreeTechs > 0)
        {
            os << " gives: " << node.nFreeTechs << " free tech" << (node.nFreeTechs > 1 ? "s" : "");
        }
        if (node.noUnhealthinessFromBuildings)
        {
            os << " no unhealthiness from buildings";
        }
        if (node.noUnhealthinessFromPopulation)
        {
            os << " no unhealthiness from population";
        }
        if (node.noUnhappiness)
        {
            os << " no unhappines in city";
        }
        if (node.startsGoldenAge)
        {
            os << " starts golden age";
        }
        if (node.makesCityCapital)
        {
            os << " makes city capital";
        }
        if (node.isGovernmentCenter)
        {
            os << " is gov. centre";
        }
        if (node.freeBuildingType != NO_BUILDING)
        {
            os << " gives free building: " << gGlobals.getBuildingInfo(node.freeBuildingType).getType();
        }
        if (!node.civicTypes.empty())
        {
            os << " allows civics: ";
            for (size_t i = 0, count = node.civicTypes.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                os << gGlobals.getCivicInfo(node.civicTypes[i]).getType();
            }
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::ReligionNode& node)
    {
        os << "\n\t";

        if (node.prereqReligion != NO_RELIGION)
        {
            os << " prereq religion = " << gGlobals.getReligionInfo(node.prereqReligion).getType();
        }
        if (node.religionType  != NO_RELIGION)
        {
            os << " religion type = " << gGlobals.getReligionInfo(node.religionType).getType();
        }
        if (node.globalReligionType  != NO_RELIGION)
        {
            os << " global religion type = " << gGlobals.getReligionInfo(node.globalReligionType).getType();
            os << " commerce change per city with religion = " << node.globalCommerceChange;
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::HurryNode& node)
    {
        os << "\n\t";

        if (node.hurryAngerModifier != 0)
        {
            os << " modifies hurry anger duration by " << node.hurryAngerModifier << '%';
        }
        if (node.globalHurryCostModifier  != 0)
        {
            os << " globally modifies hurry cost by " << node.globalHurryCostModifier << '%';
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const BuildingInfo::AreaEffectNode& node)
    {
        ShowPos showpos(os);
        os << "\n\t";
        if (node.areaHealth != 0)
        {
            os << " area health change = " << node.areaHealth;
        }
        if (node.globalHealth != 0)
        {
            os << " global health change = " << node.globalHealth;
        }
        if (node.areaHappy != 0)
        {
            os << " area happy change = " << node.areaHappy;
        }
        if (node.globalHappy != 0)
        {
            os << " global happy change = " << node.globalHappy;
        }
        return os;
    }

    void streamBuildingInfo(std::ostream& os, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        os << pBuildingInfo->getInfo();
    }
}