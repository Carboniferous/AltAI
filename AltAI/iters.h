#pragma once

#include "../CvGameCoreDLL/CvGameCoreDLL.h"
#include "../CvGameCoreDLL/CvStructs.h"

namespace AltAI
{
    struct PlotGroupIter
    {
        explicit PlotGroupIter(const CvPlayer* pPlayer_) : pPlayer(pPlayer_), iLoop(0)
        {
        }

        CvPlotGroup* operator() ()
        {
            return pPlayer->nextPlotGroup(&iLoop);
        }

        const CvPlayer* pPlayer;
        int iLoop;
    };

    struct PlotGroupPlotIter
    {
        explicit PlotGroupPlotIter(CvPlotGroup* pPlotGroup_) : pPlotGroup(pPlotGroup_), pNode(NULL)
        {
        }

        CLLNode<XYCoords>* operator() ()
        {
            return pNode ? pNode = pPlotGroup->nextPlotsNode(pNode) : pNode = pPlotGroup->headPlotsNode();
        }

        CvPlotGroup* pPlotGroup;
        CLLNode<XYCoords>* pNode;
    };

    struct AreaIter
    {
        explicit AreaIter(CvMap* pMap_) : pMap(pMap_), iLoop(0)
        {
        }

        CvArea* operator() ()
        {
            return pMap->nextArea(&iLoop);
        }

        CvMap* pMap;
        int iLoop;
    };

    struct IterPlot
    {
        IterPlot(CvPlot* pPlot_, bool done_) : pPlot(pPlot_), done(done_) {}

        operator bool () const
        {
            return !done;
        }

        bool valid() const
        {
            return pPlot != NULL;
        }

        CvPlot* operator-> () const
        {
            return pPlot;
        }

        operator CvPlot* () const
        {
            return pPlot;
        }

        CvPlot* pPlot;
        bool done;
    };

    struct NeighbourPlotIter
    {
        NeighbourPlotIter(const CvPlot* pPlot_, int xRange_ = 1, int yRange_ = 1) : 
            pPlot(pPlot_), xRange(abs(xRange_)), x(-abs(xRange_)), yRange(abs(yRange_)), y(-abs(yRange_))
        {
        }

        IterPlot operator() ()
        {
            const int thisX = x, thisY = y;

            bool done = false;
            if (x < xRange)
            {
                ++x;
            }
            else
            {
                if (y < yRange)
                {
                    ++y;
                    x = -xRange;
                }
                else if (y == yRange)
                {
                    ++y;
                }
                else
                {
                    done = true;
                }
            }

            if (y == 0 && x == 0)
            {
                ++x;
            }

            return IterPlot(done ? NULL : plotXY(pPlot->getX(), pPlot->getY(), thisX, thisY), done);
        }

        const CvPlot* pPlot;
        const int xRange, yRange;
        int x, y;
    };

    struct CardinalPlotIter
    {
        CardinalPlotIter(const CvPlot* pPlot_) : 
            pPlot(pPlot_), x(0), y(0)
        {
        }

        IterPlot operator() ()
        {
            bool done = false;

            if (x == 0 && y == 0)
            {
                x = -1;
            }
            else if (x < 0)
            {
                x = 1;
            }
            else if (x > 0)
            {
                y = -1;
                x = 0;
            }
            else if (y < 0)
            {
                y = 1;
            }
            else
            {
                done = true;
            }
            return IterPlot(done ? NULL : plotXY(pPlot->getX(), pPlot->getY(), x, y), done);
        }

        const CvPlot* pPlot;
        int x, y;
    };

    struct CityPlotIter
    {
        explicit CityPlotIter(const CvCity* pCity_) : pCity(pCity_), iLoop(0)
        {
        }

        IterPlot operator() ()
        {
            bool done = iLoop == NUM_CITY_PLOTS;
            IterPlot iterPlot(done ? NULL : pCity->getCityIndexPlot(iLoop), done);
            ++iLoop;
            return iterPlot;
        }

        const CvCity* pCity;
        int iLoop;
    };

    struct CultureRangePlotIter
    {
        CultureRangePlotIter(const CvPlot* pCentre_, CultureLevelTypes cultureLevel_)
            : pCentre(pCentre_), cultureLevel(cultureLevel_), xRange(::abs(cultureLevel_)), x(-::abs(cultureLevel_)), yRange(::abs(cultureLevel_)), y(-::abs(cultureLevel_))
        {
        }

