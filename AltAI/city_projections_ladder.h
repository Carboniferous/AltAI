#pragma once

#include "./utils.h"
#include "./hurry_helper.h"
#include "./city_data.h"

namespace AltAI
{
    typedef std::map<UnitTypes, int> GreatPersonOutputMap;
    typedef std::set<PromotionTypes> Promotions;

    struct HurryData;

    struct ProjectionLadder
    {
        struct PlotDiff
        {
            PlotDiff() : improvementType(NO_IMPROVEMENT), isWorked(false), wasWorked(false)
            {
            }

            PlotDiff(const PlotData& plotData, bool isNewWorked_, bool isOldWorked_);

            void debug(std::ostream& os) const;

            XYCoords coords;
            ImprovementTypes improvementType;
            PlotYield plotYield;
            TotalOutput actualOutput;
            bool isWorked, wasWorked;
        };

        typedef std::list<PlotDiff> PlotDiffList;

        struct ConstructedUnit
        {
            ConstructedUnit() : unitType(NO_UNIT), turns(-1), level(0), experience(0)
            {
            }

            ConstructedUnit(UnitTypes unitType_, int turns_) : unitType(unitType_), turns(turns_), level(0), experience(0)
            {
            }

            void write(FDataStreamBase* pStream) const;
            void read(FDataStreamBase* pStream);

            void debug(std::ostream& os) const;

            UnitTypes unitType;            
            Promotions promotions;
            int turns;
            int level, experience;
        };

        //std::vector<ProjectionLadder> comparisons;
        std::vector<std::pair<int, BuildingTypes> > buildings;
        std::vector<ConstructedUnit> units;

        struct Entry
        {
            Entry() : pop(0), turns(0), cost(0), storedFood(0), accumulatedProduction(0) {}
            Entry(int pop_, int turns_, int foodKeptPercent_, int accumulatedProduction_, 
                  TotalOutput output_, TotalOutput processOutput_, int cost_, const GreatPersonOutputMap& gpp_)
                : pop(pop_), turns(turns_), storedFood(foodKeptPercent_), accumulatedProduction(accumulatedProduction_),
                  output(output_), processOutput(processOutput_), cost(cost_), gpp(gpp_)
            {
            }
            int pop, turns, cost;
            int storedFood;
            int accumulatedProduction;
            TotalOutput output, processOutput;
            PlotDataList workedPlots;
            GreatPersonOutputMap gpp;
            std::vector<HurryData> hurryData;
#ifdef ALTAI_DEBUG
            std::string debugSummary;
#endif

            void write(FDataStreamBase* pStream) const;
            void read(FDataStreamBase* pStream);
        };
        std::vector<Entry> entries;
        std::vector<PlotDiffList> workedPlotDiffs;

        TotalOutput getOutput() const;
        TotalOutput getProcessOutput() const;
        int getAccumulatedProduction() const;
        //TotalOutput getOutputAfter(int turn) const;
        int getGPPTotal() const;

        int getPopChange() const;

        // first turn worked, total no. turns worked
        std::pair<int, int> getWorkedTurns(XYCoords coords) const;

        int getExpectedTurnBuilt(int cost, int itemProductionModifier, int baseModifier) const;

        void debug(std::ostream& os) const;
        void debugDiffs(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);
    };

    struct IProjectionEvent;
    typedef boost::shared_ptr<IProjectionEvent> IProjectionEventPtr;

    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    struct IProjectionEvent
    {
        virtual ~IProjectionEvent() = 0 {}

        virtual void init(const CityDataPtr&) = 0;
        virtual IProjectionEventPtr clone(const CityDataPtr&) const = 0;
        virtual void debug(std::ostream& os) const = 0;
        virtual int getTurnsToEvent() const = 0;
        virtual bool targetsCity(IDInfo city) const = 0;
        virtual bool generateComparison() const = 0;
        virtual void updateCityData(int nTurns) = 0;
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder) = 0;
    };    
}