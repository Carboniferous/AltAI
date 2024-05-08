#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"
#include "./city_projections.h"
#include "./city_improvements.h"

namespace AltAI
{
    class CityImprovementTactics
    {
    public:
        static CityImprovementTacticsPtr factoryRead(FDataStreamBase* pStream);

        CityImprovementTactics() {}
        explicit CityImprovementTactics(const std::vector<PlotImprovementData>& plotData);

        void addTactic(const IWorkerBuildTacticPtr& pBuildTactic);
        void addDependency(const ResearchTechDependencyPtr& pDependentTactic);
        void update(const Player& player, const CityDataPtr& pCityData);
        void apply(const CityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& tacticSelectionData);

        const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const;
        const std::vector<PlotImprovementData>& getImprovements() const;

        ProjectionLadder getProjection() const;
        ProjectionLadder getBaseProjection() const;
        void debug(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static const int ID = 0;

    private:
        std::vector<ResearchTechDependencyPtr> dependentTactics_;
        std::list<IWorkerBuildTacticPtr> buildTactics_;
        std::vector<PlotImprovementData> plotData_;
        ProjectionLadder base_, projection_;
        TotalOutput baseImpDelta_, projectionImpDelta_;
    };

    class EconomicImprovementTactic : public IWorkerBuildTactic
    {
    public:
        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;
    };

    class RemoveFeatureTactic : public IWorkerBuildTactic
    {
    public:
        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 1;
    };

    class ProvidesResourceTactic : public IWorkerBuildTactic
    {
    public:
        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 2;
    };

    class HappyImprovementTactic : public IWorkerBuildTactic
    {
    public:
        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 3;
    };

    class HealthImprovementTactic : public IWorkerBuildTactic
    {
    public:
        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 4;
    };

    class MilitaryImprovementTactic : public IWorkerBuildTactic
    {
    public:
        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 5;
    };

    class FreeTechTactic : public ITechTactic
    {
    public:
        virtual void debug(std::ostream& os) const;

        virtual void apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);
        virtual const int getID() const { return ID; }

        static const int ID = 0;
    };

    class FoundReligionTechTactic : public ITechTactic
    {
    public:
        FoundReligionTechTactic() : religionType(NO_RELIGION) {}
        explicit FoundReligionTechTactic(ReligionTypes religionType_) : religionType(religionType_) {}
        virtual void debug(std::ostream& os) const;

        virtual void apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);
        virtual const int getID() const { return ID; }

        static const int ID = 1;
        ReligionTypes religionType;
    };

    class ConnectsResourcesTechTactic : public ITechTactic
    {
    public:
        ConnectsResourcesTechTactic() : routeType(NO_ROUTE) {}
        explicit ConnectsResourcesTechTactic(RouteTypes routeType_) : routeType(routeType_) {}
        virtual void debug(std::ostream& os) const;

        virtual void apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);
        virtual const int getID() const { return ID; }

        static const int ID = 2;
        RouteTypes routeType;
    };

    class ConstructBuildingTechTactic : public ITechTactic
    {
    public:
        ConstructBuildingTechTactic() : buildingType(NO_BUILDING) {}
        explicit ConstructBuildingTechTactic(BuildingTypes buildingType_) : buildingType(buildingType_) {}
        virtual void debug(std::ostream& os) const;

        virtual void apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);
        virtual const int getID() const { return ID; }

        static const int ID = 3;
        BuildingTypes buildingType;
    };

    class ProvidesResourceTechTactic : public ITechTactic
    {
    public:
        ProvidesResourceTechTactic() : bonusType(NO_BONUS) {}
        explicit ProvidesResourceTechTactic(BonusTypes bonusType_) : bonusType(bonusType_) {}
        virtual void debug(std::ostream& os) const;

        virtual void apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);
        virtual const int getID() const { return ID; }

        static const int ID = 4;
        BonusTypes bonusType;
    };

    class EconomicTechTactic : public ITechTactic
    {
    public:
        EconomicTechTactic() {}
        virtual void debug(std::ostream& os) const;

        virtual void apply(const ITechTacticsPtr& pTechTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);
        virtual const int getID() const { return ID; }

        static const int ID = 5;
    };
    
}