        IterPlot operator() ()
        {
            bool found = false;
            CvPlot* pPlot = NULL;

            while (!found)
            {
                // TODO - check if can do plotDistance even if plot might not be valid
                pPlot = plotXY(pCentre->getX(), pCentre->getY(), x, y);
                if (pPlot && plotDistance(pCentre->getX(), pCentre->getY(), pPlot->getX(), pPlot->getY()) == cultureLevel)
                {
                    found = true;
                }

                if (y <= yRange)
                {
                    ++y;
                }
                else
                {
                    if (x <= xRange)
                    {
                        y = -yRange;
                        ++x;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            return IterPlot(pPlot, !found);
        }

        const CvPlot* pCentre;
        CultureLevelTypes cultureLevel;
        const int xRange, yRange;
        int x, y;
    };

    struct AreaPlotIter
    {
        explicit AreaPlotIter(const CvArea* pArea_) : theMap(gGlobals.getMap()) , pArea(pArea_), plotCount(theMap.numPlots())
        {
            plotIndex = 0;            
        }

        IterPlot operator() ()
        {
            bool found = false;
            CvPlot* pPlot = NULL;
            for (; plotIndex < plotCount;)
            {
                pPlot = theMap.plotByIndex(plotIndex++);
                if (pPlot->area() == pArea)
                {
                    found = true;
                    break;
                }
            }
            return IterPlot(pPlot, !found);
        }

        const CvMap& theMap;
        const CvArea* pArea;        
        const int plotCount;
        int plotIndex;
    };

    struct SubAreaPlotIter
    {
        explicit SubAreaPlotIter(int subAreaID_) : theMap(gGlobals.getMap()), subAreaID(subAreaID_), plotCount(theMap.numPlots())
        {
            plotIndex = 0;
        }

        IterPlot operator() ()
        {
            bool found = false;
            CvPlot* pPlot = NULL;
            for (; plotIndex < plotCount;)
            {
                pPlot = theMap.plotByIndex(plotIndex++);
                if (pPlot->getSubArea() == subAreaID)
                {
                    found = true;
                    break;
                }
            }
            return IterPlot(pPlot, !found);
        }

        const CvMap& theMap;
        const int subAreaID;
        const int plotCount;
        int plotIndex;
    };

    struct IsSubAreaP
    {
        explicit IsSubAreaP(int subAreaID_) : subAreaID(subAreaID_)
        {
        }

        bool operator () (const CvCity* pCity) const
        {
            return pCity->plot()->getSubArea() == subAreaID;
        }

        const int subAreaID;
    };

    struct CityIter
    {
        explicit CityIter(const CvPlayer& player_) : player(player_), iLoop(0)
        {
        }

        CvCity* operator() ()
        {
            return player.nextCity(&iLoop);
        }

        const CvPlayer& player;
        int iLoop;
    };

    template <typename P>
        struct CityIterP
    {
        typedef typename P Pred;

        CityIterP(const CvPlayer& player_, Pred pred_) : player(player_), pred(pred_), iLoop(0)
        {
        }

        CvCity* operator() ()
        {
            CvCity* pCity = NULL;
            for (;;)
            {
                pCity = player.nextCity(&iLoop);
                if (!pCity || pred(pCity))
                {
                    break;
                }
            }
            return pCity;
        }

        const CvPlayer& player;
        P pred;
        int iLoop;
    };

    struct PlayerIter
    {
        PlayerIter() : index(0), teamType(NO_TEAM)
        {
        }

        explicit PlayerIter(TeamTypes teamType_) : index(0), teamType(teamType_)
        {
        }

        const CvPlayerAI* operator() ()
        {
            while (index < MAX_PLAYERS)
            {
                const CvPlayerAI& refPlayer = CvPlayerAI::getPlayer((PlayerTypes)index++);
                if (refPlayer.isAlive())
                {
                    if (teamType != NO_TEAM)
                    {
                        if (refPlayer.getTeam() == teamType)
                        {
                            return &refPlayer;
                        }
                    }
                    else
                    {
                        return &refPlayer;
                    }
                }
            }

            return NULL;
        }

        int index;
        TeamTypes teamType;
    };

    struct TeamIter
    {
        TeamIter() : index(0)
        {
        }

        const CvTeamAI* operator() ()
        {
            while (index < MAX_TEAMS)
            {
                const CvTeamAI& refTeam = CvTeamAI::getTeam((TeamTypes)index++);
                if (refTeam.getAliveCount() > 0)
                {
                    return &refTeam;
                }
            }

            return NULL;
        }

        int index;
    };

    struct PlayerIDIter
    {
        PlayerIDIter() : index(0), teamType(NO_TEAM)
        {
        }

        explicit PlayerIDIter(TeamTypes teamType_) : index(0), teamType(teamType_)
        {
        }

        PlayerTypes operator() ()
        {
            while (index < MAX_PLAYERS)
            {
                const CvPlayerAI& refPlayer = CvPlayerAI::getPlayer((PlayerTypes)index++);
                if (refPlayer.isAlive())
                {
                    if (teamType != NO_TEAM)
                    {
                        if (refPlayer.getTeam() == teamType)
                        {
                            return refPlayer.getID();
                        }
                    }
                    else
                    {
                        return refPlayer.getID();
                    }
                }
            }

            return NO_PLAYER;
        }

        int index;
        TeamTypes teamType;
    };

    struct TeamIDIter
    {
        TeamIDIter() : index(0)
        {
        }

        TeamTypes operator() ()
        {
            while (index < MAX_TEAMS)
            {
                const CvTeamAI& refTeam = CvTeamAI::getTeam((TeamTypes)index++);
                if (refTeam.getAliveCount() > 0)
                {
                    return refTeam.getID();
                }
            }

            return NO_TEAM;
        }

        int index;
    };

    struct TeamCityIter
    {
        explicit TeamCityIter(TeamTypes teamType_) : teamType(teamType_), playerIter(teamType_)
        {
            setCurrentPlayer_();
        }

        CvCity* operator() ()
        {            
            while (pCurrentPlayer)
            {
                CvCity* pCity = (*pCityIter)();
                if (pCity)
                {
                    return pCity;
                }
                else
                {
                    setCurrentPlayer_();
                }
            }
            
            return (CvCity*)0;
        }

        const CvPlayerAI* setCurrentPlayer_()
        {
            pCurrentPlayer = playerIter();
            if (pCurrentPlayer)
            {
                pCityIter = boost::shared_ptr<CityIter>(new CityIter(*pCurrentPlayer));
            }
            return pCurrentPlayer;
        }

        TeamTypes teamType;
        PlayerIter playerIter;
        const CvPlayerAI* pCurrentPlayer;
        boost::shared_ptr<CityIter> pCityIter;
    };

    struct SelectionGroupIter
    {
        explicit SelectionGroupIter(const CvPlayer& player_) : player(player_), iLoop(0) {}

        CvSelectionGroup* operator() ()
        {
            return player.nextSelectionGroup(&iLoop);
        }

        const CvPlayer& player;
        int iLoop;
    };

    struct UnitGroupIter
    {
        explicit UnitGroupIter(const CvSelectionGroup* pGroup_) : pGroup(pGroup_), pNode(NULL) {}

        const CvUnit* operator() ()
        {
            pNode ? pNode = pGroup->nextUnitNode(pNode) : pNode = pGroup->headUnitNode();
            return pNode ? getUnit(pNode->m_data) : NULL;
        }

        const CvSelectionGroup* pGroup;
        CLLNode<IDInfo>* pNode;
    };

    struct UnitPlotIter
    {
        explicit UnitPlotIter(const CvPlot* pPlot_) : pPlot(pPlot_), pNode(NULL) {}

        CvUnit* operator() ()
        {
            pNode ? pNode = pPlot->nextUnitNode(pNode) : pNode = pPlot->headUnitNode();
            return pNode ? getUnit(pNode->m_data) : NULL;
        }

        const CvPlot* pPlot;
        CLLNode<IDInfo>* pNode;
    };

    struct OurUnitsPred
    {
        explicit OurUnitsPred(PlayerTypes playerType_) : playerType(playerType_)
        {
        }

        bool operator () (const CLLNode<IDInfo>* pNode) const
        {
            return pNode->m_data.eOwner == playerType;
        }

        PlayerTypes playerType;
    };

    template <typename P>
        struct UnitPlotIterP
    {
        typedef typename P Pred;

        explicit UnitPlotIterP(const CvPlot* pPlot_, Pred pred_) : pPlot(pPlot_), pNode(NULL), pred(pred_) {}

        CvUnit* operator() ()
        {
            for (pNode ? pNode = pPlot->nextUnitNode(pNode) : pNode = pPlot->headUnitNode(); pNode; pNode = pPlot->nextUnitNode(pNode))
            {
                if (pNode && pred(pNode))
                {
                    break;
                }
            }
            return pNode ? getUnit(pNode->m_data) : NULL;
        }
        
        const CvPlot* pPlot;
        CLLNode<IDInfo>* pNode;
        Pred pred;
    };

    void debugSelectionGroup(const CvSelectionGroup* pGroup, std::ostream& os);
}