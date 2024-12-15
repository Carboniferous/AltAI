#include "AltAI.h"

#include "./unit_tactics_visitors.h"
#include "./unit_info.h"
#include "./unit_info_visitors.h"
#include "./resource_info_visitors.h"
#include "./city_unit_tactics.h"
#include "./building_tactics_items.h"
#include "./unit_tactics_items.h"
#include "./building_tactics_deps.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./unit_analysis.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./city.h"
#include "./civ_helper.h"
#include "./civ_log.h"

namespace AltAI
{
    class MakeUnitTacticsDependenciesVisitor : public boost::static_visitor<>
    {
    public:
        MakeUnitTacticsDependenciesVisitor(const Player& player, const CvCity* pCity)
            : player_(player), pCity_(pCity)
        {
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            // always add bonus requirements, as cities' access can vary
            if (!node.andBonusTypes.empty() || !node.orBonusTypes.empty())
            {
                std::map<BonusTypes, TechTypes> bonusTypesRevealTechsMap;
                for (size_t i = 0, count = node.andBonusTypes.size(); i < count; ++i)
                {
                    TechTypes resourceRevealTech = getTechForResourceReveal(player_.getAnalysis()->getResourceInfo(node.andBonusTypes[i]));
                    if (resourceRevealTech != NO_TECH)
                    {
                        bonusTypesRevealTechsMap[node.andBonusTypes[i]] = resourceRevealTech;
                    }
                }
                for (size_t i = 0, count = node.orBonusTypes.size(); i < count; ++i)
                {
                    TechTypes resourceRevealTech = getTechForResourceReveal(player_.getAnalysis()->getResourceInfo(node.orBonusTypes[i]));
                    if (resourceRevealTech != NO_TECH)
                    {
                        bonusTypesRevealTechsMap[node.orBonusTypes[i]] = resourceRevealTech;
                    }
                }
                dependentTactics_.push_back(IDependentTacticPtr(new CityBonusDependency(node.andBonusTypes, node.orBonusTypes, bonusTypesRevealTechsMap, node.unitType)));
            }

            for (size_t i = 0, count = node.techTypes.size(); i < count; ++i)
            {
                if (player_.getTechResearchDepth(node.techTypes[i]) > 0)
                {
                    techDependencies_.push_back(ResearchTechDependencyPtr(new ResearchTechDependency(node.techTypes[i])));
                }
            }
            
            bool passedBuildingCheck = node.prereqBuildingType == NO_BUILDING;
            if (!passedBuildingCheck)
            {
                SpecialBuildingTypes specialBuildingType = (SpecialBuildingTypes)gGlobals.getBuildingInfo(node.prereqBuildingType).getSpecialBuildingType();
                if (specialBuildingType != NO_SPECIALBUILDING)
                {
                    passedBuildingCheck = player_.getCivHelper()->getSpecialBuildingNotRequiredCount(specialBuildingType) > 0;
                }
            }

            if (!passedBuildingCheck && pCity_)
            {
                passedBuildingCheck = pCity_->getNumBuilding(node.prereqBuildingType) > 0;
            }

            if (!passedBuildingCheck)
            {
                dependentTactics_.push_back(IDependentTacticPtr(new CityBuildingDependency(node.prereqBuildingType)));
            }
        }

        const std::vector<IDependentTacticPtr>& getDependentTactics() const
        {
            return dependentTactics_;
        }

        const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const
        {
            return techDependencies_;
        }

    private:
        const Player& player_;
        const CvCity* pCity_;
        std::vector<IDependentTacticPtr> dependentTactics_;
        std::vector<ResearchTechDependencyPtr> techDependencies_;
    };

