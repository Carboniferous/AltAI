#pragma once

#include "./utils.h"

namespace AltAI
{
    struct DotMapItem;

    struct PlotCond
    {
        virtual ~PlotCond() = 0 {}
        virtual bool operator() (const CvPlot* pPlot) const = 0;
        virtual void log(std::ostream& os) const = 0;
    };

    typedef boost::shared_ptr<PlotCond> PlotCondPtr;

    struct IsWater : PlotCond
    {
        virtual bool operator() (const CvPlot* pPlot) const
        {
            return pPlot->isWater();
        }

        virtual void log(std::ostream& os) const
        {
            os << " is water";
        }
    };

    struct IsLand : PlotCond
    {
        virtual bool operator() (const CvPlot* pPlot) const
        {
            return !pPlot->isWater();
        }

        virtual void log(std::ostream& os) const
        {
            os << " is land";
        }
    };

    struct IgnorePlot : PlotCond
    {
        explicit IgnorePlot(const CvPlot* pIgnorePlot_) : pIgnorePlot(pIgnorePlot_) {}

        virtual bool operator() (const CvPlot* pPlot) const
        {
            return pPlot != pIgnorePlot;  // TODO: check - pointer comparison
        }

        virtual void log(std::ostream& os) const
        {
            os << " is not plot: " << pIgnorePlot->getCoords();
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

        virtual void log(std::ostream& os) const
        {
            os << " has bonus";
        }

        TeamTypes teamType;
    };

    struct NoBonus : PlotCond
    {
        explicit NoBonus(TeamTypes teamType_) : teamType(teamType_) {}

        virtual bool operator() (const CvPlot* pPlot) const
        {
            return pPlot->getBonusType(teamType) == NO_BONUS;
        }

        virtual void log(std::ostream& os) const
        {
            os << " has no bonus";
        }

        TeamTypes teamType;
    };

    struct IsSubArea : PlotCond
    {
        explicit IsSubArea(int subAreaId_) : subAreaId(subAreaId_) {}

        virtual bool operator() (const CvPlot* pPlot) const
        {
            return pPlot->getSubArea() == subAreaId;
        }

        virtual void log(std::ostream& os) const
        {
            os << " has subarea id = "<< subAreaId;
        }

        int subAreaId;
    };

    struct ProjectionLadder;
    typedef std::vector<boost::tuple<FeatureTypes, ImprovementTypes, ProjectionLadder> > PlotImprovementProjections;
    typedef std::vector<std::pair<XYCoords, PlotImprovementProjections> > PlotImprovementsProjections;

    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    struct PlotBuildData
    {
        PlotBuildData() : improvement(NO_IMPROVEMENT), removedFeature(NO_FEATURE), routeType(NO_ROUTE) {}
        PlotBuildData(XYCoords coords_, ImprovementTypes improvement_, FeatureTypes removedFeature_, RouteTypes routeType_)
            : coords(coords_), improvement(improvement_), removedFeature(removedFeature_), routeType(routeType_) {}

        XYCoords coords;
        ImprovementTypes improvement;
        FeatureTypes removedFeature;        
        RouteTypes routeType;
    };

    struct PlotImprovementData
    {
        enum ImprovementState
        {
            Not_Set = -1, Not_Built = 0, Being_Built, Built, Not_Selected
        };

        enum ImprovementFlags
        {
            None = 0, IrrigationChainPlot = (1 << 0), NeedsIrrigation = (1 << 1), NeedsRoute = (1 << 2), KeepFeature = (1 << 3), RemoveFeature = (1 << 4),
            KeepExistingImprovement = (1 << 5), ImprovementMakesBonusValid = (1 << 6), WorkerNeedsTransport = (1 << 7)
        };

        struct SimulationData
        {
            SimulationData() : firstTurnWorked(-1), numTurnsWorked(0) {}
            int firstTurnWorked, numTurnsWorked;
        };

        XYCoords coords;
        FeatureTypes removedFeature;
        ImprovementTypes improvement;
        PlotYield yield;
        SimulationData simulationData;
        ImprovementState state;
        int flags;

        PlotImprovementData() : removedFeature(NO_FEATURE), improvement(NO_IMPROVEMENT), state(Not_Set), flags(None)
        {
        }

