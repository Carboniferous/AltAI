#pragma once

#include "./utils.h"

namespace AltAI
{
    struct UnitLogFileHandles;

    class UnitLog : boost::noncopyable
    {
    public:
        friend struct UnitLogFileHandles;

        static boost::shared_ptr<UnitLog> getLog(const CvPlayer& player);

        std::ofstream& getStream() { return logFile_; }

        void logSelectionGroups();
        void logSelectionGroup(CvSelectionGroup* pGroup);

    private:
        explicit UnitLog(const CvPlayer& player);

        PlayerTypes playerType_;
        std::ofstream logFile_;
    };
}