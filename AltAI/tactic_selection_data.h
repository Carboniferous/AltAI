#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

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

    struct UnitTacticValue
    {
        UnitTacticValue() : unitType(NO_UNIT), nTurns(0), unitAnalysisValue(0), moves(0), level(0)
        {
        }

        UnitTypes unitType;
        IDInfo city;
        int nTurns, unitAnalysisValue, moves, level;

        bool operator < (const UnitTacticValue& other) const;
        void debug(std::ostream& os) const;
    };

    struct MilitaryBuildingValue
    {
        MilitaryBuildingValue() : buildingType(NO_BUILDING), nTurns(0),
            freeExperience(0), globalFreeExperience(0), cityDefence(0), globalCityDefence(0), bombardDefence(0), freePromotion(NO_PROMOTION)
        {
        }

        explicit MilitaryBuildingValue(IDInfo city_) : city(city_),
            buildingType(NO_BUILDING), nTurns(0),
            freeExperience(0), globalFreeExperience(0), cityDefence(0), globalCityDefence(0), bombardDefence(0), freePromotion(NO_PROMOTION)
        {
        }

        BuildingTypes buildingType;
        IDInfo city;
        int freeExperience, globalFreeExperience, cityDefence, globalCityDefence, bombardDefence;
        std::map<DomainTypes, int> domainFreeExperience;
        std::map<UnitCombatTypes, int> combatTypeFreeExperience;
        PromotionTypes freePromotion;
        int nTurns;

        std::set<UnitTacticValue> cityDefenceUnits, thisCityDefenceUnits, cityAttackUnits, collateralUnits;

        bool operator < (const MilitaryBuildingValue& other) const;
        void debug(std::ostream& os) const;
    };

    struct CivicValue
    {
        CivicValue() : civicType(NO_CIVIC), cost(0)
        {
        }

        CivicTypes civicType;
        TotalOutput outputDelta;
        int cost;

        void debug(std::ostream& os) const;
    };

    struct WorkerUnitValue
    {
        WorkerUnitValue() : unitType(NO_UNIT), nTurns(0)
        {
        }

        UnitTypes unitType;
        int nTurns;
        TotalOutput lostOutput;

        typedef boost::tuple<XYCoords, IDInfo, TotalOutput, std::vector<TechTypes> > BuildData;
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
        IUnitEventGeneratorPtr getUnitEventGenerator() const;
        void debug(std::ostream& os) const;

    private:
        void addBuild_(BuildsMap& buildsMap_, BuildTypes buildType, BuildData buildData);
        void debugBuildsMap_(std::ostream& os, const BuildsMap& buildsMap_) const;
    };

    class BuildImprovementsUnitEventGenerator : public IUnitEventGenerator
    {
    public:
        BuildImprovementsUnitEventGenerator(bool isLand, bool isConsumed, const std::vector<IDInfo>& targetCities);
        virtual IProjectionEventPtr getProjectionEvent(const CityDataPtr& pCityData);

    private:
        bool isLand_, isConsumed_;
        std::vector<IDInfo> targetCities_;
    };

    struct SettlerUnitValue
    {
        SettlerUnitValue() : unitType(NO_UNIT), nTurns(0)
        {
        }

        UnitTypes unitType;
        int nTurns;
        TotalOutput lostOutput;

        void debug(std::ostream& os) const;
    };

    struct CultureSourceValue
    {
        CultureSourceValue() : cityCost(0), cityValue(0), globalValue(0)
        {
        }

        explicit CultureSourceValue(const CvBuildingInfo& buildingInfo);

        void debug(std::ostream& os) const;

        int cityCost, cityValue, globalValue;
    };

    struct SettledSpecialistValue
    {
        SettledSpecialistValue() : specType(NO_SPECIALIST)
        {
        }

        bool operator < (const SettledSpecialistValue& other) const;
        void debug(std::ostream& os) const;

        IDInfo city;
        TotalOutput output;
        SpecialistTypes specType;
    };

    struct TacticSelectionData
    {
        TacticSelectionData() : getFreeTech(false), freeTechValue(0)
        {
        }

        void merge(const TacticSelectionData& other);

        template <typename T> 
            typename T::value_type getBuildingValue(const T& container, BuildingTypes buildingType) const
        {
            for (T::const_iterator ci(container.begin()), ciEnd(container.end()); ci != ciEnd; ++ci)
            {
                if (ci->buildingType == buildingType)
                {
                    return *ci;
                }
            }
            return T::value_type();
        }

        template <typename T> 
            typename T::value_type getUnitValue(const T& container, UnitTypes unitType) const
        {
            for (T::const_iterator ci(container.begin()), ciEnd(container.end()); ci != ciEnd; ++ci)
            {
                if (ci->unitType == unitType)
                {
                    return *ci;
                }
            }
            return T::value_type();
        }

        std::multiset<CultureBuildingValue> smallCultureBuildings, largeCultureBuildings;
        std::multiset<EconomicBuildingValue> economicBuildings;
        std::multiset<SettledSpecialistValue> settledSpecialists;
        std::map<BuildingTypes, std::vector<BuildingTypes> > buildingsCityCanAssistWith;
        std::map<BuildingTypes, std::vector<BuildingTypes> > dependentBuildings;
        std::map<BuildingTypes, EconomicWonderValue> economicWonders, nationalWonders;
        std::set<MilitaryBuildingValue> militaryBuildings;

        std::set<UnitTacticValue> cityDefenceUnits, thisCityDefenceUnits, cityAttackUnits, 
            fieldAttackUnits, fieldDefenceUnits, collateralUnits, scoutUnits, seaCombatUnits;

        std::map<UnitTypes, WorkerUnitValue> workerUnits;
        std::map<UnitTypes, SettlerUnitValue> settlerUnits;
        std::map<BonusTypes, std::vector<XYCoords> > connectableResources;
        std::map<BonusTypes, TotalOutput> potentialResourceOutputDeltas;

        std::list<CivicValue> civicValues;

        std::vector<CultureSourceValue> cultureSources;

        std::set<BuildingTypes> exclusions;
        TotalOutput cityImprovementsDelta;
        bool getFreeTech;
        int freeTechValue;
        TotalOutput resourceOutput;
        
        std::map<ProcessTypes, TotalOutput> processOutputsMap;

        TotalOutput getEconomicBuildingOutput(BuildingTypes buildingType, IDInfo city) const;

        static bool isSignificantTacticItem(const EconomicBuildingValue& tacticItemValue, TotalOutput currentOutput, const std::vector<OutputTypes>& outputTypes);

        void debug(std::ostream& os) const;
    };
}