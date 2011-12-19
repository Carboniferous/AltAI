#include "./plot_info.h"
#include "./irrigatable_area.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"
#include "./helper_fns.h"
#include "./iters.h"
#include "./gamedata_analysis.h"
#include "./civ_log.h"

namespace AltAI
{
    namespace 
    {
        // corresponds to ::PlotTypes enumeration
        const char* plotTypeStrings[] =
        {
            "Peak", "Hills", "Land", "Ocean"
        };

        struct PlotInfoRequestData
        {
            PlayerTypes playerID;
            TeamTypes teamID;
            ::PlotTypes plotType;
            ::TerrainTypes terrainType;
            FeatureTypes featureType;
            BonusTypes bonusType;
            ImprovementTypes improvementType;
            bool isHills, isLake, isRiver, hasFreshWaterAccess;
            PlotYield parentYield, parentBonusYield;
        };

        bool isImpassable(const PlotInfoRequestData& requestData)
        {
            return requestData.plotType == PLOT_PEAK ||
                   requestData.featureType != NO_FEATURE && gGlobals.getFeatureInfo(requestData.featureType).isImpassable() ||
                   requestData.terrainType != NO_TERRAIN && gGlobals.getTerrainInfo(requestData.terrainType).isImpassable();
        }

        PlotInfo::FeatureRemovedNode makeFeatureRemovedNode(const PlotInfo::BaseNode& node)
        {
            PlotInfo::FeatureRemovedNode featureRemovedNode;
            featureRemovedNode.improvementNodes = node.improvementNodes;
            featureRemovedNode.yield = node.yield;
            featureRemovedNode.bonusYield = node.bonusYield;
            return featureRemovedNode;
        }

        // variation of CvPlot::canHaveImprovement which allows for whether the plot's irrigatable area has freshwater access
        bool couldHaveImprovement(const CvPlot* pPlot, const PlotInfoRequestData& requestData)
        {
	        if (pPlot->isCity() || pPlot->isImpassable())
	        {
		        return false;
	        }

            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(requestData.improvementType);
	        if (improvementInfo.isWater() != pPlot->isWater())
	        {
		        return false;
	        }

	        if (requestData.featureType != NO_FEATURE && gGlobals.getFeatureInfo(requestData.featureType).isNoImprovement())
            {
                return false;
	        }

	        if (requestData.bonusType != NO_BONUS && improvementInfo.isImprovementBonusMakesValid(requestData.bonusType))
	        {
		        return true;
	        }

	        if (improvementInfo.isNoFreshWater() && pPlot->isFreshWater())
	        {
		        return false;
	        }

	        if (improvementInfo.isRequiresFlatlands() && !pPlot->isFlatlands())
	        {
		        return false;
	        }

	        if (improvementInfo.isRequiresFeature() && requestData.featureType == NO_FEATURE)
	        {
		        return false;
	        }

            bool isValid = false;
	        if (improvementInfo.isHillsMakesValid() && requestData.isHills)
	        {
		        isValid = true;
	        }

	        if (improvementInfo.isFreshWaterMakesValid() && pPlot->isFreshWater())
	        {
		        isValid = true;
	        }

	        if (improvementInfo.isRiverSideMakesValid() && requestData.isRiver)
	        {
		        isValid = true;
	        }

	        if (improvementInfo.getTerrainMakesValid(requestData.terrainType))
	        {
		        isValid = true;
	        }

	        if (requestData.featureType != NO_FEATURE && improvementInfo.getFeatureMakesValid(requestData.featureType))
	        {
		        isValid = true;
	        }

	        if (!isValid)
	        {
		        return false;
	        }

	        if (improvementInfo.isRequiresRiverSide())
	        {
		        isValid = false;

                CardinalPlotIter plotIter(pPlot);

                // debug
                /*if (pPlot->getX() == 73 && pPlot->getY() == 32)
                {
                    const CvPlayerAI& player = CvPlayerAI::getPlayer(requestData.playerID);
                    std::ostream& os = CivLog::getLog(player)->getStream();
                    CardinalPlotIter plotIter(pPlot);
                    os << "\nChecking watermill at: " << XYCoords(pPlot->getX(), pPlot->getY());
                    while (IterPlot iterPlot = plotIter())
                    {
                        if (iterPlot.valid())
                        {
                            os << " " << XYCoords(iterPlot->getX(), iterPlot->getY()) << ", crossing = " << pPlot->isRiverCrossing(directionXY(pPlot, iterPlot));
			            }
		            }
                }*/

                while (IterPlot iterPlot = plotIter())
                {
                    if (iterPlot.valid())
                    {
				        if (pPlot->isRiverCrossing(directionXY(pPlot, iterPlot)))
				        {
					        if (iterPlot->getImprovementType() != requestData.improvementType)
					        {
						        isValid = true;
						        break;
					        }
				        }
			        }
		        }

		        if (!isValid)
		        {
			        return false;
		        }
	        }

	        for (int i = 0, count = NUM_YIELD_TYPES; i < count; ++i)
	        {
		        if (pPlot->calculateNatureYield(((YieldTypes)i), requestData.teamID) < improvementInfo.getPrereqNatureYield(i))
		        {
			        return false;
		        }
	        }

            /*  // deal with this is BuildConditions now
            // todo - use civhelper for techs?
            if ((requestData.teamID == NO_TEAM) || !(CvTeamAI::getTeam(requestData.teamID).isIgnoreIrrigation()))
	        {
                // here if need irrigation but have no route to fresh water and can't yet build farms without irrigation
		        if (!requestData.hasFreshWaterAccess && improvementInfo.isRequiresIrrigation())
		        {
			        return false;
		        }
	        }

            // check if can't yet chain irrigation and plot doesn't have fresh water access
            if (requestData.teamID != NO_TEAM && !CvTeamAI::getTeam(requestData.teamID).isIrrigation() && improvementInfo.isRequiresIrrigation())
            {
                return pPlot->isFreshWater(); // don't need to check flatlands as already did above.
            }
            */

	        return true;
        }

