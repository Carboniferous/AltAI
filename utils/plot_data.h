#pragma once

#include "./utils.h"

namespace AltAI
{
    typedef std::map<UnitTypes, int> GreatPersonOutputMap;
    struct GreatPersonOutput
    {
        GreatPersonOutput(UnitTypes unitType_ = NO_UNIT, int output_ = 0) : unitType(unitType_), output(output_) {}

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
                Upgrade() : improvementType(NO_IMPROVEMENT), remainingTurns(-MAX_INT) {}
                Upgrade(ImprovementTypes improvementType_, int remainingTurns_, PlotYield extraYield_) :
                    improvementType(improvementType_), remainingTurns(remainingTurns_), extraYield(extraYield_) {}

                ImprovementTypes improvementType;
                int remainingTurns;
                PlotYield extraYield;
            };

            UpgradeData() : timeHorizon(0) {}
            UpgradeData(int timeHorizon_, const std::list<Upgrade>& upgrades_) : timeHorizon(timeHorizon_), upgrades(upgrades_) {}

            int timeHorizon;
            typedef std::list<Upgrade> UpgradeList;
            typedef UpgradeList::const_iterator UpgradeListConstIter;
            typedef UpgradeList::iterator UpgradeListIter;
            UpgradeList upgrades;

            TotalOutput getExtraOutput(YieldModifier yieldModifier, CommerceModifier commerceModifier, CommerceModifier commercePercent);
            Upgrade advanceTurn();
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
            };

            // only care about current culture sources (old ones have no effect, even if they contributed to the plot's culture totals
            typedef std::map<PlayerTypes, std::pair<int /*this player's plot culture*/, std::vector<CultureSource> /*this player's influencing cities*/> > CultureSourcesMap;
            CultureSourcesMap cultureSourcesMap;

            void debug(std::ostream& os) const;
        };

        PlotData();
        PlotData(PlotYield plotYield_, Commerce commerce_, TotalOutput output_, const GreatPersonOutput& greatPersonOutput_,
                     XYCoords coords_, ImprovementTypes improvementType_ = NO_IMPROVEMENT, FeatureTypes featureType_ = NO_FEATURE, RouteTypes routeType_ = NO_ROUTE, 
                     CultureData cultureData_ = CultureData());

        bool isActualPlot() const { return coords.iX != -1; }

        void debug(std::ostream& os, bool includeUpgradeData = false, bool includeCultureData = false) const;

        PlotYield plotYield;
        Commerce commerce;
        TotalOutput output, actualOutput;  // output includes some expected future output where applicable
        GreatPersonOutput greatPersonOutput;
        XYCoords coords;
        ImprovementTypes improvementType;
        FeatureTypes featureType;
        RouteTypes routeType;
        UpgradeData upgradeData;
        CultureData cultureData;
        bool ableToWork, controlled, isWorked;
    };

    std::ostream& operator << (std::ostream& os, const GreatPersonOutput& output);

    std::ostream& operator << (std::ostream& os, const PlotData& plotData);
    std::ostream& operator << (std::ostream& os, const PlotData::UpgradeData& upgradeData);
    std::ostream& operator << (std::ostream& os, const PlotData::CultureData& cultureData);
}