#pragma once

#include "CvGameCoreDLL.h"
#include "CvStructs.h"

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
}