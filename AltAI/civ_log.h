#pragma once

#include "./utils.h"

namespace AltAI
{
    struct CivLogFileHandles;

    class CivLog : boost::noncopyable
    {
    public:
        friend struct CivLogFileHandles;

        static boost::shared_ptr<CivLog> getLog(const CvPlayer& player);

        std::ofstream& getStream() { return logFile_; }

    private:
        explicit CivLog(const CvPlayer& player);

        std::ofstream logFile_;
    };
}