#pragma once

#include "./utils.h"
#include "./unit_analysis.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class City;

    struct PlayerTactics;

    class UnitValueHelper
    {
    public:
        typedef std::pair<int, std::vector<std::pair<UnitTypes, int> > > UnitCostAndOddsData;
        typedef std::map<UnitTypes, UnitCostAndOddsData> MapT;

        int getValue(const UnitCostAndOddsData& mapEntry) const;
        void addMapEntry(MapT& unitCombatData, UnitTypes unitType, const std::vector<UnitTypes>& possibleCombatUnits, const std::vector<int>& odds) const;

        void debug(const MapT& unitCombatData, std::ostream& os) const;
    };
    
    std::pair<std::vector<UnitTypes>, std::vector<UnitTypes> > getActualAndPossibleCombatUnits(const Player& player, const CvCity* pCity, DomainTypes domainType);

    struct StackCombatData
    {
        StackCombatData() : attackerIndex(MAX_INT), defenderIndex(MAX_INT) {}
        size_t attackerIndex, defenderIndex;
        UnitOddsData odds;
    };

    StackCombatData getBestUnitOdds(const Player& player, const UnitData::CombatDetails& combatDetails, const std::vector<UnitData>& attackers, const std::vector<UnitData>& defenders, bool debug = false);

    std::list<StackCombatData> getBestUnitOdds(const Player& player, const UnitData::CombatDetails& combatDetails, 
        const std::vector<UnitData>& attackers, const std::vector<UnitData>& defenders, const int oddsThreshold, bool debug = false);

    struct StackCombatDataNode;
    typedef boost::shared_ptr<StackCombatDataNode> StackCombatDataNodePtr;

    struct StackCombatDataNode
    {
        StackCombatDataNode() : parentNode(NULL) {}
        explicit StackCombatDataNode(const StackCombatDataNode* parentNode_) : parentNode(parentNode_) {}
        StackCombatData data;
        std::vector<UnitData> attackers, defenders;
        StackCombatDataNodePtr winNode, lossNode, drawNode;
        const StackCombatDataNode* parentNode;
        StackCombatDataNodePtr getBestOutcome() const { return winNode ? winNode : drawNode ? drawNode : lossNode; }
        StackCombatDataNodePtr getWorstOutcome() const { return lossNode ? lossNode : drawNode ? drawNode : winNode; }
        bool isEndState() const { return !(winNode || drawNode || lossNode); }
    };

    struct CombatGraph
    {
        StackCombatDataNodePtr pRootNode;
        std::list<StackCombatDataNodePtr> endStates;

        struct Data
        {
            Data() : pWin(0), pLoss(0), pDraw(0) {}
            explicit Data(const CombatGraph& pGraph)
                : attackers(pGraph.pRootNode->attackers), defenders(pGraph.pRootNode->defenders), attackerUnitOdds(pGraph.pRootNode->attackers.size()), defenderUnitOdds(pGraph.pRootNode->defenders.size()),
                  longestAndShortestAttackOrder(pGraph.getLongestAndShortestAttackOrder()), pWin(0), pLoss(0), pDraw(0)
            {
            }

            bool isEmpty() const { return attackers.empty() && defenders.empty(); }
            void debug(std::ostream& os) const;
            void write(FDataStreamBase* pStream) const;
            void read(FDataStreamBase* pStream);

            std::vector<UnitData> attackers, defenders;
            std::vector<float> attackerUnitOdds, defenderUnitOdds;
            std::pair<std::list<IDInfo>, std::list<IDInfo> > longestAndShortestAttackOrder;
            float pWin, pLoss, pDraw;
        };

        Data endStatesData;

        void analyseEndStates();
        void debugEndStates(std::ostream& os) const;
        
        size_t getFirstUnitIndex(bool isAttacker) const;
        std::list<IDInfo> getUnitOrdering(std::list<StackCombatDataNodePtr>::const_iterator endStateIter) const;
        std::pair<std::list<IDInfo>, std::list<IDInfo> > getLongestAndShortestAttackOrder() const;
        float getSurvivalOdds(IDInfo unitId, bool isAttacker) const;
    };

    CombatGraph getCombatGraph(const Player& player, const UnitData::CombatDetails& combatDetails, 
        const std::vector<UnitData>& ourUnits, const std::vector<UnitData>& theirUnits, double oddsThreshold = 0.0001);

    std::map<PromotionTypes, float> getPromotionValues(const Player& player, const CvUnit* pUnit, const std::vector<PromotionTypes>& availablePromotions,
        const UnitData::CombatDetails& combatDetails, bool isAttacker,
        const std::vector<UnitData>& attackers, const std::vector<UnitData>& defenders, double oddsThreshold = 0.0001);

    struct RequiredUnitStack
    {
        typedef std::list<UnitData> UnitDataChoices;
        std::vector<UnitDataChoices> unitsToBuild;
        std::vector<IDInfo> existingUnits;
    };

    RequiredUnitStack getRequiredUnits(const Player& player, const CvPlot* pTargetPlot, const std::vector<const CvUnit*>& enemyStack, const std::set<IDInfo>& availableUnits);

    struct UnitMovementData
    {
        UnitMovementData() : moves(0), extraMovesDiscount(0),
            featureDoubleMoves(gGlobals.getNumFeatureInfos(), 0), terrainDoubleMoves(gGlobals.getNumTerrainInfos(), 0),
            isHillsDoubleMoves(false), ignoresTerrainCost(false), isEnemyRoute(false), ignoresImpassable(false), canAttack(false)
        {
        }

        explicit UnitMovementData(const CvUnit* pUnit);
        explicit UnitMovementData(const UnitData& unitData);

        // allows unique differentiation of any combination of unit movement attributes
        // useful to efficiently calculate possible moves for a large stack without checking all its units
        bool operator < (const UnitMovementData& other) const;

        // ignore flatMovementCost flag as not used for any standard game units
        int moves, extraMovesDiscount;
        std::vector<char> featureDoubleMoves, terrainDoubleMoves;
        bool isHillsDoubleMoves, ignoresTerrainCost, isEnemyRoute, ignoresImpassable;
        bool canAttack;

        void debug(std::ostream& os) const;
    };

    int landMovementCost(const UnitMovementData& unit, const CvPlot* pFromPlot, const CvPlot* pToPlot, const CvTeamAI& unitsTeam);
    int seaMovementCost(const UnitMovementData& unit, const CvPlot* pFromPlot, const CvPlot* pToPlot, const CvTeamAI& unitsTeam);

    // todo - add no. of steps data - useful for comparing paths with the same no. of turns
    struct UnitPathData
    {
        UnitPathData() : pathTurns(-1), valid(false) {}

        void calculate(const CvSelectionGroup* pGroup, const CvPlot* pTargetPlot, const int flags);
        XYCoords getLastVisiblePlotWithMP() const;
        XYCoords getFirstStepCoords() const;
        XYCoords getFirstTurnEndCoords() const;
        void debug(std::ostream& os) const;

        int pathTurns;
        bool valid;
        struct Node
        {
            Node() : turn(-1), movesLeft(-1), coords(-1, -1), visible(false) {}
            Node(int turn_, int movesLeft_, XYCoords coords_, bool visible_)
                : turn(turn_), movesLeft(movesLeft_), coords(coords_), visible(visible_)
            {}

            int turn, movesLeft;
            XYCoords coords;
            bool visible;
        };
        std::list<Node> nodes;
    };

    RequiredUnitStack getRequiredAttackStack(const Player& player, const std::vector<UnitTypes>& availableUnits, const CvPlot* pTargetPlot, 
            const std::vector<const CvUnit*>& enemyStack, const std::set<IDInfo>& existingUnits);

    // map of plots -> map of units -> movement points
    typedef std::map<const CvPlot*, std::map<const CvUnit*, int, CvUnitIDInfoOrderF>, CvPlotOrderF> UnitMovementDataMap;

    struct ReachablePlotsData
    {
        std::map<UnitMovementData, std::vector<const CvUnit*> > unitGroupingMap;
        UnitMovementDataMap unitMovementDataMap;
        PlotSet allReachablePlots;

        void debug(std::ostream& os) const;
    };

    
    void getReachablePlotsData(ReachablePlotsData& reachablePlotsData, const Player& player, const std::vector<const CvUnit*>& unitStack, bool useMaxMoves, bool allowAttack = false);

    struct GroupCombatData
    {
        void calculate(const Player& player, const CvSelectionGroup* pGroup);
        void calculate(const Player& player, const std::vector<const CvUnit*>& units);
        void debug(std::ostream& os) const;
        void debugCombatResultsMap(std::ostream& os, const std::map<XYCoords, CombatGraph::Data>& resultsMap) const;
        CombatGraph::Data getCombatData(XYCoords coords, bool isAttack) const;

        std::map<XYCoords, CombatGraph::Data> attackCombatResultsMap, defenceCombatResultsMap;
    };

    struct CombatMoveData
    {
        CombatMoveData() : stepDistanceFromTarget(-1) {}
        CombatMoveData(XYCoords coords_, int stepDistanceFromTarget_, CombatGraph::Data defenceOdds_, CombatGraph::Data attackOdds_)
            : coords(coords_), stepDistanceFromTarget(stepDistanceFromTarget_), defenceOdds(defenceOdds_), attackOdds(attackOdds_) {}

        XYCoords coords;
        int stepDistanceFromTarget;
        CombatGraph::Data defenceOdds, attackOdds;        
    };

    struct AttackMoveDataPred
    {
        explicit AttackMoveDataPred(const double threshold_) : threshold(threshold_) {}
        bool operator () (const CombatMoveData& first, const CombatMoveData& second) const;

        const double threshold;
    };

    struct DefenceMoveDataPred
    {
        explicit DefenceMoveDataPred(const double threshold_) : threshold(threshold_) {}
        bool operator () (const CombatMoveData& first, const CombatMoveData& second) const;

        const double threshold;
    };

    const CvPlot* getEscapePlot(const Player& player, const CvSelectionGroup* pGroup, const PlotSet& ourReachablePlots, const PlotSet& dangerPlots,
        const PlotUnitsMap& nearbyHostiles);

    const CvPlot* getNextMovePlot(const Player& player, const CvSelectionGroup* pGroup, const CvPlot* pTargetPlot);

    // simplified version of canMoveInto() - includes current plot, any plot which unit could attack another unit, ignores ship loading moves
    bool couldMoveUnitIntoPlot(const CvUnit* pUnit, const CvPlot* pPlot, bool includeAttackPlots, bool includeDeclareWarPlots, bool useMaxMoves, bool ignoreIfAlreadyAttacked);
}