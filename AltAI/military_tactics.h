#pragma once

#include "./player.h"
#include "./tactic_selection_data.h"

namespace AltAI
{
    class MilitaryAnalysisImpl;

    class MilitaryAnalysis
    {
    public:
        explicit MilitaryAnalysis(Player& player);

        void update();

        UnitTypes getUnitRequestBuild(const CvCity* pCity, const TacticSelectionData& tacticSelectionData);

        void addUnit(CvUnitAI* pUnit);
        void deleteUnit(CvUnit* pUnit);
        void movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot);
        void hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot);
        void addPlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot);
        void deletePlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot);

        bool updateUnit(CvUnitAI* pUnit);

        boost::shared_ptr<MilitaryAnalysisImpl> getImpl() const;

        std::set<const CvPlot*> getThreatenedPlots() const;
        std::map<int /* sub area */, std::set<XYCoords> > getLandScoutMissionRefPlots() const;

    private:
        boost::shared_ptr<MilitaryAnalysisImpl> pImpl_;
    };

    std::set<const CvPlot*> getReachablePlots(const Player& player, const CvPlot* pStackPlot, const std::vector<const CvUnit*>& unitStack);
    std::map<const CvPlot*, std::vector<const CvUnit*> > getNearbyHostileStacks(Player& player, const CvPlot* pPlot, int range);
    std::set<XYCoords> getUnitHistory(Player& player, IDInfo unit);

    void updateMilitaryAnalysis(Player& player);
    bool doUnitAnalysis(Player& player, CvUnitAI* pUnit);
}