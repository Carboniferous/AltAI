#include "AltAI.h"

#include "./unit_log.h"
#include "./helper_fns.h"
#include "./iters.h"

namespace AltAI
{
    namespace
    {
        std::string makeFileName(const CvPlayer& player)
        {
            std::ostringstream oss;
            oss << getLogDirectory() << narrow(player.getName()) << "_" << narrow(player.getCivilizationShortDescription()) << player.getID() << "_units.txt";
            return oss.str();
        }
    }

    struct UnitLogFileHandles
    {
        typedef std::map<std::string, boost::shared_ptr<UnitLog> > HandleMap;
        static HandleMap logFiles;

        static boost::shared_ptr<UnitLog> getHandle(const CvPlayer& player)
        {
            const std::string name(makeFileName(player));

            HandleMap::iterator iter(logFiles.find(name));
            if (iter == logFiles.end())
            {
                return logFiles.insert(std::make_pair(name, boost::shared_ptr<UnitLog>(new UnitLog(player)))).first->second;
            }
            else
            {
                return iter->second;
            }
        }
    };

    UnitLogFileHandles::HandleMap UnitLogFileHandles::logFiles;
    

    boost::shared_ptr<UnitLog> UnitLog::getLog(const CvPlayer& player) 
    {
        return UnitLogFileHandles::getHandle(player);
    }

    UnitLog::UnitLog(const CvPlayer& player) : playerType_(player.getID()), logFile_(makeFileName(player).c_str())
    {
        logFile_ << (player.isUsingAltAI() ? "Using AltAI = true\n" : "Using AltAI = false\n");
    }

    void UnitLog::logSelectionGroups()
    {
        SelectionGroupIter iter(CvPlayerAI::getPlayer(playerType_));
        CvSelectionGroup* pGroup = NULL;

        while (pGroup = iter())
        {
            logSelectionGroup(pGroup);
        }
    }

    void UnitLog::logSelectionGroup(CvSelectionGroup* pGroup)
    {
        CvWString AIMissionString;
        getMissionAIString(AIMissionString, pGroup->AI_getMissionAIType());

        logFile_ << "\nSelection group: " << pGroup->getID() << " mission = " << narrow(AIMissionString) << " contains: ";

        UnitGroupIter iter(pGroup);
        const CvUnit* pUnit = NULL;
        while (pUnit = iter())
        {
            CvWString AITypeString;
            getUnitAIString(AITypeString, pUnit->AI_getUnitAIType());
            logFile_ << pUnit->getUnitInfo().getType() << ", ID = " << pUnit->getID() << ", AI = " << narrow(AITypeString) << ", ";
        }
    }
}
