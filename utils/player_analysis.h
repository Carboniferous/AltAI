#pragma once

#include "./player.h"
#include "./analysis.h"
#include "./events.h"
#include "./tictacs.h"

#include <vector>

namespace AltAI
{
    class MapAnalysis;
    class UnitAnalysis;
    class UnitInfo;
    class BuildingInfo;
    class CivicInfo;
    class TechInfo;
    class ResourceInfo;

    class PlayerAnalysis
    {
    public:
        struct WorkerTechData
        {
            WorkerTechData() : techType(NO_TECH) {}

            TechTypes techType;
            std::set<BonusTypes> newPotentialWorkableBonuses;  // bonus types we could access from planned new cities
            std::set<BonusTypes> newConnectableBonuses;  // bonus types we could connect (using workers)
            std::map<BonusTypes, int> newBonusAccessCounts;
            std::map<ImprovementTypes, std::pair<TotalOutput, int> > newValuedImprovements;
            std::map<ImprovementTypes, int> newUnvaluedImprovements;

            void debug(std::ostream& os) const;
        };

        explicit PlayerAnalysis(Player& player);

        void init();
        void postCityInit();

        void update(const boost::shared_ptr<IEvent<NullRecv> >& event);

        const boost::shared_ptr<MapAnalysis>& getMapAnalysis() const
        {
            return pMapAnalysis_;
        }

        TechTypes getResearchTech(TechTypes ignoreTechType = NO_TECH);

        void recalcTechDepths();
        int getTechResearchDepth(TechTypes techType) const;
        std::vector<TechTypes> getTechsWithDepth(int depth) const;

        int getTimeHorizon() const
        {
            return timeHorizon_;
        }

        Player& getPlayer()
        {
            return player_;
        }

        const Player& getPlayer() const
        {
            return player_;
        }

        const boost::shared_ptr<PlayerTactics>& getPlayerTactics() const
        {
            return playerTactics_;
        }

        const boost::shared_ptr<UnitAnalysis>& getUnitAnalysis() const
        {
            return pUnitAnalysis_;
        }

        boost::shared_ptr<UnitInfo> getUnitInfo(UnitTypes unitType) const;
        boost::shared_ptr<BuildingInfo> getBuildingInfo(BuildingTypes buildingType) const;
        boost::shared_ptr<TechInfo> getTechInfo(TechTypes techType) const;
        boost::shared_ptr<CivicInfo> getCivicInfo(CivicTypes civicType) const;
        boost::shared_ptr<ResourceInfo> getResourceInfo(BonusTypes bonusType) const;

        SpecialistTypes getBestSpecialist(OutputTypes outputType) const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        void updateTechDepths_();

        bool sanityCheckTech_(TechTypes techType) const;
        std::map<TechTypes, WorkerTechData> getWorkerTechData_();

        Player& player_;
        boost::shared_ptr<MapAnalysis> pMapAnalysis_;
        int timeHorizon_;
        std::vector<int> techDepths_;

        boost::shared_ptr<PlayerTactics> playerTactics_;
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis_;

        void analyseUnits_();
        void analyseBuildings_();
        void analyseTechs_();
        void analyseCivics_();
        void analyseResources_();
        void analyseSpecialists_();

        std::map<UnitTypes, boost::shared_ptr<UnitInfo> > unitsInfo_;
        std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> > buildingsInfo_;
        std::map<TechTypes, boost::shared_ptr<TechInfo> > techsInfo_;
        std::map<CivicTypes, boost::shared_ptr<CivicInfo> > civicsInfo_;
        std::map<BonusTypes, boost::shared_ptr<ResourceInfo> > resourcesInfo_;

        std::map<OutputTypes, SpecialistTypes> bestSpecialistTypesMap_;
    };
}