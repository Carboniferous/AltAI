#pragma once

#include "./utils.h"

namespace AltAI
{
    class PlotInfo;
    typedef boost::shared_ptr<PlotInfo> PlotInfoPtr;

    typedef std::map<UnitTypes, int> GreatPersonOutputMap;
    struct GreatPersonOutput
    {
        GreatPersonOutput(UnitTypes unitType_ = NO_UNIT, int output_ = 0) : unitType(unitType_), output(output_) {}

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        UnitTypes unitType;
        int output;
    };

    struct PlotData
    {
        struct UpgradeData
        {
            struct Upgrade
            {
                // stores whole upgrade chain (doesn't support cycles), with times offset from time horizon
                // e.g. if horizon = 50, and actual improvement is a cottage with 4 turns left to upgrade, we would have something like (depends on factors like civics for yields and upgrade rates):
                // {IMPROVEMENT_HAMLET, 46, (0, 0, 1)}, {IMPROVEMENT_VILLAGE, 26, (0, 0, 1)}, {IMPROVEMENT_TOWN, -14, (0, 0, 1)}
                // since the town has a -ve value for remaining turns, its potential value is ignored for the purpose of choosing the improvment (until at least 14 turns have passed of being worked)
                // after four turns the first entry becomes {IMPROVEMENT_HAMLET, 50, (0, 0, 1)}, so upgrades and is removed from the list
                // todo - call remainingTurns something more logical, or better make it count down not up!
                // todo - maybe just store whole time for upgrades beyond current improvement so we don't have to iterate over the whole list each turn
                // todo - also deal with civic change affecting upgrade rate
                Upgrade() : improvementType(NO_IMPROVEMENT), remainingTurns(-MAX_INT) {}
                Upgrade(ImprovementTypes improvementType_, int remainingTurns_, PlotYield extraYield_) :
                    improvementType(improvementType_), remainingTurns(remainingTurns_), extraYield(extraYield_) {}

                // save/load functions
                void write(FDataStreamBase* pStream) const;
                void read(FDataStreamBase* pStream);

                ImprovementTypes improvementType;
                int remainingTurns;
                PlotYield extraYield;
            };

            UpgradeData() : timeHorizon(0) {}
            UpgradeData(int timeHorizon_, const std::list<Upgrade>& upgrades_) : timeHorizon(timeHorizon_), upgrades(upgrades_) {}

            // save/load functions
            void write(FDataStreamBase* pStream) const;
            void read(FDataStreamBase* pStream);

            int timeHorizon;
            typedef std::list<Upgrade> UpgradeList;
            typedef UpgradeList::const_iterator UpgradeListConstIter;
            typedef UpgradeList::iterator UpgradeListIter;
            UpgradeList upgrades;

            TotalOutput getExtraOutput(YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent);
            Upgrade advanceTurn(int nTurns);
            void debug(std::ostream& os) const;            
        };

        struct CultureData
        {
            CultureData() : ownerAndCultureTrumpFlag(NO_PLAYER, false) {}
            CultureData(const CvPlot* pPlot, PlayerTypes player, const CvCity* pOriginalCity);
            std::pair<PlayerTypes, bool> ownerAndCultureTrumpFlag;  // flag is true if plot is owned by reason other than highest culture

            struct CultureSource
            {
                CultureSource() : output(0), range(0) {}
                CultureSource(int output_, int range_, const CvCity* pSourceCity_) : output(output_), range(range_), city(pSourceCity_->getIDInfo()) {}
                int output, range;
                IDInfo city;

                // save/load functions
                void write(FDataStreamBase* pStream) const;
                void read(FDataStreamBase* pStream);
            };

            // only care about current culture sources (old ones have no effect, even if they contributed to the plot's culture totals
            typedef std::map<PlayerTypes, std::pair<int /*this player's plot culture*/, std::vector<CultureSource> /*this player's influencing cities*/> > CultureSourcesMap;
            CultureSourcesMap cultureSourcesMap;

            // save/load functions
            void write(FDataStreamBase* pStream) const;
            void read(FDataStreamBase* pStream);

            void debug(std::ostream& os) const;
        };

        PlotData();
        PlotData(const PlotInfoPtr& pPlotInfo_, PlotYield plotYield_, Commerce commerce_, TotalOutput output_, const GreatPersonOutput& greatPersonOutput_,
                 XYCoords coords_, ImprovementTypes improvementType_ = NO_IMPROVEMENT, BonusTypes bonusType_ = NO_BONUS, FeatureTypes featureType_ = NO_FEATURE, RouteTypes routeType_ = NO_ROUTE, 
                 CultureData cultureData_ = CultureData());

        bool isActualPlot() const { return coords.iX != -1; }

        void debug(std::ostream& os, bool includeUpgradeData = false, bool includeCultureData = false) const;
        void debugSummary(std::ostream& os) const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        PlotInfoPtr pPlotInfo;
        PlotYield plotYield;
        Commerce commerce;
        TotalOutput output, actualOutput;  // output includes some expected future output where applicable
        GreatPersonOutput greatPersonOutput;
        XYCoords coords;
        ImprovementTypes improvementType;
        BonusTypes bonusType;
        FeatureTypes featureType;
        RouteTypes routeType;
        UpgradeData upgradeData;
        CultureData cultureData;
        bool ableToWork, controlled, isWorked;
    };

    struct IsWorked
    {
        bool operator() (const PlotData& plotData) const
        {
            return plotData.isWorked;
        }
    };

    struct Unworked
    {
        bool operator() (const PlotData& plotData) const
        {
            return !plotData.isWorked;
        }
    };

    struct IsWorkable
    {
        bool operator() (const PlotData& plotData) const
        {
            return plotData.ableToWork && plotData.controlled;
        }
    };

    struct PlotDataFinder
    {
        explicit PlotDataFinder(const PlotData& plot_) : plot(plot_) {}

        bool operator() (const PlotData& other) const
        {
            return plot.coords == other.coords && plot.isWorked == other.isWorked;
        }

        const PlotData& plot;
    };

    struct PlotCoordsFinder
    {
        explicit PlotCoordsFinder(const XYCoords coords_) : coords(coords_) {}

        bool operator() (const PlotData& plotData) const
        {
            return coords == plotData.coords;
        }

        const XYCoords coords;
    };

    struct PlotDataOrderF
    {
        bool operator() (const PlotData& first, const PlotData& second) const
        {
            return first.coords < second.coords;
        }
    };

    std::ostream& operator << (std::ostream& os, const GreatPersonOutput& output);

    std::ostream& operator << (std::ostream& os, const PlotData& plotData);
    std::ostream& operator << (std::ostream& os, const PlotData::UpgradeData& upgradeData);
    std::ostream& operator << (std::ostream& os, const PlotData::CultureData& cultureData);
}