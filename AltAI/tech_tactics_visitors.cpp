#include "AltAI.h"

#include "./tech_tactics_visitors.h"
#include "./tech_info.h"
#include "./tech_info_visitors.h"
#include "./plot_info_visitors.h"
#include "./civic_info_visitors.h"
#include "./resource_info_visitors.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./building_tactics_items.h"
#include "./building_tactics_deps.h"
#include "./player_tech_tactics.h"
#include "./tech_tactics_items.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./helper_fns.h"
#include "./city.h"
#include "./civ_helper.h"
#include "./civ_log.h"
#include "./city_log.h"
#include "./error_log.h"
#include "./gamedata_analysis.h"
#include "./map_analysis.h"

namespace AltAI
{
    namespace
    {
        std::vector<PlotImprovementData> getProjectedTechImprovements(IDInfo city)
        {
            CityImprovementManager improvementManager(city, true);
            const CvCity* pCity = ::getCity(city);
            const int lookaheadDepth = 3;
            CityDataPtr pCityData(new CityData(pCity, true, lookaheadDepth));
            improvementManager.simulateImprovements(pCityData, lookaheadDepth, __FUNCTION__);

#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(city.eOwner))->getStream();
            os << "\ngetResearchTech: improvements = ";
            improvementManager.logImprovements(CivLog::getLog(CvPlayerAI::getPlayer(city.eOwner))->getStream());
#endif
            return improvementManager.getImprovements();  // this will return improvements with all techs applied up to specified depth
            /*const std::vector<PlotImprovementData>& improvements = improvementManager.getImprovements();
            std::map<TechTypes, std::vector<PlotImprovementData> > techImprovementsMap;

            for (size_t i = 0, count = improvements.size(); i < count; ++i)
            {
                if (improvements[i].isSelectedAndNotBuilt())
                {
                    ImprovementTypes improvementType = improvements[i].improvement;
                    TechTypes techType = GameDataAnalysis::getTechTypeForBuildType(GameDataAnalysis::getBuildTypeForImprovementType(improvementType));
                    techImprovementsMap[techType].push_back(improvements[i]);
                    if (improvements[i].removedFeature != NO_FEATURE)
                    {
                        TechTypes removeFeatureTechType = GameDataAnalysis::getTechTypeToRemoveFeature(improvements[i].removedFeature);
                        if (removeFeatureTechType != techType)
                        {
                            techImprovementsMap[removeFeatureTechType].push_back(improvements[i]);
                        }
                    }
                }
                else if (improvements[i].flags == PlotImprovementData::Built)
                {
                }
            }

            return techImprovementsMap;*/
        }
    }
    
    class MakeCityBuildTacticsVisitor : public boost::static_visitor<>
    {
    public:
        MakeCityBuildTacticsVisitor(const Player& player, const City& city, const std::vector<PlotImprovementData>& plotData,
            const std::vector<ImprovementTypes>& affectedImprovements, const std::vector<FeatureTypes>& removeFeatureTypes)
            : player_(player), city_(city), plotData_(plotData), currentIndex_(0), 
              affectedImprovements_(affectedImprovements), removeFeatureTypes_(removeFeatureTypes)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const TechInfo::BaseNode& node)
        {
            for (size_t i = 0, plotCount = plotData_.size(); i < plotCount; ++i)
            {
                currentIndex_ = i;
                for (size_t j = 0, nodeCount = node.nodes.size(); j < nodeCount; ++j)
                {
                    boost::apply_visitor(*this, node.nodes[j]);
                }
            }
        }

