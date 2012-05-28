#pragma once

#include "./utils.h"

namespace AltAI
{
    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;
    class BuildingInfo;
    struct ProjectionLadder;
    struct IProjectionEvent;

    typedef boost::shared_ptr<IProjectionEvent> IProjectionEventPtr;

    struct IProjectionEvent
    {
        virtual ~IProjectionEvent() = 0 {}
        virtual void debug(std::ostream& os) const = 0;
        virtual int getTurnsToEvent() const = 0;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder) = 0;
    };    

    class ProjectionPopulationEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionPopulationEvent>
    {
    public:
        explicit ProjectionPopulationEvent(const CityDataPtr& pCityData);

        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
    };

    class ProjectionBuildingEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionBuildingEvent>
    {
    public:
        ProjectionBuildingEvent(const CityDataPtr& pCityData, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder);

    private:
        const CvCity* pCity_;
        CityDataPtr pCityData_;
        boost::shared_ptr<BuildingInfo> pBuildingInfo_;
        int requiredProduction_;
        int accumulatedTurns_;
    };

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
    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, int nTurns, std::vector<IProjectionEventPtr>& events);
}