#include "./tech_info_streams.h"

namespace AltAI
{
    std::ostream& operator << (std::ostream& os, const TechInfo::NullNode& node)
    {
        return os << "(Empty Node) ";
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::BuildingNode& node)
    {
        const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(node.buildingType);

        if (node.obsoletes)
        {
            os << " obsoletes: ";
        }
        else
        {
            os << " allows: ";
        }
        return os << buildingInfo.getType();
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::ImprovementNode& node)
    {
        if (node.workerSpeedModifier != 0)
        {
            os << " worker speed increase: " << node.workerSpeedModifier;
        }

        if (node.improvementType != NO_IMPROVEMENT)
        {
            if (node.allowsImprovement)
            {
                os << " allows: ";
            }
            else
            {
                os << " ";
            }

            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(node.improvementType);
            os << improvementInfo.getType();

            if (!isEmpty(node.modifier))
            {
                os << " yield modifier = " << node.modifier;
            }
        }

        if (node.removeFeatureType != NO_FEATURE)
        {
            const CvFeatureInfo& featureInfo = gGlobals.getFeatureInfo(node.removeFeatureType);
            os << " can remove: " << featureInfo.getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::CivicNode& node)
    {
        return os << " allows: " << gGlobals.getCivicInfo(node.civicType).getType();
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::UnitNode& node)
    {
        if (node.unitType != NO_UNIT)
        {
            os << " allows: " << gGlobals.getUnitInfo(node.unitType).getType(); 
        }

        for (size_t i = 0, count = node.promotions.size(); i < count; ++i)
        {
            if (i == 0)
            {
                os << " allows: ";
            }
            else
            {
                os << ", ";
            }
            os << gGlobals.getPromotionInfo(node.promotions[i]).getType();
        }

        for (size_t i = 0, count = node.domainExtraMoves.size(); i < count; ++i)
        {
            if (i > 0)
            {
                os << ", ";
            }
            os << node.domainExtraMoves[i].second << " extra move(s) on: " << gGlobals.getDomainInfo(node.domainExtraMoves[i].first).getType();
        }

        return os;
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::BonusNode& node)
    {
        if (node.revealBonus != NO_BONUS)
        {
            os << " reveals: " << gGlobals.getBonusInfo(node.revealBonus).getType();
        }
        if (node.tradeBonus != NO_BONUS)
        {
            os << " can use: " << gGlobals.getBonusInfo(node.tradeBonus).getType();
        }
        if (node.obsoleteBonus != NO_BONUS)
        {
            os << " obsoletes: " << gGlobals.getBonusInfo(node.obsoleteBonus).getType();
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::CommerceNode& node)
    {
        return os << " can adjust: " << gGlobals.getCommerceInfo(node.adjustableCommerceType).getType();
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::ProcessNode& node)
    {
        return os << " can use process: " << gGlobals.getProcessInfo(node.processType).getType()
            << " production to commerce modifier = " << node.productionToCommerceModifier;
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::RouteNode& node)
    {
        return os << " can build: " << gGlobals.getRouteInfo(node.routeType).getType() ;
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::TradeNode& node)
    {
        if (node.terrainType != NO_TERRAIN)
        {
            os << " can trade over: " << gGlobals.getTerrainInfo(node.terrainType).getType();
        }

        if (node.isRiverTrade)
        {
            os << " can trade along rivers ";
        }

        if (node.extraTradeRoutes > 0)
        {
            os << " provides " << node.extraTradeRoutes << " extra trade route(s) ";
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::FirstToNode& node)
    {
        if (node.freeTechCount > 0)
        {
            os << ", grants " << node.freeTechCount << " tech(s)";
        }

        if (node.foundReligion)
        {
            os << ", can found a religion";
        }

        if (node.freeUnitClass != NO_UNITCLASS)
        {
            os << ", gives free " << gGlobals.getUnitClassInfo(node.freeUnitClass).getType();
        }

        return os << " if first discoverer ";
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::MiscEffectNode& node)
    {
        if (node.enablesOpenBorders)
        {
            os << ", enables open borders ";
        }
        if (node.enablesTechTrading)
        {
            os << ", enables tech trading ";
        }
        if (node.enablesGoldTrading)
        {
            os << ", enables gold trading ";
        }
        if (node.enablesMapTrading)
        {
            os << ", enables map trading ";
        }
        if (node.enablesWaterWork)
        {
            os << ", can work water tiles ";
        }
        if (node.ignoreIrrigation)
        {
            os << ", can build farms without irrigation ";
        }
        if (node.carriesIrrigation)
        {
            os << ", can chain irrigation ";
        }
        if (node.extraWaterSight)
        {
            os << ", extra sight over water ";
        }
        if (node.centresMap)
        {
            os << ", centers map ";
        }
        if (node.enablesBridgeBuilding)
        {
            os << ", enables bridge building ";
        }
        if (node.enablesDefensivePacts)
        {
            os << ", enables defensive pacts ";
        }
        if (node.enablesPermanentAlliances)
        {
            os << ", enables permanent alliances ";
        }
        return os;
    }

    std::ostream& operator << (std::ostream& os, const TechInfo::BaseNode& node)
    {
        for (size_t i = 0, count = node.orTechs.size(); i < count; ++i)
        {
            if (i == 0)
            {
                os << " Or techs: ";
            }
            else
            {
                os << ", ";
            }
            const CvTechInfo& techInfo = gGlobals.getTechInfo(node.orTechs[i]);
            os << techInfo.getType();
        }

        for (size_t i = 0, count = node.andTechs.size(); i < count; ++i)
        {
            if (i == 0)
            {
                os << " And techs: ";
            }
            else
            {
                os << ", ";
            }
            const CvTechInfo& techInfo = gGlobals.getTechInfo(node.andTechs[i]);
            os << techInfo.getType();
        }

        if (node.happy > 0)
        {
            os << " happiness = " << node.happy;
        }

        if (node.health > 0)
        {
            os << " healthiness = " << node.health;
        }
        
        for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
        {
            os << node.nodes[i];
        }
        return os;
    }
}