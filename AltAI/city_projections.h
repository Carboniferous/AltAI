#pragma once

#include "./utils.h"
#include "./city_projections_ladder.h"

namespace AltAI
{
    class Player;
    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    class BuildingInfo;
    class UnitInfo;
    class CivicInfo;

    struct ProjectionLadder;
    struct IProjectionEvent;
    typedef boost::shared_ptr<IProjectionEvent> IProjectionEventPtr;

    struct IProjectionEvent
    {
        virtual ~IProjectionEvent() = 0 {}
        virtual void init(const CityDataPtr&) = 0;
        virtual void debug(std::ostream& os) const = 0;
        virtual int getTurnsToEvent() const = 0;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder) = 0;
    };    

    class ProjectionPopulationEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionPopulationEvent>
    {
    public:
        virtual void init(const CityDataPtr& pCityData);
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
    };

    class ProjectionBuildingEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionBuildingEvent>
    {
    public:
        ProjectionBuildingEvent(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);

        virtual void init(const CityDataPtr& pCityData);
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
        boost::shared_ptr<BuildingInfo> pBuildingInfo_;
        int requiredProduction_;
        int accumulatedTurns_;
    };

    class ProjectionUnitEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionUnitEvent>
    {
    public:
        ProjectionUnitEvent(const CvCity* pCity, const boost::shared_ptr<UnitInfo>& pUnitInfo);

        virtual void init(const CityDataPtr& pCityData);
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
        boost::shared_ptr<UnitInfo> pUnitInfo_;
        bool isFoodProduction_;
        int requiredProduction_;
        int accumulatedTurns_;
    };

    class ProjectionGlobalBuildingEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionGlobalBuildingEvent>
    {
    public:
        ProjectionGlobalBuildingEvent(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int turnBuilt, const CvCity* pBuiltInCity);

        virtual void init(const CityDataPtr& pCityData);
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder);

    private:        
        CityDataPtr pCityData_;
        boost::shared_ptr<BuildingInfo> pBuildingInfo_;
        int remainingTurns_;
        const CvCity* pBuiltInCity_;
    };

    class ProjectionChangeCivicEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionChangeCivicEvent>
    {
    public:
        ProjectionChangeCivicEvent(const boost::shared_ptr<CivicInfo>& pCivicInfo, int turnsToChange);

        virtual void init(const CityDataPtr& pCityData);
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual IProjectionEventPtr update(int nTurns, ProjectionLadder& ladder);

    private:        
        CityDataPtr pCityData_;
        boost::shared_ptr<CivicInfo> pCivicInfo_;
        int turnsToChange_;
    };

    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, int nTurns, std::vector<IProjectionEventPtr>& events);
}