        std::vector<PlotInfo::BuildCondition> improvementBuildConditions(const CvPlot* pPlot, const PlotInfoRequestData& requestData)
        {
            std::vector<PlotInfo::BuildCondition> conditions;

            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(requestData.improvementType);
            BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(requestData.improvementType);
            TechTypes techType = buildType != NO_BUILD ? (TechTypes)gGlobals.getBuildInfo(buildType).getTechPrereq() : NO_TECH;

            if (techType != NO_TECH)
            {
                conditions.push_back(PlotInfo::HasTech(techType));
            }

            // fresh water check is because no point in adding the other conditions in that case
            // can always build farms if bonus is valid
            if (improvementInfo.isRequiresIrrigation() && !pPlot->isFreshWater() && (requestData.bonusType == NO_BONUS || !improvementInfo.isImprovementBonusMakesValid(requestData.bonusType)))
            {
                PlotInfo::BuildOrCondition ignoresIrrigationConditions, carriesIrrigationConditions, orConditions;

                std::vector<TechTypes> techs = GameDataAnalysis::getIgnoreIrrigationTechs();
                for (int i = 0, count = techs.size(); i < count; ++i)
                {
                    ignoresIrrigationConditions.conditions.push_back(PlotInfo::HasTech(techs[i]));
                }
                orConditions.conditions.push_back(ignoresIrrigationConditions);

                // means chained irrigation is potentially available
                if (requestData.hasFreshWaterAccess)
                {
                    std::vector<TechTypes> techs = GameDataAnalysis::getCarriesIrrigationTechs();
                    for (int i = 0, count = techs.size(); i < count; ++i)
                    {
                        carriesIrrigationConditions.conditions.push_back(PlotInfo::HasTech(techs[i]));
                    }
                    orConditions.conditions.push_back(carriesIrrigationConditions);
                }

                conditions.push_back(orConditions);
            }

            return conditions;
        }

