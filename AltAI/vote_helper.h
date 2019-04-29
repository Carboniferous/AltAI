#pragma once

#include "./utils.h"

namespace AltAI
{
    class VoteHelper;
    typedef boost::shared_ptr<VoteHelper> VoteHelperPtr;

    class VoteHelper
    {
    public:
        explicit VoteHelper(const CvCity* pCity);
        VoteHelperPtr clone() const;

        PlotYield getReligiousBuildingYieldChange(ReligionTypes religionType) const;
        Commerce getReligiousBuildingCommerceChange(ReligionTypes religionType) const;

    private:
        const CvCity* pCity_;
        // bit overcomplicated - but supports the most general case of free specs from multiple vote sources which have the same religion (incl. none)
        std::map<ReligionTypes, std::map<SpecialistTypes, size_t > > voteSourceFreeSpecs_;
        std::map<ReligionTypes, PlotYield> religiousBuildingYieldChangesMap_;
        std::map<ReligionTypes, Commerce> religiousBuildingCommerceChangesMap_;
    };
}