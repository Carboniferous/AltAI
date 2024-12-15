#pragma once

#include "./utils.h"
#include "./player.h"
#include "./tactics_interfaces.h"
#include "./tactic_selection_data.h"

namespace AltAI
{
    struct PlayerTactics;

    class ReligionAnalysis
    {
    public:
        explicit ReligionAnalysis(Player& player);

        void init();
        void addCity(const CvCity* pCity);
        void addReligion(const CvCity* pCity, ReligionTypes religionType, bool newValue);

        void update();
        void update(City& city);
        void apply(TacticSelectionData& selectionData);
        void apply(TacticSelectionDataMap& selectionDataMap, int depTacticFlags);

        ReligionUnitRequestData getUnitRequestBuild(const CvCity* pCity, PlayerTactics& playerTactics);
        void setUnitRequestBuild(const CvCity* pCity, PlayerTactics& playerTactics);

        void updateMissionaryMission(CvUnitAI* pUnit);

    private:
        std::set<DependencyItemSet, DependencyItemsComp> getReligionDeps() const;
        bool cityHasRequest_(IDInfo city, ReligionTypes religionType) const;
        void debugRequestsMap(std::ostream& os) const;

        Player& player_;
        std::map<IDInfo, std::set<ReligionTypes> > cityReligions_;
        std::set<ReligionTypes> ourReligions_;
        std::map<IDInfo, std::map<ReligionTypes, std::pair<int, ReligionUnitRequestData> > > religionRequestsMap_;
        std::map<IDInfo, IDInfo> missionaryMissionMap_;
    };

    class ReligionTactics : public boost::enable_shared_from_this<ReligionTactics>
    {
    public:
        ReligionTactics() : playerType_(NO_PLAYER), religionType_(NO_RELIGION), spreadUnitType_(NO_UNIT), lastTurnCalculated_(-1) {}
        ReligionTactics(PlayerTypes playerType, ReligionTypes religionType);

        void addTactic(const IReligionTacticPtr& pReligionTactic);
        void setTechDependency(const ResearchTechDependencyPtr& pTechDependency);
        void update(Player& player);
        void update(Player& player, City& city);
        void updateDependencies(const Player& player);
        void apply(TacticSelectionDataMap& tacticSelectionDataMap, int depTacticFlags);
        void apply(TacticSelectionData& selectionData);

        ReligionTypes getReligionType() const;
        PlayerTypes getPlayerType() const;
        UnitTypes getSpreadUnitType() const;
        BuildingTypes getSpreadBuildingType() const;

        ResearchTechDependencyPtr getTechDependency() const;

        int getTurnLastUpdated() const;
        void debug(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static const int ReligionTacticsID = 0;
        static ReligionTacticsPtr factoryRead(FDataStreamBase* pStream);

    private:
        PlayerTypes playerType_;
        ReligionTypes religionType_;
        UnitTypes spreadUnitType_;
        BuildingTypes spreadBuildingType_;
        std::list<IReligionTacticPtr> religionTactics_;
        ResearchTechDependencyPtr techDependency_;
        std::map<IDInfo, ProjectionLadder> cityProjections_;
        int lastTurnCalculated_;  // not saved currently  - need to save all projection data first to avoid recalc after load
    };

    class EconomicReligionTactic : public IReligionTactic
    {
    public:
        virtual void debug(std::ostream&) const;

        virtual void update(const ReligionTacticsPtr&, City&);
        virtual void update(const ReligionTacticsPtr&, Player&);
        virtual void apply(const ReligionTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = IReligionTactic::EconomicReligionTacticID;

    private:
        // ladder with religion, base ladder
        std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> > cityProjections_;
    };

    class UnitReligionTactic : public IReligionTactic
    {
    public:
        virtual void debug(std::ostream&) const;

        virtual void update(const ReligionTacticsPtr&, City&);
        virtual void update(const ReligionTacticsPtr&, Player&);
        virtual void apply(const ReligionTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = IReligionTactic::UnitReligionTacticID;

    private:
    };

    bool doMissionaryMove(Player& player, CvUnitAI* pUnit);
}