        void operator() (const TechInfo::ImprovementNode& node)
        {
            // improvement type should already be the base one for upgradable imps (otherwise the yield will be wrong)
            if (node.improvementType == plotData_[currentIndex_].improvement)
            {                
                if (node.allowsImprovement)
                {
                    improvementPlots_.insert(currentIndex_);
                    impTacticItems_.push_back(IWorkerBuildTacticPtr(new EconomicImprovementTactic()));

                    if (plotData_[currentIndex_].flags & PlotImprovementData::ImprovementMakesBonusValid)
                    {
                        impTacticItems_.push_back(IWorkerBuildTacticPtr(new ProvidesResourceTactic()));
                    }
                }
                else if (!isEmpty(node.modifier))
                {
                    improvementPlots_.insert(currentIndex_);
                    // no specific build tactic - passive upgrades to output - so still want to simulate change for this tech
                }
                if (std::find(affectedImprovements_.begin(), affectedImprovements_.end(), node.improvementType) != affectedImprovements_.end())
                {
                    affectedImprovements_.push_back(node.improvementType);
                }
            }

            if (node.removeFeatureType != NO_FEATURE && node.removeFeatureType == plotData_[currentIndex_].removedFeature)
            {
                removeFeaturePlots_.insert(currentIndex_);
                impTacticItems_.push_back(IWorkerBuildTacticPtr(new RemoveFeatureTactic()));

                if (std::find(removeFeatureTypes_.begin(), removeFeatureTypes_.end(), node.removeFeatureType) != removeFeatureTypes_.end())
                {
                    removeFeatureTypes_.push_back(node.removeFeatureType);
                }
            }
        }

        CityImprovementTacticsPtr getTactic() const
        {
            std::vector<PlotImprovementData> affectedPlots;
            for (size_t i = 0, plotCount = plotData_.size(); i < plotCount; ++i)
            {
                bool isImprovement = improvementPlots_.find(i) != improvementPlots_.end();
                bool isFeatureRemove = removeFeaturePlots_.find(i) != removeFeaturePlots_.end();

                if (isImprovement || isFeatureRemove)
                {
                    affectedPlots.push_back(plotData_[i]);
                    if (isFeatureRemove)
                    {
                        // todo - handle any output from removing the feature (chopping)
                        affectedPlots.rbegin()->improvement = NO_IMPROVEMENT;
                        const CvPlot* pPlot = gGlobals.getMap().plot(plotData_[i].coords.iX, plotData_[i].coords.iY);
                        const PlotInfo::PlotInfoNode& plotInfo = player_.getAnalysis()->getMapAnalysis()->getPlotInfoNode(pPlot);
                        affectedPlots.rbegin()->yield = getYield(plotInfo, player_.getPlayerID(), NO_IMPROVEMENT, NO_FEATURE, pPlot->getRouteType(), false);
                    }
                }                
            }

            CityImprovementTacticsPtr pTactic = CityImprovementTacticsPtr(new CityImprovementTactics(affectedPlots));
            for (size_t i = 0, count = impTacticItems_.size(); i < count; ++i)
            {
                pTactic->addTactic(impTacticItems_[i]);
            }
            return pTactic;
        }

