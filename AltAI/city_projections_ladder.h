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
            Entry() : pop(0), turns(0), cost(0) {}
            Entry(int pop_, int turns_, TotalOutput output_, TotalOutput processOutput_, int cost_, const GreatPersonOutputMap& gpp_)
                : pop(pop_), turns(turns_), output(output_), processOutput(processOutput_), cost(cost_), gpp(gpp_)
            {
            }
            int pop, turns, cost;
            TotalOutput output, processOutput;
            GreatPersonOutputMap gpp;

            void write(FDataStreamBase* pStream) const;
            void read(FDataStreamBase* pStream);
        };
        std::vector<Entry> entries;

        TotalOutput getOutput() const;
        TotalOutput getProcessOutput() const;
        TotalOutput getOutputAfter(int turn) const;
        int getGPPTotal() const;

        int getPopChange() const;

        void debug(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);
    };
}