#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;
    class BuildingInfo;

    struct ProjectionLadder
    {
        std::vector<std::pair<int, BuildingTypes> > buildings;
        struct Entry
        {
            Entry(int pop_, int turns_, TotalOutput output_, int cost_) : pop(pop_), turns(turns_), output(output_), cost(cost_)
            {
            }
            int pop, turns, cost;
            TotalOutput output;
        };
        std::vector<Entry> entries;

        TotalOutput getOutput() const;
        TotalOutput getOutputAfter(int turn) const;
        void debug(std::ostream& os) const;
    };

    class BuildingInfo;
    class Player;

    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int nTurns);
    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, int nTurns);
}