    class MakeUnitTacticsVisitor : public boost::static_visitor<>
    {
    public:
        MakeUnitTacticsVisitor(const Player& player, const City& city, UnitTypes unitType)
            : unitInfo_(gGlobals.getUnitInfo(unitType)), player_(player), city_(city), unitType_(unitType)
        {
            pUnitAnalysis_ = player_.getAnalysis()->getUnitAnalysis();           
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            pTactic_ = CityUnitTacticsPtr(new CityUnitTactics(unitType_, city_.getCvCity()->getIDInfo()));

            MakeUnitTacticsDependenciesVisitor dependentTacticsVisitor(player_, city_.getCvCity());
            dependentTacticsVisitor(node);

            const std::vector<IDependentTacticPtr>& dependentTactics = dependentTacticsVisitor.getDependentTactics();
            for (size_t i = 0, count = dependentTactics.size(); i < count; ++i)
            {
                pTactic_->addDependency(dependentTactics[i]);
            }
            /*const std::vector<ResearchTechDependencyPtr>& techDeps = dependentTacticsVisitor.getTechDependencies();
            for (size_t i = 0, count = techDeps.size(); i < count; ++i)
            {
                pTactic_->addDependency(techDeps[i]);
            }*/

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }            
        }

        void operator() (const UnitInfo::CityCombatNode& node)
        {
        }

        void operator() (const UnitInfo::CollateralNode& node)
        {
        }

        void operator() (const UnitInfo::ReligionNode& node)
        {
            if (!node.religionSpreads.empty())
            {
                pTactic_->addTactic(ICityUnitTacticPtr(new SpreadReligionUnitTactic(node.religionSpreads)));
            }
        }
        
        void operator() (const UnitInfo::MiscAbilityNode& node)
        {
            if (node.canFoundCity)
            {
                pTactic_->addTactic(ICityUnitTacticPtr(new BuildCityUnitTactic()));
            }
        }

        void operator() (const UnitInfo::BuildNode& node)
        {
            pTactic_->addTactic(ICityUnitTacticPtr(new BuildImprovementsUnitTactic(node.buildTypes)));
        }

        void operator() (const UnitInfo::PromotionsNode& node)
        {
            for (std::set<UnitInfo::PromotionsNode::Promotion>::const_iterator ci(node.promotions.begin()), ciEnd(node.promotions.end()); ci != ciEnd; ++ci)
            {
                if (ci->level == 0)
                {
                    freePromotions_.insert(ci->promotionType);
                }
            }
        }
        
        void operator() (const UnitInfo::CombatBonusNode& node)
        {
        }

        void operator() (const UnitInfo::AirCombatNode& node)
        {
        }

        void operator() (const UnitInfo::UpgradeNode& node)
        {
        }

        const CityUnitTacticsPtr& getTactic() const
        {
            return pTactic_;
        }

        const Promotions& getFreePromotions() const
        {
            return freePromotions_;
        }

