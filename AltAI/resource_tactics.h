#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"
#include "./tactic_selection_data.h"

namespace AltAI
{
    class ResourceTactics : public boost::enable_shared_from_this<ResourceTactics>
    {
    public:
        ResourceTactics() : playerType_(NO_PLAYER), bonusType_(NO_BONUS) {}
        ResourceTactics(PlayerTypes playerType, BonusTypes bonusType) : playerType_(playerType), bonusType_(bonusType) {}

        void addTactic(const IResourceTacticPtr& pResourceTactic);
        void setTechDependency(const ResearchTechDependencyPtr& pTechDependency);
        void update(const Player& player);
        void updateDependencies(const Player& player);
        void apply(TacticSelectionDataMap& tacticSelectionDataMap, int ignoreFlags);
        void apply(TacticSelectionData& selectionData);

        BonusTypes getBonusType() const;
        PlayerTypes getPlayerType() const;

        ResearchTechDependencyPtr getTechDependency() const;

        void debug(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static const int ID = 0;
        static ResourceTacticsPtr factoryRead(FDataStreamBase* pStream);

    private:
        PlayerTypes playerType_;
        BonusTypes bonusType_;
        std::list<IResourceTacticPtr> resourceTactics_;
        ResearchTechDependencyPtr techDependency_;
        std::map<IDInfo, ProjectionLadder> cityProjections_;
    };

    class EconomicResourceTactic : public IResourceTactic
    {
    public:
        virtual void debug(std::ostream&) const;

        virtual void update(const ResourceTacticsPtr&, const City&);
        virtual void update(const ResourceTacticsPtr&, const Player&);
        virtual void apply(const ResourceTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = 0;

    private:
        std::map<IDInfo, ProjectionLadder> cityProjections_;
    };

    class UnitResourceTactic : public IResourceTactic
    {
    public:
        virtual void debug(std::ostream&) const;

        virtual void update(const ResourceTacticsPtr&, const City&);
        virtual void update(const ResourceTacticsPtr&, const Player&);
        virtual void apply(const ResourceTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = 1;
    };
}