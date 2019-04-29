#include "./utils.h"

namespace AltAI
{
    struct SharedPlot
    {
        SharedPlot() : coords(-1, -1) {}
        explicit SharedPlot(XYCoords coords_) : coords(coords_) {}

        XYCoords coords;
        typedef std::set<IDInfo> PossibleCities;
        typedef PossibleCities::const_iterator PossibleCitiesConstIter;
        typedef PossibleCities::iterator PossibleCitiesIter;
        PossibleCities possibleCities;
        IDInfo assignedCity, assignedImprovementCity;

        bool isShared() const { return possibleCities.size() > 1; }
    };

    struct CitySharedPlots
    {
        CitySharedPlots() {}
        explicit CitySharedPlots(IDInfo city_) : city(city_) {}

        IDInfo city;
        std::set<XYCoords> sharedPlots;

        bool isSharedPlot(XYCoords coords) const
        {
            return sharedPlots.find(coords) != sharedPlots.end();
        }
    };
}