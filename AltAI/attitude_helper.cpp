#include "AltAI.h"

#include "./attitude_helper.h"
#include "./civ_log.h"

namespace AltAI
{
    AttitudeHelper::AttitudeData::AttitudeStrings attitudeStrings(boost::assign::list_of
        ("Base")("CloseBorders")("War")("Peace")("SameReligion")("DifferentReligion")
        ("ResourceTrade")("OpenBorders")("FavouredCivic")("Memory"));

    AttitudeHelper::AttitudeHelper(const CvPlayer* pPlayer) : playerType_(pPlayer->getID())
    {
        calcAttitudeTowards_((CvPlayerAI*)pPlayer);
    }

    void AttitudeHelper::AttitudeChange::debug(std::ostream& os) const
    {
        bool first = true;
        for (size_t i = 0; i < AttitudeData::AttitudeTypeCount; ++i)
        {
            if (delta[i] != 0)
            {
                if (!first) os << ", ";
                else first = false;
                os << attitudeStrings[i] << " = " << delta[i];
            }
        }
    }

    // updates the attitude data of the player with ID of playerType_ towards pToPlayer
    // i.e. what you as pToPlayer would see querying playerType_'s opinion of you.
    // e.g. 'We are angry you have fallen under the sway of a heathen religion..."
    void AttitudeHelper::updateAttitudeTowards(const CvPlayerAI* pToPlayer)
    {
        AttitudeData previousAttitude = currentAttitude_;
        calcAttitudeTowards_(pToPlayer);
        bool hasChanged = false;
        AttitudeData::Attitude delta;
        boost::tie(hasChanged, delta) = calcAttitudeChange_(previousAttitude);

        if (hasChanged)
        {
            AttitudeChange attitudeChange(gGlobals.getGame().getGameTurn(), delta);
            attitudeHistory_.push_back(attitudeChange);
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(*pToPlayer)->getStream();
            os << "\nAttitude change towards: " << pToPlayer->getID() << " from: " << playerType_ << " ";
            attitudeChange.debug(os);
#endif
        }
    }

    void AttitudeHelper::calcAttitudeTowards_(const CvPlayerAI* pToPlayer)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*pToPlayer)->getStream();
        os << "\nattitude calc towards: " << pToPlayer->getID() << " for player: " << playerType_ << ": ";
#endif
        CvGameAI& refGame = gGlobals.getGame();
        const int everAliveCount = refGame.countCivPlayersEverAlive();        

        PlayerTypes fromPlayer = playerType_, toPlayer = pToPlayer->getID();        
        CvPlayerAI* pFromPlayer = &CvPlayerAI::getPlayer(playerType_);
        TeamTypes fromTeam = pFromPlayer->getTeam(), toTeam = pToPlayer->getTeam();
        CvTeamAI& pRefFromTeam = CvTeamAI::getTeam(fromTeam), &pRefToTeam = CvTeamAI::getTeam(toTeam);

        const CvLeaderHeadInfo& fromLeaderInfo = gGlobals.getLeaderHeadInfo(pFromPlayer->getPersonalityType());
        const CvLeaderHeadInfo& toLeaderInfo = gGlobals.getLeaderHeadInfo(pToPlayer->getPersonalityType());

        int iAttitude = fromLeaderInfo.getBaseAttitude();
#ifdef ALTAI_DEBUG
        os << " base attitude: " << iAttitude;
#endif
        iAttitude += gGlobals.getHandicapInfo(pToPlayer->getHandicapType()).getAttitudeChange();
#ifdef ALTAI_DEBUG
        os << " level adj: " << gGlobals.getHandicapInfo(pToPlayer->getHandicapType()).getAttitudeChange();