        bool improvementNodeHasNoYieldImprovement(const PlotInfo::BaseNode& baseNode, const PlotInfo::ImprovementNode& improvementNode)
        {
            return baseNode.yield == improvementNode.yield && baseNode.bonusYield == improvementNode.bonusYield
                && isEmpty(improvementNode.techYields) && isEmpty(improvementNode.civicYields);
        }

        std::pair<PlotYield, PlotYield> getImprovementAndBonusYields(const CvImprovementInfo& improvementInfo, const PlotInfoRequestData& requestData)
        {
            PlotYield improvementYield, bonusYield;
            for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
            {
                improvementYield[yieldType] = improvementInfo.getYieldChange(yieldType) + improvementInfo.getIrrigatedYieldChange(yieldType);
                bonusYield[yieldType] = 0;

                if (requestData.isRiver)
                {
                    improvementYield[yieldType] += improvementInfo.getRiverSideYieldChange(yieldType);
                }
                if (requestData.isHills)
                {
                    improvementYield[yieldType] += improvementInfo.getHillsYieldChange(yieldType);
                }
                if (requestData.bonusType != NO_BONUS)
                {
                    bonusYield[yieldType] += improvementInfo.getImprovementBonusYield(requestData.bonusType, yieldType);
                }
            }
            return std::make_pair(improvementYield, bonusYield);
        }

        template <typename NodeType> void getTechYields(NodeType& node, const CvImprovementInfo& improvementInfo)
        {
            for (int techIndex = 0, techCount = gGlobals.getNumTechInfos(); techIndex < techCount; ++techIndex)
            {
                PlotYield techYield;
                for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
                {
		            techYield[yieldType] = improvementInfo.getTechYieldChanges(techIndex, yieldType);
                }

                if (!isEmpty(techYield))
                {
                    node.techYields.push_back(std::make_pair((TechTypes)techIndex, techYield));
                }
            }
        }

        // TODO - do route yields stack (i.e. when a route is built, are old routes kept)?
        template <typename NodeType> void getRouteYields(NodeType& node, const CvImprovementInfo& improvementInfo)
        {
            for (int routeIndex = 0, routeCount = gGlobals.getNumRouteInfos(); routeIndex < routeCount; ++routeIndex)
            {
                // wtf?
                PlotYield routeYield = (const_cast<CvImprovementInfo&>(improvementInfo).getRouteYieldChangesArray(routeIndex));

                if (!isEmpty(routeYield))
                {
                    node.routeYields.push_back(std::make_pair(GameDataAnalysis::getTechTypeForRouteType((RouteTypes)routeIndex),
                        std::make_pair((RouteTypes)routeIndex, routeYield)));
                }
            }
        }

        template <typename NodeType> void getCivicYields(NodeType& node, ImprovementTypes improvementType)
        {
            for (int civicIndex = 0, civicCount = gGlobals.getNumCivicInfos(); civicIndex < civicCount; ++civicIndex)
	        {
                const CvCivicInfo& civicInfo = gGlobals.getCivicInfo((CivicTypes)civicIndex);
                PlotYield civicYield;
                for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
                {
		            civicYield[yieldType] = civicInfo.getImprovementYieldChanges(improvementType, yieldType);
                }

                if (!isEmpty(civicYield))
                {
                    node.civicYields.push_back(std::make_pair((CivicTypes)civicIndex, civicYield));
                }
		    }
        }

