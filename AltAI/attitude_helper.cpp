#include "AltAI.h"

#include "./attitude_helper.h"
#include "./civ_log.h"
#include "./helper_fns.h"

#include <numeric>

namespace AltAI
{
    AttitudeHelper::AttitudeData::AttitudeStrings AttitudeHelper::AttitudeData::attitudeStrings = boost::assign::list_of
        ("Base")("CloseBorders")("War")("Peace")("SameReligion")("DifferentReligion")
        ("ResourceTrade")("OpenBorders")("FavouredCivic")("Memory");

    int AttitudeHelper::AttitudeData::getAttitude() const
    {
        int total = 0;
        return std::accumulate(attitude.begin(), attitude.end(), total);
        return total;
    }

    AttitudeHelper::AttitudeHelper(const CvPlayer* pPlayer, PlayerTypes toPlayerType, bool areAttitudesToUs)
      : areAttitudesToUs_(areAttitudesToUs), fromPlayerType_(pPlayer->getID()), toPlayerType_(toPlayerType)
    {
        calcAttitude_();
    }

    void AttitudeHelper::AttitudeChange::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        bool first = true;
        for (size_t i = 0; i < AttitudeData::AttitudeTypeCount; ++i)
        {
            if (delta[i] != 0)
            {
                if (!first) os << ", ";
                else first = false;
                os << AttitudeHelper::AttitudeData::attitudeStrings[i] << " = " << delta[i];
            }
        }
#endif
    }

    // updates the attitude data of the player with ID of fromPlayerType_ towards pToPlayer
    // i.e. what you as pToPlayer would see querying fromPlayerType_'s opinion of you.
    // e.g. 'We are angry you have fallen under the sway of a heathen religion..."
    void AttitudeHelper::updateAttitude()
    {
        AttitudeData previousAttitude = currentAttitude_;
        calcAttitude_();
        bool hasChanged = false;
        AttitudeData::Attitude delta;
        boost::tie(hasChanged, delta) = calcAttitudeChange_(previousAttitude);

        if (hasChanged)
        {
            AttitudeChange attitudeChange(gGlobals.getGame().getGameTurn(), delta);
            attitudeHistory_.push_back(attitudeChange);
#ifdef ALTAI_DEBUG
            std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(areAttitudesToUs_ ? toPlayerType_: fromPlayerType_))->getStream();
            os << "\nAttitude change towards player: " << toPlayerType_ << " from: " << fromPlayerType_ << " (" << safeGetPlayerName(fromPlayerType_) << ") ";
            attitudeChange.debug(os);
#endif
        }
    }

    void AttitudeHelper::calcAttitude_()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(areAttitudesToUs_ ? toPlayerType_: fromPlayerType_))->getStream();
        os << "\nattitude calc towards: " << toPlayerType_ << " (" << safeGetPlayerName(toPlayerType_) << ") for player: " << fromPlayerType_ << " (" << safeGetPlayerName(fromPlayerType_) << "): ";
#endif
        CvGameAI& refGame = gGlobals.getGame();
        const int everAliveCount = refGame.countCivPlayersEverAlive();        

        const CvPlayerAI* pFromPlayer = &CvPlayerAI::getPlayer(fromPlayerType_), *pToPlayer = &CvPlayerAI::getPlayer(toPlayerType_);
        TeamTypes fromTeam = pFromPlayer->getTeam(), toTeam = pToPlayer->getTeam();
        const CvTeamAI& pRefFromTeam = CvTeamAI::getTeam(fromTeam), &pRefToTeam = CvTeamAI::getTeam(toTeam);

        const CvLeaderHeadInfo& fromLeaderInfo = gGlobals.getLeaderHeadInfo(pFromPlayer->getPersonalityType());
        const CvLeaderHeadInfo& toLeaderInfo = gGlobals.getLeaderHeadInfo(pToPlayer->getPersonalityType());

        int iAttitude = fromLeaderInfo.getBaseAttitude();
#ifdef ALTAI_DEBUG
        os << " base attitude: " << iAttitude;
#endif
        iAttitude += gGlobals.getHandicapInfo(pToPlayer->getHandicapType()).getAttitudeChange();
#ifdef ALTAI_DEBUG
        os << ", level adj: " << gGlobals.getHandicapInfo(pToPlayer->getHandicapType()).getAttitudeChange();
