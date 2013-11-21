#include "AltAI.h"

#include "./player_tech_tactics.h"

namespace AltAI
{
    void PlayerTechTactics::addTactic(const ITechTacticPtr& pTechTactic)
    {
        techTactics_.push_back(pTechTactic);
    }

    void PlayerTechTactics::apply(TacticSelectionData& selectionData)
    {
        for (size_t i = 0, count = techTactics_.size(); i < count; ++i)
        {
            techTactics_[i]->apply(shared_from_this(), selectionData);
        }
    }

    void PlayerTechTactics::debug(std::ostream& os) const
    {
        os << "\nPlayer tech tactics for tech: " << gGlobals.getTechInfo(techType_).getType();
        for (size_t i = 0, count = techTactics_.size(); i < count; ++i)
        {
            techTactics_[i]->debug(os);
        }
    }

    TechTypes PlayerTechTactics::getTechType() const
    {
        return techType_;
    }

    PlayerTypes PlayerTechTactics::getPlayer() const
    {
        return playerType_;
    }

    void PlayerTechTactics::write(FDataStreamBase* pStream) const
    {
        pStream->Write(ID);

        pStream->Write(techType_);
        pStream->Write(playerType_);

        const size_t tacticCount = techTactics_.size();
        pStream->Write(tacticCount);
        for (size_t i = 0; i < tacticCount; ++i)
        {
            techTactics_[i]->write(pStream);
        }
    }

    void PlayerTechTactics::read(FDataStreamBase* pStream)
    {
        pStream->Read((int*)&techType_);
        pStream->Read((int*)&playerType_);

        size_t tacticCount;
        pStream->Read(&tacticCount);
        techTactics_.clear();
        for (size_t i = 0; i < tacticCount; ++i)
        {
            techTactics_.push_back(ITechTactic::factoryRead(pStream));
        }
    }
}