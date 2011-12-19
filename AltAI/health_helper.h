#pragma once

#include "./utils.h"

namespace AltAI
{

    class HealthHelper
    {
    public:
        explicit HealthHelper(const CvCity* pCity);

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
    };
}