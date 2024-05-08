#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    struct PlotData;

    class CultureHelper;
    typedef boost::shared_ptr<CultureHelper> CultureHelperPtr;

    class CultureHelper
    {
    public:
        explicit CultureHelper(const CvCity* pCity);
        CultureHelperPtr clone() const;

        void advanceTurns(CityData& data, int nTurns);
        CultureLevelTypes getCultureLevel() const;
        int getTurnsToNextLevel(CityData& data) const;

        void setCultureLevel(CultureLevelTypes cultureLevel);

    private:
        bool checkCulturalLevel_();
        bool updatePlot_(PlotData& plotData, bool culturalLevelChange, const CvCity* pCity, int cultureOutput, int nTurns);

        PlayerTypes owner_;
        int cityCulture_;
        CultureLevelTypes cultureLevel_;
        int CITY_FREE_CULTURE_GROWTH_FACTOR_;
    };
}