#endif

        // calc peace-weights if to player is not human
        if (!pToPlayer->isHuman())
	    {
		    iAttitude += (4 - ::abs(pFromPlayer->AI_getPeaceWeight() - pToPlayer->AI_getPeaceWeight()));
		    iAttitude += std::min<int>(fromLeaderInfo.getWarmongerRespect(), toLeaderInfo.getWarmongerRespect());
#ifdef ALTAI_DEBUG
            os << ", to pws: " << pToPlayer->AI_getPeaceWeight() << " b: " << toLeaderInfo.getBasePeaceWeight();
            os << ", from pws: " << pFromPlayer->AI_getPeaceWeight() << " b: " << fromLeaderInfo.getBasePeaceWeight();
            os << ", from-to pw_diff: " << 4 - ::abs(pFromPlayer->AI_getPeaceWeight() - pToPlayer->AI_getPeaceWeight());
            os << ", wmr: " << std::min<int>(fromLeaderInfo.getWarmongerRespect(), toLeaderInfo.getWarmongerRespect());
#endif
	    }

        // adjust by team sizes (if we have a team with more players, our attitude decreases and vica-versa)
        iAttitude -= std::max<int>(0, pRefToTeam.getNumMembers() - pRefFromTeam.getNumMembers());

        // score positions
        // rank of zero is highest score: calc 'from' ranking - 'to' ranking
        // the lower a player's ranking, the larger the value of getPlayerRank - e.g. = 6 for bottom in seven player game
        int iRankDifference = refGame.getPlayerRank(fromPlayerType_) - refGame.getPlayerRank(toPlayerType_);
        if (iRankDifference > 0)
	    {
            // +ve diff: 'from' position is worse than 'to' player's: iWorseRankDifferenceAttitudeChange is -ve (except for Wilhelm)
            // generally will make from player's attitude worse towards the 'to' player
		    iAttitude += (fromLeaderInfo.getWorseRankDifferenceAttitudeChange() * iRankDifference) / (everAliveCount + 1);
#ifdef ALTAI_DEBUG
            os << ", rank +ve: " << (fromLeaderInfo.getWorseRankDifferenceAttitudeChange() * iRankDifference) / (everAliveCount + 1);
#endif
	    }
	    else
	    {
            // equal or -ve diff: 'from' player's position is better than 'to' player's  - iBetterRankDifferenceAttitudeChange is +ve (except for Wilhelm)
            // generally will improve from player's attitude towards the 'to' player
		    iAttitude += (toLeaderInfo.getBetterRankDifferenceAttitudeChange() * -(iRankDifference)) / (everAliveCount + 1);
#ifdef ALTAI_DEBUG
            os << ", rank -ve: " << (toLeaderInfo.getBetterRankDifferenceAttitudeChange() * -(iRankDifference)) / (everAliveCount + 1);
#endif
	    }

        // add one if both in bottom half of score table
        if (refGame.getPlayerRank(fromPlayerType_) >= everAliveCount / 2 && refGame.getPlayerRank(toPlayerType_) >= everAliveCount / 2)
	    {
		    iAttitude++;
#ifdef ALTAI_DEBUG
            os << ", +1 for low ranks";
#endif
	    }

        // war success
        if (pRefToTeam.AI_getWarSuccess(fromTeam) > pRefFromTeam.AI_getWarSuccess(toTeam))
	    {
		    iAttitude += fromLeaderInfo.getLostWarAttitudeChange();  // always -1
#ifdef ALTAI_DEBUG
            os << ", lost war: " << fromLeaderInfo.getLostWarAttitudeChange();
#endif
	    }
        currentAttitude_.attitude[AttitudeData::Base] = iAttitude;

        currentAttitude_.attitude[AttitudeData::CloseBorders] = pFromPlayer->AI_getCloseBordersAttitude(toPlayerType_);
        currentAttitude_.attitude[AttitudeData::War] = pFromPlayer->AI_getWarAttitude(toPlayerType_);
        currentAttitude_.attitude[AttitudeData::Peace] = pFromPlayer->AI_getPeaceAttitude(toPlayerType_);
        currentAttitude_.attitude[AttitudeData::SameReligion] = pFromPlayer->AI_getSameReligionAttitude(toPlayerType_);
        currentAttitude_.attitude[AttitudeData::DifferentReligion] = pFromPlayer->AI_getDifferentReligionAttitude(toPlayerType_);
        currentAttitude_.attitude[AttitudeData::ResourceTrade] = pFromPlayer->AI_getBonusTradeAttitude(toPlayerType_);
        currentAttitude_.attitude[AttitudeData::OpenBorders] = pFromPlayer->AI_getOpenBordersAttitude(toPlayerType_);
        currentAttitude_.attitude[AttitudeData::FavouredCivic] = pFromPlayer->AI_getFavoriteCivicAttitude(toPlayerType_);
        currentAttitude_.attitude[AttitudeData::Memory] = 0;
        for (int memoryType = 0; memoryType < NUM_MEMORY_TYPES; ++memoryType)
	    {
            currentAttitude_.attitude[AttitudeData::Memory] += pFromPlayer->AI_getMemoryAttitude(toPlayerType_, (MemoryTypes)memoryType);
        }

#ifdef ALTAI_DEBUG
        os << " => final base val: " << iAttitude << "\n\t";
        debug(os);
        os << ", fn val: " << pFromPlayer->AI_getAttitudeVal(toPlayerType_);
#endif
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

    void AttitudeHelper::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        bool first = true;
        for (size_t i = 0; i < AttitudeData::AttitudeTypeCount; ++i)
        {
            if (currentAttitude_.attitude[i] != 0)
            {
                if (!first) os << ", ";
                else first = false;
                os << AttitudeHelper::AttitudeData::attitudeStrings[i] << " = " << currentAttitude_.attitude[i];
            }
        }
#endif
    }
}