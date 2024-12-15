#pragma once

#include "./player.h"
#include "./tactic_selection_data.h"

namespace AltAI
{
    class MilitaryAnalysisImpl;

    class MilitaryAnalysis;
    typedef boost::shared_ptr<MilitaryAnalysis> MilitaryAnalysisPtr;

    class MilitaryAnalysis
    {
    public:
        explicit MilitaryAnalysis(Player& player);

        void update();

        UnitRequestData getUnitRequestBuild(const CvCity* pCity, const TacticSelectionData& tacticSelectionData);
        std::pair<IDInfo, UnitTypes> getPriorityUnitBuild(IDInfo city);

        void addOurUnit(CvUnitAI* pUnit, const CvUnit* pUpgradingUnit = (const CvUnit*)0);
        void deleteOurUnit(CvUnit* pUnit, const CvPlot* pPlot);
        void withdrawOurUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot);

        void movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot);
        void hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot, bool moved);
        void addPlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot);
        void deletePlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot);
        void withdrawPlayerUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot);

        void addOurCity(const CvCity* pCity);
        void deleteCity(const CvCity* pCity);

        void addPlayerCity(const CvCity* pCity);
        void deletePlayerCity(const CvCity* pCity);
        void addHostilePlotsWithUnknownCity(const std::vector<XYCoords>& coords);

        void addNewSubArea(int subArea);

        bool updateOurUnit(CvUnitAI* pUnit);
        PromotionTypes promoteUnit(CvUnitAI* pUnit);

        const std::map<IDInfo, UnitHistory>& getUnitHistories() const;

        const std::list<MilitaryMissionDataPtr>& getMissions() const;

        MilitaryMissionDataPtr getMissionData(CvUnitAI* pUnit);
        MilitaryMissionDataPtr getCityDefenceMission(IDInfo city);

        const std::map<XYCoords, std::list<IDInfo> >& getEnemyStacks() const;
        PlotUnitDataMap getEnemyUnitData(int subArea) const;

        const std::map<XYCoords, CombatData>& getAttackCombatMap() const;
        const std::map<XYCoords, CombatData>& getDefenceCombatMap() const;

        const std::map<XYCoords, CombatGraph::Data>& getAttackCombatData() const;
        const std::map<XYCoords, CombatGraph::Data>& getDefenceCombatData() const;

        boost::shared_ptr<MilitaryAnalysisImpl> getImpl() const;

        PlotSet getThreatenedPlots() const;
        std::set<IDInfo> getUnitsThreateningPlots(const PlotSet& plots) const;
        PlotUnitsMap getPlotsThreatenedByUnits(const PlotSet& plots) const;
        CvUnit* getNextAttackUnit();

        std::map<int /* sub area */, std::set<XYCoords> > getLandScoutMissionRefPlots() const;

        size_t getUnitCount(MissionAITypes missionType, UnitTypes unitType = NO_UNIT, int subArea = -1) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static float attThreshold, defAttackThreshold, defThreshold, hostileAttackThreshold;

    private:
        boost::shared_ptr<MilitaryAnalysisImpl> pImpl_;
    };

    PlotUnitsMap getNearbyHostileStacks(const Player& player, const CvPlot* pPlot, int range);
    std::set<XYCoords> getUnitHistory(const Player& player, IDInfo unit);

    void updateMilitaryAnalysis(Player& player);
    bool doUnitAnalysis(Player& player, CvUnitAI* pUnit);
}