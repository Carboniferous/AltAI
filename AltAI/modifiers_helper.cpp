#include "./modifiers_helper.h"
#include "./building_helper.h"
#include "./religion_helper.h"
#include "./bonus_helper.h"
#include "./city_data.h"
#include "./game.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"

namespace AltAI
{
    ModifiersHelper::ModifiersHelper(const CvCity* pCity, CityData& data) : data_(data)
    {
        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity->getOwner());
        pPlayerAnalysis_ = gGlobals.getGame().getAltAI()->getPlayer(pCity->getOwner())->getAnalysis();

        for (int i = 0; i < NUM_YIELD_TYPES; ++i)
        {
            yieldModifier_[i] = pCity->getYieldRateModifier((YieldTypes)i);
            powerYieldModifier_[i] = pCity->getPowerYieldRateModifier((YieldTypes)i);
            bonusYieldModifier_[i] = pCity->getBonusYieldRateModifier((YieldTypes)i);
            areaYieldModifier_[i] = pCity->area()->getYieldRateModifier(pCity->getOwner(), (YieldTypes)i);
            playerYieldModifier_[i] = player.getYieldRateModifier((YieldTypes)i);
            if (pCity->isCapital())
            {
                capitalYieldModifier_[i] = player.getCapitalYieldRateModifier((YieldTypes)i);
            }
        }

        for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
        {
            commerceModifier_[i] = pCity->getCommerceRateModifier((CommerceTypes)i);
            playerCommerceModifier_[i] = player.getCommerceRateModifier((CommerceTypes)i);
            if (pCity->isCapital())
            {
                capitalCommerceModifier_[i] = player.getCapitalCommerceRateModifier((CommerceTypes)i);
            }
        }

        militaryProductionModifier_ = player.getMilitaryProductionModifier();

        spaceProductionModifier_ = pCity->getSpaceProductionModifier();
        playerSpaceProductionModifier_ = player.getSpaceProductionModifier();

        stateReligionBuildingProductionModifier_ = 0;
   		stateReligionBuildingProductionModifier_ = player.getStateReligionBuildingProductionModifier();

        // these don't change in the course of a normal game
        worldWonderProductionModifier_ = player.getMaxGlobalBuildingProductionModifier();
        teamWonderProductionModifier_ = player.getMaxTeamBuildingProductionModifier();
        nationalWonderProductionModifier_ = player.getMaxPlayerBuildingProductionModifier();
    }

    YieldModifier ModifiersHelper::getTotalYieldModifier() const
    {
        YieldModifier modifier = yieldModifier_ + bonusYieldModifier_ + areaYieldModifier_ + playerYieldModifier_ + capitalYieldModifier_;
        if (data_.getBuildingsHelper()->isPower())
        {
            modifier += powerYieldModifier_;
        }

        return modifier;
    }

    CommerceModifier ModifiersHelper::getTotalCommerceModifier() const
    {
        return commerceModifier_ + playerCommerceModifier_ + capitalCommerceModifier_;
    }

    int ModifiersHelper::getUnitProductionModifier(UnitTypes unitType) const
    {
        return (gGlobals.getUnitInfo(unitType).isMilitaryProduction() ? militaryProductionModifier_ : 0) + pPlayerAnalysis_->getPlayerUnitProductionModifier(unitType);
    }

    int ModifiersHelper::getBuildingProductionModifier(BuildingTypes buildingType) const
    {
        const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);
        BuildingClassTypes buildingClassType = (BuildingClassTypes)buildingInfo.getBuildingClassType();

        int modifier = pPlayerAnalysis_->getPlayerBuildingProductionModifier(buildingType);

        if (::isWorldWonderClass(buildingClassType))
        {
            modifier += worldWonderProductionModifier_;
        }
        
        if (::isTeamWonderClass(buildingClassType))
	    {
		    modifier += teamWonderProductionModifier_;
	    }

	    if (::isNationalWonderClass(buildingClassType))
        {
            modifier += nationalWonderProductionModifier_;
        }

        for (int i = 0, count = gGlobals.getNumBonusInfos(); i < count; ++i)
        {
            if (data_.getBonusHelper()->getNumBonuses((BonusTypes)i) > 0)
            {
                modifier += buildingInfo.getBonusProductionModifier(i);
            }
        }

        ReligionTypes religionType = data_.getReligionHelper()->getStateReligion();
        if (religionType != NO_RELIGION && data_.getReligionHelper()->isHasReligion(religionType))
        {
            modifier += stateReligionBuildingProductionModifier_;
        }

        return modifier;
    }

    int ModifiersHelper::getProjectProductionModifier(ProjectTypes projectType) const
    {
        int modifier = 0;
        const CvProjectInfo& projectInfo = gGlobals.getProjectInfo(projectType);

        if (projectInfo.isSpaceship())
        {
            modifier += spaceProductionModifier_;
            modifier += playerSpaceProductionModifier_;
        }

        for (int i = 0, count = gGlobals.getNumBonusInfos(); i < count; ++i)
        {
            if (data_.getBonusHelper()->getNumBonuses((BonusTypes)i) > 0)
            {
                modifier += projectInfo.getBonusProductionModifier(i);
            }
        }

        return modifier;
    }

    void ModifiersHelper::changeYieldModifier(YieldModifier modifier)
    {
        yieldModifier_ += modifier;
    }

    void ModifiersHelper::changePowerYieldModifier(YieldModifier modifier)
    {
        powerYieldModifier_ += modifier;
    }

    void ModifiersHelper::changeBonusYieldModifier(YieldModifier modifier)
    {
        bonusYieldModifier_ += modifier;
    }

    void ModifiersHelper::changeAreaYieldModifier(YieldModifier modifier)
    {
        areaYieldModifier_ += modifier;
    }

    void ModifiersHelper::changePlayerYieldModifier(YieldModifier modifier)
    {
        playerYieldModifier_ += modifier;
    }

    void ModifiersHelper::changeCapitalYieldModifier(YieldModifier modifier)
    {
        capitalYieldModifier_ += modifier;
    }

    void ModifiersHelper::changeCommerceModifier(CommerceModifier modifier)
    {
        commerceModifier_ += modifier;
    }

    void ModifiersHelper::changePlayerCommerceModifier(CommerceModifier modifier)
    {
        playerCommerceModifier_ += modifier;
    }

    void ModifiersHelper::changeCapitalCommerceModifier(CommerceModifier modifier)
    {
        capitalCommerceModifier_ += modifier;
    }

    void ModifiersHelper::changeStateReligionBuildingProductionModifier(int change)
    {
        stateReligionBuildingProductionModifier_ += change;
    }

    void ModifiersHelper::changeMilitaryProductionModifier(int change)
    {
        militaryProductionModifier_ += change;
    }

    void ModifiersHelper::changeSpaceProductionModifier(int change)
    {
        spaceProductionModifier_ += change;
    }

    void ModifiersHelper::changePlayerSpaceProductionModifier(int change)
    {
        playerSpaceProductionModifier_ += change;
    }
}