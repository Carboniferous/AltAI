#pragma once

#include "./utils.h"

namespace AltAI
{
    struct CivLogFileHandles;

    class ErrorLog : boost::noncopyable
    {
    public:
        friend struct ErrorLogFileHandles;

        static boost::shared_ptr<ErrorLog> getLog(const CvPlayer& player);

        std::ofstream& getStream() { return logFile_; }

    private:
        explicit ErrorLog(const CvPlayer& player);

        std::ofstream logFile_;
    };
}