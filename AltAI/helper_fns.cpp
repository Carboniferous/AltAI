#include "AltAI.h"

#include "./helper_fns.h"

namespace AltAI
{
    std::string getLogDirectory()
    {
        const char* rootPath = ::getenv("USERPROFILE");
        std::string path = rootPath ? rootPath : "C:";
            
        path.append("\\My Documents\\My Games\\Beyond The Sword\\Logs\\");
        return path;
    }

    PlotYield getSpecialistYield(const CvPlayer& player, SpecialistTypes specialistType)
    {
        PlotYield yield;

        for (int i = 0; i < NUM_YIELD_TYPES; ++i)
        {
            yield[i] = player.specialistYield(specialistType, (YieldTypes)i);
        }

        return yield;
    }

    Commerce getSpecialistCommerce(const CvPlayer& player, SpecialistTypes specialistType)
    {
        Commerce commerce;

        for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
        {
            commerce[i] = player.specialistCommerce(specialistType, (CommerceTypes)i);
        }

        return commerce;
    }

    int getNumVassalCities(const CvPlayer& player)
    {
        // any player on a team which has vassals will get these cities counted (so beware double or worse counting)
        TeamTypes teamType = player.getTeam();

        int iNumVassalCities = 0;
	    for (int iPlayer = 0; iPlayer < MAX_CIV_PLAYERS; ++iPlayer)
	    {
            const CvPlayer& kLoopPlayer = CvPlayerAI::getPlayer((PlayerTypes)iPlayer);
            if (kLoopPlayer.getTeam() != teamType && CvTeamAI::getTeam(kLoopPlayer.getTeam()).isVassal(teamType))
		    {
			    iNumVassalCities += kLoopPlayer.getNumCities();
		    }
	    }
	    
        return iNumVassalCities;
    }

    BuildingClassTypes getBuildingClass(BuildingTypes buildingType)
    {
        return (BuildingClassTypes)gGlobals.getBuildingInfo(buildingType).getBuildingClassType();
    }

    BuildingTypes getPlayerVersion(PlayerTypes playerType, BuildingClassTypes buildingClassType)
    {
        if (buildingClassType == NO_BUILDINGCLASS)
        {
            return NO_BUILDING;
        }

        BuildingTypes defaultBuilding = (BuildingTypes)gGlobals.getBuildingClassInfo(buildingClassType).getDefaultBuildingIndex();
        if (playerType != NO_PLAYER)
        {
            CivilizationTypes civType = CvPlayerAI::getPlayer(playerType).getCivilizationType();

            if (civType != NO_CIVILIZATION)
            {
                BuildingTypes civBuilding = (BuildingTypes)gGlobals.getCivilizationInfo(civType).getCivilizationBuildings(buildingClassType);
                if (defaultBuilding != civBuilding)
                {
                    return civBuilding;
                }
            }
        }
        return defaultBuilding;
    }

    UnitTypes getPlayerVersion(PlayerTypes playerType, UnitClassTypes unitClassType)
    {
        if (unitClassType == NO_UNITCLASS)
        {
            return NO_UNIT;
        }

        UnitTypes defaultUnit = (UnitTypes)gGlobals.getUnitClassInfo(unitClassType).getDefaultUnitIndex();
        if (playerType != NO_PLAYER)
        {
            CivilizationTypes civType = CvPlayerAI::getPlayer(playerType).getCivilizationType();

            if (civType != NO_CIVILIZATION)
            {
                UnitTypes civUnit = (UnitTypes)gGlobals.getCivilizationInfo(civType).getCivilizationUnits(unitClassType);
                if (defaultUnit != civUnit)
                {
                    return civUnit;
                }
            }
        }
        return defaultUnit;
    }

    TeamTypes PlayerIDToTeamID(PlayerTypes player)
    {
        return CvTeamAI::getTeam(CvPlayerAI::getPlayer(player).getTeam()).getID();
    }

    int getActualUpgradeTurns(int rawTurns, int upgradeRate)
    {
        int actualTurns = rawTurns;

        if (upgradeRate > 0)
        {
            actualTurns = rawTurns / upgradeRate;
            if (actualTurns * upgradeRate < rawTurns)
            {
                ++actualTurns;
            }
        }

        return actualTurns;
    }

    ImprovementTypes getBaseImprovement(ImprovementTypes upgradedType)
    {
        if (upgradedType == NO_IMPROVEMENT)
        {
            return NO_IMPROVEMENT;
        }
        
        ImprovementTypes downGradeType = upgradedType, lastType = upgradedType;
        do
        {
            lastType = downGradeType;
            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(downGradeType);
            downGradeType = (ImprovementTypes)improvementInfo.getImprovementPillage();
        }
        while (downGradeType != NO_IMPROVEMENT);

        return lastType;
    }

    bool improvementIsUpgradeOf(ImprovementTypes currentPlotImprovement, ImprovementTypes requestedImprovement)
    {
        if (currentPlotImprovement == NO_IMPROVEMENT || requestedImprovement == NO_IMPROVEMENT)
        {
            return false;
        }

        if (currentPlotImprovement == requestedImprovement)
        {
            return true;
        }

        ImprovementTypes downGradeType = currentPlotImprovement;
        do
        {
            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(downGradeType);
            downGradeType = (ImprovementTypes)improvementInfo.getImprovementPillage();

            if (downGradeType == requestedImprovement)
            {
                return true;
            }
        }
        while (downGradeType != NO_IMPROVEMENT);

        return false;
    }

    bool improvementIsFinalUpgrade(ImprovementTypes improvementType)
    {
        if (improvementType == NO_IMPROVEMENT)
        {
            return false;
        }

        const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(improvementType);

        return improvementInfo.getImprovementUpgrade() == NO_IMPROVEMENT && improvementInfo.getImprovementPillage() != NO_IMPROVEMENT;
    }

    bool nextImprovementIsFinalUpgrade(ImprovementTypes improvementType)
    {
        if (improvementType == NO_IMPROVEMENT)
        {
            return false;
        }

        const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo(improvementType);
        return improvementIsFinalUpgrade((ImprovementTypes)improvementInfo.getImprovementUpgrade());
    }

    ImprovementTypes getBonusImprovementType(BonusTypes bonusType)
    {
        for (int i = 0, count = gGlobals.getNumImprovementInfos(); i < count; ++i)
        {
            const CvImprovementInfo& improvementInfo = gGlobals.getImprovementInfo((ImprovementTypes)i);
            if (improvementInfo.isImprovementBonusMakesValid(bonusType) && !improvementInfo.isActsAsCity())
            {
                return (ImprovementTypes)i;
            }
        }
        return NO_IMPROVEMENT;
    }

    ProcessTypes getProcessType(CommerceTypes commerceType)
    {
        int bestModifier = 0;
        ProcessTypes bestProcessType = NO_PROCESS;

        for (int i = 0, count = gGlobals.getNumProcessInfos(); i < count; ++i)
        {
            const CvProcessInfo& processInfo = gGlobals.getProcessInfo((ProcessTypes)i);
            for (int j = 0; j < NUM_COMMERCE_TYPES; ++j)
            {
                int thisModifier = processInfo.getProductionToCommerceModifier(j);
                if (thisModifier > bestModifier)
                {
                    thisModifier = bestModifier;
                    bestProcessType = (ProcessTypes)i;
                }
            }
        }
        return bestProcessType;
    }
}