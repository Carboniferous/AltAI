#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;
    class TechInfo;
    class ICityBuildingTactics;
    typedef boost::shared_ptr<ICityBuildingTactics> ICityBuildingTacticsPtr;

    struct PlayerTactics
    {
        explicit PlayerTactics(Player& player_) : player(player_) {}
        void init();
        ResearchTech getResearchTech(TechTypes ignoreTechType = NO_TECH);
        ResearchTech getResearchTechData(TechTypes techType) const;

        void updateBuildingTactics();
        void updateTechTactics();
        void updateUnitTactics();
        void updateProjectTactics();

        void updateFirstToTechTactics(TechTypes techType);

        void selectTechTactics();
        void selectUnitTactics();
        void selectBuildingTactics();
        void selectBuildingTactics(const City& city);
        void selectProjectTactics();
        void selectProjectTactics(const City& city);

        void updateCityBuildingTactics(const boost::shared_ptr<TechInfo>& pTechInfo);

        ConstructItem getBuildItem(const City& city);

        void selectCityTactics();

        void debugTactics();

        std::list<ResearchTech> possibleTechTactics_, selectedTechTactics_;
        ConstructList possibleUnitTactics_, selectedUnitTactics_;
        ConstructList possibleBuildingTactics_, possibleProjectTactics_;
        std::map<IDInfo, ConstructList> selectedCityBuildingTactics_, selectedCityProjectTactics_;

        typedef std::list<ICityBuildingTacticsPtr> CityBuildingTacticsList;
        typedef std::map<IDInfo, CityBuildingTacticsList> CityBuildingTacticsMap;
        CityBuildingTacticsMap cityBuildingTacticsMap_;

        Player& player;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);
    };
}