#include "./utils.h"

namespace AltAI
{
    struct SharedPlotItem
    {
        SharedPlotItem() : coords(-1, -1), assignedImprovement(IDInfo(), boost::make_tuple(NO_IMPROVEMENT, NO_FEATURE, PlotYield(), 0)) {}

        XYCoords coords;
        typedef std::list<IDInfo> PossibleCities;
        typedef PossibleCities::const_iterator PossibleCitiesConstIter;
        typedef PossibleCities::iterator PossibleCitiesIter;
        PossibleCities possibleCities;
        IDInfo assignedCity;

        bool isShared() const { return !(coords == XYCoords(-1, -1)); }

        // owning city assigned desired improvement
        typedef std::pair<IDInfo, boost::tuple<ImprovementTypes, FeatureTypes, PlotYield, int> > AssignedImprovement;
        AssignedImprovement assignedImprovement;
    };

    typedef boost::shared_ptr<SharedPlotItem> SharedPlotItemPtr;

    struct CitySharedPlots
    {
        explicit CitySharedPlots(IDInfo idInfo) : city(idInfo) {}

        IDInfo city;
        typedef std::list<SharedPlotItemPtr> SharedPlots;
        typedef SharedPlots::iterator SharedPlotsIter;
        typedef SharedPlots::const_iterator SharedPlotsConstIter;
        SharedPlots sharedPlots;

        bool operator < (const CitySharedPlots& other) const
        {
            return city < other.city;
        }
    };
}