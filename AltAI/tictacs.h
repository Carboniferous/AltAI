#pragma once

#include "./utils.h"
#include "./tactic_actions.h"
#include "./tactics_interfaces.h"
#include "./tactic_selection_data.h"
#include "./unit.h"

namespace AltAI
{
    class Player;
    class City;
    class TechInfo;

    struct PlayerTactics
    {
        explicit PlayerTactics(Player& player_) : player(player_), initialised_(false) {}
        void init();
        bool isInitialised() const { return initialised_; }

        ResearchTech getResearchTech(TechTypes ignoreTechType = NO_TECH);
        ResearchTech getResearchTechData(TechTypes techType) const;

        CivicTypes chooseCivic(CivicOptionTypes civicOptionType);

        void updateBuildingTactics();
        void updateTechTactics();
        //void updateUnitTactics();
        //void updateProjectTactics();

        void makeInitialUnitTactics();
        void makeSpecialistUnitTactics();
        void makeCivicTactics();
        void makeResourceTactics();
        void makeReligionTactics();

        void updateFirstToTechTactics(TechTypes techType);

        void deleteCity(const CvCity* pCity);

        void updateCityBuildingTactics(TechTypes techType);
        void updateCityBuildingTactics(IDInfo city, BuildingTypes buildingType, int buildingChangeCount);
        void updateCityBuildingTactics(IDInfo city);
        void updateCityReligionBuildingTactics(ReligionTypes religionType);

        void updateCivicTactics(TechTypes techType);

        void addNewCityBuildingTactics(IDInfo city);
        void addNewCityUnitTactics(IDInfo city);
        void addCityImprovementTactics(IDInfo city);

        void updateCityBuildingTacticsDependencies();

        void updateLimitedBuildingTacticsDependencies();

        //void updateCityGlobalBuildingTactics(IDInfo city, BuildingTypes buildingType, int buildingChangeCount);
        void updateGlobalBuildingTacticsDependencies();
        void eraseLimitedBuildingTactics(BuildingTypes buildingType);
        bool hasBuildingTactic(BuildingTypes buildingType) const;

        void updateCityUnitTactics(TechTypes techType);
        void updateCityUnitTacticsExperience(IDInfo city);
        void updateCityUnitTactics(IDInfo city);
        void updateCityUnitTactics(const City& city, ReligionTypes religionType);

        //todo - void updateCityImprovementTactics(const boost::shared_ptr<TechInfo>& pTechInfo);

        void eraseBuildingTacticSelectionData(IDInfo city, BuildingTypes buildingType);

        TacticSelectionDataMap& getBaseTacticSelectionDataMap();
        TacticSelectionData& getBaseTacticSelectionData();
        TacticSelectionDataMap& getCityTacticSelectionDataMap(IDInfo city);
        TacticSelectionData& getCityTacticSelectionData(IDInfo city);

        std::map<int, ICityBuildingTacticsPtr> getCityBuildingTactics(BuildingTypes buildingType) const;
        std::map<int, ICityBuildingTacticsPtr> getCitySpecialBuildingTactics(BuildingTypes buildingType) const;
        ICityBuildingTacticsPtr getCityBuildingTactics(IDInfo city, BuildingTypes buildingType) const;

        ConstructItem getBuildItem(City& city);
        bool getSpecialistBuild(CvUnitAI* pUnit);

        std::map<BuildingTypes, std::set<BuildingTypes> > getBuildingsCityCanAssistWith(IDInfo city) const;
        std::map<BuildingTypes, std::set<BuildingTypes> > getPossibleDependentBuildings(IDInfo city) const;

        UnitAction getConstructedUnitAction(const CvUnit* pUnit) const;

        std::pair<std::vector<UnitTypes>, std::vector<UnitTypes> > getActualAndPossibleCombatUnits(IDInfo city, DomainTypes domainType) const;

        UnitTacticsPtr getUnitTactics(UnitTypes unitType) const;

        template <typename DepType>
            void getUnitsWithDep(std::map<DepType, std::vector<UnitTypes> >& depMap, int depID) const;

        void debugTactics(bool inclOutputs = true);

        // ordinary buildings tactics, keyed by city IDInfo
        typedef std::map<BuildingTypes, ICityBuildingTacticsPtr> CityBuildingTacticsList;
        typedef std::map<IDInfo, CityBuildingTacticsList> CityBuildingTacticsMap;
        CityBuildingTacticsMap cityBuildingTacticsMap_, specialCityBuildingsTacticsMap_;
        std::set<BuildingTypes> availableGeneralBuildingsList_;

        // process tactics
        typedef std::map<ProcessTypes, IProcessTacticsPtr> ProcessTacticsMap;
        ProcessTacticsMap processTacticsMap_;

        // world wonders
        typedef std::map<BuildingTypes, ILimitedBuildingTacticsPtr> LimitedBuildingsTacticsMap;
        LimitedBuildingsTacticsMap nationalBuildingsTacticsMap_, globalBuildingsTacticsMap_;

        // improvement build tactics, keyed by city IDInfo
        typedef std::list<CityImprovementTacticsPtr> CityImprovementTacticsList;
        typedef std::map<IDInfo, CityImprovementTacticsList> CityImprovementTacticsMap;
        CityImprovementTacticsMap cityImprovementTacticsMap_;

        // unit tactics
        typedef std::map<UnitTypes, UnitTacticsPtr> UnitTacticsMap;
        UnitTacticsMap unitTacticsMap_, specialUnitTacticsMap_;

        // tech tactics
        typedef std::map<TechTypes, ITechTacticsPtr> TechTacticsMap;
        TechTacticsMap techTacticsMap_;

        // civic tactics
        typedef std::map<CivicTypes, CivicTacticsPtr> CivicTacticsMap;
        CivicTacticsMap civicTacticsMap_;

        // resource tactics
        typedef std::map<BonusTypes, ResourceTacticsPtr> ResourceTacticsMap;
        ResourceTacticsMap resourceTacticsMap_;

        // religion tactics
        typedef std::map<ReligionTypes, ReligionTacticsPtr> ReligionTacticsMap;
        ReligionTacticsMap religionTacticsMap_;

        Player& player;
        TacticSelectionDataMap tacticSelectionDataMap;
        std::map<IDInfo, TacticSelectionDataMap> cityTacticSelectionDataMap;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        void addBuildingTactics_(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, CvCity* pCity);
        void addSpecialBuildingTactics_(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, CvCity* pCity);
        void addUnitTactics_(const boost::shared_ptr<UnitInfo>& pUnitInfo, CvCity* pCity);

        bool initialised_;
    };
}