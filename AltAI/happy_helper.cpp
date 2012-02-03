#include "./happy_helper.h"

namespace AltAI
{
    HappyHelper::HappyHelper(const CvCity* pCity)
        : pCity_(pCity), hurryHelper_(pCity)
    {
        population_ = pCity_->getPopulation();
        PERCENT_ANGER_DIVISOR_ = gGlobals.getPERCENT_ANGER_DIVISOR();
        const CvPlayer& player = CvPlayerAI::getPlayer(pCity_->getOwner());

        noUnhappiness_ = pCity_->isNoUnhappiness();
        overcrowdingPercentAnger_ = pCity_->getOvercrowdingPercentAnger();
        noMilitaryPercentAnger_ = pCity_->getNoMilitaryPercentAnger();
        culturePercentAnger_ = pCity_->getCulturePercentAnger();
        religionPercentAnger_ = pCity_->getReligionPercentAnger();

        conscriptPercentAnger_ = pCity_->getConscriptPercentAnger();
        defyResolutionPercentAnger_ = pCity_->getDefyResolutionPercentAnger();
        warWearinessPercentAnger_ = pCity_->getWarWearinessPercentAnger();

        for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
        {
            civicPercentAnger_.push_back(player.getCivicPercentAnger((CivicTypes)i));
        }

        targetNumCities_ = gGlobals.getWorldInfo(gGlobals.getMap().getWorldSize()).getTargetNumCities();

        largestCityHappiness_ = pCity_->getLargestCityHappiness();
        militaryHappiness_ = pCity_->getMilitaryHappiness();
        currentStateReligionHappiness_ = pCity_->getCurrentStateReligionHappiness();

        buildingBadHappiness_ = pCity_->getBuildingBadHappiness();
        buildingGoodHappiness_ = pCity_->getBuildingGoodHappiness();

        extraBuildingBadHappiness_ = pCity_->getExtraBuildingBadHappiness();
        extraBuildingGoodHappiness_ = pCity_->getExtraBuildingGoodHappiness();

        featureBadHappiness_ = pCity_->getFeatureBadHappiness();
        featureGoodHappiness_ = pCity_->getFeatureGoodHappiness();

        bonusBadHappiness_ = pCity_->getBonusBadHappiness();
        bonusGoodHappiness_ = pCity_->getBonusGoodHappiness();

        religionBadHappiness_ = pCity_->getReligionBadHappiness();
        religionGoodHappiness_ = pCity_->getReligionGoodHappiness();

        commerceHappiness_ = pCity_->getCommerceHappiness();
        areaBuildingHappiness_ = pCity_->area()->getBuildingHappiness(pCity_->getOwner());
        playerBuildingHappiness_ = player.getBuildingHappiness();
        cityExtraHappiness_ = pCity_->getExtraHappiness();
        playerExtraHappiness_ = player.getExtraHappiness();
        levelHappyBonus_ = gGlobals.getHandicapInfo(pCity_->getHandicapType()).getHappyBonus();

        vassalUnhappiness_ = pCity_->getVassalUnhappiness();
        vassalHappiness_ = pCity_->getVassalHappiness();

        espionageHappinessCounter_ = pCity_->getEspionageHappinessCounter();

        tempHappyTimer_ = pCity_->getHappinessTimer();
        TEMP_HAPPY_ = gGlobals.getDefineINT("TEMP_HAPPY");
    }

    int HappyHelper::happyPopulation() const
    {
        int iHappiness = 0;

	    iHappiness += std::max<int>(0, largestCityHappiness_);
	    iHappiness += std::max<int>(0, militaryHappiness_);
	    iHappiness += std::max<int>(0, currentStateReligionHappiness_);
	    iHappiness += std::max<int>(0, buildingGoodHappiness_);
	    iHappiness += std::max<int>(0, extraBuildingGoodHappiness_);
	    iHappiness += std::max<int>(0, featureGoodHappiness_);
	    iHappiness += std::max<int>(0, bonusGoodHappiness_);
	    iHappiness += std::max<int>(0, religionGoodHappiness_);
	    iHappiness += std::max<int>(0, commerceHappiness_);
	    iHappiness += std::max<int>(0, areaBuildingHappiness_);
	    iHappiness += std::max<int>(0, playerBuildingHappiness_);
	    iHappiness += std::max<int>(0, cityExtraHappiness_ + playerExtraHappiness_);
	    iHappiness += std::max<int>(0, levelHappyBonus_);
	    iHappiness += std::max<int>(0, vassalHappiness_);

	    if (tempHappyTimer_ > 0)
	    {
		    iHappiness += TEMP_HAPPY_;
	    }

	    return std::max<int>(0, iHappiness);
    }

