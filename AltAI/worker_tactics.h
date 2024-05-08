#pragma once

#include "./player.h"

namespace AltAI
{
    class WorkerAnalysisImpl;

    class WorkerAnalysis
    {
    public:
        explicit WorkerAnalysis(Player& player);

        void deleteUnit(CvUnit* pUnit);
        void updateWorkerMission(CvUnitAI* pUnit);
        void updatePlotData();
        void updatePossibleMissionsData();
        void updatePlotOwner(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner);
        void updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType);
        void updateOwnedPlotImprovement(const CvPlot* pPlot, ImprovementTypes oldImprovementType);
        void updateCity(const CvCity* pCity, bool remove);
        void updateCityBonusCount(const CvCity* pCity, BonusTypes bonusType, int delta);
        //void pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pEvent);
        void updatePlotRevealed(const CvPlot* pPlot);

        std::vector<Unit::WorkerMission> getWorkerMissions(CvUnitAI* pUnit) const;
        std::multimap<int, const CvPlot*> getUnitBonusValuePlotMap() const;
        std::set<IDInfo> getUnitsToEscort() const;
        void setEscort(const IDInfo& escortUnit);

        void logMissions(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        boost::shared_ptr<WorkerAnalysisImpl> pImpl_;
    };

    UnitMissionPtr workerMissionFactoryHelper();

    void updateWorkerAnalysis(Player& player);
    bool doWorkerMove(Player& player, CvUnitAI* pUnit);
}