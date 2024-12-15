#include "AltAI.h"

#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"
#include "./gamedata_analysis.h"
#include "./civ_helper.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./civ_log.h"
#include "./building_info_construct_visitors.h"
#include "./buildings_info.h"

namespace AltAI
{
    namespace
    {
        class PlotBuildCondVisitor : public boost::static_visitor<bool>
        {
        public:
            explicit PlotBuildCondVisitor(const CvCity* pCity) : pCity_(pCity), pPlot_(pCity->plot())
            {
            }

            explicit PlotBuildCondVisitor(const CvPlot* pPlot) : pCity_(NULL), pPlot_(pPlot)
            {
            }

            result_type operator() (const BuildingInfo::NullNode&) const
            {
                return false;
            }

            // could check if we can construct any of the required buildings (maybe make this a separate visitor)
            result_type operator() (const BuildingInfo::RequiredBuildings& node) const
            {
                // if just checking plot - don't worry about required buildings
                if (!pCity_)
                {
                    return true;
                }

                for (size_t i = 0, count = node.cityBuildings.size(); i < count; ++i)
                {
                    if (pCity_->getNumBuilding(node.cityBuildings[i]) == 0)
                    {
                        return false;
                    }
                }

                for (size_t i = 0, count = node.buildingCounts.size(); i < count; ++i)
                {
                    int buildingCount = 0;
                    CityIter iter(CvPlayerAI::getPlayer(pCity_->getOwner()));
                    while (CvCity* pCity = iter())
                    {
                        buildingCount += pCity->getNumBuilding(node.buildingCounts[i].first);

                        if (buildingCount >= node.buildingCounts[i].second)
                        {
                            break;
                        }
                    }

                    if (buildingCount < node.buildingCounts[i].second)
                    {
                        return false;
                    }
                }

                return true;
            }

            result_type operator() (const BuildingInfo::IsRiver& node) const
            {
                return pPlot_->isRiver();
            }

            result_type operator() (const BuildingInfo::MinArea& node) const
            {
                if (node.isWater)
                {
                    return pPlot_->isCoastalLand(node.minimumSize);
                }
                else
                {
                    return pPlot_->area()->getNumTiles() >= node.minimumSize;
                }
            }

            result_type operator() (const BuildingInfo::IsHolyCity& node) const
            {
                // if just checking plot - don't worry about city based requirements
                if (!pCity_)
                {
                    return true;
                }

                return pCity_->isHolyCity(node.religionType);
            }

            result_type operator() (const BuildingInfo::BuildOrCondition& node) const
            {
                for (size_t i = 0, count = node.conditions.size(); i < count; ++i)
                {
                    if (boost::apply_visitor(*this, node.conditions[i]))
                    {
                        return true;
                    }
                }
                return false;
            }

        private:
            const CvCity* pCity_;
            const CvPlot* pPlot_;
        };

        class CouldConstructBuildingVisitor : public boost::static_visitor<bool>
        {
        public:
            CouldConstructBuildingVisitor(const Player& player, const CvCity* pCity, BuildingTypes buildingType, int lookaheadDepth, bool ignoreRequiredBuildings, bool ignoreCost)
                : player_(player), pCity_(pCity), pPlot_(pCity->plot()), 
                  buildingType_(buildingType), lookaheadDepth_(lookaheadDepth), ignoreRequiredBuildings_(ignoreRequiredBuildings), ignoreCost_(ignoreCost)
            {
                civHelper_ = player.getCivHelper();
                pAnalysis_ = player.getAnalysis();
            }

            CouldConstructBuildingVisitor(const Player& player, const CvPlot* pPlot, BuildingTypes buildingType, int lookaheadDepth)
                : player_(player), pCity_(NULL), pPlot_(pPlot), buildingType_(buildingType), lookaheadDepth_(lookaheadDepth), ignoreRequiredBuildings_(true), ignoreCost_(false)
            {
                civHelper_ = player.getCivHelper();
                pAnalysis_ = player.getAnalysis();
            }

            template <typename T>
                bool operator() (const T&) const
            {
                return true;
            }

