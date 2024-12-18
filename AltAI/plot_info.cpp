#include "AltAI.h"

#include "./plot_info.h"
#include "./irrigatable_area.h"
#include "./game.h"
#include "./player.h"
#include "./unit.h"
#include "./helper_fns.h"
#include "./iters.h"
#include "./gamedata_analysis.h"
#include "./civ_log.h"

#include "boost/assign/list_of.hpp"

namespace AltAI
{
    namespace 
    {
        // corresponds to ::PlotTypes enumeration
        const char* plotTypeStrings[] =
        {
            "Peak", "Hills", "Land", "Ocean"
        };

        int makeKey(int plotType, int terrainType, int featureType, int bonusType, int waterType, int riverMask, int cityType)
        {
            static const int numPlotTypes = ::NUM_PLOT_TYPES;
            static const int numTerrainTypes = 1 + gGlobals.getNumTerrainInfos();
            static const int numFeatureTypes = 1 + gGlobals.getNumFeatureInfos();  // for NO_FEATURE
            static const int numBonusTypes = 1 + gGlobals.getNumBonusInfos();  // for NO_BONUS
            static const int numWaterTypes = 5;
            static const int numRiverMaskTypes = 16;

            return plotType + numPlotTypes *
                (terrainType + numTerrainTypes *
                    (featureType + numFeatureTypes *
                        (bonusType + numBonusTypes *
                            (waterType + numWaterTypes * 
                                (riverMask + numRiverMaskTypes *
                                    (cityType))))));
        }

