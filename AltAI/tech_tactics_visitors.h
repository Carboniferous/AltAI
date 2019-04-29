#pragma once

#include "./utils.h"
#include "./tactic_actions.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class Player;
    class TechInfo;

    //ResearchTech getReligionTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);
    //ResearchTech getEconomicTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);
    //ResearchTech getMilitaryTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);
    //ResearchTech getWorkerTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);

    std::list<CityImprovementTacticsPtr> makeCityBuildTactics(const Player& player, const City& city);

    ITechTacticsPtr makeTechTactics(const Player& player, const boost::shared_ptr<TechInfo>& pTechInfo);
}