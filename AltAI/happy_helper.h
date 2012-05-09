#pragma once

#include "./utils.h"

class CvCity;

namespace AltAI
{
    class CityData;

    class HappyHelper;
    typedef boost::shared_ptr<HappyHelper> HappyHelperPtr;
    
    // simulates happy/unhappy logic in CvCity; allows simulation of changing (un)happiness
    class HappyHelper
    {
    public:
        explicit HappyHelper(const CvCity* pCity);
        HappyHelperPtr clone() const;

        int happyPopulation() const;
        int angryPopulation(const CityData& data) const;

        void advanceTurn(CityData& data);

        void setPopulation(CityData& data, int population);

        void changeBuildingGoodHappiness(int change);
        void changeBuildingBadHappiness(int change);

        void changeExtraBuildingGoodHappiness(int change);
        void changeExtraBuildingBadHappiness(int change);

        void changeBonusGoodHappiness(int change);
        void changeBonusBadHappiness(int change);

        void changeFeatureGoodHappiness(int change);
        void changeFeatureBadHappiness(int change);

        void changeAreaBuildingHappiness(int change);

        void changeLargestCityHappiness(int change);

        void setMilitaryHappiness(int happyPerUnit);

        void changePlayerBuildingHappiness(int change);
        void changePlayerHappiness(int change);

        void setNoUnhappiness(bool newState);

    private:
        void setOvercrowdingPercentAnger_();

        const CvCity* pCity_;

        int population_;
        bool noUnhappiness_;

        int overcrowdingPercentAnger_, noMilitaryPercentAnger_, culturePercentAnger_, religionPercentAnger_;
        int conscriptPercentAnger_, defyResolutionPercentAnger_, warWearinessPercentAnger_;
        std::vector<int> civicPercentAnger_;
        
        int largestCityHappiness_, militaryHappiness_, currentStateReligionHappiness_;

        int buildingBadHappiness_, buildingGoodHappiness_, extraBuildingBadHappiness_, extraBuildingGoodHappiness_;
        int featureBadHappiness_, featureGoodHappiness_, bonusBadHappiness_, bonusGoodHappiness_;
        int religionBadHappiness_, religionGoodHappiness_;
        int commerceHappiness_, areaBuildingHappiness_, playerBuildingHappiness_;
        int cityExtraHappiness_, playerExtraHappiness_, levelHappyBonus_;
        int vassalUnhappiness_, vassalHappiness_, espionageHappinessCounter_;

        int tempHappyTimer_;

        int targetNumCities_;
        int PERCENT_ANGER_DIVISOR_, TEMP_HAPPY_;
    };
}