    private:
        std::vector<PlotImprovementData> plotData_;
        std::set<size_t> improvementPlots_, removeFeaturePlots_;
        std::vector<ImprovementTypes> affectedImprovements_;
        std::vector<FeatureTypes> removeFeatureTypes_;
        size_t currentIndex_;
        const Player& player_;
        const City& city_;
        std::vector<IWorkerBuildTacticPtr> impTacticItems_;
    };

    class MakePlayerTechTacticsVisitor : public boost::static_visitor<>
    {
    public:
        MakePlayerTechTacticsVisitor(TechTypes techType, const Player& player) : techType_(techType), player_(player), isEconomicTech_(false)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const TechInfo::BaseNode& node)
        {
            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }
        }

        void operator() (const TechInfo::BuildingNode& node)
        {
            if (!node.obsoletes)
            {
                if (gGlobals.getBuildingInfo(node.buildingType).getProductionCost() > 0)
                {
                    BuildingClassTypes buildingClassType = (BuildingClassTypes)gGlobals.getBuildingInfo(node.buildingType).getBuildingClassType();
                    const bool isWorldWonder = isWorldWonderClass(buildingClassType), isNationalWonder = isNationalWonderClass(buildingClassType);

                    if (!isWorldWonder && !isNationalWonder)
                    {
                        pTactics_ = makeTactics_();
                        pTactics_->addTactic(ITechTacticPtr(new ConstructBuildingTechTactic(node.buildingType)));
                    }
                }
            }
        }

        void operator() (const TechInfo::ImprovementNode& node)
        {
            if (!isEmpty(node.modifier))
            {
                isEconomicTech_ = true;
            }
        }

        void operator() (const TechInfo::BonusNode& node)
        {
            if (node.tradeBonus != NO_BONUS)
            {
                pTactics_ = makeTactics_();
                pTactics_->addTactic(ITechTacticPtr(new ProvidesResourceTechTactic(node.tradeBonus)));
            }
        }

        void operator() (const TechInfo::RouteNode& node)
        {
            if (node.routeType != NO_ROUTE)
            {
                pTactics_ = makeTactics_();
                pTactics_->addTactic(ITechTacticPtr(new ConnectsResourcesTechTactic(node.routeType)));
            }
        }

        void operator() (const TechInfo::TradeNode& node)
        {
            if (node.extraTradeRoutes != 0)
            {
                isEconomicTech_ = true;
            }
        }

        void operator() (const TechInfo::FirstToNode& node)
        {
            if (node.freeTechCount > 0)
            {
                pTactics_ = makeTactics_();
                pTactics_->addTactic(ITechTacticPtr(new FreeTechTactic()));
            }

            if (node.foundReligion && !gGlobals.getGame().isReligionFounded(node.defaultReligionType))
            {                
                pTactics_ = makeTactics_();
                pTactics_->addTactic(ITechTacticPtr(new FoundReligionTechTactic(node.defaultReligionType)));
            }
        }

        const ITechTacticsPtr& getTactics()
        {
            if (isEconomicTech_)
            {
                pTactics_ = makeTactics_();
                pTactics_->addTactic(ITechTacticPtr(new EconomicTechTactic()));
            }
            return pTactics_;
        }

    private:        
        ITechTacticsPtr makeTactics_()
        {
            if (!pTactics_)
            {
                pTactics_ = ITechTacticsPtr(new PlayerTechTactics(techType_, player_.getPlayerID()));
            }
            return pTactics_;
        }

        TechTypes techType_;
        const Player& player_;
        ITechTacticsPtr pTactics_;
        bool isEconomicTech_;
    };

    std::list<CityImprovementTacticsPtr> makeCityBuildTactics(const Player& player, const City& city)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        std::list<CityImprovementTacticsPtr> cityBuildTactics;

        std::vector<PlotImprovementData> techImprovements = 
            getProjectedTechImprovements(IDInfo(player.getPlayerID(), city.getID()));

        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const int depth = player.getTechResearchDepth((TechTypes)i);

            if (depth > 0 && depth < 3)
            {
                const boost::shared_ptr<TechInfo>& pTechInfo = player.getAnalysis()->getTechInfo((TechTypes)i);
                std::vector<ImprovementTypes> affectedImprovements;
                std::vector<FeatureTypes> removeFeatureTypes;
                if (techAffectsImprovements(pTechInfo, affectedImprovements, removeFeatureTypes))
                {
//#ifdef ALTAI_DEBUG
//                    os << "\nchecking imp tactics for: " << gGlobals.getTechInfo((TechTypes)i).getType();
//                    os << " ";
//                    for (size_t j = 0, impCount = affectedImprovements.size(); j < impCount; ++j)
//                    {
//                        os << gGlobals.getImprovementInfo(affectedImprovements[j]).getType() << " ";
//                    }
//                    os << " ";
//                    for (size_t j = 0, featureCount = removeFeatureTypes.size(); j < featureCount; ++j)
//                    {
//                        os << gGlobals.getFeatureInfo(removeFeatureTypes[j]).getType() << " ";
//                    }
//#endif
                    MakeCityBuildTacticsVisitor visitor(player, city, techImprovements, affectedImprovements, removeFeatureTypes);
                    boost::apply_visitor(visitor, pTechInfo->getInfo());

                    const CityImprovementTacticsPtr& pTactic = visitor.getTactic();
                    if (pTactic)
                    {
                        pTactic->addDependency(ResearchTechDependencyPtr(new ResearchTechDependency(pTechInfo->getTechType())));

#ifdef ALTAI_DEBUG
                        os << " - created imp tactic: ";
                        pTactic->debug(os);
#endif

                        cityBuildTactics.push_back(pTactic);
                    }
                }
            }
        }

        return cityBuildTactics;
    }

    ITechTacticsPtr makeTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        MakePlayerTechTacticsVisitor visitor(pTechInfo->getTechType(), player);
        boost::apply_visitor(visitor, pTechInfo->getInfo());

        return visitor.getTactics();
    }
}