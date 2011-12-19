#include "./civ_log.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        std::string makeFileName(const CvPlayer& player)
        {
            std::ostringstream oss;
            oss << getLogDirectory() << narrow(player.getName()) << "_" << narrow(player.getCivilizationShortDescription()) << player.getID() << ".txt";
            return oss.str();
        }
    }

    struct CivLogFileHandles
    {
        typedef std::map<std::string, boost::shared_ptr<CivLog> > HandleMap;
        static HandleMap logFiles;

        static boost::shared_ptr<CivLog> getHandle(const CvPlayer& player)
        {
            const std::string name(makeFileName(player));

            HandleMap::iterator iter(logFiles.find(name));
            if (iter == logFiles.end())
            {
                return logFiles.insert(std::make_pair(name, boost::shared_ptr<CivLog>(new CivLog(player)))).first->second;
            }
            else
            {
                return iter->second;
            }
        }
    };

    CivLogFileHandles::HandleMap CivLogFileHandles::logFiles;
    

    boost::shared_ptr<CivLog> CivLog::getLog(const CvPlayer& player) 
    {
        return CivLogFileHandles::getHandle(player);
    }

    CivLog::CivLog(const CvPlayer& player) : logFile_(makeFileName(player).c_str())
    {
        logFile_ << (player.isUsingAltAI() ? "Using AltAI = true\n" : "Using AltAI = false\n");
    }
}
