#pragma once

#include "./utils.h"
#include "./tactic_actions.h"

namespace AltAI
{
    class Player;
    class TechInfo;

    ResearchTech getReligionTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);
    ResearchTech getEconomicTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);
    ResearchTech getMilitaryTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);
    ResearchTech getWorkerTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);
}