            bool operator() (const BuildingInfo::BaseNode& node) const
            {
                if (!ignoreCost_ && node.cost < 0)
                {
                    return false;
                }

                // if we want to support multiple buildings of the same type - need to store the max count in BaseNode info
                if (pCity_ && pCity_->getNumBuilding(buildingType_) > 0)
                {
                    return false;
                }

                for (size_t i = 0, count = node.techs.size(); i < count; ++i)
                {
                    // if we don't we have the tech and its depth is deeper than our lookaheadDepth, return false
                    if (!civHelper_->hasTech(node.techs[i]) &&
                        (lookaheadDepth_ == 0 ? true : pAnalysis_->getTechResearchDepth(node.techs[i]) > lookaheadDepth_))
                    {
                        return false;
                    }
                }
            
                PlotBuildCondVisitor plotConditionsVisitor(pPlot_);
                if (pCity_ && !ignoreRequiredBuildings_)
                {
                    plotConditionsVisitor = PlotBuildCondVisitor(pCity_);
                }

                for (size_t i = 0, count = node.buildConditions.size(); i < count; ++i)
                {
                    if (!boost::apply_visitor(plotConditionsVisitor, node.buildConditions[i]))
                    {
                        return false;
                    }
                }

                for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
                {
                    if (!boost::apply_visitor(*this, node.nodes[i]))
                    {
                        return false;
                    }
                }

                return true;
            }

            bool operator() (const BuildingInfo::BonusNode& node) const
            {
                // todo - add any required bonus check here
                return true;
            }

            bool operator() (const BuildingInfo::ReligionNode& node) const
            {
                return node.prereqReligion == NO_RELIGION || player_.getCvPlayer()->getHasReligionCount(node.prereqReligion) > 0;
            }

            bool operator() (const BuildingInfo::MiscEffectNode& node) const
            {
                // can't build buildings which make city a gov centre if city is already a gov centre
                return !(node.isGovernmentCenter && pCity_ && pCity_->isGovernmentCenter());
            }

        private:
            const Player& player_;        
            const CvCity* pCity_;
            const CvPlot* pPlot_;
            BuildingTypes buildingType_;
            int lookaheadDepth_;
            bool ignoreRequiredBuildings_, ignoreCost_;
            boost::shared_ptr<CivHelper> civHelper_;
            boost::shared_ptr<PlayerAnalysis> pAnalysis_;
        };

        class ConditionalBuildingHasPlotYieldChange : public boost::static_visitor<ConditionalPlotYieldEnchancingBuilding>
        {
        public:
            explicit ConditionalBuildingHasPlotYieldChange(BuildingTypes buildingType) : buildingType_(buildingType)
            {
            }

            template <typename T>
                result_type operator() (const T&) const
            {
                return result_type();
            }

            result_type operator() (const BuildingInfo::BaseNode& node) const
            {
                if (node.buildConditions.empty())
                {
                    return result_type();
                }

                result_type result(buildingType_);
                result.buildConditions = node.buildConditions;

                for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
                {
                    result_type nodeResult(boost::apply_visitor(*this, node.nodes[i]));
                    if (!nodeResult.conditionalYieldChanges.empty())
                    {
                        std::copy(nodeResult.conditionalYieldChanges.begin(), nodeResult.conditionalYieldChanges.end(), std::back_inserter(result.conditionalYieldChanges));
                    }
                }
                return result.conditionalYieldChanges.empty() ? result_type() : result;
            }

            result_type operator() (const BuildingInfo::YieldNode& node) const
            {
                result_type nodeResult(buildingType_);
                if (node.plotCond)
                {
                    nodeResult.conditionalYieldChanges.push_back(std::make_pair(node.plotCond, node.yield));
                }
                return nodeResult;
            }

        private:
            BuildingTypes buildingType_;
        };
    }

    struct CityConditionYieldHelperImpl
    {
        explicit CityConditionYieldHelperImpl(PlayerTypes playerType)
        {
            conditionalPlotYieldEnchancingBuildings = GameDataAnalysis::getInstance()->getConditionalPlotYieldEnhancingBuildings(playerType);
        }

