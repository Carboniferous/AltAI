#include "./spec_info.h"
#include "./helper_fns.h"

namespace AltAI
{
    namespace
    {
        struct SpecInfoRequestData
        {
            SpecInfoRequestData(SpecialistTypes specialistType_ = NO_SPECIALIST, PlayerTypes playerType_ = NO_PLAYER) : specialistType(specialistType_), playerType(playerType_)
            {
            }
            SpecialistTypes specialistType;
            PlayerTypes playerType;
        };

        void getCivicNode(SpecInfo::BaseNode& baseNode, const CvSpecialistInfo& specialistInfo, const SpecInfoRequestData& requestData)
        {
            SpecInfo::CivicNode node;
            for (int i = 0, count = gGlobals.getNumCivicInfos(); i < count; ++i)
            {
                const CvCivicInfo& civicInfo = gGlobals.getCivicInfo((CivicTypes)i);
                Commerce commerce = civicInfo.getSpecialistExtraCommerceArray();
                if (!isEmpty(commerce))
                {
                    node.civicsExtraCommerce.push_back(std::make_pair((CivicTypes)i, commerce));
                }
            }
            baseNode.nodes.push_back(node);
        }

        void getBuildingNode(SpecInfo::BaseNode& baseNode, const CvSpecialistInfo& specialistInfo, const SpecInfoRequestData& requestData)
        {
            SpecInfo::BuildingNode node;
            for (int i = 0, count = gGlobals.getNumBuildingInfos(); i < count; ++i)
            {
                const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo((BuildingTypes)i);
                Commerce commerce = buildingInfo.getSpecialistExtraCommerceArray();
                if (!isEmpty(commerce))
                {
                    node.buildingsExtraCommerce.push_back(std::make_pair((BuildingTypes)i, commerce));
                }

                PlotYield yield = buildingInfo.getSpecialistYieldChangeArray(requestData.specialistType);
                if (!isEmpty(yield))
                {
                    node.buildingExtraYields.push_back(std::make_pair((BuildingTypes)i, yield));
                }
            }
            baseNode.nodes.push_back(node);
        }


        SpecInfo::BaseNode getBaseNode(const SpecInfoRequestData& requestData)
        {
            SpecInfo::BaseNode node;
            const CvSpecialistInfo& specInfo = gGlobals.getSpecialistInfo(requestData.specialistType);

            node.yield = specInfo.getYieldChangeArray();

            for (int i = 0; i < NUM_COMMERCE_TYPES; ++i)  // no array fn - but yields got one - wtf?
            {
                node.commerce[i] = specInfo.getCommerceChange(i);
            }

            getCivicNode(node, specInfo, requestData);
            getBuildingNode(node, specInfo, requestData);

            return node;
        }
    }

    SpecInfo::SpecInfo(SpecialistTypes specialistType, PlayerTypes playerType) : specialistType_(specialistType), playerType_(playerType)
    {
        init_();
    }

    void SpecInfo::init_()
    {
        infoNode_ = getBaseNode(SpecInfoRequestData(specialistType_, playerType_));
    }

    const SpecInfo::SpecInfoNode& SpecInfo::getInfo() const
    {
        return infoNode_;
    }

}