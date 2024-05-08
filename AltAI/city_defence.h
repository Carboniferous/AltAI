#pragma once

#include "./player.h"
#include "./unit_tactics.h"

namespace AltAI
{
    class CityDefenceAnalysisImpl;

    class CityDefenceAnalysis
    {
    public:
        explicit CityDefenceAnalysis(Player& player);

        void addOurUnit(const CvUnit* pUnit, const CvCity* pCity);
        void addOurCity(IDInfo city);
        void deleteOurCity(IDInfo city);

        void noteHostileStack(IDInfo city, const std::vector<const CvUnit*> enemyStack);

        std::vector<RequiredUnitStack::UnitDataChoices> getRequiredUnits(IDInfo city);
    private:
        boost::shared_ptr<CityDefenceAnalysisImpl> pImpl_;
    };
}