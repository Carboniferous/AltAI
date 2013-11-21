#include "AltAI.h"

#include "./error_log.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        std::string makeFileName(const CvPlayer& player)
        {
            std::ostringstream oss;
            oss << getLogDirectory() << narrow(player.getName()) << "_" << narrow(player.getCivilizationShortDescription()) << player.getID() << "_errors.txt";
            return oss.str();
        }
    }

    struct ErrorLogFileHandles
    {
        typedef std::map<std::string, boost::shared_ptr<ErrorLog> > HandleMap;
        static HandleMap logFiles;

        static boost::shared_ptr<ErrorLog> getHandle(const CvPlayer& player)
        {
            const std::string name(makeFileName(player));

            HandleMap::iterator iter(logFiles.find(name));
            if (iter == logFiles.end())
            {
                return logFiles.insert(std::make_pair(name, boost::shared_ptr<ErrorLog>(new ErrorLog(player)))).first->second;
            }
            else
            {
                return iter->second;
            }
        }
    };

    ErrorLogFileHandles::HandleMap ErrorLogFileHandles::logFiles;
    

    boost::shared_ptr<ErrorLog> ErrorLog::getLog(const CvPlayer& player) 
    {
        return ErrorLogFileHandles::getHandle(player);
    }

    ErrorLog::ErrorLog(const CvPlayer& player) : logFile_(makeFileName(player).c_str())
    {
    }
}
