#pragma once

#include "./utils.h"
#include "./tactic_actions.h"
#include "./city_projections_ladder.h"

namespace AltAI
{
    class Player;
    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    class CityImprovementManager;
    typedef boost::shared_ptr<CityImprovementManager> CityImprovementManagerPtr;

    class BuildingInfo;
    class UnitInfo;
    class CivicInfo;

    class ProjectionPopulationEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionPopulationEvent>
    {
    public:
        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
    };

    class ProjectionCultureLevelEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionCultureLevelEvent>
    {
    public:
        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
    };

    class ProjectionImprovementUpgradeEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionImprovementUpgradeEvent>
    {
    public:
        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
    };

    class ProjectionBuildingEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionBuildingEvent>
    {
    public:
        ProjectionBuildingEvent(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo);
        ProjectionBuildingEvent(const CvCity* pCity, const boost::shared_ptr<BuildingInfo>& pBuildingInfo, const std::pair<HurryTypes, int>& hurryEvent);

        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        ProjectionBuildingEvent(const boost::shared_ptr<BuildingInfo>& pBuildingInfo, int accumulatedTurns, const std::pair<HurryTypes, int>& hurryEvent);

        CityDataPtr pCityData_;
        boost::shared_ptr<BuildingInfo> pBuildingInfo_;
        int accumulatedTurns_;
        std::pair<HurryTypes, int> hurryEvent_;
    };

    class ProjectionUnitEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionUnitEvent>
    {
    public:
        ProjectionUnitEvent(const CvCity* pCity, const boost::shared_ptr<UnitInfo>& pUnitInfo);

        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        ProjectionUnitEvent(const boost::shared_ptr<UnitInfo>& pUnitInfo, bool isFoodProduction, int requiredProduction, int accumulatedTurns);

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
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
        boost::shared_ptr<BuildingInfo> pBuildingInfo_;
        int remainingTurns_;
        const CvCity* pBuiltInCity_;
        bool complete_;
    };

    class ProjectionChangeCivicEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionChangeCivicEvent>
    {
    public:
        ProjectionChangeCivicEvent(CivicOptionTypes civicOptionType, CivicTypes civicType, int turnsToChange);

        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:        
        CityDataPtr pCityData_;
        CivicOptionTypes civicOptionType_;
        CivicTypes civicType_;
        int turnsToChange_;
    };

    class ProjectionHappyTimerEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionHappyTimerEvent>
    {
    public:
        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        CityDataPtr pCityData_;
    };

    class ProjectionHurryEvent : public IProjectionEvent, public boost::enable_shared_from_this<ProjectionHurryEvent>
    {
    public:
        explicit ProjectionHurryEvent(const HurryData& hurryData) : hurryData_(hurryData) {}
        virtual void init(const CityDataPtr& pCityData);
        virtual IProjectionEventPtr clone(const CityDataPtr& pCityData) const;
        virtual void debug(std::ostream& os) const;
        virtual int getTurnsToEvent() const;
        virtual bool targetsCity(IDInfo city) const;
        virtual bool generateComparison() const;
        virtual void updateCityData(int nTurns);
        virtual IProjectionEventPtr updateEvent(int nTurns, ProjectionLadder& ladder);

    private:
        bool havePopulation_() const;
        bool angryTimerExpired_() const;
        CityDataPtr pCityData_;
        HurryData hurryData_;
    };

	struct IProjectionEvent;
    typedef boost::shared_ptr<IProjectionEvent> IProjectionEventPtr;

    ProjectionLadder getProjectedOutput(const Player& player, const CityDataPtr& pCityData, int nTurns, std::vector<IProjectionEventPtr>& events, 
        const ConstructItem& constructItem, const std::string& sourceFunc, bool doComparison = false, bool debug = false);
}