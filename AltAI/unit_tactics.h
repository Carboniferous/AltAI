#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;

    struct PlayerTactics;

    class UnitValueHelper
    {
    public:
        typedef std::pair<int, std::vector<std::pair<UnitTypes, int> > > UnitCostAndOddsData;
        typedef std::map<UnitTypes, UnitCostAndOddsData> MapT;

        int getValue(const UnitCostAndOddsData& mapEntry) const;
        void addMapEntry(MapT& unitCombatData, UnitTypes unitType, const std::vector<UnitTypes>& possibleCombatUnits, const std::vector<int>& odds) const;

        void debug(const MapT& unitCombatData, std::ostream& os) const;
    };

    bool getSpecialistBuild(const PlayerTactics& playerTactics, CvUnitAI* pUnit);
    
    std::pair<std::vector<UnitTypes>, std::vector<UnitTypes> > getActualAndPossibleCombatUnits(const Player& player, const CvCity* pCity, DomainTypes domainType);

    struct UnitAction
    {
        XYCoords targetPlot;
    };

    const CvPlot* getEscapePlot(const Player& player, CvUnit* pUnit, const std::set<const CvPlot*>& dangerPlots, const std::map<const CvPlot*, std::vector<const CvUnit*> >& nearbyHostiles);
}