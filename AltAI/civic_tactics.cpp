#include "AltAI.h"

#include "civic_tactics.h"
#include "civic_tactics_items.h"
#include "player_analysis.h"
#include "tictacs.h"
#include "civ_log.h"

namespace AltAI
{
    namespace
    {
        struct CivicSelectionData
        {
            explicit CivicSelectionData(PlayerTactics& playerTactics_) : tacticSelectionDataMap(playerTactics_.getBaseTacticSelectionDataMap())
            {
            }

            TacticSelectionDataMap& tacticSelectionDataMap;
        };
    }

    CivicTypes chooseCivic(PlayerTactics& playerTactics, CivicOptionTypes civicOptionType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream();
#endif

        for (int civicType = 0; civicType < GC.getNumCivicInfos(); ++civicType)
	    {
		    if (GC.getCivicInfo((CivicTypes)civicType).getCivicOptionType() == civicOptionType)
		    {
                if (playerTactics.player.getCvPlayer()->canDoCivics((CivicTypes)civicType))
                {
                    PlayerTactics::CivicTacticsMap::iterator cIter = playerTactics.civicTacticsMap_.find((CivicTypes)civicType);
                    if (cIter != playerTactics.civicTacticsMap_.end())
                    {
                        TacticSelectionData selectionData;
                        cIter->second->apply(selectionData);
#ifdef ALTAI_DEBUG
                        selectionData.debug(os);
#endif
                    }
                }
            }
        }

        return NO_CIVIC;
    }
}