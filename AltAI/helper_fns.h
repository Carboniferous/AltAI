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

    ProcessTypes getProcessType(CommerceTypes commerceType);

    int getNumVassalCities(const CvPlayer& player);

    BuildingClassTypes getBuildingClass(BuildingTypes buildingType);
    BuildingTypes getPlayerVersion(PlayerTypes playerType, BuildingClassTypes buildingClassType);
    UnitTypes getPlayerVersion(PlayerTypes playerType, UnitClassTypes unitClassType);

    TeamTypes PlayerIDToTeamID(PlayerTypes player);
}