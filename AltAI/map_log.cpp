#include "AltAI.h"

#include "./map_log.h"
#include "./plot_info.h"
#include "./plot_info_visitors_streams.h"
#include "./map_analysis.h"
#include "./helper_fns.h"
#include <iomanip>

namespace AltAI
{
    namespace
    {
        std::string makeFileName(const CvPlayer& player)
        {
            std::ostringstream oss;
            oss << getLogDirectory() << narrow(player.getName()) << "_" << narrow(player.getCivilizationShortDescription()) << player.getID() << "map.txt";
            return oss.str();
        }
    }

    struct MapLogFileHandles
    {
        typedef std::map<std::string, boost::shared_ptr<MapLog> > HandleMap;
        static HandleMap logFiles;

        static boost::shared_ptr<MapLog> getHandle(const CvPlayer& player)
        {
            const std::string name(makeFileName(player));

            HandleMap::iterator iter(logFiles.find(name));
            if (iter == logFiles.end())
            {
                return logFiles.insert(std::make_pair(name, boost::shared_ptr<MapLog>(new MapLog(player)))).first->second;
            }
            else
            {
                return iter->second;
            }
        }
    };

    MapLogFileHandles::HandleMap MapLogFileHandles::logFiles;
    

    boost::shared_ptr<MapLog> MapLog::getLog(const CvPlayer& player) 
    {
        return MapLogFileHandles::getHandle(player);
    }

    MapLog::MapLog(const CvPlayer& player) : logFile_(makeFileName(player).c_str()), player_(player)
    {
    }

    void MapLog::init()
    {
        const CvMap& theMap = gGlobals.getMap();
        const int xWidth = theMap.getGridWidth();
        const int yWidth = theMap.getGridHeight();

        plots_.resize(xWidth);
        for (int x = 0; x < xWidth; ++x)
        {
            plots_[x].resize(yWidth);
        }

        PlayerTypes playerType = player_.getID();
        TeamTypes teamType = player_.getTeam();
        for (int y = yWidth - 1; y > 0; --y)
        {
            for (int x = 0; x < xWidth; ++x)
            {
		        const CvPlot* pPlot = theMap.plot(x, y);
                if (pPlot->isRevealed(teamType, false))
                {
                    if (pPlot->isCity())
                    {
                        const CvCity* pCity = pPlot->getPlotCity();
                        if (pCity->getOwner() == playerType)
                        {
                            plots_[x][y] = "XXX";
                        }
                        else
                        {
                            plots_[x][y] = "xxx";
                        }
                    }
                    else
                    {
                        PlotInfo plotInfo(pPlot, playerType);
                        plots_[x][y] = getPlotDebugString(plotInfo.getInfo(), playerType);
                    }
                }
                else
                {
                    plots_[x][y] = "   ";
                }
            }
        }
    }

    void MapLog::logMap(const std::map<int, std::vector<std::pair<XYCoords, int> >, std::greater<int> >& bestSites, int count)
    {
        std::map<XYCoords, int> coordMap;

        int counter = 0;
        for (std::map<int, std::vector<std::pair<XYCoords, int> >, std::greater<int> >::const_iterator ci(bestSites.begin()), ciEnd(bestSites.end()); ci != ciEnd; ++ci)
        {
            for (size_t i = 0, plotCount = ci->second.size(); i < plotCount; ++i)
            {
                if (count < 0 || counter++ < count)
                {
                    coordMap.insert(std::make_pair(ci->second[i].first, ci->first));
                }
            }
        }
        logMap(coordMap, count);
    }

    void MapLog::logMap(const std::map<XYCoords, int>& coordMap, int count)
    {
        const CvMap& theMap = gGlobals.getMap();
        const int xWidth = theMap.getGridWidth();
        const int yHeight = theMap.getGridHeight();

        logFile_ << "\nTurn = " << gGlobals.getGame().getGameTurn() << "\n";

        for (int y = yHeight - 1; y >= 0; --y)
        {
            for (int x = 0; x < xWidth; ++x)
            {
                logFile_ << "|";

                std::map<XYCoords, int>::const_iterator siteIter = coordMap.find(XYCoords(x, y));
                if (siteIter != coordMap.end())
                {
                    logFile_ << std::setw(3) << siteIter->second;
                }
                else
                {
                    logFile_ << plots_[x][y];
                }
            }
            logFile_ << "|\n";
        }
    }
}
