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
        std::pair<IDInfo, UnitTypes> getPriorityUnitBuild(IDInfo city);

        void addOurUnit(CvUnitAI* pUnit, const CvUnit* pUpgradingUnit = (const CvUnit*)0);
        void deleteOurUnit(CvUnit* pUnit, const CvPlot* pPlot);
        void withdrawOurUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot);

        void movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot);
        void hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot, bool moved);
        void addPlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot);
        void deletePlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot);
        void withdrawPlayerUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot);

        void addCity(const CvCity* pCity);
        void deleteCity(const CvCity* pCity);

        void addNewSubArea(int subArea);

        bool updateOurUnit(CvUnitAI* pUnit);
        PromotionTypes promoteUnit(CvUnitAI* pUnit);

        MilitaryMissionDataPtr getMissionData(CvUnitAI* pUnit);

        boost::shared_ptr<MilitaryAnalysisImpl> getImpl() const;

        PlotSet getThreatenedPlots() const;
        std::set<IDInfo> getUnitsThreateningPlots(const PlotSet& plots) const;
        PlotUnitsMap getPlotsThreatenedByUnits(const PlotSet& plots) const;
        const std::map<XYCoords, CombatData>& getAttackableUnitsMap() const;
        CvUnit* getNextAttackUnit();

        std::map<int /* sub area */, std::set<XYCoords> > getLandScoutMissionRefPlots() const;

        size_t getUnitCount(MissionAITypes missionType, UnitTypes unitType = NO_UNIT, int subArea = -1) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        boost::shared_ptr<MilitaryAnalysisImpl> pImpl_;
    };

    PlotUnitsMap getNearbyHostileStacks(const Player& player, const CvPlot* pPlot, int range);
    std::set<XYCoords> getUnitHistory(const Player& player, IDInfo unit);

    void updateMilitaryAnalysis(Player& player);
    bool doUnitAnalysis(Player& player, CvUnitAI* pUnit);
}