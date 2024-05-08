#pragma once

#include "./player.h"

namespace AltAI
{
    class GreatPeopleAnalysisImpl;

    class GreatPeopleAnalysis
    {
    public:
        explicit GreatPeopleAnalysis(Player& player);
        void updateCity(const CvCity* pCity, bool remove);

    private:
        boost::shared_ptr<GreatPeopleAnalysisImpl> pImpl_;
    };

    bool getSpecialistBuild(const PlayerTactics& playerTactics, CvUnitAI* pUnit);
}