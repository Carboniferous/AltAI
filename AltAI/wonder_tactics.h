#pragma once

namespace AltAI
{
    class Player;
    class WonderAnalysisImpl;

    class WonderAnalysis
    {
    public:
        explicit WonderAnalysis(Player& player);
        void updateCity(const CvCity* pCity, bool remove);

    private:
        boost::shared_ptr<WonderAnalysisImpl> pImpl_;
    };
}