#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"
#include "./city_projections.h"
#include "./city_improvements.h"

namespace AltAI
{
    class CityImprovementTactics : public ICityImprovementTactics
    {
    public:
        CityImprovementTactics() {}
        explicit CityImprovementTactics(const std::vector<CityImprovementManager::PlotImprovementData>& plotData);

        virtual void addTactic(const IWorkerBuildTacticPtr& pBuildTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void update(const Player& player, const CityDataPtr& pCityData);

        virtual ProjectionLadder getProjection() const;
        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;

    private:
        std::vector<IDependentTacticPtr> dependentTactics_;
        std::list<IWorkerBuildTacticPtr> buildTactics_;
        std::vector<CityImprovementManager::PlotImprovementData> plotData_;
        ProjectionLadder projection_;
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
}