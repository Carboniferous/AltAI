#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class PlayerTechTactics : public ITechTactics, public boost::enable_shared_from_this<PlayerTechTactics>
    {
    public:
        PlayerTechTactics() : techType_(NO_TECH), playerType_(NO_PLAYER) {}
        PlayerTechTactics(TechTypes techType, PlayerTypes playerType) : techType_(techType), playerType_(playerType) {}

        virtual void debug(std::ostream&) const;
        virtual TechTypes getTechType() const;
        virtual PlayerTypes getPlayer() const;

        virtual void addTactic(const ITechTacticPtr&);
        virtual void removeTactic(const int tacticID);
        virtual void apply(TacticSelectionData&);

        // save/load functions
        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int PlayerTechTacticsID = 0;

    private:
        TechTypes techType_;
        PlayerTypes playerType_;
        
        std::list<ITechTacticPtr> techTactics_;
    };
}