#include "./tech_info.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        struct TechInfoRequestData
        {
            TechInfoRequestData(TechTypes techType_ = NO_TECH, PlayerTypes playerType_ = NO_PLAYER) : techType(techType_), playerType(playerType_)
            {
            }
            TechTypes techType;
            PlayerTypes playerType;
        };

        void getBuildingNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
            {
                BuildingTypes buildingType = getPlayerVersion(requestData.playerType, (BuildingClassTypes)i);

                if (buildingType == NO_BUILDING)
                {
                    continue;
                }

                const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);

                if (buildingInfo.getPrereqAndTech() == requestData.techType)
                {
                    baseNode.nodes.push_back(TechInfo::BuildingNode(buildingType));
                    continue;
                }
                else
                {
                    for (int j = 0; j < gGlobals.getNUM_BUILDING_AND_TECH_PREREQS(); ++j)
	                {
                		if (buildingInfo.getPrereqAndTechs(j) == requestData.techType)
                        {
                            baseNode.nodes.push_back(TechInfo::BuildingNode(buildingType));
                            continue;
                        }
                    }
                }

                if (buildingInfo.getObsoleteTech() == requestData.techType)
                {
                    baseNode.nodes.push_back(TechInfo::BuildingNode(buildingType, true));
                }
            }
        }

        void getImprovementNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            int workerSpeedModifier = techInfo.getWorkerSpeedModifier();
            if (workerSpeedModifier != 0)
            {
                baseNode.nodes.push_back(TechInfo::ImprovementNode(NO_IMPROVEMENT, false, NO_FEATURE, PlotYield(), workerSpeedModifier));
            }

            for (int i = 0, count = gGlobals.getNumBuildInfos(); i < count; ++i)
            {
                const CvBuildInfo& buildInfo = gGlobals.getBuildInfo((BuildTypes)i);
                ImprovementTypes improvementType = (ImprovementTypes)buildInfo.getImprovement();

                if (buildInfo.getTechPrereq() == requestData.techType && improvementType != NO_IMPROVEMENT)
                {
                    baseNode.nodes.push_back(TechInfo::ImprovementNode(improvementType, true));
                }
                else if (improvementType == NO_IMPROVEMENT)
                {
                    for (int j = 0, count = gGlobals.getNumFeatureInfos(); j < count; ++j)
                    {
                        if (buildInfo.getFeatureTech(j) == requestData.techType)
                        {
                            baseNode.nodes.push_back(TechInfo::ImprovementNode(NO_IMPROVEMENT, false, (FeatureTypes)j));
                        }
                    }
                }
            }

            for (int i = 0, count = gGlobals.getNumImprovementInfos(); i < count; ++i)
            {
                const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo((ImprovementTypes)i);

                // Perhaps there is a good reason why getTechYieldChangesArray() is not const?
                PlotYield yieldChange = const_cast<CvImprovementInfo&>(improvementInfo).getTechYieldChangesArray(requestData.techType);
                if (!isEmpty(yieldChange))
                {
                    baseNode.nodes.push_back(TechInfo::ImprovementNode((ImprovementTypes)i, false, NO_FEATURE, yieldChange));
                }
            }
        }

        void getCivicNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
            {
                const CvCivicInfo& civicInfo = gGlobals.getCivicInfo((CivicTypes)i);
                if (civicInfo.getTechPrereq() == requestData.techType)
                {
                    baseNode.nodes.push_back(TechInfo::CivicNode((CivicTypes)i));
                }
            }
        }

        void getUnitNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            {
                TechInfo::UnitNode unitNode;
                for (int i = 0; i < NUM_DOMAIN_TYPES; ++i)
                {
                    int extraMoves = techInfo.getDomainExtraMoves(i);
                    if (extraMoves > 0)
                    {
                        unitNode.domainExtraMoves.push_back(std::make_pair((DomainTypes)i, extraMoves));
                    }
                }

                if (!unitNode.domainExtraMoves.empty())
                {
                    baseNode.nodes.push_back(unitNode);
                }
            }

            {
                TechInfo::UnitNode unitNode;
                for (int i = 0, count = gGlobals.getNumPromotionInfos(); i < count; ++i)
                {
                    const CvPromotionInfo& promotionInfo = gGlobals.getPromotionInfo((PromotionTypes)i);
                    if (promotionInfo.getTechPrereq() == requestData.techType)
                    {
                        unitNode.promotions.push_back((PromotionTypes)i);
                    }
                }

                if (!unitNode.promotions.empty())
                {
                    baseNode.nodes.push_back(unitNode);
                }
            }

            for (int i = 0, count = gGlobals.getNumUnitClassInfos(); i < count; ++i)
            {
                UnitTypes unitType = getPlayerVersion(requestData.playerType, (UnitClassTypes)i);
                if (unitType == NO_UNIT)
                {
                    continue;
                }

                const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

                if (unitInfo.getPrereqAndTech() == requestData.techType)
                {
                    baseNode.nodes.push_back(TechInfo::UnitNode(unitType));
                    continue;
                }

                for (int j = 0, andCount = gGlobals.getNUM_UNIT_AND_TECH_PREREQS(); j < andCount; ++j)
                {
                    if (unitInfo.getPrereqAndTechs(j) == requestData.techType)
                    {
                        baseNode.nodes.push_back(TechInfo::UnitNode(unitType));
                        break;
                    }
                }
            }
        }

        void getBonusNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumBonusInfos(); i < count; ++i)
            {
                const CvBonusInfo& bonusInfo = gGlobals.getBonusInfo((BonusTypes)i);
                TechTypes revealTech = (TechTypes)bonusInfo.getTechReveal();
                TechTypes tradeTech = (TechTypes)bonusInfo.getTechCityTrade();
                TechTypes obsoleteTech = (TechTypes)bonusInfo.getTechObsolete();
                if (revealTech == requestData.techType || tradeTech == requestData.techType || obsoleteTech == requestData.techType)
                {
                    BonusTypes bonusType = (BonusTypes)i;
                    baseNode.nodes.push_back(TechInfo::BonusNode(revealTech == requestData.techType ? bonusType : NO_BONUS,
                        tradeTech == requestData.techType ? bonusType : NO_BONUS, obsoleteTech == requestData.techType ? bonusType : NO_BONUS));
                }
            }
        }

        void getCommerceNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
            {
                if (techInfo.isCommerceFlexible(i))
                {
                    baseNode.nodes.push_back(TechInfo::CommerceNode((CommerceTypes)i));
                }
            }
        }

        void getProcessNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumProcessInfos(); i < count; ++i)
            {
                const CvProcessInfo& processInfo = gGlobals.getProcessInfo((ProcessTypes)i);
                if (processInfo.getTechPrereq() == requestData.techType)
                {
                    CommerceModifier modifier;
                    for (int j = 0; j < NUM_COMMERCE_TYPES; ++j)
                    {
                        modifier[j] = processInfo.getProductionToCommerceModifier(j);
                    }
                    baseNode.nodes.push_back(TechInfo::ProcessNode((ProcessTypes)i, modifier));
                }
            }
        }

        void getRouteNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            for (int i = 0, count = gGlobals.getNumBuildInfos(); i < count; ++i)
            {
                const CvBuildInfo& buildInfo = gGlobals.getBuildInfo((BuildTypes)i);
                RouteTypes routeType = NO_ROUTE;

                if (buildInfo.getTechPrereq() == requestData.techType && (routeType = (RouteTypes)buildInfo.getRoute()) != NO_ROUTE)
                {
                    baseNode.nodes.push_back(TechInfo::RouteNode(routeType));
                }
            }
        }

        void getTradeNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            int extraTradeRoutes = techInfo.getTradeRoutes();
            if (extraTradeRoutes != 0)
            {
                baseNode.nodes.push_back(TechInfo::TradeNode(NO_TERRAIN, false, extraTradeRoutes));
            }

            if (techInfo.isRiverTrade())
            {
                baseNode.nodes.push_back(TechInfo::TradeNode(NO_TERRAIN, true));
            }

            for (int i = 0, count = gGlobals.getNumTerrainInfos(); i < count; ++i)
            {
                if (techInfo.isTerrainTrade((TerrainTypes)i))
                {
                    baseNode.nodes.push_back(TechInfo::TradeNode((TerrainTypes)i));
                }
            }
        }

        void getFirstToNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            bool foundsReligion = false;
            for (int i = 0, count = gGlobals.getNumReligionInfos(); i < count; ++i)
            {
                const CvReligionInfo& religionInfo = gGlobals.getReligionInfo((ReligionTypes)i);
                if (religionInfo.getTechPrereq() == requestData.techType)
                {
                    foundsReligion = true;
                    break;
                }
            }

            int freeTechCount = techInfo.getFirstFreeTechs();
            UnitClassTypes unitClassType = (UnitClassTypes)techInfo.getFirstFreeUnitClass();

            if (freeTechCount > 0 || foundsReligion || unitClassType != NO_UNITCLASS)
            {
                baseNode.nodes.push_back(TechInfo::FirstToNode(techInfo.getFirstFreeTechs(), foundsReligion, (UnitClassTypes)techInfo.getFirstFreeUnitClass()));
            }
        }

        void getMiscEffectNode(TechInfo::BaseNode& baseNode, const CvTechInfo& techInfo, const TechInfoRequestData& requestData)
        {
            bool enablesOpenBorders = techInfo.isOpenBordersTrading(), enablesTechTrading = techInfo.isTechTrading(), enablesGoldTrading = techInfo.isGoldTrading(), enablesMapTrading = techInfo.isMapTrading();
            bool enablesWaterWork = techInfo.isWaterWork(), ignoreIrrigation = techInfo.isIgnoreIrrigation(), carriesIrrigation = techInfo.isIrrigation();
            bool extraWaterSight = techInfo.isExtraWaterSeeFrom(), centresMap = techInfo.isMapCentering();
            bool enablesBridgeBuilding = techInfo.isBridgeBuilding();
            bool enablesDefensivePacts = techInfo.isDefensivePactTrading(), enablesPermanentAlliances = techInfo.isPermanentAllianceTrading();

            if (enablesOpenBorders || enablesTechTrading || enablesGoldTrading || enablesMapTrading || enablesWaterWork || ignoreIrrigation || carriesIrrigation ||
                extraWaterSight || centresMap || enablesBridgeBuilding || enablesDefensivePacts || enablesPermanentAlliances)
            {
                baseNode.nodes.push_back(
                    TechInfo::MiscEffectNode(enablesOpenBorders, enablesTechTrading, enablesGoldTrading, enablesMapTrading,
                        enablesWaterWork, ignoreIrrigation, carriesIrrigation, extraWaterSight, centresMap,
                        enablesBridgeBuilding, enablesDefensivePacts, enablesPermanentAlliances));
            }
        }

        TechInfo::BaseNode getBaseNode(const TechInfoRequestData& requestData)
        {
            TechInfo::BaseNode node;
            node.tech = requestData.techType;

            // todo - check player specific features (e.g. buildings) are only added for given player
            const CvTechInfo& techInfo = gGlobals.getTechInfo(requestData.techType);

            node.happy = techInfo.getHappiness();
            node.health = techInfo.getHealth();

            for (int i = 0, count = gGlobals.getNUM_AND_TECH_PREREQS(); i < count; ++i)
	        {
		        TechTypes andTech = (TechTypes)techInfo.getPrereqAndTechs(i);

		        if (andTech != NO_TECH)
                {
                    node.andTechs.push_back(andTech);
                }
            }

            for (int i = 0, count = gGlobals.getNUM_OR_TECH_PREREQS(); i < count; ++i)
	        {
		        TechTypes orTech = (TechTypes)techInfo.getPrereqOrTechs(i);

		        if (orTech != NO_TECH)
                {
                    node.orTechs.push_back(orTech);
                }
            }

            getBuildingNode(node, techInfo, requestData);
            getImprovementNode(node, techInfo, requestData);
            getCivicNode(node, techInfo, requestData);
            getUnitNode(node, techInfo, requestData);
            getBonusNode(node, techInfo, requestData);
            getCommerceNode(node, techInfo, requestData);
            getProcessNode(node, techInfo, requestData);
            getRouteNode(node, techInfo, requestData);
            getTradeNode(node, techInfo, requestData);
            getFirstToNode(node, techInfo, requestData);
            getMiscEffectNode(node, techInfo, requestData);

            return node;
        }
    }

    TechInfo::TechInfo(TechTypes techType, PlayerTypes playerType) : techType_(techType), playerType_(playerType)
    {
        init_();
    }

    void TechInfo::init_()
    {
        infoNode_ = getBaseNode(TechInfoRequestData(techType_, playerType_));
    }

    const TechInfo::TechInfoNode& TechInfo::getInfo() const
    {
        return infoNode_;
    }

}