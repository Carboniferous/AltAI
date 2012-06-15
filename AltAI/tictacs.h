#pragma once

#include "./utils.h"
#include "./tactic_actions.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class Player;
    class City;
    class TechInfo;

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

        void deleteCity(const CvCity* pCity);

        void updateCityBuildingTactics(TechTypes techType);
        void updateCityBuildingTactics(IDInfo city, BuildingTypes buildingType, int newCount);
        void updateCityBuildingTactics(IDInfo city);
        void updateCityReligionBuildingTactics(ReligionTypes religionType);

        void addNewCityBuildingTactics(IDInfo city);

        void updateCityBuildingTacticsDependencies();

        void updateLimitedBuildingTacticsDependencies();

        void updateCityGlobalBuildingTactics(IDInfo city, BuildingTypes buildingType, int newCount);
        void updateGlobalBuildingTacticsDependencies();
        void eraseLimitedBuildingTactics(BuildingTypes buildingType);

        void updateCityImprovementTactics(const boost::shared_ptr<TechInfo>& pTechInfo);

        ConstructItem getBuildItem(const City& city);

        void selectCityTactics();

        std::map<IDInfo, std::vector<BuildingTypes> > getBuildingsCityCanAssistWith(IDInfo city) const;

        void debugTactics();

        std::list<ResearchTech> possibleTechTactics_, selectedTechTactics_;
        ConstructList possibleUnitTactics_, selectedUnitTactics_;
        ConstructList possibleBuildingTactics_, possibleProjectTactics_;
        std::map<IDInfo, ConstructList> selectedCityBuildingTactics_, selectedCityProjectTactics_;

        // ordinary buildings tactics, keyed by city IDInfo
        typedef std::map<BuildingTypes, ICityBuildingTacticsPtr> CityBuildingTacticsList;
        typedef std::map<IDInfo, CityBuildingTacticsList> CityBuildingTacticsMap;
        CityBuildingTacticsMap cityBuildingTacticsMap_;

        // world wonders
        typedef std::map<BuildingTypes, ILimitedBuildingTacticsPtr> LimitedBuildingsTacticsMap;
        LimitedBuildingsTacticsMap nationalBuildingsTacticsMap_, globalBuildingsTacticsMap_;

        // improvement build tactics, keyed by city IDInfo
        typedef std::list<ICityImprovementTacticsPtr> CityImprovementTacticsList;
        typedef std::map<IDInfo, CityImprovementTacticsList> CityImprovementTacticsMap;
        CityImprovementTacticsMap cityImprovementTacticsMap_;

        Player& player;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        void addBuildingTactics_(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, CvCity* pCity);
    };
}