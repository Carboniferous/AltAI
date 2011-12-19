#pragma once

#include "./utils.h"

namespace AltAI
{
    // visitors include Output calculator (for simulations)
    class SpecInfo
    {
    public:
        SpecInfo(SpecialistTypes specialistType, PlayerTypes playerType);

        SpecialistTypes getSpecType() const
        {
            return specialistType_;
        }

        struct NullNode
        {
            NullNode() {}
        };

        struct BaseNode;

        struct UnitNode
        {
        };

        struct CivicNode
        {
            std::vector<std::pair<CivicTypes, Commerce> > civicsExtraCommerce;
        };

        struct BuildingNode
        {
            std::vector<std::pair<BuildingTypes, Commerce> > buildingsExtraCommerce;
            std::vector<std::pair<BuildingTypes, PlotYield> > buildingExtraYields;
        };

        struct MiscEffectNode
        {
        };

        typedef boost::variant<NullNode, boost::recursive_wrapper<BaseNode>, UnitNode, CivicNode, BuildingNode, MiscEffectNode> SpecInfoNode;

        struct BaseNode
        {
            BaseNode()
            {
            }
            std::vector<SpecInfoNode> nodes;

            PlotYield yield;
            Commerce commerce;
        };

        const SpecInfoNode& getInfo() const;

    private:
        void init_();

        SpecialistTypes specialistType_;
        PlayerTypes playerType_;
        SpecInfoNode infoNode_;
    };
}