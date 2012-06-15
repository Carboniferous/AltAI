#pragma once

#include "./utils.h"

namespace AltAI
{
    typedef std::map<UnitTypes, int> GreatPersonOutputMap;

    struct ProjectionLadder
    {
        std::vector<std::pair<int, BuildingTypes> > buildings;
        std::vector<std::pair<int, UnitTypes> > units;        

        struct Entry
        {
            Entry(int pop_, int turns_, TotalOutput output_, int cost_, const GreatPersonOutputMap& gpp_)
                : pop(pop_), turns(turns_), output(output_), cost(cost_), gpp(gpp_)
            {
            }
            int pop, turns, cost;
            TotalOutput output;
            GreatPersonOutputMap gpp;
        };
        std::vector<Entry> entries;

        TotalOutput getOutput() const;
        TotalOutput getOutputAfter(int turn) const;
        int getGPPTotal() const;

        void debug(std::ostream& os) const;
    };
}