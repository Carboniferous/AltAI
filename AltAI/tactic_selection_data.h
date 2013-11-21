#include "./utils.h"

namespace AltAI
{
    struct CultureBuildingValue
    {
        CultureBuildingValue() : buildingType(NO_BUILDING), nTurns(0)
        {
        }

        BuildingTypes buildingType;
        IDInfo city;
        int nTurns;
        TotalOutput output;

        bool operator < (const CultureBuildingValue& other) const;
        void debug(std::ostream& os) const;
    };

    struct EconomicBuildingValue
    {
        EconomicBuildingValue() : buildingType(NO_BUILDING), nTurns(0)
        {
        }

        BuildingTypes buildingType;
        IDInfo city;
        int nTurns;
        TotalOutput output;

        bool operator < (const EconomicBuildingValue& other) const;
        void debug(std::ostream& os) const;
    };

    struct EconomicWonderValue
    {
        std::vector<std::pair<IDInfo, EconomicBuildingValue> > buildCityValues;
        void debug(std::ostream& os) const;
    };

    struct MilitaryBuildingValue
    {
        MilitaryBuildingValue() : buildingType(NO_BUILDING), nTurns(0)
        {
        }

        BuildingTypes buildingType;
        IDInfo city;
        int freeExperience, globalFreeExperience;
        std::map<DomainTypes, int> domainFreeExperience;
        std::map<UnitCombatTypes, int> combatTypeFreeExperience;
        PromotionTypes freePromotion;
        int nTurns;

        bool operator < (const MilitaryBuildingValue& other) const;
        void debug(std::ostream& os) const;
    };

    struct UnitTacticValue
    {
        UnitTacticValue() : unitType(NO_UNIT), nTurns(0), unitAnalysisValue(0)
        {
        }

        UnitTypes unitType;
        int nTurns, unitAnalysisValue;

        bool operator < (const UnitTacticValue& other) const;
        void debug(std::ostream& os) const;
    };

    struct WorkerUnitValue
    {
        WorkerUnitValue() : unitType(NO_UNIT), nTurns(0)
        {
        }

        UnitTypes unitType;
        int nTurns;
        typedef boost::tuple<XYCoords, TotalOutput, std::vector<TechTypes> > BuildData;
        typedef std::map<BuildTypes, std::vector<BuildData> > BuildsMap;
        BuildsMap buildsMap, consumedBuildsMap, nonCityBuildsMap;

        void addBuild(BuildTypes buildType, BuildData buildData);
        void addNonCityBuild(BuildTypes buildType, BuildData buildData);

        bool isReusable() const;
        bool isConsumed() const;

        bool operator < (const WorkerUnitValue& other) const;
        int getBuildValue() const;
        int getHighestConsumedBuildValue() const;
        int getBuildsCount() const;
        void debug(std::ostream& os) const;

    private:
        void addBuild_(BuildsMap& buildsMap_, BuildTypes buildType, BuildData buildData);
        void debugBuildsMap_(std::ostream& os, const BuildsMap& buildsMap_) const;
    };

    struct SettlerUnitValue
    {
        SettlerUnitValue() : unitType(NO_UNIT), nTurns(0)
        {
        }

        UnitTypes unitType;
        int nTurns;

        void debug(std::ostream& os) const;
    };

    struct TacticSelectionData
    {
        TacticSelectionData() : getFreeTech(false), freeTechValue(0)
        {
        }

        std::multiset<CultureBuildingValue> smallCultureBuildings, largeCultureBuildings;
        std::multiset<EconomicBuildingValue> economicBuildings;
        std::map<IDInfo, std::vector<BuildingTypes> > buildingsCityCanAssistWith;
        std::map<BuildingTypes, std::vector<BuildingTypes> > dependentBuildings;
        std::map<BuildingTypes, EconomicWonderValue> economicWonders, nationalWonders;
        std::set<MilitaryBuildingValue> militaryBuildings;

        std::set<UnitTacticValue> cityDefenceUnits, cityAttackUnits, collateralUnits;

        std::map<UnitTypes, WorkerUnitValue> workerUnits;
        std::map<UnitTypes, SettlerUnitValue> settlerUnits;

        std::set<BuildingTypes> exclusions;
        TotalOutput cityImprovementsDelta;
        bool getFreeTech;
        int freeTechValue;
        
        std::map<ProcessTypes, TotalOutput> processOutputsMap;

        TotalOutput getEconomicBuildingOutput(BuildingTypes buildingType, IDInfo city) const;

        void debug(std::ostream& os) const;
    };
}