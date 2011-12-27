#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;

    struct PlayerTactics
    {
        explicit PlayerTactics(Player& player_) : player(player_) {}
        void init();
        ResearchTech getResearchTech(TechTypes ignoreTechType = NO_TECH);
        ResearchTech getResearchTechData(TechTypes techType) const;

        void updateBuildingTactics();
        void updateTechTactics();
        void updateUnitTactics();

        void selectTechTactics();
        void selectUnitTactics();
        void selectBuildingTactics();
        void selectBuildingTactics(const City& city);

        ConstructItem getBuildItem(const City& city);

        void selectCityTactics();

        void debugTactics();

        std::list<ResearchTech> possibleTechTactics_, selectedTechTactics_;
        ConstructList possibleUnitTactics_, selectedUnitTactics_;
        ConstructList possibleBuildingTactics_;
        std::map<IDInfo, ConstructList > selectedCityBuildingTactics_;

        Player& player;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);
    };
}