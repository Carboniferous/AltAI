#include "AltAI.h"

#include "./gamedata_analysis.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"
#include "./tech_info_streams.h"
#include "./buildings_info.h"
#include "./building_info_construct_visitors.h"
#include "./helper_fns.h"
#include "./civ_log.h"

#include <sstream>

namespace AltAI
{
    namespace
    {
        std::vector<BuildTypes> getImpBuildTypes()
        {
            std::vector<BuildTypes> impBuildTypes(gGlobals.getNumImprovementInfos(), NO_BUILD);
            for (int buildInfoIndex = 0, buildInfoCount = gGlobals.getNumBuildInfos(); buildInfoIndex < buildInfoCount; ++buildInfoIndex)
            {
                ImprovementTypes buildImpType = (ImprovementTypes)gGlobals.getBuildInfo((BuildTypes)buildInfoIndex).getImprovement();
                if (buildImpType != NO_IMPROVEMENT)
                {
                    impBuildTypes[buildImpType] = (BuildTypes)buildInfoIndex;
                }
            }
            return impBuildTypes;
        }
    }

    boost::shared_ptr<GameDataAnalysis> GameDataAnalysis::instance_;

    boost::shared_ptr<GameDataAnalysis> GameDataAnalysis::getInstance()
    {
        if (instance_ == NULL)
        {
            instance_ = boost::shared_ptr<GameDataAnalysis>(new GameDataAnalysis());
        }
        return instance_;
    }

    void GameDataAnalysis::analyse()
    {
    }

    void GameDataAnalysis::analyseForPlayer(const Player& player)
    {
        if (!player.getCvPlayer()->isBarbarian() && !player.getCvPlayer()->isMinorCiv() && player.getCvPlayer()->isAlive())
        {
            PlayerTypes playerID = player.getCvPlayer()->getID();

            findUniqueUnits_(playerID);

            playerData_[playerID].conditionalPlotYieldEnhancingBuildings = getConditionalYieldEnchancingBuildings(playerID);
        }
    }

    std::pair<bool, int> GameDataAnalysis::isUniqueUnit(PlayerTypes playerID, int unitClassID) const
    {
        std::map<PlayerTypes, PlayerData>::const_iterator ci(playerData_.find(playerID));
        if (ci != playerData_.end())
        {
            for (size_t i = 0, count = ci->second.uniqueUnits.size(); i < count; ++i)
            {
                if (unitClassID == ci->second.uniqueUnits[i].first)
                {
                    return std::make_pair(true, ci->second.uniqueUnits[i].second);
                }
            }
        }
        return std::make_pair(false, -1);
    }

    void GameDataAnalysis::findUniqueUnits_(PlayerTypes playerID)
    {
        CivilizationTypes civType = CvPlayerAI::getPlayer((PlayerTypes)playerID).getCivilizationType();

        if (civType != NO_CIVILIZATION)
        {
            const int unitClassCount = gGlobals.getNumUnitClassInfos();

            for (int classIndex = 0; classIndex < unitClassCount; ++classIndex)
            {
                if ((gGlobals.getUnitClassInfo((UnitClassTypes)classIndex).getDefaultUnitIndex()) != 
                    gGlobals.getCivilizationInfo(civType).getCivilizationUnits(classIndex))
                {
#ifdef ALTAI_DEBUG
                    {
                        std::ostringstream oss;
                        oss << classIndex << gGlobals.getUnitClassInfo((UnitClassTypes)classIndex).getType() << "\n";
                        OutputDebugString(oss.str().c_str());
                    }
#endif
                    playerData_[playerID].uniqueUnits.push_back(std::make_pair(classIndex, gGlobals.getCivilizationInfo(civType).getCivilizationUnits(classIndex)));
                }
            }
#ifdef ALTAI_DEBUG
            {
                const PlayerData& playerData = playerData_[playerID];
                std::ostringstream oss;
                oss << "Player: " << playerID << " has unique units: ";
                for (int i = 0, count = playerData.uniqueUnits.size(); i < count; ++i)
                {
                    const CvUnitInfo& info = gGlobals.getUnitInfo((::UnitTypes)playerData.uniqueUnits[i].second);
                    oss << info.getType();
                }
                oss << "\n";
                OutputDebugString(oss.str().c_str());
            }
#endif
        }
    }

    // todo - is this a defined int?
    TechTypes GameDataAnalysis::getCanWorkWaterTech()
    {
        static TechTypes tech = getCanWorkWaterTech_();
        return tech;
    }

    TechTypes GameDataAnalysis::getCanWorkWaterTech_()
    {
        const int techCount = gGlobals.getNumTechInfos();

        for (int i = 0; i < techCount; ++i)
        {
            const CvTechInfo& techInfo = gGlobals.getTechInfo((TechTypes)i);
            if (techInfo.isWaterWork())
            {
                return (TechTypes)i;
            }
        }

        return NO_TECH;
    }

    std::vector<TechTypes> GameDataAnalysis::getIgnoreIrrigationTechs()
    {
        static std::vector<TechTypes> techs = getIgnoreIrrigationTechs_();
        return techs;
    }

