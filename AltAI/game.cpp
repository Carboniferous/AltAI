#include "AltAI.h"

#include "./game.h"
#include "./plot_info.h"
#include "./player.h"
#include "./city.h"
#include "./unit.h"
#include "./team.h"
#include "./gamedata_analysis.h"
#include "./sub_area.h"
#include "./irrigatable_area.h"
#include "./error_log.h"
#include "./civ_log.h"
#include "./iters.h"

#include "CvPlayerAI.h"

namespace AltAI
{
    Game::Game(CvGame* pGame) : pGame_(pGame), init_(false)
    {
        GameDataAnalysis::getInstance()->analyse();
    }

    void Game::addPlayer(CvPlayer* player)
    {
        PlayerMap::iterator playerIter = players_.insert(std::make_pair(player->getID(), boost::shared_ptr<Player>(new Player(player)))).first;

        TeamMap::iterator teamIter(teams_.find(player->getTeam()));

        if (teamIter == teams_.end())
        {
            teamIter = teams_.insert(std::make_pair(player->getTeam(), boost::shared_ptr<Team>(new Team(&CvTeamAI::getTeam(player->getTeam()))))).first;
        }
        teamIter->second->addPlayer(playerIter->second);
    }

    void Game::addPlayerDone(CvPlayer* player)
    {
        //if (player->isAlive())
        //{
        //}
    }

    void Game::setInit(bool value)
    {
        init_ = value;
    }

    bool Game::isInit() const
    {
        return init_;
    }

    boost::shared_ptr<Player> Game::getPlayer(PlayerTypes playerType) const
    {
        PlayerMap::const_iterator iter(players_.find(playerType));
        if (iter == players_.end())
        {
            std::ostream& os = ErrorLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
            os << "Player: " << playerType << " not found?\n";
            return boost::shared_ptr<Player>();
        }

        return iter->second;
    }

    boost::shared_ptr<Team> Game::getTeam(TeamTypes teamType) const
    {
        TeamMap::const_iterator iter(teams_.find(teamType));
        if (iter == teams_.end())
        {
            AltAI::PlayerIDIter playerIter(teamType);
            PlayerTypes playerType = NO_PLAYER;
            while ((playerType = playerIter()) != NO_PLAYER)
            {
                std::ostream& os = ErrorLog::getLog(CvPlayerAI::getPlayer(playerType))->getStream();
                os << "Team: " << teamType << " not found?\n";
            }
            return boost::shared_ptr<Team>();
        }

        return iter->second;
    }

    // todo - get area counting code out of here; separate to logging
    void Game::logMap(const CvMap& map) const
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer((PlayerTypes)0))->getStream();
#endif
        std::map<int, std::pair<int, int> > irrigatableAreaCounts;

        // TODO check that at least one plot in the area has fresh water if it is irrigatable
        for (int plotIndex = 0, plotCount = map.numPlots(); plotIndex < plotCount; ++plotIndex)
	    {
		    CvPlot* pPlot = map.plotByIndex(plotIndex);
            int irrigatableAreaID = pPlot->getIrrigatableArea();
            if (irrigatableAreaID != FFreeList::INVALID_INDEX)
            {
                int subAreaID = pPlot->getSubArea();
                std::map<int, std::pair<int, int> >::iterator iter = irrigatableAreaCounts.find(irrigatableAreaID);
                if (iter == irrigatableAreaCounts.end())
                {
                    irrigatableAreaCounts.insert(std::make_pair(irrigatableAreaID, std::make_pair(subAreaID, 1)));
                }
                else
                {
                    FAssertMsg(iter->second.first == subAreaID, "Irrigatable Area has inconsistent SubArea IDs");
                    ++iter->second.second;
                }
            }
        }
#ifdef ALTAI_DEBUG
        os << "\n";
#endif
        for (std::map<int, std::pair<int, int> >::const_iterator ci(irrigatableAreaCounts.begin()), ciEnd(irrigatableAreaCounts.end()); ci != ciEnd; ++ci)
        {
            boost::shared_ptr<AltAI::IrrigatableArea> pIrrigatableArea = map.getIrrigatableArea(ci->first);
            pIrrigatableArea->setNumTiles(ci->second.second);
#ifdef ALTAI_DEBUG
            {
                os << "\nIrrigatable Area: " << ci->first << " irrigatable = " << pIrrigatableArea->isIrrigatable() 
                   << " wet = " << pIrrigatableArea->hasFreshWaterAccess()
                   << " (Sub Area ID = " << pIrrigatableArea->getSubAreaID() << ") has: "
                   << ci->second.second << " plots (out of " << map.getSubArea(ci->second.first)->getNumTiles() << ")\n";
            }
#endif
            if (pIrrigatableArea->isIrrigatable() && !pIrrigatableArea->hasFreshWaterAccess())
            {
#ifdef ALTAI_DEBUG
                os << "\n";
                for (int plotIndex = 0, plotCount = map.numPlots(); plotIndex < plotCount; ++plotIndex)
	            {
		            CvPlot* pPlot = map.plotByIndex(plotIndex);
                    int irrigatableAreaID = pPlot->getIrrigatableArea();
                    if (irrigatableAreaID == ci->first)
                    {
                        os << XYCoords(pPlot->getX(), pPlot->getY()) << ", ";
                    }
                }
                os << "\n";
#endif
            }
        }
#ifdef ALTAI_DEBUG
        os << "\n";
#endif
    }
}