#endif

        // calc peace-weights if to player is not human
        if (!pToPlayer->isHuman())
	    {
		    iAttitude += (4 - ::abs(pFromPlayer->AI_getPeaceWeight() - pToPlayer->AI_getPeaceWeight()));
		    iAttitude += std::min<int>(fromLeaderInfo.getWarmongerRespect(), toLeaderInfo.getWarmongerRespect());
#ifdef ALTAI_DEBUG
            os << " our pws: " << pToPlayer->AI_getPeaceWeight() << " b: " << toLeaderInfo.getBasePeaceWeight();
            os << " their pws: " << pFromPlayer->AI_getPeaceWeight() << " b: " << fromLeaderInfo.getBasePeaceWeight();
            os << " pw1: " << 4 - ::abs(pFromPlayer->AI_getPeaceWeight() - pToPlayer->AI_getPeaceWeight());
            os << " pw2: " << std::min<int>(fromLeaderInfo.getWarmongerRespect(), toLeaderInfo.getWarmongerRespect());
#endif
	    }

        // adjust by team sizes (if we have a team with more players, our attitude decreases and vica-versa)
        iAttitude -= std::max<int>(0, pRefToTeam.getNumMembers() - pRefFromTeam.getNumMembers());

        // score positions
        // rank of zero is highest score: calc our rank - theirs
        int iRankDifference = refGame.getPlayerRank(fromPlayer) - refGame.getPlayerRank(toPlayer);
        if (iRankDifference > 0)
	    {
            // +ve diff: our position is worse than 'to' player's - iWorseRankDifferenceAttitudeChange is -ve (except for Wilhelm)
            // generally will make our attitude worse towards the 'to' player
		    iAttitude += (fromLeaderInfo.getWorseRankDifferenceAttitudeChange() * iRankDifference) / (everAliveCount + 1);
#ifdef ALTAI_DEBUG
            os << " rank +ve: " << (fromLeaderInfo.getWorseRankDifferenceAttitudeChange() * iRankDifference) / (everAliveCount + 1);
#endif
	    }
	    else
	    {
            // -ve diff: our position is better than 'to' player's  - iBetterRankDifferenceAttitudeChange is +ve (except for Wilhelm)
            // generally will improve our attitude towards the 'to' player
		    iAttitude += (toLeaderInfo.getBetterRankDifferenceAttitudeChange() * -(iRankDifference)) / (everAliveCount + 1);
#ifdef ALTAI_DEBUG
            os << " rank -ve: " << (toLeaderInfo.getBetterRankDifferenceAttitudeChange() * -(iRankDifference)) / (everAliveCount + 1);
#endif
	    }

        // add one if both in bottom half of score table
        if (refGame.getPlayerRank(fromPlayer) >= everAliveCount / 2 && refGame.getPlayerRank(toPlayer) >= everAliveCount / 2)
	    {
		    iAttitude++;
#ifdef ALTAI_DEBUG
            os << " +1 for low ranks";
#endif
	    }

        // war success
        if (pRefToTeam.AI_getWarSuccess(fromTeam) > pRefFromTeam.AI_getWarSuccess(toTeam))
	    {
		    iAttitude += fromLeaderInfo.getLostWarAttitudeChange();  // always -1
#ifdef ALTAI_DEBUG
            os << " lost war: " << fromLeaderInfo.getLostWarAttitudeChange();
#endif
	    }
#ifdef ALTAI_DEBUG
        os << " final base val: " << iAttitude;
#endif
        currentAttitude_.attitude[AttitudeData::Base] = iAttitude;

        currentAttitude_.attitude[AttitudeData::CloseBorders] = pFromPlayer->AI_getCloseBordersAttitude(toPlayer);
        currentAttitude_.attitude[AttitudeData::War] = pFromPlayer->AI_getWarAttitude(toPlayer);
        currentAttitude_.attitude[AttitudeData::Peace] = pFromPlayer->AI_getPeaceAttitude(toPlayer);
        currentAttitude_.attitude[AttitudeData::SameReligion] = pFromPlayer->AI_getSameReligionAttitude(toPlayer);
        currentAttitude_.attitude[AttitudeData::DifferentReligion] = pFromPlayer->AI_getDifferentReligionAttitude(toPlayer);
        currentAttitude_.attitude[AttitudeData::ResourceTrade] = pFromPlayer->AI_getBonusTradeAttitude(toPlayer);
        currentAttitude_.attitude[AttitudeData::OpenBorders] = pFromPlayer->AI_getOpenBordersAttitude(toPlayer);
    }

    std::pair<bool, AttitudeHelper::AttitudeData::Attitude> AttitudeHelper::calcAttitudeChange_(const AttitudeData& previousAttitude) const
    {
        AttitudeHelper::AttitudeData::Attitude delta;
        bool hasChanged = false;

        for (size_t i = 0; i < AttitudeData::AttitudeTypeCount; ++i)
        {
            delta[i] = currentAttitude_.attitude[i] - previousAttitude.attitude[i];
            if (!hasChanged && delta[i] != 0)
            {
                hasChanged = true;
            }
        }

        return std::make_pair(hasChanged, delta);
    }
}