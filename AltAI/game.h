#pragma once

#include "./utils.h"

namespace AltAI
{
    class Player;
	typedef boost::shared_ptr<Player> PlayerPtr;
    class Team;
    typedef boost::shared_ptr<Team> TeamPtr;

    // singleton class which contains the current game state for all players
    class Game : boost::noncopyable
    {
    public:
        explicit Game(CvGame* pGame);
        void addPlayer(CvPlayer* player);
        void addPlayerDone(CvPlayer* player);
        void setInit(bool value);
        bool isInit() const;

        void logMap(const CvMap& map) const;

        PlayerPtr getPlayer(PlayerTypes playerType) const;
        TeamPtr getTeam(TeamTypes teamType) const;
    private:
        typedef std::map<PlayerTypes, PlayerPtr> PlayerMap;
        typedef std::map<TeamTypes, TeamPtr> TeamMap;

        PlayerMap players_;
        TeamMap teams_;
        CvGame* pGame_;
        bool init_;
    };
}