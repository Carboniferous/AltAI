#include "./tech_tactics_visitors.h"
#include "./tech_info.h"
#include "./tech_info_visitors.h"
#include "./building_info_visitors.h"
#include "./civic_info_visitors.h"
#include "./resource_info_visitors.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./helper_fns.h"
#include "./city.h"
#include "./civ_log.h"

namespace AltAI
{
    class MakeReligionTechConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeReligionTechConditionsVisitor(const Player& player, TechTypes techType) : player_(player), researchTech_(techType, player.getAnalysis()->getTechResearchDepth(techType))
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

        void operator() (const TechInfo::FirstToNode& node)
        {
            if (node.foundReligion)
            {
                for (int i = 0, count = gGlobals.getNumReligionInfos(); i < count; ++i)
                {
                    const CvReligionInfo& religionInfo = gGlobals.getReligionInfo((ReligionTypes)i);
                    TechTypes religionTech = (TechTypes)religionInfo.getTechPrereq();
                    if (religionTech == researchTech_.techType)
                    {
                        if (!gGlobals.getGame().isReligionSlotTaken((ReligionTypes)i))
                        {
                            researchTech_.techFlags |= TechFlags::Found_Religion;
                        }
                        break;
                    }
                }
            }
        }

        ResearchTech getResearchTech() const
        {
            return researchTech_.hasFlags() ? researchTech_ : ResearchTech();
        }

