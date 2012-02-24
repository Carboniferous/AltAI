#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    struct PlotData;

    class CultureHelper
    {
    public:
        CultureHelper(const CvCity* pCity, CityData& data);

        void advanceTurn(bool includeUnclaimedPlots);

    private:
        CityData& data_;

        bool checkCulturalLevel_();
        bool updatePlot_(PlotData& plotData, bool culturalLevelChange, const CvCity* pCity, int cultureOutput);

        PlayerTypes owner_;
        int cityCulture_;
        CultureLevelTypes cultureLevel_;
        int CITY_FREE_CULTURE_GROWTH_FACTOR_;
    };

    typedef boost::shared_ptr<CultureHelper> CultureHelperPtr;
}