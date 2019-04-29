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
        void logMissions(std::ostream& os) const;

    private:
        boost::shared_ptr<WorkerAnalysisImpl> pImpl_;
    };

    void updateWorkerAnalysis(Player& player);
    bool doWorkerAnalysis(Player& player, CvUnitAI* pUnit);
}