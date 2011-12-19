#pragma once

#include "./utils.h"

namespace AltAI
{
    class Player;
    class Team;

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

        boost::shared_ptr<Player> getPlayer(PlayerTypes playerType) const;
        boost::shared_ptr<Team> getTeam(TeamTypes teamType) const;
    private:
        typedef std::map<PlayerTypes, boost::shared_ptr<Player> > PlayerMap;
        typedef std::map<TeamTypes, boost::shared_ptr<Team> > TeamMap;

        PlayerMap players_;
        TeamMap teams_;
        CvGame* pGame_;
        bool init_;
    };
}