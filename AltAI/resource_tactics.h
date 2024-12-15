#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"
#include "./tactic_selection_data.h"

namespace AltAI
{
    class ResourceTactics : public boost::enable_shared_from_this<ResourceTactics>
    {
    public:
        ResourceTactics() : playerType_(NO_PLAYER), bonusType_(NO_BONUS), lastTurnCalculated_(-1) {}
        ResourceTactics(PlayerTypes playerType, BonusTypes bonusType) : playerType_(playerType), bonusType_(bonusType), lastTurnCalculated_(-1) {}

        void addTactic(const IResourceTacticPtr& pResourceTactic);
        void setTechDependency(const ResearchTechDependencyPtr& pTechDependency);
        void update(Player& player);
        void updateDependencies(const Player& player);
        void apply(TacticSelectionDataMap& tacticSelectionDataMap, int depTacticFlags);
        void apply(TacticSelectionData& selectionData);

        BonusTypes getBonusType() const;
        PlayerTypes getPlayerType() const;

        ResearchTechDependencyPtr getTechDependency() const;

        int getTurnLastUpdated() const;
        void debug(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static const int ResourceTacticsID = 0;
        static ResourceTacticsPtr factoryRead(FDataStreamBase* pStream);

    private:
        PlayerTypes playerType_;
        BonusTypes bonusType_;
        std::list<IResourceTacticPtr> resourceTactics_;
        ResearchTechDependencyPtr techDependency_;
        std::map<IDInfo, ProjectionLadder> cityProjections_;
        int lastTurnCalculated_;  // not saved currently  - need to save all projection data first to avoid recalc after load
    };

    class EconomicResourceTactic : public IResourceTactic
    {
    public:
        virtual void debug(std::ostream&) const;

        virtual void update(const ResourceTacticsPtr&, City&);
        virtual void update(const ResourceTacticsPtr&, Player&);
        virtual void apply(const ResourceTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = IResourceTactic::EconomicResourceTacticID;

    private:
        // resource ladder, base ladder
        std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> > cityProjections_;
    };

    class UnitResourceTactic : public IResourceTactic
    {
    public:
        UnitResourceTactic() : unitType_(NO_UNIT) {}
        explicit UnitResourceTactic(UnitTypes unitType) : unitType_(unitType) {}

        virtual void debug(std::ostream&) const;

        virtual void update(const ResourceTacticsPtr&, City&);
        virtual void update(const ResourceTacticsPtr&, Player&);
        virtual void apply(const ResourceTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = IResourceTactic::UnitResourceTacticID;

    private:
        UnitTypes unitType_;
    };

    class BuildingResourceTactic : public IResourceTactic
    {
    public:
        BuildingResourceTactic() : buildingType_(NO_BUILDING), resourceModifier_(0) {}
        BuildingResourceTactic(BuildingTypes buildingType, int resourceModifier) : buildingType_(buildingType), resourceModifier_(resourceModifier) {}

        virtual void debug(std::ostream&) const;

        virtual void update(const ResourceTacticsPtr&, City&);
        virtual void update(const ResourceTacticsPtr&, Player&);
        virtual void apply(const ResourceTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = IResourceTactic::BuildingResourceTacticID;

    private:
        BuildingTypes buildingType_;
        int resourceModifier_;
    };
}