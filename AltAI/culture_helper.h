#pragma once

#include "./utils.h"
#include "./city_data.h"

namespace AltAI
{
    class CultureHelper
    {
    public:
        explicit CultureHelper(const CvCity* pCity);

        void advanceTurn(CityData& data, bool includeUnclaimedPlots);

    private:
        bool checkCulturalLevel_();
        bool updatePlot_(PlotData& plotData, bool culturalLevelChange, const CvCity* pCity, int cultureOutput);

        PlayerTypes owner_;
        int cityCulture_;
        CultureLevelTypes cultureLevel_;
        int CITY_FREE_CULTURE_GROWTH_FACTOR_;
    };
}