#pragma once

#include "./utils.h"

namespace AltAI
{
    struct DotMapItem;

    struct PlotCond
    {
        virtual ~PlotCond() = 0 {}
        virtual bool operator() (const CvPlot* pPlot) const = 0;
    };

    struct IsWater : PlotCond
    {
        virtual bool operator() (const CvPlot* pPlot) const
        {
            return pPlot->isWater();
        }
    };

    struct IsLand : PlotCond
    {
        virtual bool operator() (const CvPlot* pPlot) const
        {
            return !pPlot->isWater();
        }
    };

    struct IgnorePlot : PlotCond
    {
        explicit IgnorePlot(const CvPlot* pIgnorePlot_) : pIgnorePlot(pIgnorePlot_) {}

        virtual bool operator() (const CvPlot* pPlot) const
        {
            return pPlot != pIgnorePlot;  // TODO: check - pointer comparison
        }

        const CvPlot* pIgnorePlot;
    };

    struct HasBonus : PlotCond
    {
        explicit HasBonus(TeamTypes teamType_) : teamType(teamType_) {}

        virtual bool operator() (const CvPlot* pPlot) const
        {
            return pPlot->getBonusType(teamType) != NO_BONUS;
        }

        TeamTypes teamType;
    };

    class CityImprovementManager
    {
    public:
        enum ImprovementState
        {
            Not_Set = -1, Not_Built = 0, Being_Built, Built, Not_Selected
        };

        enum ImprovementFlags
        {
            None = 0, IrrigationChainPlot = (1 << 0), NeedsIrrigation = (1 << 1), NeedsRoute = (1 << 2), KeepFeature = (1 << 3), RemoveFeature = (1 << 4),
            KeepExistingImprovement = (1 << 5), ImprovementMakesBonusValid = (1 << 6), WorkerNeedsTransport = (1 << 7)
        };

        typedef boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, PlotYield, TotalOutput, ImprovementState, int /*ImprovementFlags*/> PlotImprovementData;

        CityImprovementManager()
            : includeUnclaimedPlots_(false)
        {
        }

        explicit CityImprovementManager(IDInfo city, bool includeUnclaimedPlots = false);

        void calcImprovements(const std::vector<YieldTypes>& yieldTypes, int targetSize, int lookAheadDepth = 0);

        bool operator < (const CityImprovementManager& other) const
        {
            return city_ < other.city_;
        }

        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes> getBestImprovementNotBuilt(TotalOutputWeights outputWeights, bool whichMakesBonusValid, bool simulatedOnly,
            const std::vector<boost::shared_ptr<PlotCond> >& conditions = std::vector<boost::shared_ptr<PlotCond> >()) const;
        ImprovementTypes getBestImprovementNotBuilt(XYCoords coords) const;
        int getRank(XYCoords coords, TotalOutputWeights outputWeights) const;
        int getNumImprovementsNotBuilt() const;

        std::pair<XYCoords, RouteTypes> getBestRoute() const;

        const std::vector<PlotImprovementData>& getImprovements() const;
        std::vector<PlotImprovementData>& getImprovements();

        bool getIncludeUnclaimedPlots() const { return includeUnclaimedPlots_; }

        TotalOutput simulateImprovements(TotalOutputWeights outputWeights, const std::string& logLabel = "");
        bool updateImprovements(const CvPlot* pPlot, ImprovementTypes improvementType);

        PlotYield getProjectedYield(int citySize, YieldPriority yieldP, YieldWeights yieldW) const;

        XYCoords getIrrigationChainPlot(XYCoords destination, ImprovementTypes improvementType);
        ImprovementTypes getSubstituteImprovement(XYCoords coords);

        void logImprovements() const;
        static void logImprovement(std::ostream& os, const PlotImprovementData& improvement);

        IDInfo getCity() const { return city_; }

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static void writeImprovements(FDataStreamBase* pStream, const std::vector<PlotImprovementData>& improvements);
        static void readImprovements(FDataStreamBase* pStream, std::vector<PlotImprovementData>& improvements);

    private:
        void markFeaturesToKeep_(DotMapItem& dotMapItem) const;
        void markPlotsWhichNeedIrrigation_();
        void markPlotsWhichNeedRoute_();
        void markPlotsWhichNeedTransport_();
        void markPlotsWhichNeedFeatureRemoved_();
        std::pair<int, XYCoords> getPathAndCost_(FAStar* pIrrigationPathFinder, XYCoords start, XYCoords destination, PlayerTypes playerType) const;
        
        IDInfo city_;
        std::vector<PlotImprovementData> improvements_;
        const bool includeUnclaimedPlots_;

        PlotYield maxProdYield_, maxCommerceYield_;
    };

    std::vector<CityImprovementManager::PlotImprovementData> 
            findNewImprovements(const std::vector<CityImprovementManager::PlotImprovementData>& baseImprovements, const std::vector<CityImprovementManager::PlotImprovementData>& newImprovements);

    struct ImprovementCoordsFinder
    {
        explicit ImprovementCoordsFinder(XYCoords coords_) : coords(coords_)
        {
        }

        bool operator() (const CityImprovementManager::PlotImprovementData& other) const
        {
            return boost::get<0>(other) == coords;
        }

        XYCoords coords;
    };
}