        PlotInfo::UpgradeNode getUpgradeNode(const CvPlot* pPlot, const PlotInfoRequestData& requestData)
        {
            PlotInfo::UpgradeNode node;
            node.yield = requestData.parentYield;
            node.bonusYield = requestData.parentBonusYield;
            node.improvementType = requestData.improvementType;

            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(requestData.improvementType);

            BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType(requestData.improvementType);
            if (buildType != NO_BUILD)
            {
                node.buildConditions.push_back(PlotInfo::HasTech((TechTypes)gGlobals.getBuildInfo(buildType).getTechPrereq()));
            }

            PlotYield improvementYield, bonusYield;
            boost::tie(improvementYield, bonusYield) = getImprovementAndBonusYields(improvementInfo, requestData);

            node.yield += improvementYield;
            node.bonusYield += bonusYield;

            // get tech modifiers
            getTechYields(node, improvementInfo);

            // get route modifiers
            getRouteYields(node, improvementInfo);

            // get civic modifiers
            getCivicYields(node, requestData.improvementType);

            ImprovementTypes upgradeType = (ImprovementTypes)improvementInfo.getImprovementUpgrade();
            if (upgradeType != NO_IMPROVEMENT)
            {
                PlotInfoRequestData upgradeRequestData(requestData);
                upgradeRequestData.parentBonusYield = node.bonusYield;
                upgradeRequestData.improvementType = upgradeType;
                node.upgradeNode.push_back(getUpgradeNode(pPlot, upgradeRequestData));
            }
            return node;
        }

        PlotInfo::ImprovementNode getImprovementNode(const CvPlot* pPlot, const PlotInfoRequestData& requestData)
        {
            PlotInfo::ImprovementNode node;
            node.yield = requestData.parentYield;
            node.bonusYield = requestData.parentBonusYield;
            node.improvementType = requestData.improvementType;

            if (requestData.improvementType != NO_IMPROVEMENT)
            {
                const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(requestData.improvementType);

                node.buildConditions = improvementBuildConditions(pPlot, requestData);

                PlotYield improvementYield, bonusYield;
                boost::tie(improvementYield, bonusYield) = getImprovementAndBonusYields(improvementInfo, requestData);

                node.yield += improvementYield;
                node.bonusYield += bonusYield;

                // get tech modifiers
                getTechYields(node, improvementInfo);

                // get route modifiers
                getRouteYields(node, improvementInfo);

                // get civic modifiers
                getCivicYields(node, requestData.improvementType);

                ImprovementTypes upgradeType = (ImprovementTypes)improvementInfo.getImprovementUpgrade();
                if (upgradeType != NO_IMPROVEMENT)
                {
                    PlotInfoRequestData upgradeRequestData(requestData);
                    upgradeRequestData.parentBonusYield = node.bonusYield;
                    upgradeRequestData.improvementType = upgradeType;
                    node.upgradeNode.push_back(getUpgradeNode(pPlot, upgradeRequestData));
                }
            }

            return node;
        }

