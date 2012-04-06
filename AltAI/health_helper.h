#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;

    class HealthHelper;
    typedef boost::shared_ptr<HealthHelper> HealthHelperPtr;

    class HealthHelper
    {
    public:
        explicit HealthHelper(const CvCity* pCity);
        HealthHelperPtr clone() const;

        int goodHealth() const;
        int badHealth() const;

        void advanceTurn();

        void setPopulation(int population);

        void changePlayerHealthiness(int change);

        void changeBuildingGoodHealthiness(int change);
        void changeBuildingBadHealthiness(int change);

        void changeExtraBuildingGoodHealthiness(int change);
        void changeExtraBuildingBadHealthiness(int change);

        void changeBonusGoodHealthiness(int change);
        void changeBonusBadHealthiness(int change);

        void updatePowerHealth(const CityData& cityData);

        void setNoUnhealthinessFromBuildings();
        void setNoUnhealthinessFromPopulation();

    private:
        const CvCity* pCity_;
        int population_;

        int freshWaterBadHealth_, freshWaterGoodHealth_;
        int featureBadHealth_, featureGoodHealth_;
        int powerBadHealth_, powerGoodHealth_;
        int bonusBadHealth_, bonusGoodHealth_;
        int buildingBadHealth_, buildingGoodHealth_;
        int areaBuildingBadHealth_, areaBuildingGoodHealth_;
        int playerBuildingBadHealth_, playerBuildingGoodHealth_;
        int extraBuildingBadHealth_, extraBuildingGoodHealth_;
        int extraPlayerHealth_, extraHealth_;
        int levelHealth_;

        int espionageHealthCounter_;

        bool noUnhealthinessFromPopulation_, noUnhealthinessFromBuildings_;

        int unhealthyPopulation_() const;

        int POWER_HEALTH_CHANGE_, DIRTY_POWER_HEALTH_CHANGE_;
    };
}