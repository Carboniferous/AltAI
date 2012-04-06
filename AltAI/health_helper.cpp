#include "./health_helper.h"
#include "./city_data.h"
#include "./building_helper.h"

namespace AltAI
{
    HealthHelper::HealthHelper(const CvCity* pCity) : pCity_(pCity)
    {
        POWER_HEALTH_CHANGE_ = gGlobals.getDefineINT("POWER_HEALTH_CHANGE");
        DIRTY_POWER_HEALTH_CHANGE_  = gGlobals.getDefineINT("DIRTY_POWER_HEALTH_CHANGE");

        population_ = pCity_->getPopulation();
        const CvPlayerAI& player = CvPlayerAI::getPlayer(pCity_->getOwner());

        espionageHealthCounter_ = pCity_->getEspionageHealthCounter();

        freshWaterBadHealth_ = pCity_->getFreshWaterBadHealth();
        freshWaterGoodHealth_ = pCity_->getFreshWaterGoodHealth();
        
        featureBadHealth_ = pCity_->getFeatureBadHealth();
        featureGoodHealth_ = pCity_->getFeatureGoodHealth();

        powerBadHealth_ = pCity_->getPowerBadHealth();
        powerGoodHealth_ = pCity_->getPowerGoodHealth();

        bonusBadHealth_ = pCity_->getBonusBadHealth();
        bonusGoodHealth_ = pCity_->getBonusGoodHealth();

        buildingBadHealth_ = pCity_->getBuildingBadHealth();
        buildingGoodHealth_ = pCity_->getBuildingGoodHealth();

        areaBuildingBadHealth_ = pCity_->area()->getBuildingBadHealth(pCity_->getOwner());
        areaBuildingGoodHealth_ = pCity_->area()->getBuildingGoodHealth(pCity_->getOwner());

        playerBuildingBadHealth_ = player.getBuildingBadHealth();
        playerBuildingGoodHealth_ = player.getBuildingGoodHealth();

        extraBuildingBadHealth_ = pCity_->getExtraBuildingBadHealth();
        extraBuildingGoodHealth_ = pCity_->getExtraBuildingGoodHealth();

        extraPlayerHealth_ = player.getExtraHealth();
        extraHealth_ = pCity_->getExtraHealth();

        levelHealth_ = gGlobals.getHandicapInfo(pCity_->getHandicapType()).getHealthBonus();

        noUnhealthinessFromPopulation_ = pCity_->isNoUnhealthyPopulation();
        noUnhealthinessFromBuildings_ = pCity_->isBuildingOnlyHealthy();
    }

    HealthHelperPtr HealthHelper::clone() const
    {
        HealthHelperPtr copy = HealthHelperPtr(new HealthHelper(*this));
        return copy;
    }

    int HealthHelper::goodHealth() const
    {
        int iTotalHealth = 0;

        iTotalHealth += std::max<int>(0, freshWaterGoodHealth_);
        iTotalHealth += std::max<int>(0, featureGoodHealth_);
        iTotalHealth += std::max<int>(0, powerGoodHealth_);
        iTotalHealth += std::max<int>(0, bonusGoodHealth_);
        iTotalHealth += std::max<int>(0, buildingGoodHealth_ + areaBuildingGoodHealth_ + playerBuildingGoodHealth_ + extraBuildingGoodHealth_);
        iTotalHealth += std::max<int>(0, extraPlayerHealth_ + extraHealth_);
        iTotalHealth += std::max<int>(0, levelHealth_);

	    return iTotalHealth;
    }

    int HealthHelper::badHealth() const
    {
        int iTotalHealth = 0;

        iTotalHealth -= std::max<int>(0, espionageHealthCounter_);
        iTotalHealth += std::min<int>(0, freshWaterBadHealth_);
        iTotalHealth += std::min<int>(0, featureBadHealth_);
        iTotalHealth += std::min<int>(0, powerBadHealth_);
        iTotalHealth += std::min<int>(0, bonusBadHealth_);
        if (!noUnhealthinessFromBuildings_)
        {
            iTotalHealth += std::min<int>(0, buildingBadHealth_ + areaBuildingBadHealth_ + playerBuildingBadHealth_ + extraBuildingBadHealth_);
        }
        iTotalHealth += std::min<int>(0, extraPlayerHealth_ + extraHealth_);
        iTotalHealth += std::min<int>(0, levelHealth_);
        iTotalHealth += std::min<int>(0, extraBuildingBadHealth_);  // ?? seems odd to count this twice - but that's what the code in CvCity does

    	return unhealthyPopulation_() - iTotalHealth;
    }

    void HealthHelper::advanceTurn()
    {
        if (espionageHealthCounter_ > 0)
        {
            --espionageHealthCounter_;
        }
    }

    void HealthHelper::setPopulation(int population)
    {
        population_ = population;
    }

    void HealthHelper::changePlayerHealthiness(int change)
    {
        extraPlayerHealth_ += change;
    }

    void HealthHelper::changeBuildingGoodHealthiness(int change)
    {
        buildingGoodHealth_ += change;
    }

    void HealthHelper::changeBuildingBadHealthiness(int change)
    {
        buildingBadHealth_ += change;
    }

    void HealthHelper::changeExtraBuildingGoodHealthiness(int change)
    {
        extraBuildingGoodHealth_ += change;
    }

    void HealthHelper::changeExtraBuildingBadHealthiness(int change)
    {
        extraBuildingBadHealth_ += change;
    }

    void HealthHelper::changeBonusGoodHealthiness(int change)
    {
        bonusGoodHealth_ += change;
    }

    void HealthHelper::changeBonusBadHealthiness(int change)
    {
        bonusBadHealth_ += change;
    }

    void HealthHelper::setNoUnhealthinessFromBuildings()
    {
        noUnhealthinessFromBuildings_ = true;
    }

    void HealthHelper::updatePowerHealth(const CityData& cityData)
    {
        if (cityData.getBuildingsHelper()->isPower())
        {
            if (POWER_HEALTH_CHANGE_ > 0)
            {
                powerGoodHealth_ = POWER_HEALTH_CHANGE_;
            }
            else
            {
                powerBadHealth_ = POWER_HEALTH_CHANGE_;
            }
        }

        if (cityData.getBuildingsHelper()->isDirtyPower())
        {
            if (DIRTY_POWER_HEALTH_CHANGE_ > 0)
            {
                powerGoodHealth_ += DIRTY_POWER_HEALTH_CHANGE_;
            }
            else
            {
                powerBadHealth_ += DIRTY_POWER_HEALTH_CHANGE_;
            }
        }
    }

    void HealthHelper::setNoUnhealthinessFromPopulation()
    {
        noUnhealthinessFromPopulation_ = true;
    }

    int HealthHelper::unhealthyPopulation_() const
    {
        return noUnhealthinessFromPopulation_ ? 0 : population_;  // todo - subtraction of angry pop if required (for building settlers?)
    }
}