        PlotInfo::BaseNode getHeadNode(const CvPlot* pPlot, const PlotInfoRequestData& requestData)
        {
            PlotInfo::BaseNode headNode;
            headNode.isImpassable = isImpassable(requestData);
            headNode.isFreshWater = pPlot->isFreshWater();
            headNode.hasPotentialFreshWaterAccess = requestData.hasFreshWaterAccess;
            headNode.plotType = requestData.plotType;
            headNode.terrainType = requestData.terrainType;

            bool canRemoveFeature = requestData.featureType != NO_FEATURE && GameDataAnalysis::getBuildTypeToRemoveFeature(requestData.featureType) != NO_BUILD;

            if (canRemoveFeature)
            {
                PlotInfoRequestData featureRemoveRequestData(requestData);
                featureRemoveRequestData.featureType = NO_FEATURE;
                headNode.featureType = requestData.featureType;
                headNode.featureRemovedNode = makeFeatureRemovedNode(getHeadNode(pPlot, featureRemoveRequestData));
                headNode.featureRemoveTech = GameDataAnalysis::getTechTypeToRemoveFeature(requestData.featureType);
            }

            const CvTerrainInfo& terrainInfo = gGlobals.getTerrainInfo(requestData.terrainType);

            int yields[NUM_YIELD_TYPES];

            for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
            {
                const CvYieldInfo& yieldInfo = gGlobals.getYieldInfo(YieldTypes(yieldType));

                yields[yieldType] = terrainInfo.getYield(yieldType);  // would have been good to get a function which returned the yields in one go

                if (requestData.isLake)
                {
                    yields[yieldType] += yieldInfo.getLakeChange();
                }
                else
                {
                    switch (requestData.plotType)
                    {
                        case ::PLOT_PEAK:
                            yields[yieldType] += yieldInfo.getPeakChange();
                            break;
                        case ::PLOT_HILLS:
                            yields[yieldType] += yieldInfo.getHillsChange();
                            break;
                        case ::PLOT_OCEAN:
                        case ::PLOT_LAND:
                        default:
                            break;
                    }
                }

                if (requestData.featureType == NO_FEATURE)
                {
                    if (requestData.isRiver)
                    {
                        yields[yieldType] += terrainInfo.getRiverYieldChange(yieldType);
                    }
                    if (requestData.isHills)
                    {
                        yields[yieldType] += terrainInfo.getHillsYieldChange(yieldType);
                    }
                }
                else
                {
                    const CvFeatureInfo& featureInfo = gGlobals.getFeatureInfo(requestData.featureType);

                    yields[yieldType] += featureInfo.getYieldChange(yieldType);
                    if (requestData.isRiver)
                    {
                        yields[yieldType] += featureInfo.getRiverYieldChange(yieldType);
                    }
                    if (requestData.isHills)
                    {
                        yields[yieldType] += featureInfo.getHillsYieldChange(yieldType);
                    }
                }

                yields[yieldType] = std::max<int>(0, yields[yieldType]);
            }

            if (requestData.bonusType != NO_BONUS)
            {
                int bonusYields[NUM_YIELD_TYPES];
                const CvBonusInfo& bonusInfo = gGlobals.getBonusInfo((BonusTypes)requestData.bonusType);
                for (int yieldType = 0; yieldType < NUM_YIELD_TYPES; ++yieldType)
                {
                    bonusYields[yieldType] = bonusInfo.getYieldChange(yieldType);
                }
                headNode.bonusYield = bonusYields;
                headNode.bonusType = requestData.bonusType;
            }

            headNode.yield = yields;

            for (int improvementIndex = 0, improvementCount = gGlobals.getNumImprovementInfos(); improvementIndex < improvementCount; ++improvementIndex)
            {
                PlotInfoRequestData improvementRequestData(requestData);
                improvementRequestData.improvementType = (ImprovementTypes)improvementIndex;
                improvementRequestData.parentYield = headNode.yield;
                improvementRequestData.parentBonusYield = headNode.bonusYield;

                // debug
                /*if (pPlot->getX() == 73 && pPlot->getY() == 32)
                {
                    const CvPlayerAI& player = CvPlayerAI::getPlayer(requestData.playerID);
                    std::ostream& os = CivLog::getLog(player)->getStream();
                    os << "\nChecking for improvement at: " << XYCoords(pPlot->getX(), pPlot->getY()) << gGlobals.getImprovementInfo((ImprovementTypes)improvementIndex).getType()
                        << " couldHaveImprovement = " << couldHaveImprovement(pPlot, improvementRequestData);
                }*/

                // won't show up a mine if can't see copper on flatlands, for example.
                if (couldHaveImprovement(pPlot, improvementRequestData))
                {
                    const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo((ImprovementTypes)improvementIndex);
                    // if we are doing the improvements which don't require the (possible) feature on this plot, ignore ones which do
                    // since canHaveImprovement() will still return true for them and we did them already
                    if (requestData.featureType == NO_FEATURE && improvementInfo.isRequiresFeature())
                    {
                        continue;
                    }

                    BuildTypes buildType = GameDataAnalysis::getBuildTypeForImprovementType((ImprovementTypes)improvementIndex);

                    if (buildType != NO_BUILD)
                    {
                        // not all features can be removed - if have feature then consider builds which don't remove it
                        if ((requestData.featureType != NO_FEATURE && !gGlobals.getBuildInfo(buildType).isFeatureRemove(requestData.featureType))
                            ||
                            requestData.featureType == NO_FEATURE)
                        {
                            PlotInfo::ImprovementNode improvementNode = getImprovementNode(pPlot, improvementRequestData);

                            // check something has actually improved (will ignore e.g. forts)
                            if (!improvementNodeHasNoYieldImprovement(headNode, improvementNode))
                            {
                                headNode.improvementNodes.push_back(improvementNode);
                            }
                        }
                    }
                }
            }

            if (pPlot->isWater())
            {
                headNode.tech = GameDataAnalysis::getCanWorkWaterTech();
            }

            return headNode;
        }  
    }
}