        PlotYield getExtraConditionalYield(XYCoords cityCoords, XYCoords plotCoords)
        {
            PlotYield extraYield;
            for (size_t i = 0, count = conditionalPlotYieldEnchancingBuildings.size(); i < count; ++i)
            {
                bool isValid = true;
                for (size_t j = 0, count = conditionalPlotYieldEnchancingBuildings[i].buildConditions.size(); j < count; ++j)
                {
                    isValid = isValid && boost::apply_visitor(
                        PlotBuildCondVisitor(gGlobals.getMap().plot(cityCoords.iX, cityCoords.iY)), conditionalPlotYieldEnchancingBuildings[i].buildConditions[j]);
                }

                if (isValid)
                {
                    for (size_t j = 0, count = conditionalPlotYieldEnchancingBuildings[i].conditionalYieldChanges.size(); j < count; ++j)
                    {
                        CvPlot* pPlot = gGlobals.getMap().plot(plotCoords.iX, plotCoords.iY);

                        if ((pPlot->*(conditionalPlotYieldEnchancingBuildings[i].conditionalYieldChanges[j].first))())
                        {
                            extraYield += conditionalPlotYieldEnchancingBuildings[i].conditionalYieldChanges[j].second;
                        }
                    }
                }
            }
            return extraYield;
        }

        std::vector<ConditionalPlotYieldEnchancingBuilding> conditionalPlotYieldEnchancingBuildings;
    };

    CityConditionYieldHelper::CityConditionYieldHelper(PlayerTypes playerType) : pImpl(new CityConditionYieldHelperImpl(playerType))
    {
    }

    PlotYield CityConditionYieldHelper::getExtraConditionalYield(XYCoords cityCoords, XYCoords plotCoords)
    {
        return pImpl->getExtraConditionalYield(cityCoords, plotCoords);
    }

    std::vector<ConditionalPlotYieldEnchancingBuilding> getConditionalYieldEnchancingBuildings(PlayerTypes playerType)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(playerType);
        const boost::shared_ptr<PlayerAnalysis>& pPlayerAnalysis = gGlobals.getGame().getAltAI()->getPlayer(playerType)->getAnalysis();

        std::vector<ConditionalPlotYieldEnchancingBuilding> buildings;

        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            BuildingTypes buildingType = (BuildingTypes)gGlobals.getCivilizationInfo(player.getCivilizationType()).getCivilizationBuildings(i);

            if (!isLimitedWonderClass(getBuildingClass(buildingType)))
            {
                boost::shared_ptr<BuildingInfo> pBuildingInfo = pPlayerAnalysis->getBuildingInfo(buildingType);
                if (!pBuildingInfo)
                {
//#ifdef ALTAI_DEBUG
//                    std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
//                    os << "\nSkipping conditional building: " << gGlobals.getBuildingInfo(buildingType).getType();
//#endif
                    continue;
                }

                ConditionalPlotYieldEnchancingBuilding result(boost::apply_visitor(ConditionalBuildingHasPlotYieldChange(buildingType), pBuildingInfo->getInfo()));
                if (result.buildingType != NO_BUILDING)
                {
                    buildings.push_back(result);
                }
            }
        }

#ifdef ALTAI_DEBUG
        // debug
        {
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
            for (size_t i = 0, count = buildings.size(); i < count; ++i)
            {
                if (i > 0) os << ", ";
                else os << "\nConditional Yield Enhancing buildings: ";
                os << gGlobals.getBuildingInfo(buildings[i].buildingType).getType();
            }
        }