    int HappyHelper::angryPopulation() const
    {
	    int iUnhappiness = 0;

	    if (!noUnhappiness_)
	    {
		    int iAngerPercent = 0;

		    iAngerPercent += overcrowdingPercentAnger_;
		    iAngerPercent += noMilitaryPercentAnger_;
		    iAngerPercent += culturePercentAnger_;
		    iAngerPercent += religionPercentAnger_;
		    iAngerPercent += hurryHelper_.getHurryPercentAnger();
		    iAngerPercent += conscriptPercentAnger_;
		    iAngerPercent += defyResolutionPercentAnger_;
		    iAngerPercent += warWearinessPercentAnger_;

		    for (int i = 0, count = civicPercentAnger_.size(); i < count; ++i)
		    {
			    iAngerPercent += civicPercentAnger_[i];
		    }

		    iUnhappiness = (iAngerPercent * population_) / PERCENT_ANGER_DIVISOR_;

		    iUnhappiness -= std::min<int>(0, largestCityHappiness_);
		    iUnhappiness -= std::min<int>(0, militaryHappiness_);
		    iUnhappiness -= std::min<int>(0, currentStateReligionHappiness_);
		    iUnhappiness -= std::min<int>(0, buildingBadHappiness_);
		    iUnhappiness -= std::min<int>(0, extraBuildingBadHappiness_);
		    iUnhappiness -= std::min<int>(0, featureBadHappiness_);
		    iUnhappiness -= std::min<int>(0, bonusBadHappiness_);
		    iUnhappiness -= std::min<int>(0, religionBadHappiness_);
		    iUnhappiness -= std::min<int>(0, commerceHappiness_);
		    iUnhappiness -= std::min<int>(0, areaBuildingHappiness_);
		    iUnhappiness -= std::min<int>(0, playerBuildingHappiness_);
		    iUnhappiness -= std::min<int>(0, cityExtraHappiness_ + playerExtraHappiness_);
		    iUnhappiness -= std::min<int>(0, levelHappyBonus_);
		    iUnhappiness += std::max<int>(0, vassalUnhappiness_);
		    iUnhappiness += std::max<int>(0, espionageHappinessCounter_);
	    }

	    return std::max<int>(0, iUnhappiness);
    }

    void HappyHelper::advanceTurn()
    {
        hurryHelper_.advanceTurn();
        if (--tempHappyTimer_ < 0)
        {
            tempHappyTimer_ = 0;
        }
    }

    void HappyHelper::setPopulation(int population)
    {
        population_ = population;
        setOvercrowdingPercentAnger_();
        hurryHelper_.setPopulation(population);
    }

    void HappyHelper::setOvercrowdingPercentAnger_()
    {
	    if (population_ > 0)
	    {
		    overcrowdingPercentAnger_ = 1 + (population_ * PERCENT_ANGER_DIVISOR_) / std::max<int>(1, population_);
	    }
    }

    void HappyHelper::changeBuildingGoodHappiness(int change)
    {
        buildingGoodHappiness_ += change;
    }

    void HappyHelper::changeBuildingBadHappiness(int change)
    {
        buildingBadHappiness_ += change;
    }

    void HappyHelper::changeExtraBuildingGoodHappiness(int change)
    {
        extraBuildingGoodHappiness_ += change;
    }

    void HappyHelper::changeExtraBuildingBadHappiness(int change)
    {
        extraBuildingBadHappiness_ += change;
    }

    void HappyHelper::changeBonusGoodHappiness(int change)
    {
        buildingGoodHappiness_ += change;
    }

    void HappyHelper::changeBonusBadHappiness(int change)
    {
        buildingBadHappiness_ += change;
    }

    void HappyHelper::changeFeatureGoodHappiness(int change)
    {
        featureGoodHappiness_ += change;
    }

    void HappyHelper::changeFeatureBadHappiness(int change)
    {
        featureBadHappiness_ += change;
    }

    void HappyHelper::changeLargestCityHappiness(int change)
    {
        // use actual rank - more interested in preserving relative order than actual size
        if (pCity_->findPopulationRank() <= targetNumCities_)
	    {
		    largestCityHappiness_ += change;
	    }
    }

    void HappyHelper::setMilitaryHappiness(int happyPerUnit)
    {
        militaryHappiness_ = pCity_->getMilitaryHappinessUnits() * happyPerUnit;
    }

    void HappyHelper::changePlayerHappiness(int change)
    {
        playerExtraHappiness_ += change;
    }

    void HappyHelper::setNoUnhappiness(bool newState)
    {
        noUnhappiness_ = newState;
    }
}