namespace AltAI
{
    PlotInfo::PlotInfo(const CvPlot* pPlot, PlayerTypes playerType) : pPlot_(pPlot), playerType_(playerType), key_(-1)
    {
        bool hasFreshWaterAccess = hasFreshWaterAccess_();

        int riverMask = makeRiverMask_();

        /*if (riverMask > 0)
        {
            const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType);
            std::ostream& os = CivLog::getLog(player)->getStream();
            os << "\nplot: " << XYCoords(pPlot->getX(), pPlot->getY()) << " mask = " << riverMask;
        }*/

        key_ = makeKey_(hasFreshWaterAccess, riverMask);
        init_(hasFreshWaterAccess);
    }

    int PlotInfo::makeRiverMask_() const
    {
        int riverMask = 0;
        if (pPlot_->getRiverCrossingCount() > 0)
        {
            CardinalPlotIter plotIter(pPlot_);
            while (IterPlot iterPlot = plotIter())
            {
                if (iterPlot.valid())
                {
                    DirectionTypes directionType = directionXY(pPlot_, iterPlot);
                    if (pPlot_->isRiverCrossing(directionType))
                    {
                        riverMask |= (1 << directionType / 2);  // this relies on the cardinal direction type enums being 0, 2, 4, 6 (N, E, S, W)
                    }
			    }
            }
		}
        return riverMask;
    }

    int PlotInfo::getKey() const
    {
        return key_;
    }

    int PlotInfo::getKeyWithFeature(FeatureTypes featureType) const
    {
        PlotInfo copy(*this);
        BaseNode& node = boost::get<BaseNode>(copy.node_);
        node.featureType = featureType;
        
        copy.makeKey_(hasFreshWaterAccess_(), makeRiverMask_());
        return copy.key_;
    }

    const PlotInfo::PlotInfoNode& PlotInfo::getInfo() const
    {
        return node_;
    }

    void PlotInfo::init_(bool hasFreshWaterAccess)
    {
        PlotInfoRequestData requestData;

        requestData.playerID = playerType_;
        requestData.teamID = PlayerIDToTeamID(playerType_);

        requestData.plotType = pPlot_->getPlotType();  // Peak, Hills, (Flat)land, Ocean
        requestData.terrainType = pPlot_->getTerrainType();  // Grass, Plains, Desert, Tundra, Snow, Coast, Ocean, Peak, Hill
        requestData.featureType = pPlot_->getFeatureType();  // Ice, Jungle, Oasis, FloodPlains, Forest, Fallout
        requestData.improvementType = NO_IMPROVEMENT;

        // this call only gets resources the team in question can know about legitimately
        requestData.bonusType = pPlot_->getBonusType(requestData.teamID);

        requestData.isLake = requestData.plotType == ::PLOT_OCEAN && pPlot_->isLake();
        requestData.isRiver = pPlot_->getRiverCrossingCount() > 0;
        requestData.isHills = requestData.plotType == PLOT_HILLS;
        requestData.hasFreshWaterAccess = hasFreshWaterAccess;

        node_ = getHeadNode(pPlot_, requestData);
    }

