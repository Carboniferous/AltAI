#include "AltAI.h"

#include "./vote_helper.h"

namespace AltAI
{
    VoteHelper::VoteHelper(const CvCity* pCity) : pCity_(pCity)
    {
        const CvPlayer& player = CvPlayerAI::getPlayer(pCity_->getOwner());

        for (int iVoteSource = 0; iVoteSource < gGlobals.getNumVoteSourceInfos(); ++iVoteSource)
        {
            // this vote source is active
            if (gGlobals.getGame().isDiploVote((VoteSourceTypes)iVoteSource))
            {
                if (!player.isLoyalMember((VoteSourceTypes)iVoteSource)) // this returns true even if the vote source is not active, unless we defied something
                {
                    continue;
                }

                const CvVoteSourceInfo& voteSourceInfo = gGlobals.getVoteSourceInfo((VoteSourceTypes)iVoteSource);
        		ReligionTypes religionType = gGlobals.getGame().getVoteSourceReligion((VoteSourceTypes)iVoteSource);

                if (religionType == NO_RELIGION || pCity->isHasReligion(religionType))
                {
                    SpecialistTypes freeSpecType = (SpecialistTypes)voteSourceInfo.getFreeSpecialist();
                    if (freeSpecType != NO_SPECIALIST)
                    {
                        ++voteSourceFreeSpecs_[religionType][freeSpecType];
                    }
                }

                if (religionType != NO_RELIGION && pCity->isHasReligion(religionType))
                {
                    for (int i = 0; i < NUM_YIELD_TYPES; ++i)
                    {
                        religiousBuildingYieldChangesMap_[religionType][i] += voteSourceInfo.getReligionYield(i);
                    }

                    for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)
                    {
                        religiousBuildingCommerceChangesMap_[religionType][i] += voteSourceInfo.getReligionCommerce(i);
                    }
                }
            }
        }
    }

    VoteHelperPtr VoteHelper::clone() const
    {
        VoteHelperPtr copy = VoteHelperPtr(new VoteHelper(*this));
        return copy;
    }

    PlotYield VoteHelper::getReligiousBuildingYieldChange(ReligionTypes religionType) const
    {
        std::map<ReligionTypes, PlotYield>::const_iterator ci = religiousBuildingYieldChangesMap_.find(religionType);
        if (ci != religiousBuildingYieldChangesMap_.end())
        {
            return ci->second;
        }
        return PlotYield();
    }

    Commerce VoteHelper::getReligiousBuildingCommerceChange(ReligionTypes religionType) const
    {
        std::map<ReligionTypes, Commerce>::const_iterator ci = religiousBuildingCommerceChangesMap_.find(religionType);
        if (ci != religiousBuildingCommerceChangesMap_.end())
        {
            return ci->second;
        }
        return Commerce();
    }
}