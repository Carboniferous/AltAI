#include "AltAI.h"

#include "./happy_helper.h"
#include "./religion_helper.h"
#include "./city_data.h"

namespace AltAI
{
    HappyHelper::HappyHelper(const CvCity* pCity)
        : pCity_(pCity)
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
        militaryHappinessUnitCount_ = pCity_->getMilitaryHappinessUnits();
        militaryHappinessPerUnit_ = player.getHappyPerMilitaryUnit();
        
        buildingBadHappiness_ = pCity_->getBuildingBadHappiness();
        buildingGoodHappiness_ = pCity_->getBuildingGoodHappiness();

        extraBuildingBadHappiness_ = pCity_->getExtraBuildingBadHappiness();
        extraBuildingGoodHappiness_ = pCity_->getExtraBuildingGoodHappiness();

        featureBadHappiness_ = pCity_->getFeatureBadHappiness();
        featureGoodHappiness_ = pCity_->getFeatureGoodHappiness();

        bonusBadHappiness_ = pCity_->getBonusBadHappiness();
        bonusGoodHappiness_ = pCity_->getBonusGoodHappiness();

        // store these as civics (and buildings?) can affect their values
        stateReligionHappiness_ = player.getStateReligionHappiness();
        nonStateReligionHappiness_ = player.getNonStateReligionHappiness();
        // actual total good/bad religion based happy values - adopting/switching state religion and buildings can affect these totals
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

    HappyHelperPtr HappyHelper::clone() const
    {
        HappyHelperPtr copy = HappyHelperPtr(new HappyHelper(*this));
        return copy;
    }

    int HappyHelper::happyPopulation(const CityData& data) const
    {
        int iHappiness = 0;

        iHappiness += std::max<int>(0, largestCityHappiness_);
        iHappiness += std::max<int>(0, militaryHappinessPerUnit_ * militaryHappinessUnitCount_);
        iHappiness += std::max<int>(0, religionGoodHappiness_);
        iHappiness += std::max<int>(0, buildingGoodHappiness_);
        iHappiness += std::max<int>(0, extraBuildingGoodHappiness_);
        iHappiness += std::max<int>(0, featureGoodHappiness_);
        iHappiness += std::max<int>(0, bonusGoodHappiness_);        
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

    int HappyHelper::angryPopulation(const CityData& data) const
    {
        int iUnhappiness = 0;

        if (!noUnhappiness_)
        {
            int iAngerPercent = 0;

            iAngerPercent += overcrowdingPercentAnger_;
            iAngerPercent += noMilitaryPercentAnger_;
            iAngerPercent += culturePercentAnger_;
            iAngerPercent += religionPercentAnger_;
            iAngerPercent += data.getHurryHelper()->getHurryPercentAnger();
            iAngerPercent += conscriptPercentAnger_;
            iAngerPercent += defyResolutionPercentAnger_;
            iAngerPercent += warWearinessPercentAnger_;

            for (int i = 0, count = civicPercentAnger_.size(); i < count; ++i)
            {
                iAngerPercent += civicPercentAnger_[i];
            }

            iUnhappiness = (iAngerPercent * population_) / PERCENT_ANGER_DIVISOR_;

            iUnhappiness -= std::min<int>(0, largestCityHappiness_);
            iUnhappiness -= std::min<int>(0, militaryHappinessPerUnit_ * militaryHappinessUnitCount_);
            iUnhappiness -= std::min<int>(0, stateReligionHappiness_);
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

    void HappyHelper::advanceTurn(CityData& data)
    {
        data.getHurryHelper()->advanceTurns(1);
        if (--tempHappyTimer_ < 0)
        {
            tempHappyTimer_ = 0;
        }
    }

    void HappyHelper::setPopulation(CityData& data, int population)
    {
        population_ = population;
        setOvercrowdingPercentAnger_();
        data.getHurryHelper()->setPopulation(population);
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
        bonusGoodHappiness_ += change;
    }

    void HappyHelper::changeBonusBadHappiness(int change)
    {
        bonusBadHappiness_ += change;
    }

    void HappyHelper::changeFeatureGoodHappiness(int change)
    {
        featureGoodHappiness_ += change;
    }

    void HappyHelper::changeFeatureBadHappiness(int change)
    {
        featureBadHappiness_ += change;
    }

    void HappyHelper::changeAreaBuildingHappiness(int change)
    {
        areaBuildingHappiness_ += change;
    }

    void HappyHelper::changeLargestCityHappiness(int change)
    {
        // use actual rank - more interested in preserving relative order than actual size
        if (pCity_->findPopulationRank() <= targetNumCities_)
        {
            largestCityHappiness_ += change;
        }
    }

    void HappyHelper::setMilitaryHappinessPerUnit(int happyPerUnit)
    {
        militaryHappinessPerUnit_ = happyPerUnit;
    }

    void HappyHelper::changeMilitaryHappinessUnits(int change)
    {
        militaryHappinessUnitCount_ += change;
    }

    void HappyHelper::changePlayerBuildingHappiness(int change)
    {
        playerBuildingHappiness_ += change;
    }

    void HappyHelper::changePlayerHappiness(int change)
    {
        playerExtraHappiness_ += change;
    }

    int HappyHelper::getReligionGoodHappiness() const
    {
        return religionGoodHappiness_;
    }

    int HappyHelper::getReligionBadHappiness() const
    {
        return religionBadHappiness_;
    }

    int HappyHelper::getStateReligionHappiness() const
    {
        return stateReligionHappiness_;
    }

    int HappyHelper::getNonStateReligionHappiness() const
    {
        return nonStateReligionHappiness_;
    }

    void HappyHelper::setReligionGoodHappiness(int value)
    {
        religionGoodHappiness_ = value;
    }

    void HappyHelper::setReligionBadHappiness(int value)
    {
        religionBadHappiness_ = value;
    }

    void HappyHelper::setStateReligionHappiness(int value)
    {
        stateReligionHappiness_ = value;
    }

    void HappyHelper::setNonStateReligionHappiness(int value)
    {
        nonStateReligionHappiness_ = value;
    }

    void HappyHelper::setNoUnhappiness(bool newState)
    {
        noUnhappiness_ = newState;
    }

    bool HappyHelper::isNoUnhappiness() const
    {
        return noUnhappiness_;
    }
}