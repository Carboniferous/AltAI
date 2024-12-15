#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"
#include "./tactic_selection_data.h"

namespace AltAI
{
    class CivicTactics : public boost::enable_shared_from_this<CivicTactics>
    {
    public:
        CivicTactics() : playerType_(NO_PLAYER), civicType_(NO_CIVIC) {}
        CivicTactics(PlayerTypes playerType, CivicTypes civicType) : playerType_(playerType), civicType_(civicType), civicCost_(0) {}

        void addTactic(const ICivicTacticPtr& pCivicTactic);
        void setTechDependency(const ResearchTechDependencyPtr& pTechDependency);
        void update(Player& player);
        void updateDependencies(Player& player);
        void apply(TacticSelectionDataMap& tacticSelectionDataMap, int depTacticFlags);
        void apply(TacticSelectionData& selectionData);

        CivicTypes getCivicType() const;
        PlayerTypes getPlayerType() const;

        ResearchTechDependencyPtr getTechDependency() const;

        void debug(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static const int CivicTacticsID = 0;
        static CivicTacticsPtr factoryRead(FDataStreamBase* pStream);

    private:
        PlayerTypes playerType_;
        CivicTypes civicType_;
        std::list<ICivicTacticPtr> civicTactics_;
        ResearchTechDependencyPtr techDependency_;
        std::map<IDInfo, ProjectionLadder> cityProjections_;
        int civicCost_;
    };

    class EconomicCivicTactic : public ICivicTactic
    {
    public:
        virtual void debug(std::ostream&) const;
        virtual void update(const CivicTacticsPtr&, Player&);
        virtual void apply(const CivicTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        std::map<IDInfo, ProjectionLadder> cityProjections_;

        static const int ID = ICivicTactic::EconomicCivicTacticID;
    };

    class HurryCivicTactic : public ICivicTactic
    {
    public:
        HurryCivicTactic() : hurryType_(NO_HURRY), productionPerPopulation_(0), goldPerProduction_(0) {}
        explicit HurryCivicTactic(HurryTypes hurryType);

        virtual void debug(std::ostream&) const;
        virtual void update(const CivicTacticsPtr&, Player&);
        virtual void apply(const CivicTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = ICivicTactic::HurryCivicTacticID;

        HurryTypes hurryType_;
        int productionPerPopulation_, goldPerProduction_;
        std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> > cityProjections_;
    };

    class HappyPoliceCivicTactic : public ICivicTactic
    {
    public:
        HappyPoliceCivicTactic() : happyPerUnit_(0) {}
        explicit HappyPoliceCivicTactic(int happyPerUnit);

        virtual void debug(std::ostream&) const;
        virtual void update(const CivicTacticsPtr&, Player&);
        virtual void apply(const CivicTacticsPtr&, TacticSelectionData&);

        virtual void write(FDataStreamBase*) const;
        virtual void read(FDataStreamBase*);

        static const int ID = ICivicTactic::HappyPoliceCivicTacticID;

        int happyPerUnit_;
        std::map<IDInfo, std::pair<ProjectionLadder, ProjectionLadder> > cityProjections_;
    };
}