        PlotImprovementData(XYCoords coords_, FeatureTypes removedFeature_, ImprovementTypes improvement_,
            PlotYield yield_, ImprovementState state_, int flags_)
            : coords(coords_), removedFeature(removedFeature_), improvement(improvement_),
              yield(yield_), state(state_), flags(flags_)
        {
        }

        bool isSelectedAndNotBuilt() const
        {
            return state == PlotImprovementData::Not_Built && simulationData.firstTurnWorked > -1;
        }
    };

    class CityImprovementManager
    {
    public:
        ~CityImprovementManager();

        //typedef boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, PlotYield, TotalOutput, ImprovementState, int /*ImprovementFlags*/> PlotImprovementData;

        CityImprovementManager()
            : includeUnclaimedPlots_(false)
        {
        }

        CityImprovementManager(IDInfo city, bool includeUnclaimedPlots = false);

        bool operator < (const CityImprovementManager& other) const
        {
            return city_ < other.city_;
        }

        boost::tuple<XYCoords, FeatureTypes, ImprovementTypes, int> getBestImprovementNotBuilt(bool whichMakesBonusValid, bool selectedOnly,
            const std::vector<PlotCondPtr >& conditions = std::vector<PlotCondPtr >()) const;

        ImprovementTypes getBestImprovementNotBuilt(XYCoords coords) const;

        int getRank(XYCoords coords) const;
        int getNumImprovementsNotBuilt() const;
        int getNumImprovementsBuilt() const;

        std::pair<XYCoords, RouteTypes> getBestRoute() const;

        const std::vector<PlotImprovementData>& getImprovements() const;
        std::vector<PlotImprovementData>& getImprovements();
        TotalOutput getImprovementsDelta() const;

        std::pair<bool, PlotImprovementData*> findImprovement(XYCoords coords);

        bool getIncludeUnclaimedPlots() const { return includeUnclaimedPlots_; }

        void simulateImprovements(const CityDataPtr& pCityData, int lookAheadDepth = 0, const std::string& logLabel = "");
        bool updateImprovements(const CvPlot* pPlot, ImprovementTypes improvementType, FeatureTypes featureType, RouteTypes routeType, bool simulated = false);

        XYCoords getIrrigationChainPlot(XYCoords destination);
        ImprovementTypes getSubstituteImprovement(const CityDataPtr& pCityData, XYCoords coords);

        void logImprovements() const;
        void logImprovements(std::ostream& os) const;
        static void logImprovement(std::ostream& os, const PlotImprovementData& improvement);

        IDInfo getCity() const { return city_; }

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static void writeImprovements(FDataStreamBase* pStream, const std::vector<PlotImprovementData>& improvements);
        static void readImprovements(FDataStreamBase* pStream, std::vector<PlotImprovementData>& improvements);

    private:
        void calcImprovements_(const CityDataPtr& pCityData, const std::vector<YieldTypes>& yieldTypes, int targetSize, int lookAheadDepth = 0);
        void markFeaturesToKeep_(DotMapItem& dotMapItem) const;
        void markPlotsWhichNeedIrrigation_();
        void markPlotsWhichNeedRoute_();
        void markPlotsWhichNeedTransport_();
        void markPlotsWhichNeedFeatureRemoved_();
        boost::tuple<int, int, XYCoords> getPathAndCost_(FAStar* pIrrigationPathFinder, XYCoords start, XYCoords destination, PlayerTypes playerType) const;

        IDInfo city_;
        std::vector<PlotImprovementData> improvements_;
        TotalOutput improvementsDelta_;
        const bool includeUnclaimedPlots_;

        PlotYield maxProdYield_, maxCommerceYield_;
    };

    std::vector<PlotImprovementData> 
            findNewImprovements(const std::vector<PlotImprovementData>& baseImprovements, const std::vector<PlotImprovementData>& newImprovements);

    struct ImprovementCoordsFinder
    {
        explicit ImprovementCoordsFinder(XYCoords coords_) : coords(coords_)
        {
        }

        bool operator() (const PlotImprovementData& other) const
        {
            return other.coords == coords;
        }

        XYCoords coords;
    };
}