#endif
        return buildings;
    }

    std::map<XYCoords, PlotYield> getExtraConditionalYield(PlayerTypes playerType, int lookaheadDepth, 
        const std::pair<XYCoords, std::map<int, std::set<XYCoords> > >& plotData,       
        std::map<BuildingTypes, PlotYield>& requiredBuildings)
    {
        const PlayerPtr& player = gGlobals.getGame().getAltAI()->getPlayer(playerType);        
        std::vector<ConditionalPlotYieldEnchancingBuilding> conditionalYieldEnchancingBuildings = GameDataAnalysis::getInstance()->getConditionalPlotYieldEnhancingBuildings(playerType);;
        std::map<XYCoords, PlotYield> extraYieldsMap;
        for (size_t i = 0, count = conditionalYieldEnchancingBuildings.size(); i < count; ++i)
        {
            bool isValid = true;
            const boost::shared_ptr<BuildingInfo>& pBuildingInfo = player->getAnalysis()->getBuildingInfo(conditionalYieldEnchancingBuildings[i].buildingType);
            const BuildingInfo::BaseNode& node = boost::get<BuildingInfo::BaseNode>(pBuildingInfo->getInfo());

            for (size_t j = 0, techCount = node.techs.size(); j < techCount; ++j)
            {
                const int depth = player->getTechResearchDepth(node.techs[j]);
                if (depth > lookaheadDepth)
                {
                    isValid = false;
                    break;
                }
            }

            if (!isValid)
            {
                continue;
            }

            for (size_t j = 0, count = conditionalYieldEnchancingBuildings[i].buildConditions.size(); j < count; ++j)
            {
                isValid = isValid && boost::apply_visitor(PlotBuildCondVisitor(gGlobals.getMap().plot(plotData.first.iX, plotData.first.iY)), conditionalYieldEnchancingBuildings[i].buildConditions[j]);
            }

            if (isValid)
            {
                for (size_t j = 0, count = conditionalYieldEnchancingBuildings[i].conditionalYieldChanges.size(); j < count; ++j)
                {
                    CvPlot* pPlot = gGlobals.getMap().plot(plotData.first.iX, plotData.first.iY);

                    if ((pPlot->*(conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].first))())
                    {
                        extraYieldsMap[plotData.first] += conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].second;
                        requiredBuildings[conditionalYieldEnchancingBuildings[i].buildingType] += conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].second;
                    }

                    for (std::map<int, std::set<XYCoords> >::const_iterator ci(plotData.second.begin()), ciEnd(plotData.second.end()); ci != ciEnd; ++ci)
                    {
                        for (std::set<XYCoords>::const_iterator si(ci->second.begin()), siEnd(ci->second.end()); si != siEnd; ++si)
                        {
                            pPlot = gGlobals.getMap().plot(si->iX, si->iY);

                            if ((pPlot->*(conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].first))())
                            {
                                extraYieldsMap[*si] += conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].second;
                                requiredBuildings[conditionalYieldEnchancingBuildings[i].buildingType] += conditionalYieldEnchancingBuildings[i].conditionalYieldChanges[j].second;
                            }
                        }
                    }
                }
            }
        }

#ifdef ALTAI_DEBUG  // need to pass player in
        /*{
            if (!extraYieldsMap.empty())
            {
                PlotYield totalExtraYield;
                for (std::map<XYCoords, PlotYield>::const_iterator ci(extraYieldsMap.begin()), ciEnd(extraYieldsMap.end()); ci != ciEnd; ++ci)
                {
                    totalExtraYield += ci->second;
                }

                std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
                os << "\nextra plotyield = " << totalExtraYield;
            }
        }*/
#endif

        return extraYieldsMap;
    }

    bool couldConstructBuilding(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, bool ignoreRequiredBuildings)
    {
        return boost::apply_visitor(CouldConstructBuildingVisitor(player, city.getCvCity(), pBuildingInfo->getBuildingType(), lookaheadDepth, ignoreRequiredBuildings, false), pBuildingInfo->getInfo());
    }

    bool couldConstructUnitBuilding(const Player& player, const City& city, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        return boost::apply_visitor(CouldConstructBuildingVisitor(player, city.getCvCity(), pBuildingInfo->getBuildingType(), lookaheadDepth, false, true), pBuildingInfo->getInfo());
    }

    bool couldConstructBuilding(const Player& player, const CvPlot* pPlot, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        return boost::apply_visitor(CouldConstructBuildingVisitor(player, pPlot, pBuildingInfo->getBuildingType(), lookaheadDepth), pBuildingInfo->getInfo());
    }

    bool couldConstructSpecialBuilding(const Player& player, int lookaheadDepth, const boost::shared_ptr<BuildingInfo>& pBuildingInfo)
    {
        const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(pBuildingInfo->getBuildingType());
        SpecialBuildingTypes specialBuildingType = (SpecialBuildingTypes)buildingInfo.getSpecialBuildingType();

        if (specialBuildingType != NO_SPECIALBUILDING)
        {
            TechTypes prereqTech = (TechTypes)gGlobals.getSpecialBuildingInfo(specialBuildingType).getTechPrereq();
            if (prereqTech != NO_TECH)
            {
                if (!player.getCivHelper()->hasTech(prereqTech) &&
                    (lookaheadDepth == 0 ? true : player.getAnalysis()->getTechResearchDepth(prereqTech) > lookaheadDepth))
                {
                    return false;
                }
            }
        }

        return true;
    }
}