#include "AltAI.h"

#include "./resource_info.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"
#include "./gamedata_analysis.h"

namespace AltAI
{
    namespace
    {
        struct ResourceInfoRequestData
        {
            ResourceInfoRequestData() : bonusType(NO_BONUS), playerType(NO_PLAYER), unitType(NO_UNIT) {}
            BonusTypes bonusType;
            PlayerTypes playerType;
            ::UnitTypes unitType;
        };

        bool empty(const ResourceInfo::BuildingNode& node)
        {
            return node.productionModifier == 0 && node.bonusHappy == 0 && node.bonusHealth == 0 && node.yieldModifier == YieldModifier();
        }

        ResourceInfo::BaseNode getBaseNode(const ResourceInfoRequestData& requestData)
        {
            ResourceInfo::BaseNode node;
            node.bonusType = requestData.bonusType;

            CivilizationTypes civType = CvPlayerAI::getPlayer(requestData.playerType).getCivilizationType();
            if (civType == NO_CIVILIZATION)
            {
                return node;
            }

            const CvBonusInfo& bonusInfo = gGlobals.getBonusInfo(requestData.bonusType);            

            node.revealTech = (TechTypes)bonusInfo.getTechReveal();
            node.obsoleteTech = (TechTypes)bonusInfo.getTechObsolete();

            

            node.baseHealth = bonusInfo.getHealth();
            node.baseHappy = bonusInfo.getHappiness();

            for (size_t unitClassIndex = 0, count = gGlobals.getNumUnitClassInfos(); unitClassIndex < count; ++unitClassIndex)
            {
                ::UnitTypes unitType = (::UnitTypes)gGlobals.getCivilizationInfo(civType).getCivilizationUnits(unitClassIndex);
                if (unitType == NO_UNIT)
                {
                    continue;
                }
                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

                BonusTypes andBonusType = (BonusTypes)unitInfo.getPrereqAndBonus();
                std::vector<BonusTypes> orBonusTypes;
                for (size_t i = 0, count = gGlobals.getNUM_UNIT_PREREQ_OR_BONUSES(); i < count; ++i)
                {
                    BonusTypes orBonus = (BonusTypes)unitInfo.getPrereqOrBonuses(i);
                    if (unitInfo.getPrereqOrBonuses(i) != NO_BONUS)
                    {
                        orBonusTypes.push_back(orBonus);
                    }
                }

                if (andBonusType == requestData.bonusType || std::find(orBonusTypes.begin(), orBonusTypes.end(), requestData.bonusType) != orBonusTypes.end())
                {
                    ResourceInfo::UnitNode unitNode;
                    unitNode.unitType = unitType;
                    unitNode.isAndRequirement = andBonusType == requestData.bonusType;
                    node.unitNodes.push_back(unitNode);
                }
            }

            for (size_t buildingClassIndex = 0, count = gGlobals.getNumBuildingClassInfos(); buildingClassIndex < count; ++buildingClassIndex)
            {
                BuildingTypes buildingType = (BuildingTypes)gGlobals.getCivilizationInfo(civType).getCivilizationBuildings(buildingClassIndex);
                if (buildingType == NO_BUILDING)
                {
                    continue;
                }
                const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);

                ResourceInfo::BuildingNode buildingNode;
                buildingNode.buildingType = buildingType;
                buildingNode.productionModifier = buildingInfo.getBonusProductionModifier(requestData.bonusType);
                buildingNode.bonusHappy = buildingInfo.getBonusHappinessChanges(requestData.bonusType);
                buildingNode.bonusHealth = buildingInfo.getBonusHealthChanges(requestData.bonusType);

                buildingNode.yieldModifier = buildingInfo.getBonusYieldModifierArray(requestData.bonusType);

                // TODO - power bonus, bonus access removed (National Park), prereq bonuses (example building(s)?), generated bonuses
                // do corps separately

                if (!empty(buildingNode))
                {
                    node.buildingNodes.push_back(buildingNode);
                }
            }

            return node;
        }
    }

    ResourceInfo::ResourceInfo(BonusTypes bonusType, PlayerTypes playerType) : bonusType_(bonusType), playerType_(playerType)
    {
        // check each improvement type to see if it makes the resource valid - if so, check for build types which can produce the improvement
        for (int i = 0, improvementCount = gGlobals.getNumImprovementInfos(); i < improvementCount; ++i)
        {
            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo((ImprovementTypes)i);
            if (improvementInfo.isImprovementBonusMakesValid(bonusType))
            {
                for (int j = 0, buildCount = gGlobals.getNumBuildInfos(); j < buildCount; ++j)
                {
                    const CvBuildInfo& buildInfo = gGlobals.getBuildInfo((BuildTypes)j);
                    if ((ImprovementTypes)buildInfo.getImprovement() == (ImprovementTypes)i)
                    {
                        buildTypes.push_back((BuildTypes)j);
                    }
                }
            }
        }

        init_();
    }

    void ResourceInfo::init_()
    {
        ResourceInfoRequestData requestData;
        requestData.bonusType = bonusType_;
        requestData.playerType = playerType_;
        infoNode_ = getBaseNode(requestData);
    }

    const ResourceInfo::ResourceInfoNode& ResourceInfo::getInfo() const
    {
        return infoNode_;
    }
}