    private:
        UnitTypes unitType_;
        const CvUnitInfo& unitInfo_;
        const Player& player_;
        const City& city_;
        CityUnitTacticsPtr pTactic_;
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis_;
        Promotions freePromotions_;
    };

    class MakeSpecialUnitTacticsVisitor : public boost::static_visitor<>
    {
    public:
        MakeSpecialUnitTacticsVisitor(const Player& player, UnitTypes unitType)
            : unitInfo_(gGlobals.getUnitInfo(unitType)), player_(player), unitType_(unitType)
        {
            pUnitAnalysis_ = player_.getAnalysis()->getUnitAnalysis();           
        }

        template <typename T>
            void operator() (const T&)
        {
        }

        void operator() (const UnitInfo::BaseNode& node)
        {
            pTactic_ = UnitTacticsPtr(new UnitTactics(player_.getPlayerID(), unitType_));

            for (size_t i = 0, count = node.nodes.size(); i < count; ++i)
            {
                boost::apply_visitor(*this, node.nodes[i]);
            }            
        }

        void operator() (const UnitInfo::MiscAbilityNode& node)
        {
            if (node.canBuildSpecialBuilding)
            {
                for (size_t i = 0, count = node.specialBuildings.buildings.size(); i < count; ++i)
                {
                    pTactic_->addTactic(IBuiltUnitTacticPtr(new BuildSpecialBuildingUnitTactic(node.specialBuildings.buildings[i])));
                    pTactic_->addDependency(boost::shared_ptr<CivUnitDependency>(new CivUnitDependency(unitType_)));
                }
            }

            if (node.canDiscoverTech)
            {
                pTactic_->addTactic(IBuiltUnitTacticPtr(new DiscoverTechUnitTactic()));
            }

            if (node.canCreateGreatWork)
            {
                pTactic_->addTactic(IBuiltUnitTacticPtr(new CreateGreatWorkUnitTactic()));
            }

            if (node.canDoTradeMission)
            {
                pTactic_->addTactic(IBuiltUnitTacticPtr(new TradeMissionUnitTactic()));
            }

            for (size_t i = 0, count = node.settledSpecialists.size(); i < count; ++i)
            {
                pTactic_->addTactic(IBuiltUnitTacticPtr(new JoinCityUnitTactic(node.settledSpecialists[i])));
            }

            if (node.canHurryBuilding)
            {
                pTactic_->addTactic(IBuiltUnitTacticPtr(new HurryBuildingUnitTactic(node.hurryBuilding.baseHurry, node.hurryBuilding.multiplier)));
            }
        }

        const UnitTacticsPtr& getTactic() const
        {
            return pTactic_;
        }

    private:
        UnitTypes unitType_;
        const CvUnitInfo& unitInfo_;
        const Player& player_;
        UnitTacticsPtr pTactic_;
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis_;        
    };

    CityUnitTacticsPtr makeCityUnitTactics(const Player& player, const City& city, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        static const int MAX_HIT_POINTS = gGlobals.getMAX_HIT_POINTS();  // 100

        const UnitTypes unitType = pUnitInfo->getUnitType();
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(pUnitInfo->getUnitType());
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis = player.getAnalysis()->getUnitAnalysis();   

        MakeUnitTacticsVisitor visitor(player, city, pUnitInfo->getUnitType());
        boost::apply_visitor(visitor, pUnitInfo->getInfo());
        CityUnitTacticsPtr pCityUnitTactics = visitor.getTactic();

        int freeExperience = ((CvCity*)city.getCvCity())->getProductionExperience(unitType);
        int maxPromotionLevel = player.getAnalysis()->getUnitLevel(freeExperience);

        if (unitInfo.getDomainType() == DOMAIN_LAND && unitInfo.getCombat() > 0)
        {
            if (!unitInfo.isOnlyDefensive())
            {
                {
                    /*UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCityAttackPromotions(unitType, maxPromotionLevel);

                    if (levelsAndPromotions.second.empty())
                    {
                        levelsAndPromotions = pUnitAnalysis->getCombatPromotions(unitType, maxPromotionLevel);
                    }

                    UnitData unitData(unitType, levelsAndPromotions.second);*/
                    pCityUnitTactics->addTactic(ICityUnitTacticPtr(new CityAttackUnitTactic(visitor.getFreePromotions())));
                }

                if (unitInfo.getCombatLimit() == 100)
                {
                    /*UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCombatPromotions(unitType, maxPromotionLevel);

                    UnitData unitData(unitType, levelsAndPromotions.second);*/
                    pCityUnitTactics->addTactic(ICityUnitTacticPtr(new FieldAttackUnitTactic(visitor.getFreePromotions())));
                }

                // todo - refine this check
                if (unitInfo.getCollateralDamage() > 0)
                {
                    /*UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCityAttackPromotions(unitType, maxPromotionLevel);

                    if (levelsAndPromotions.second.empty())
                    {
                        levelsAndPromotions = pUnitAnalysis->getCollateralPromotions(unitType, maxPromotionLevel);
                    }

                    UnitData unitData(unitType, levelsAndPromotions.second);*/
                    pCityUnitTactics->addTactic(ICityUnitTacticPtr(new CollateralUnitTactic(visitor.getFreePromotions())));
                }
            }
            // can include defensive only units here, e.g. machine guns
            if (unitInfo.isMilitaryHappiness())  // to exclude scouts and explorers
            {
                /*UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCityDefencePromotions(unitType, maxPromotionLevel);

                if (levelsAndPromotions.second.empty())
                {
                    levelsAndPromotions = pUnitAnalysis->getCityDefencePromotions(unitType, maxPromotionLevel);
                }

                UnitData unitData(unitType, levelsAndPromotions.second);*/
                pCityUnitTactics->addTactic(ICityUnitTacticPtr(new CityDefenceUnitTactic(visitor.getFreePromotions())));
                pCityUnitTactics->addTactic(ICityUnitTacticPtr(new ThisCityDefenceUnitTactic(visitor.getFreePromotions())));
            
                /*UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCombatPromotions(unitType, maxPromotionLevel);

                UnitData unitData(unitType, levelsAndPromotions.second);*/
                pCityUnitTactics->addTactic(ICityUnitTacticPtr(new FieldDefenceUnitTactic(visitor.getFreePromotions())));
            }

            {
                //UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCombatPromotions(unitType, maxPromotionLevel);
                //// todo - get movement promotions
                //UnitData unitData(unitType, levelsAndPromotions.second);
                pCityUnitTactics->addTactic(ICityUnitTacticPtr(new ScoutUnitTactic(visitor.getFreePromotions())));
            }
        }
        else if (unitInfo.getDomainType() == DOMAIN_SEA)
        {
            if (unitInfo.getCombat() > 0 && !unitInfo.isOnlyDefensive())
            {
                /*UnitAnalysis::RemainingLevelsAndPromotions levelsAndPromotions = pUnitAnalysis->getCombatPromotions(unitType, maxPromotionLevel);

                UnitData unitData(unitType, levelsAndPromotions.second);*/

                //if (unitData.baseCombat > 0)
                {
                    pCityUnitTactics->addTactic(ICityUnitTacticPtr(new SeaAttackUnitTactic(visitor.getFreePromotions())));
                }
            }

            if (unitInfo.getCargoSpace() > 0)
            {
                // todo
            }

            // todo - add sea scout unit tactic entries
        }

        return pCityUnitTactics;
    }

    UnitTacticsPtr makeUnitTactics(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        const int lookAheadDepth = 2;
        if (couldConstructUnit(player, lookAheadDepth, pUnitInfo, true, true))
        {
            const UnitTypes unitType = pUnitInfo->getUnitType();
            const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

            UnitTacticsPtr pTactic(new UnitTactics(player.getPlayerID(), unitType));

            CityIter iter(*player.getCvPlayer());

            while (CvCity* pCity = iter())
            {
                const City& city = player.getCity(pCity);
#ifdef ALTAI_DEBUG
                std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
                // this does the area check for units which have min area reqs
                if (!couldEverConstructUnit(player, city, pUnitInfo, lookAheadDepth))
                {
#ifdef ALTAI_DEBUG
                    os << "\n" << __FUNCTION__ << " Skipping tactic for unit: " << unitInfo.getType() << " for city: " << narrow(pCity->getName());
#endif
                    continue;
                }
#ifdef ALTAI_DEBUG
                os << "\n" << __FUNCTION__ << " Adding tactic for unit: " << unitInfo.getType() << " for city: " << narrow(pCity->getName());
#endif
                
                pTactic->addCityTactic(pCity->getIDInfo(), makeCityUnitTactics(player, city, pUnitInfo));
#ifdef ALTAI_DEBUG
                pTactic->debug(os);
#endif
            }

            MakeUnitTacticsDependenciesVisitor dependentTacticsVisitor(player, NULL);
            boost::apply_visitor(dependentTacticsVisitor, pUnitInfo->getInfo());

            const std::vector<ResearchTechDependencyPtr>& dependentTechs = dependentTacticsVisitor.getTechDependencies();
            for (size_t i = 0, count = dependentTechs.size(); i < count; ++i)
            {
                pTactic->addTechDependency(dependentTechs[i]);
            }

            return pTactic;
        }

        return UnitTacticsPtr();
    }

    UnitTacticsPtr makeSpecialUnitTactics(const Player& player, const boost::shared_ptr<UnitInfo>& pUnitInfo)
    {
        const UnitTypes unitType = pUnitInfo->getUnitType();
        const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

        boost::shared_ptr<UnitAnalysis> pUnitAnalysis = player.getAnalysis()->getUnitAnalysis();   

        MakeSpecialUnitTacticsVisitor visitor(player, pUnitInfo->getUnitType());
        boost::apply_visitor(visitor, pUnitInfo->getInfo());
        return visitor.getTactic();
    }
}