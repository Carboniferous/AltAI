#pragma once

#include "./utils.h"

namespace AltAI
{
    std::string getLogDirectory();

    PlotYield getSpecialistYield(const CvPlayer& player, SpecialistTypes specialistType);
    Commerce getSpecialistCommerce(const CvPlayer& player, SpecialistTypes specialistType);

    int getActualUpgradeTurns(int rawTurns, int upgradeRate);
    ImprovementTypes getBaseImprovement(ImprovementTypes upgradedType);
    bool improvementIsUpgradeOf(ImprovementTypes currentPlotImprovement, ImprovementTypes requestedImprovement);
    bool improvementIsFinalUpgrade(ImprovementTypes improvementType);
    bool nextImprovementIsFinalUpgrade(ImprovementTypes improvementType);

    ImprovementTypes getBonusImprovementType(BonusTypes bonusType);

    int getImprovementBuildTime(BuildTypes buildType, FeatureTypes featureType, TerrainTypes terrainType);

    ProcessTypes getProcessType(CommerceTypes commerceType);

    int getNumVassalCities(const CvPlayer& player);
    bool isFriendlyCity(TeamTypes teamId, const CvCity* pCity);
    std::vector<int> getBorderingSubAreas(TeamTypes teamId, const CvPlot* pPlot);

    BuildingClassTypes getBuildingClass(BuildingTypes buildingType);
    BuildingTypes getPlayerVersion(PlayerTypes playerType, BuildingClassTypes buildingClassType);
    UnitTypes getPlayerVersion(PlayerTypes playerType, UnitClassTypes unitClassType);

    TeamTypes PlayerIDToTeamID(PlayerTypes player);

    std::string safeGetCityName(IDInfo city);
    std::string safeGetCityName(const CvCity* pCity);

    std::string getUnitAIString(UnitAITypes eUnitAI);
    std::string getMissionAIString(MissionAITypes eMissionAI);
}