        bool testKeyUniqueness()
        {
            static const int numPlotTypes = ::NUM_PLOT_TYPES;
            static const int numTerrainTypes = gGlobals.getNumTerrainInfos();
            static const int numFeatureTypes = 1 + gGlobals.getNumFeatureInfos();  // for NO_FEATURE
            static const int numBonusTypes = 1 + gGlobals.getNumBonusInfos();  // for NO_BONUS
            static const int numWaterTypes = 5;
            static const int numRiverMaskTypes = 16;
            static const int numCityTypes = 2;

            typedef std::map<int, std::vector<boost::tuple<int, int, int, int, int, int, int> > > KeyMap;
            KeyMap keyMap;

            std::vector<int> keys(numPlotTypes * numTerrainTypes * numFeatureTypes * numBonusTypes * numWaterTypes * numRiverMaskTypes, 0);

            for (int i = 0; i < numPlotTypes; ++i)
            {
                for (int j = 0; j < numTerrainTypes; ++j)
                {
                    for (int k = 0; k < numFeatureTypes; ++k)
                    {
                        for (int l = 0; l < numBonusTypes; ++l)
                        {
                            for (int m = 0; m < numWaterTypes; ++m)
                            {
                                for (int n = 0; n < numRiverMaskTypes; ++n)
                                {
                                    for (int p = 0; p < numCityTypes; ++p)
                                    {
                                        keyMap[makeKey(i, j, k, l, m, n, p)].push_back(boost::make_tuple(i, j, k, l, m, n, p));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            const CvPlayerAI& player = CvPlayerAI::getPlayer((PlayerTypes)0);
            std::ostream& os = CivLog::getLog(player)->getStream();
            bool unique = true;

            for (KeyMap::const_iterator ci(keyMap.begin()), ciEnd(keyMap.end()); ci != ciEnd; ++ci)
            {
                if (ci->second.size() > 1)
                {
                    unique = false;
                    os << "\nKey: " << ci->first;
                    for (size_t i = 0, count = ci->second.size(); i < count; ++i)
                    {
                        os << "(" << boost::get<0>(ci->second[i]) << ", " << boost::get<1>(ci->second[i]) << ", " << boost::get<2>(ci->second[i])
                           << ", " << boost::get<3>(ci->second[i]) << ", " << boost::get<4>(ci->second[i]) << ", " << boost::get<5>(ci->second[i])
                           << ", " << boost::get<6>(ci->second[i]) << ") ";
                    }
                }
            }
            return unique;
        }

        struct PlotInfoRequestData
        {
            PlayerTypes playerID;
            TeamTypes teamID;
            ::PlotTypes plotType;
            ::TerrainTypes terrainType;
            FeatureTypes featureType;
            BonusTypes bonusType;
            ImprovementTypes improvementType;
            bool isHills, isLake, isRiver, hasFreshWaterAccess, isFreshWater;
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
            // assume we can build improvements if we can't see any city here (no cheating!)
            if ((pPlot->isCity() && pPlot->getPlotCity()->wasRevealed(requestData.teamID, false)) || pPlot->isImpassable())
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
            else if (improvementInfo.isFreshWaterMakesValid() && pPlot->isFreshWater())
            {
                isValid = true;
            }
            else if (improvementInfo.isRiverSideMakesValid() && requestData.isRiver)
            {
                isValid = true;
            }
            else if (improvementInfo.getTerrainMakesValid(requestData.terrainType))
            {
                isValid = true;
            }
            else if (requestData.featureType != NO_FEATURE && improvementInfo.getFeatureMakesValid(requestData.featureType))
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

                while (IterPlot iterPlot = plotIter())
                {
                    if (iterPlot.valid())
                    {
                        if (pPlot->isRiverCrossing(directionXY(pPlot, iterPlot)))
                        {
                            // check can actually build in dotmap/city improvements logic - as allows selection of set of plots to optimise placement
                            //if (iterPlot->getImprovementType() != requestData.improvementType)
                            //{
                                isValid = true;
                                break;
                            //}
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

            if (improvementInfo.isRequiresRiverSide())
            {
                conditions.push_back(PlotInfo::HasAvailableRiverSide());
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
                improvementYield[yieldType] = improvementInfo.getYieldChange(yieldType);
                const int irrigatedYieldChange = improvementInfo.getIrrigatedYieldChange(yieldType);

                // since can build non fresh water farms without freshwater on bonuses, we want to
                // keep the irrigated yield separate for a tech requirement for those cases
                if (irrigatedYieldChange > 0 && (requestData.isFreshWater || requestData.bonusType == NO_BONUS))
                {
                    improvementYield[yieldType] += irrigatedYieldChange;
                }
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
                    bonusYield[yieldType] = improvementInfo.getImprovementBonusYield(requestData.bonusType, yieldType);
                }
            }
            return std::make_pair(improvementYield, bonusYield);
        }

        template <typename NodeType> void getTechYields(NodeType& node, const CvImprovementInfo& improvementInfo, const PlotInfoRequestData& requestData)
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

            // set tech requirement in build conditions, but add tech yield for irrigation to non irrigated bonus improvements
            // covers the case of wheat/rice/corn which is not irrigated until civil service
            // since allows separation of the irrigated yield and bonus yields which are available with different techs
            // the ability to build the improvement is covered by the tech in the build conditions
            // and the irrigation bonus comes here, whereas for non bonus non-irrigated farms the ability to build
            // the improvement is sufficent (biology still adds another tech yield in both cases)
            if (requestData.bonusType != NO_BONUS && !requestData.isFreshWater && requestData.hasFreshWaterAccess)
            {
                PlotYield irrigatedYield = const_cast<CvImprovementInfo&>(improvementInfo).getIrrigatedYieldChangeArray();
                if (!isEmpty(irrigatedYield))                
                {
                    std::vector<TechTypes> carryIrrigationTechs = GameDataAnalysis::getCarriesIrrigationTechs();
                    if (!carryIrrigationTechs.empty())
                    {
                        // todo - handle case of more than one tech granting carry irrigation and take one with least depth here
                        node.techYields.push_back(std::make_pair(carryIrrigationTechs[0], irrigatedYield));
                    }
                }                
            }
        }

        // TODO - do route yields stack (i.e. when a route is built, are old routes kept)?
        template <typename NodeType> void getRouteYields(NodeType& node, const CvImprovementInfo& improvementInfo)
        {
            for (int routeIndex = 0, routeCount = gGlobals.getNumRouteInfos(); routeIndex < routeCount; ++routeIndex)
            {
                // wtf?
                PlotYield routeYield = const_cast<CvImprovementInfo&>(improvementInfo).getRouteYieldChangesArray(routeIndex);

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
            getTechYields(node, improvementInfo, requestData);

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
                getTechYields(node, improvementInfo, requestData);

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
            headNode.featureType = requestData.featureType;

            bool canRemoveFeature = requestData.featureType != NO_FEATURE && GameDataAnalysis::getBuildTypeToRemoveFeature(requestData.featureType) != NO_BUILD;

            if (canRemoveFeature)
            {
                PlotInfoRequestData featureRemoveRequestData(requestData);
                featureRemoveRequestData.featureType = NO_FEATURE;                
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
                        if (requestData.featureType == NO_FEATURE
                            ||
                            (requestData.featureType != NO_FEATURE && !gGlobals.getBuildInfo(buildType).isFeatureRemove(requestData.featureType)))
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

        template <typename NodeType> bool nodesAreEqual(const NodeType& first, const NodeType& second)
        {
            if (first.yield != second.yield || first.bonusYield != second.bonusYield || first.buildConditions.size() != second.buildConditions.size() ||
                first.techYields.size() != second.techYields.size() || first.civicYields.size() != second.civicYields.size() || first.routeYields.size() != second.routeYields.size())
            {
                return false;
            }

            for (size_t i = 0, count = first.buildConditions.size(); i < count; ++i)
            {
                if (!(first.buildConditions[i] == second.buildConditions[i]))
                {
                    return false;
                }
            }

            for (size_t i = 0, count = first.techYields.size(); i < count; ++i)
            {
                if (first.techYields[i].first != second.techYields[i].first || first.techYields[i].second != second.techYields[i].second)
                {
                    return false;
                }
            }

            for (size_t i = 0, count = first.civicYields.size(); i < count; ++i)
            {
                if (first.civicYields[i].first != second.civicYields[i].first || first.civicYields[i].second != second.civicYields[i].second)
                {
                    return false;
                }
            }

            for (size_t i = 0, count = first.routeYields.size(); i < count; ++i)
            {
                if (first.routeYields[i].first != second.routeYields[i].first || first.routeYields[i].second.first != second.routeYields[i].second.first ||
                    first.routeYields[i].second.second != second.routeYields[i].second.second)
                {
                    return false;
                }
            }

            if (!(first.upgradeNode == second.upgradeNode))
            {
                return false;
            }

            return true;
        }
    }
}

namespace AltAI
{
    std::string shortTerrainType(TerrainTypes terrainType)
    {
        if (terrainType == NO_TERRAIN)
        {
            return "NO_TERRAIN";
        }

        const CvTerrainInfo& terrainInfo = gGlobals.getTerrainInfo(terrainType);
        
        std::string typeString = terrainInfo.getType();

        static std::map<std::string, std::string> terrainStringsMap = boost::assign::map_list_of("TERRAIN_GRASS", "grassland")
            ("TERRAIN_PLAINS", "plains")("TERRAIN_DESERT", "desert")("TERRAIN_TUNDRA", "tundra")
            ("TERRAIN_SNOW", "ice")("TERRAIN_COAST", "coast")("TERRAIN_OCEAN", "ocean")
            ("TERRAIN_PEAK", "mountain")("TERRAIN_HILL", "hills");

        std::map<std::string, std::string>::const_iterator ci = terrainStringsMap.find(terrainInfo.getType());

        if (ci != terrainStringsMap.end())
        {
            return ci->second;
        }
        else
        {
            return "Unknown terrain type";
        }
    }

    std::string shortFeatureType(FeatureTypes featureType)
    {
        if (featureType == NO_FEATURE)
        {
            return "";
        }

        const CvFeatureInfo& featureInfo = gGlobals.getFeatureInfo(featureType);
        
        std::string typeString = featureInfo.getType();

        static std::map<std::string, std::string> featureStringsMap = boost::assign::map_list_of
            ("FEATURE_ICE", "ice")("FEATURE_JUNGLE", "jungle")("FEATURE_OASIS", "oasis")
            ("FEATURE_FLOOD_PLAINS", "floodplains")("FEATURE_FOREST", "forest")("FEATURE_FALLOUT", "fallout");

        std::map<std::string, std::string>::const_iterator ci = featureStringsMap.find(featureInfo.getType());

        if (ci != featureStringsMap.end())
        {
            return ci->second;
        }
        else
        {
            return "Unknown feature type";
        }
    }

    std::string shortImprovementType(ImprovementTypes improvementType)
    {
        if (improvementType == NO_IMPROVEMENT)
        {
            return "";
        }

        const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(improvementType);
        
        std::string typeString = improvementInfo.getType();

        static std::map<std::string, std::string> improvementStringsMap = boost::assign::map_list_of
            ("IMPROVEMENT_LAND_WORKED", "")("IMPROVEMENT_WATER_WORKED", "")("IMPROVEMENT_CITY_RUINS", "ruins")
            ("IMPROVEMENT_GOODY_HUT", "goody hut")("IMPROVEMENT_FARM", "farm")("IMPROVEMENT_FISHING_BOATS", "fishing boats")
            ("IMPROVEMENT_WHALING_BOATS", "whaling boats")("IMPROVEMENT_MINE", "mine")("IMPROVEMENT_WORKSHOP", "workshop")
            ("IMPROVEMENT_LUMBERMILL", "lumbermill")("IMPROVEMENT_WINDMILL", "windmill")("IMPROVEMENT_WATERMILL", "watermill")
            ("IMPROVEMENT_PLANTATION", "plantation")("IMPROVEMENT_QUARRY", "quarry")("IMPROVEMENT_PASTURE", "pasture")
            ("IMPROVEMENT_CAMP", "camp")("IMPROVEMENT_WELL", "well")("IMPROVEMENT_OFFSHORE_PLATFORM", "offshore platform")
            ("IMPROVEMENT_WINERY", "winery")("IMPROVEMENT_COTTAGE", "cottage")("IMPROVEMENT_HAMLET", "hamlet")
            ("IMPROVEMENT_VILLAGE", "village")("IMPROVEMENT_TOWN", "town")("IMPROVEMENT_FORT", "fort")
            ("IMPROVEMENT_FOREST_PRESERVE", "preserve");

        std::map<std::string, std::string>::const_iterator ci = improvementStringsMap.find(improvementInfo.getType());

        if (ci != improvementStringsMap.end())
        {
            return ci->second;
        }
        else
        {
            return "Unknown improvement type";
        }
    }

    std::string shortBonusType(BonusTypes bonusType)
    {
        if (bonusType == NO_BONUS)
        {
            return "";
        }

        const CvBonusInfo& bonusInfo = gGlobals.getBonusInfo(bonusType);
        
        std::string typeString = bonusInfo.getType();

        static std::map<std::string, std::string> bonusStringsMap = boost::assign::map_list_of
            ("BONUS_ALUMINUM", "aluminium")("BONUS_COAL", "coal")("BONUS_COPPER", "copper")
            ("BONUS_HORSE", "horses")("BONUS_IRON", "iron")("BONUS_MARBLE", "marble")
            ("BONUS_OIL", "oil")("BONUS_STONE", "stone")("BONUS_URANIUM", "uranium")
            ("BONUS_BANANA", "bananas")("BONUS_CLAM", "clam")("BONUS_CORN", "corn")
            ("BONUS_COW", "cows")("BONUS_CRAB", "crab")("BONUS_DEER", "deer")
            ("BONUS_FISH", "fish")("BONUS_PIG", "pigs")("BONUS_RICE", "rice")
            ("BONUS_SHEEP", "sheep")("BONUS_WHEAT", "wheat")("BONUS_DYE", "dyes")
            ("BONUS_FUR", "furs")("BONUS_GEMS", "gems")("BONUS_GOLD", "gold")
            ("BONUS_INCENSE", "incense")("BONUS_IVORY", "elephants")("BONUS_SILK", "silk")
            ("BONUS_SILVER", "silver")("BONUS_SPICES", "spices")("BONUS_SUGAR", "sugar")
            ("BONUS_WINE", "wine")("BONUS_WHALE", "whale")("BONUS_DRAMA", "hit dramas")
            ("BONUS_MUSIC", "hit musicals")("BONUS_MOVIES", "hit movies");

        std::map<std::string, std::string>::const_iterator ci = bonusStringsMap.find(bonusInfo.getType());

        if (ci != bonusStringsMap.end())
        {
            return ci->second;
        }
        else
        {
            return "Unknown bonus type";
        }
    }

    PlotInfo::PlotInfo(const CvPlot* pPlot, PlayerTypes playerType) : pPlot_(pPlot), playerType_(playerType), key_(-1)
    {
#ifdef ALTAI_DEBUG
        static bool test = testKeyUniqueness();
#endif
        bool hasFreshWaterAccess = hasFreshWaterAccess_();

        int riverMask = makeRiverMask_();

        /*if (riverMask > 0)
        {
            const CvPlayerAI& player = CvPlayerAI::getPlayer(playerType);
            std::ostream& os = CivLog::getLog(player)->getStream();
            os << "\nplot: " << pPlot->getCoords() << " mask = " << riverMask;
        }*/

        key_ = makeKey_(hasFreshWaterAccess, riverMask);
        init_(hasFreshWaterAccess);
    }

    bool PlotInfo::BuildOrCondition::operator == (const PlotInfo::BuildOrCondition& other) const
    {
        if (conditions.size() != other.conditions.size())
        {
            return false;
        }

        for (size_t i = 0, count = conditions.size(); i < count; ++i)
        {
            if (!(conditions[i] == other.conditions[i]))
            {
                return false;
            }
        }

        return true;
    }

    bool PlotInfo::ImprovementNode::operator == (const PlotInfo::ImprovementNode& other) const
    {
        return nodesAreEqual(*this, other);
    }

    bool PlotInfo::FeatureRemovedNode::operator == (const PlotInfo::FeatureRemovedNode& other) const
    {
        if (yield != other.yield || bonusYield != other.bonusYield || improvementNodes.size() != other.improvementNodes.size())
        {
            return false;
        }

        for (size_t i = 0, count = improvementNodes.size(); i < count; ++i)
        {
            if (!(improvementNodes[i] == other.improvementNodes[i]))
            {
                return false;
            }
        }

        return true;
    }

    bool PlotInfo::UpgradeNode::operator == (const PlotInfo::UpgradeNode& other) const
    {
        return nodesAreEqual(*this, other);
    }

    bool PlotInfo::BaseNode::operator == (const PlotInfo::BaseNode& other) const
    {
        // note - don't compare: hasPotentialFreshWaterAccess, as this can differ, but only matters if the improvement node for farms and their build conditions differ
        if (isImpassable != other.isImpassable || isFreshWater != other.isFreshWater || 
            yield != other.yield || bonusYield != other.bonusYield || tech != other.tech ||
            plotType != other.plotType || terrainType != other.terrainType || bonusType != other.bonusType || featureType != other.featureType ||
            improvementNodes.size() != other.improvementNodes.size() || !(featureRemovedNode == featureRemovedNode))
        {
            return false;
        }

        for (size_t i = 0, count = improvementNodes.size(); i < count; ++i)
        {
            if (!(improvementNodes[i] == other.improvementNodes[i]))
            {
                return false;
            }
        }

        return true;
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
        requestData.hasFreshWaterAccess = hasFreshWaterAccess;
        requestData.isFreshWater = pPlot_->isFreshWater();
        requestData.isHills = pPlot_->isHills();

        node_ = getHeadNode(pPlot_, requestData);
    }

    int PlotInfo::makeKey_(bool hasFreshWaterAccess, int riverMask) const
    {
        //static const int numPlotTypes = 1 + ::NUM_PLOT_TYPES;
        //static const int numTerrainTypes = 2 + gGlobals.getNumTerrainInfos();
        //static const int numFeatureTypes = 2 + gGlobals.getNumFeatureInfos();  // for NO_FEATURE
        //static const int numBonusTypes = 2 + gGlobals.getNumBonusInfos();  // for NO_BONUS

        TeamTypes teamID = PlayerIDToTeamID(playerType_);

        int plotType = pPlot_->getPlotType();  // Peak, Hills, (Flat)land, Ocean
        int terrainType = pPlot_->getTerrainType();  // Grass, Plains, Desert, Tundra, Snow, Coast, Ocean, Peak, Hill
        // 1 + for NO_FEATURE
        int featureType = 1 + pPlot_->getFeatureType();  // Ice, Jungle, Oasis, FloodPlains, Forest, Fallout
        
        // this call only gets resources the team in question can know about legitimately
        int bonusType = 1 + pPlot_->getBonusType(teamID);

        bool isLake = plotType == ::PLOT_OCEAN && pPlot_->isLake();
        bool isRiver = pPlot_->getRiverCrossingCount() > 0;
        bool hasFreshWater = pPlot_->isFreshWater();
        // sea will be 0, lakes: 1, potential fresh water: 2, adjacent to fresh water: 3 (can't be a water plot), riverside: 4
        int waterType = isLake ? 4 : (isRiver ? 3 : (hasFreshWater ? 2 : (hasFreshWaterAccess ? 1 : 0)));
        //const int numWaterTypes = 5;

        //const int numRiverMaskTypes = 16;

        int cityType = pPlot_->isCity() && pPlot_->getPlotCity()->wasRevealed(teamID, false) ? 1 : 0;
        //const int numCityTypes = 2;

        FAssertMsg(!(isLake && isRiver), "Plot is river and lake?");

        int key = makeKey(plotType, terrainType, featureType, bonusType, waterType, riverMask, cityType);

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