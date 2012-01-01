#pragma once

#include "./utils.h"

class CvPlot;

namespace AltAI
{
    typedef std::vector<std::pair<XYCoords, std::vector<std::pair<FeatureTypes, ImprovementTypes> > > > PlotsAndImprovements;

    // TODO - add route yield data in
    class PlotInfo
    {
    public:
        //struct PlotYieldInfo
        //{
        //    PlotYieldInfo();
        //    PlotYield baseYield, terrainModifiers, resourceModifiers, eventModifiers, citySquareModifiers, goldenAgeModifiers;
        //    std::map<int, PlotYield> improvementModifiers, resourceImprovementModifiers, traitModifiers, buildingModifiers;  // can trait modifiers be improvement specific?
        //    std::map<int, std::map<int, PlotYield> > civicModifiers, techModifiers;  // civic and tech modifiers are improvement specific
        //    int bonusType;
        //    void debug(std::ostream& os, XYCoords coords, bool includeImprovements = true) const;
        //};
        //static PlotYieldInfo getInfo(XYCoords coords, PlayerTypes playerType);

        PlotInfo(const CvPlot* pPlot, PlayerTypes playerType);

        int getKey() const;
        int getKeyWithFeature(FeatureTypes featureType) const;

        struct NullNode
        {
            NullNode() {}
            bool operator == (const NullNode&) const
            {
                return true;
            }
        };

        struct HasTech
        {
            explicit HasTech(TechTypes techType_) : techType(techType_) {}
            TechTypes techType;

            bool operator == (const HasTech& other) const
            {
                return techType == other.techType;
            }
        };

        // farms: HasTech(Agriculture) and IsFreshWater or HasTech(CivilService) and hasFreshWaterAccess or HasTech(Biology)
        // since we know if we have freshwater access (both immediate and chained) this is not stored here, but is taken into account
        // in building up the tech conditions tree
        // all static conditions are pre-computed - i.e. rules like being riverside (for watermills) which don't change with time.
        // todo - may wish to expand this to better handle watermills (since they also depend on the location of other 'mills)
        struct BuildOrCondition;

        typedef boost::variant<NullNode, HasTech, boost::recursive_wrapper<BuildOrCondition> > BuildCondition;

        struct BuildOrCondition
        {
            std::vector<BuildCondition> conditions;

            bool operator == (const BuildOrCondition& other) const;
        };

        struct BaseNode;
        struct UpgradeNode;
        struct ImprovementNode;
        
        struct FeatureRemovedNode
        {
            FeatureRemovedNode() {}
            PlotYield yield, bonusYield;
            std::vector<ImprovementNode> improvementNodes;

            bool operator == (const FeatureRemovedNode& other) const;
        };

        struct UpgradeNode
        {
            UpgradeNode() : improvementType(NO_IMPROVEMENT) {}
            PlotYield yield, bonusYield;
            ImprovementTypes improvementType;
            std::vector<BuildCondition> buildConditions;
            std::vector<std::pair<TechTypes, PlotYield> > techYields;
            std::vector<std::pair<CivicTypes, PlotYield> > civicYields;
            std::vector<std::pair<TechTypes, std::pair<RouteTypes, PlotYield> > > routeYields;
            std::vector<UpgradeNode> upgradeNode;  // empty if no further upgrade

            bool operator == (const UpgradeNode& other) const;
        };

        typedef boost::variant<NullNode,
            boost::recursive_wrapper<BaseNode>,
            UpgradeNode,
            FeatureRemovedNode,
            boost::recursive_wrapper<ImprovementNode> > PlotInfoNode;

        struct BaseNode
        {
            BaseNode() : isImpassable(false), isFreshWater(false), hasPotentialFreshWaterAccess(false),
                         tech(NO_TECH), featureRemoveTech(NO_TECH), plotType(NO_PLOT),
                         terrainType(NO_TERRAIN), bonusType(NO_BONUS), featureType(NO_FEATURE)
            {
            }

            bool isImpassable;
            bool isFreshWater, hasPotentialFreshWaterAccess;
            PlotYield yield, bonusYield;
            TechTypes tech, featureRemoveTech;
            PlotTypes plotType;
            TerrainTypes terrainType;
            BonusTypes bonusType;
            FeatureTypes featureType;
            PlotInfoNode featureRemovedNode;  // NullNode or FeatureRemovedNode
            std::vector<ImprovementNode> improvementNodes;

            bool operator == (const BaseNode& other) const;
        };

        struct ImprovementNode
        {
            ImprovementNode() : improvementType(NO_IMPROVEMENT) {}
            PlotYield yield, bonusYield;
            ImprovementTypes improvementType;
            std::vector<BuildCondition> buildConditions;
            std::vector<std::pair<TechTypes, PlotYield> > techYields;
            std::vector<std::pair<CivicTypes, PlotYield> > civicYields;
            std::vector<std::pair<TechTypes, std::pair<RouteTypes, PlotYield> > > routeYields;
            std::vector<UpgradeNode> upgradeNode;  // empty if no upgrade

            bool operator == (const ImprovementNode& other) const;
        };

        const PlotInfoNode& getInfo() const;

    private:
        //static void getImprovementInfo_(PlotYieldInfo& info, int improvementIndex, int teamID, bool isHills, bool isRiver);
        int makeKey_(bool hasFreshWaterAccess, int riverMask) const;
        int makeRiverMask_() const;
        void init_(bool hasFreshWaterAccess);
        bool hasFreshWaterAccess_() const;

        const CvPlot* pPlot_;
        PlayerTypes playerType_;
        int key_;

        PlotInfoNode node_;
    };
}