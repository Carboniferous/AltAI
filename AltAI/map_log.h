#pragma once

#include "./utils.h"

namespace AltAI
{
    struct MapLogFileHandles;
    class MapAnalysis;

    class MapLog : boost::noncopyable
    {
    public:
        friend struct MapLogFileHandles;

        void init();

        static boost::shared_ptr<MapLog> getLog(const CvPlayer& player);

        void logMap(const std::map<int, std::vector<std::pair<XYCoords, int> >, std::greater<int> >& bestSites, int count = -1);
        void MapLog::logMap(const std::map<XYCoords, int>& coordMap, int count = -1);

        std::ofstream& getStream() { return logFile_; }

    private:
        explicit MapLog(const CvPlayer& player);

        std::ofstream logFile_;
        const CvPlayer& player_;
        std::vector<std::vector<std::string> > plots_;
    };
}