    std::vector<TechTypes> GameDataAnalysis::getIgnoreIrrigationTechs_()
    {
        std::vector<TechTypes> techs;

        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const CvTechInfo& techInfo = gGlobals.getTechInfo((TechTypes)i);
            if (techInfo.isIgnoreIrrigation())
            {
                techs.push_back((TechTypes)i);
            }
        }

        return techs;
    }

    std::vector<TechTypes> GameDataAnalysis::getCarriesIrrigationTechs()
    {
        static std::vector<TechTypes> techs = getCarriesIrrigationTechs_();
        return techs;
    }

    std::vector<TechTypes> GameDataAnalysis::getCarriesIrrigationTechs_()
    {
        std::vector<TechTypes> techs;

        for (int i = 0, count = gGlobals.getNumTechInfos(); i < count; ++i)
        {
            const CvTechInfo& techInfo = gGlobals.getTechInfo((TechTypes)i);
            if (techInfo.isIrrigation())
            {
                techs.push_back((TechTypes)i);
            }
        }

        return techs;
    }

    BuildTypes GameDataAnalysis::getBuildTypeForImprovementType(ImprovementTypes improvementType)
    {
        static std::vector<BuildTypes> buildTypesForImpTypes = getImpBuildTypes();

        return improvementType != NO_IMPROVEMENT ? buildTypesForImpTypes[improvementType] : NO_BUILD;

        /*if (improvementType != NO_IMPROVEMENT)
        {
            for (int i = 0, count = gGlobals.getNumBuildInfos(); i < count; ++i)
            {
                if (gGlobals.getBuildInfo((BuildTypes)i).getImprovement() == improvementType)
                {
                    return (BuildTypes)i;
                }
            }
        }

        return NO_BUILD;*/
    }

    BuildTypes GameDataAnalysis::getBuildTypeForRouteType(RouteTypes routeType)
    {
        if (routeType != NO_ROUTE)
        {
            for (int i = 0, count = gGlobals.getNumBuildInfos(); i < count; ++i)
            {
                if (gGlobals.getBuildInfo((BuildTypes)i).getRoute() == routeType)
                {
                    return (BuildTypes)i;
                }
            }
        }

        return NO_BUILD;
    }

    GameDataAnalysis::BuildsResourcesMap GameDataAnalysis::getBonusTypesForBuildTypes_()
    {
        BuildsResourcesMap bonusBuildsMap;

        for (int i = 0, bonusCount = gGlobals.getNumBonusInfos(); i < bonusCount; ++i)
        {
            for (int j = 0, improvementCount = gGlobals.getNumImprovementInfos(); j < improvementCount; ++j)
            {
                const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo((ImprovementTypes)j);
                if (improvementInfo.isImprovementBonusMakesValid((BonusTypes)i))
                {
                    BuildTypes buildType = getBuildTypeForImprovementType((ImprovementTypes)j);
                    if (buildType != NO_BUILD)
                    {
                        bonusBuildsMap.insert(std::make_pair(buildType, (BonusTypes)i));
                    }
                }
            }
        }

        return bonusBuildsMap;
    }

    GameDataAnalysis::ResourcesBuildMap GameDataAnalysis::getResourcesBuildMap_()
    {
        ResourcesBuildMap resourcesBuildMap;

        for (int i = 0, bonusCount = gGlobals.getNumBonusInfos(); i < bonusCount; ++i)
        {
            for (int j = 0, improvementCount = gGlobals.getNumImprovementInfos(); j < improvementCount; ++j)
            {
                const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo((ImprovementTypes)j);
                if (improvementInfo.isImprovementBonusMakesValid((BonusTypes)i))
                {
                    BuildTypes buildType = getBuildTypeForImprovementType((ImprovementTypes)j);
                    if (buildType != NO_BUILD)
                    {
                        // assumes only only type of build/improvement can access resource
                        resourcesBuildMap.insert(std::make_pair((BonusTypes)i, buildType));
                        break;
                    }
                }
            }
        }

        return resourcesBuildMap;
    }

    std::vector<BonusTypes> GameDataAnalysis::getBonusTypesForBuildType(BuildTypes buildType)
    {
        static BuildsResourcesMap bonusBuildsMap = getBonusTypesForBuildTypes_();

        std::vector<BonusTypes> bonusTypes;
        const std::pair<BuildsResourcesMap::const_iterator, BuildsResourcesMap::const_iterator> iters = bonusBuildsMap.equal_range(buildType);

        for (BuildsResourcesMap::const_iterator ci(iters.first), ciEnd(iters.second); ci != ciEnd; ++ci)
        {
            bonusTypes.push_back(ci->second);
        }
        return bonusTypes;
    }

    TechTypes GameDataAnalysis::getTechTypeForRouteType(RouteTypes routeType)
    {
        for (int i = 0, count = gGlobals.getNumBuildInfos(); i < count; ++i)
        {
            const CvBuildInfo& buildInfo = gGlobals.getBuildInfo((BuildTypes)i);
            if (buildInfo.getRoute() == routeType)
            {
                return (TechTypes)buildInfo.getTechPrereq();
            }
        }

        return NO_TECH;
    }

    TechTypes GameDataAnalysis::getTechTypeForBuildType(BuildTypes buildType)
    {
        return (TechTypes)gGlobals.getBuildInfo(buildType).getTechPrereq();
    }

    BuildTypes GameDataAnalysis::getBuildTypeToRemoveFeature(FeatureTypes featureType)
    {
        if (featureType != NO_FEATURE)
        {
            for (int i = 0, count = gGlobals.getNumBuildInfos(); i < count; ++i)
            {
                const CvBuildInfo& buildInfo = gGlobals.getBuildInfo((BuildTypes)i);
                if (buildInfo.getImprovement() == NO_IMPROVEMENT && buildInfo.isFeatureRemove(featureType))
                {
                    return (BuildTypes)i;
                }
            }
        }

        return NO_BUILD;
    }

    bool GameDataAnalysis::doesBuildTypeRemoveFeature(BuildTypes buildType, FeatureTypes featureType)
    {
        return buildType == NO_BUILD || featureType == NO_FEATURE ? false : gGlobals.getBuildInfo(buildType).isFeatureRemove(featureType);
    }

    bool GameDataAnalysis::isBadFeature(FeatureTypes featureType)
    {
        if (featureType == NO_FEATURE)
        {
            return false;
        }

        const CvFeatureInfo& featureInfo = gGlobals.getFeatureInfo(featureType);

        return (featureInfo.getYieldChange(YIELD_FOOD) + featureInfo.getYieldChange(YIELD_PRODUCTION)) <= 0 || featureInfo.getHealthPercent() < 0;
    }

    TechTypes GameDataAnalysis::getTechTypeToRemoveFeature(FeatureTypes featureType)
    {
        if (featureType != NO_FEATURE)
        {
            for (int i = 0, count = gGlobals.getNumBuildInfos(); i < count; ++i)
            {
                const CvBuildInfo& buildInfo = gGlobals.getBuildInfo((BuildTypes)i);
                if (buildInfo.getImprovement() == NO_IMPROVEMENT && buildInfo.isFeatureRemove(featureType))
                {
                    return (TechTypes)buildInfo.getFeatureTech(featureType);
                }
            }
        }

        return NO_TECH;
    }

    TechTypes GameDataAnalysis::getTechTypeForResourceBuild(BonusTypes bonusType)
    {
        static ResourcesBuildMap resourcesBuildMap = getResourcesBuildMap_();

        ResourcesBuildMap::const_iterator ci = resourcesBuildMap.find(bonusType);
        if (ci != resourcesBuildMap.end())
        {
            return getTechTypeForBuildType(ci->second);
        }
        return NO_TECH;
    }

    GreatPersonOutput GameDataAnalysis::getSpecialistUnitTypeAndOutput(SpecialistTypes specialistType, PlayerTypes playerType)
    {
        const CvSpecialistInfo& specInfo = gGlobals.getSpecialistInfo(specialistType);
        UnitTypes unitType = getPlayerVersion(playerType, (UnitClassTypes)specInfo.getGreatPeopleUnitClass());
        int output = specInfo.getGreatPeopleRateChange();

        // for information purposes - only barbs can't generate GPP, I think
        FAssertMsg(unitType != NO_UNIT || unitType == NO_UNIT && output == 0 || playerType == BARBARIAN_PLAYER, "Specialist with no corresponding unit type producing gpp points?")

        return GreatPersonOutput(unitType, unitType == NO_UNIT ? 0 : output);
    }

    std::vector<ConditionalPlotYieldEnchancingBuilding> GameDataAnalysis::getConditionalPlotYieldEnhancingBuildings(PlayerTypes playerType, const CvCity* pCity) const
    {
        std::map<PlayerTypes, PlayerData>::const_iterator playerIter(playerData_.find(playerType));

        if (playerIter != playerData_.end())
        {
            if (pCity)  // check can build or have already built (todo - handle lookahead techs)
            {
                std::vector<ConditionalPlotYieldEnchancingBuilding> buildings;
                for (size_t i = 0, count = playerIter->second.conditionalPlotYieldEnhancingBuildings.size(); i < count; ++i)
                {
                    BuildingTypes buildingType = playerIter->second.conditionalPlotYieldEnhancingBuildings[i].buildingType;
                    if (pCity->canConstruct(buildingType) || pCity->getFirstBuildingOrder(buildingType) != -1 || pCity->getNumBuilding(buildingType) > 0)
                    {
                        buildings.push_back(playerIter->second.conditionalPlotYieldEnhancingBuildings[i]);
                    }
                }
                return buildings;
            }
            else
            {
                return playerIter->second.conditionalPlotYieldEnhancingBuildings;
            }
        }
        else
        {
            return std::vector<ConditionalPlotYieldEnchancingBuilding>();
        }
    }
}