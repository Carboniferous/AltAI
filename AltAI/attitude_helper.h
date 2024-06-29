#pragma once 

#include "./utils.h"

namespace AltAI
{
    // stores attitude data for playerType_ towards player in owning OpponentsAnalysis object
    class AttitudeHelper
    {
    public:
        struct AttitudeData
        {
            // todo - defensive pacts and various other diplo stuff
            enum AttitudeT
            {
                None = -1, Base = 0, CloseBorders, War, Peace, SameReligion, DifferentReligion, 
                ResourceTrade, OpenBorders, FavouredCivic, Memory, AttitudeTypeCount
            };

            typedef boost::array<short, AttitudeTypeCount> Attitude;
            Attitude attitude;

            typedef boost::array<std::string, AttitudeTypeCount> AttitudeStrings;
            static AttitudeStrings attitudeStrings;
        };

        struct AttitudeChange
        {
            AttitudeChange() : turn(-1) {}
            AttitudeChange(int turn_, AttitudeData::Attitude delta_) : turn(turn_), delta(delta_) {}

            int turn;
            AttitudeData::Attitude delta;

            void debug(std::ostream& os) const;
        };

        explicit AttitudeHelper(const CvPlayer* pPlayer);

        void updateAttitudeTowards(const CvPlayerAI* pToPlayer);

    private:
        void calcAttitudeTowards_(const CvPlayerAI* pToPlayer);
        std::pair<bool, AttitudeData::Attitude> calcAttitudeChange_(const AttitudeData& previousAttitude) const;

        PlayerTypes playerType_;
        AttitudeData currentAttitude_;
        std::vector<AttitudeChange> attitudeHistory_;
    };
}