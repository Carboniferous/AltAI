#pragma once 

#include "./utils.h"

namespace AltAI
{
    // stores attitude data for fromPlayerType_ towards player in owning OpponentsAnalysis object
    class AttitudeHelper
    {
    public:
        struct AttitudeData
        {
            AttitudeData() 
            {
                attitude.assign(0);
            }

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

            int getAttitude() const;
        };

        struct AttitudeChange
        {
            AttitudeChange() : turn(-1) {}
            AttitudeChange(int turn_, AttitudeData::Attitude delta_) : turn(turn_), delta(delta_) {}

            int turn;
            AttitudeData::Attitude delta;

            void debug(std::ostream& os) const;
        };

        AttitudeHelper(const CvPlayer* pPlayer, PlayerTypes toPlayerType, bool areAttitudesToUs);

        void updateAttitude();
        const AttitudeData& getCurrentAttitude() const { return currentAttitude_; }
        bool areAttitudesToUs() const { return areAttitudesToUs_; }
        void debug(std::ostream& os) const;

    private:
        void calcAttitude_();

        std::pair<bool, AttitudeData::Attitude> calcAttitudeChange_(const AttitudeData& previousAttitude) const;

        bool areAttitudesToUs_;
        PlayerTypes fromPlayerType_, toPlayerType_;
        AttitudeData currentAttitude_;
        std::vector<AttitudeChange> attitudeHistory_;
    };
}