    private:
        const Player& player_;
        ResearchTech researchTech_;
    };

    class MakeEconomicTechConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeEconomicTechConditionsVisitor(const Player& player, TechTypes techType) : player_(player), researchTech_(techType, player.getAnalysis()->getTechResearchDepth(techType))
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
                boost::shared_ptr<BuildingInfo> pBuildingInfo = player_.getAnalysis()->getBuildingInfo(node.buildingType);
                if (pBuildingInfo && buildingHasPotentialEconomicImpact(pBuildingInfo))
                {
                    researchTech_.possibleBuildings.push_back(node.buildingType);
                }
            }
        }

        void operator() (const TechInfo::CivicNode& node)
        {
            if (civicHasPotentialEconomicImpact(player_.getPlayerID(), player_.getAnalysis()->getCivicInfo(node.civicType)))
            {
                researchTech_.possibleCivics.push_back(node.civicType);
            }
        }

        void operator() (const TechInfo::ImprovementNode& node)
        {
            if (node.modifier[YIELD_FOOD] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Food;
            }
            if (node.modifier[YIELD_PRODUCTION] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Production;
            }
            if (node.modifier[YIELD_COMMERCE] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Commerce;
            }
            if (node.workerSpeedModifier > 0)
            {
                researchTech_.workerFlags |= WorkerFlags::Faster_Workers;
            }
        }

        void operator() (const TechInfo::BonusNode& node)
        {
            // if trade and reveal are same tech - both bonus types would be the same - so only need to add once
            if (node.revealBonus != NO_BONUS)
            {
                researchTech_.possibleBonuses.push_back(node.revealBonus);
            }
            else if (node.tradeBonus != NO_BONUS)
            {
                researchTech_.possibleBonuses.push_back(node.tradeBonus);
            }
        }

        void operator() (const TechInfo::CommerceNode& node)
        {
            researchTech_.techFlags |= TechFlags::Flexible_Commerce;

            if (node.adjustableCommerceType == COMMERCE_GOLD)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Gold;
            }
            else if (node.adjustableCommerceType == COMMERCE_RESEARCH)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Research;
            }
            else if (node.adjustableCommerceType == COMMERCE_CULTURE)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Culture;
            }
            else if (node.adjustableCommerceType == COMMERCE_ESPIONAGE)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Espionage;
            }
        }

        void operator() (const TechInfo::ProcessNode& node)
        {
            researchTech_.techFlags |= TechFlags::Flexible_Commerce;

            if (node.productionToCommerceModifier[COMMERCE_GOLD] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Gold;
            }
            else if (node.productionToCommerceModifier[COMMERCE_RESEARCH] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Research;
            }
            else if (node.productionToCommerceModifier[COMMERCE_CULTURE] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Culture;
            }
            else if (node.productionToCommerceModifier[COMMERCE_ESPIONAGE] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Espionage;
            }

            researchTech_.possibleProcesses.push_back(node.processType);
        }

        void operator() (const TechInfo::TradeNode& node)
        {
            if (node.isRiverTrade || node.terrainType != NO_TERRAIN || node.extraTradeRoutes > 0)
            {
                researchTech_.techFlags |= TechFlags::Trade_Routes;
            }
        }

        void operator() (const TechInfo::FirstToNode& node)
        {
            if (node.foundReligion)
            {
                for (int i = 0, count = gGlobals.getNumReligionInfos(); i < count; ++i)
                {
                    const CvReligionInfo& religionInfo = gGlobals.getReligionInfo((ReligionTypes)i);
                    TechTypes religionTech = (TechTypes)religionInfo.getTechPrereq();
                    if (religionTech == researchTech_.techType)
                    {
                        if (!gGlobals.getGame().isReligionSlotTaken((ReligionTypes)i))
                        {
                            researchTech_.economicFlags |= EconomicFlags::Output_Culture;
                        }
                        break;
                    }
                }
            }
            if (node.freeTechCount > 0)
            {
                researchTech_.techFlags |= TechFlags::Free_Tech;
            }
            if (node.freeUnitClass != NO_UNITCLASS)
            {
                researchTech_.techFlags |= TechFlags::Free_GP;
            }
        }

        void operator() (const TechInfo::MiscEffectNode& node)
        {
            //bool enablesOpenBorders, enablesTechTrading, enablesGoldTrading, enablesMapTrading;
            //bool enablesWaterWork, ignoreIrrigation, carriesIrrigation;
            //bool extraWaterSight, centresMap;
            //bool enablesBridgeBuilding;
            //bool enablesDefensivePacts, enablesPermanentAlliances;
        }

        ResearchTech getResearchTech() const
        {
            return researchTech_.hasFlags() || !researchTech_.possibleBuildings.empty() || !researchTech_.possibleCivics.empty() ? researchTech_ : ResearchTech();
        }

    private:

        ResearchTech researchTech_;
        const Player& player_;
    };

    class MakeMilitaryTechConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeMilitaryTechConditionsVisitor(const Player& player, TechTypes techType) : player_(player), researchTech_(techType, player.getAnalysis()->getTechResearchDepth(techType))
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
                boost::shared_ptr<BuildingInfo> pBuildingInfo = player_.getAnalysis()->getBuildingInfo(node.buildingType);
                if (pBuildingInfo && buildingHasPotentialMilitaryImpact(player_.getPlayerID(), pBuildingInfo))
                {
                    researchTech_.possibleBuildings.push_back(node.buildingType);
                }
            }
        }

        void operator() (const TechInfo::CivicNode& node)
        {
            if (civicHasPotentialMilitaryImpact(player_.getPlayerID(), player_.getAnalysis()->getCivicInfo(node.civicType)))
            {
                researchTech_.possibleCivics.push_back(node.civicType);
            }
        }

        void operator() (const TechInfo::UnitNode& node)
        {
            if (node.unitType != NO_UNIT)
            {
                researchTech_.possibleUnits.push_back(node.unitType);
            }

            if (!node.domainExtraMoves.empty())
            {
                researchTech_.militaryFlags |= MilitaryFlags::Output_Extra_Mobility;
            }

            if (!node.promotions.empty())
            {
                researchTech_.militaryFlags |= MilitaryFlags::Output_Experience;
            }
        }

        void operator() (const TechInfo::BonusNode& node)
        {
            // if trade and reveal are same tech - both bonus types would be the same - so only need to add once
            if (node.revealBonus != NO_BONUS)
            {
                std::pair<int, int> militaryUnitCounts = getResourceMilitaryUnitCount(player_.getAnalysis()->getResourceInfo(node.revealBonus));
                if (militaryUnitCounts.first > 0 || militaryUnitCounts.second > 0)
                {
                    researchTech_.possibleBonuses.push_back(node.revealBonus);
                }
            }
            else if (node.tradeBonus != NO_BONUS)
            {
                std::pair<int, int> militaryUnitCounts = getResourceMilitaryUnitCount(player_.getAnalysis()->getResourceInfo(node.tradeBonus));
                if (militaryUnitCounts.first > 0 || militaryUnitCounts.second > 0)
                {
                    researchTech_.possibleBonuses.push_back(node.tradeBonus);
                }
            }
        }

        void operator() (const TechInfo::FirstToNode& node)
        {
            if (node.freeUnitClass != NO_UNITCLASS)
            {
                UnitTypes unitType = getPlayerVersion(player_.getPlayerID(), node.freeUnitClass);
                if (unitType != NO_UNIT)
                {
                    const CvUnitInfo& unitInfo = gGlobals.getUnitInfo(unitType);

                    for (int i = 0, count = gGlobals.getNumSpecialistInfos(); i < count; ++i)
	                {
		                if (unitInfo.getGreatPeoples(i))
                        {
                            const CvSpecialistInfo& specialistInfo = gGlobals.getSpecialistInfo((SpecialistTypes)i);
                            if (specialistInfo.getExperience() > 0)
                            {
                                researchTech_.techFlags |= TechFlags::Free_GP;
                                break;
                            }
                        }
                    }
                }
            }
        }

        void operator() (const TechInfo::MiscEffectNode& node)
        {
            //bool enablesOpenBorders, enablesTechTrading, enablesGoldTrading, enablesMapTrading;
            //bool enablesWaterWork, ignoreIrrigation, carriesIrrigation;
            //bool extraWaterSight, centresMap;
            //bool enablesBridgeBuilding;
            //bool enablesDefensivePacts, enablesPermanentAlliances;
        }

        ResearchTech getResearchTech() const
        {
            return researchTech_.hasFlags() || !researchTech_.possibleBuildings.empty() || !researchTech_.possibleUnits.empty() || !researchTech_.possibleCivics.empty() ? researchTech_ : ResearchTech();
        }

    private:

        ResearchTech researchTech_;
        const Player& player_;
    };

    class MakeWorkerTechConditionsVisitor : public boost::static_visitor<>
    {
    public:
        MakeWorkerTechConditionsVisitor(const Player& player, TechTypes techType) : player_(player), researchTech_(techType, player.getAnalysis()->getTechResearchDepth(techType))
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

        void operator() (const TechInfo::ImprovementNode& node)
        {
            if (node.modifier[YIELD_FOOD] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Food;
                researchTech_.workerFlags |= WorkerFlags::Better_Improvements;
            }
            if (node.modifier[YIELD_PRODUCTION] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Production;
                researchTech_.workerFlags |= WorkerFlags::Better_Improvements;
            }
            if (node.modifier[YIELD_COMMERCE] > 0)
            {
                researchTech_.economicFlags |= EconomicFlags::Output_Commerce;
                researchTech_.workerFlags |= WorkerFlags::Better_Improvements;
            }

            if (node.workerSpeedModifier > 0)
            {
                researchTech_.workerFlags |= WorkerFlags::Faster_Workers;
            }

            if (node.allowsImprovement)
            {
                researchTech_.possibleImprovements.push_back(node.improvementType);
                researchTech_.workerFlags |= WorkerFlags::New_Improvements;
            }

            if (node.removeFeatureType != NO_FEATURE)
            {
                researchTech_.workerFlags |= WorkerFlags::Remove_Features;
                researchTech_.possibleRemovableFeatures.push_back(node.removeFeatureType);
            }
        }

        void operator() (const TechInfo::BonusNode& node)
        {
            // if trade and reveal are same tech - both bonus types would be the same - so only need to add once
            if (node.revealBonus != NO_BONUS)
            {
                researchTech_.possibleBonuses.push_back(node.revealBonus);
            }
            else if (node.tradeBonus != NO_BONUS)
            {
                researchTech_.possibleBonuses.push_back(node.tradeBonus);
            }
        }

        void operator() (const TechInfo::MiscEffectNode& node)
        {
            //bool enablesWaterWork, ignoreIrrigation, carriesIrrigation;
            //bool enablesBridgeBuilding;
        }

        ResearchTech getResearchTech() const
        {
            return researchTech_.hasFlags() ? researchTech_ : ResearchTech();
        }

    private:

        ResearchTech researchTech_;
        const Player& player_;
    };

    ResearchTech getReligionTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        MakeReligionTechConditionsVisitor visitor(player, pTechInfo->getTechType());
        boost::apply_visitor(visitor, pTechInfo->getInfo());

        return visitor.getResearchTech();
    }

    ResearchTech getEconomicTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        MakeEconomicTechConditionsVisitor visitor(player, pTechInfo->getTechType());
        boost::apply_visitor(visitor, pTechInfo->getInfo());

        return visitor.getResearchTech();
    }

    ResearchTech getMilitaryTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        MakeMilitaryTechConditionsVisitor visitor(player, pTechInfo->getTechType());
        boost::apply_visitor(visitor, pTechInfo->getInfo());

        return visitor.getResearchTech();
    }

    ResearchTech getWorkerTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo)
    {
        MakeWorkerTechConditionsVisitor visitor(player, pTechInfo->getTechType());
        boost::apply_visitor(visitor, pTechInfo->getInfo());

        return visitor.getResearchTech();
    }

}