    int PlotInfo::makeKey_(bool hasFreshWaterAccess, int riverMask) const
    {
        static const int numPlotTypes = 1 + ::NUM_PLOT_TYPES;
        static const int numTerrainTypes = 2 + gGlobals.getNumTerrainInfos();
        static const int numFeatureTypes = 2 + gGlobals.getNumFeatureInfos();  // for NO_FEATURE
        static const int numBonusTypes = 2 + gGlobals.getNumBonusInfos();  // for NO_BONUS

        TeamTypes teamID = PlayerIDToTeamID(playerType_);

        int plotType = 1 + pPlot_->getPlotType();  // Peak, Hills, (Flat)land, Ocean
        int terrainType = 2 + pPlot_->getTerrainType();  // Grass, Plains, Desert, Tundra, Snow, Coast, Ocean, Peak, Hill
        // 2 + makes value > 0 and key +ve
        int featureType = 2 + pPlot_->getFeatureType();  // Ice, Jungle, Oasis, FloodPlains, Forest, Fallout
        
        // this call only gets resources the team in question can know about legitimately
        int bonusType = 2 + pPlot_->getBonusType(teamID);

        bool isLake = plotType == ::PLOT_OCEAN && pPlot_->isLake();
        bool isRiver = pPlot_->getRiverCrossingCount() > 0;
        int waterType = 1 + (isLake ? 3 : (isRiver ? 2 : (hasFreshWaterAccess ? 1 : 0)));
        const int numWaterTypes = 5;

        const int numRiverMaskTypes = 16;

        int cityType = pPlot_->isCity() ? 2 : 1;
        const int numCityTypes = 3;

        FAssertMsg(!(isLake && isRiver), "Plot is river and lake?");

        //int key = numPlotTypes * (plotType + 
        //                numTerrainTypes * (terrainType + 
        //                    numFeatureTypes * (featureType + 
        //                        numBonusTypes * (bonusType + 
        //                            numWaterTypes * (waterType +
        //                                numRiverMaskTypes * (1 + riverMask +
        //                                    numCityTypes * cityType))))));
        int key = plotType + (numPlotTypes *
                        terrainType + (numTerrainTypes * 
                            featureType + (numFeatureTypes * 
                                bonusType + (numBonusTypes * 
                                    waterType + (numWaterTypes * 
                                        (1 + riverMask) + (numRiverMaskTypes *
                                            cityType))))));

        {
            /*if (XYCoords(pPlot_->getX(), pPlot_->getY()) == XYCoords(54, 39) || key == 5389245)
            {
                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType_))->getStream();
                os << "\nnumPlotTypes: " << numPlotTypes << ", plotType: " << plotType
                   << ", numTerrainTypes: " << numTerrainTypes << ", terrainType: " << terrainType 
                   << ", numFeatureTypes: " << numFeatureTypes << ", featureType: " << featureType 
                   << ", numBonusTypes: " << numBonusTypes << ", bonusType: " << bonusType
                   << ", numWaterTypes: " << numWaterTypes << ", waterType: " << waterType
                   << ", numRiverMaskTypes: " << numRiverMaskTypes << ", (1 + riverMask): " << (1 + riverMask)
                   << ", numCityTypes: " << numCityTypes << ", cityType: " << cityType;
            }*/
        }

        return key;
    }

    bool PlotInfo::hasFreshWaterAccess_() const
    {
        bool hasFreshWaterAccess = false;
        int irrigatableAreaID = pPlot_->getIrrigatableArea();
        if (irrigatableAreaID != FFreeList::INVALID_INDEX)
        {
            boost::shared_ptr<IrrigatableArea> pIrrigatableArea = gGlobals.getMap().getIrrigatableArea(irrigatableAreaID);
            hasFreshWaterAccess = pIrrigatableArea->hasFreshWaterAccess();
        }
        return hasFreshWaterAccess;
    }
}