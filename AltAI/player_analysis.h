#pragma once

#include "./player.h"
#include "./analysis.h"
#include "./events.h"
#include "./map_delta.h"
#include "./tictacs.h"

#include <vector>

namespace AltAI
{    
    class MilitaryAnalysis;
    class GreatPeopleAnalysis;
    class UnitAnalysis;
    class UnitInfo;
    class BuildingInfo;
    class CivicInfo;
    class TechInfo;
    class ResourceInfo;
    class ProjectInfo;

    class MapAnalysis;
    class WorkerAnalysis;

    class PlayerAnalysis
    {
    public:
        explicit PlayerAnalysis(Player& player);

        void init();
        void postCityInit();

        // need to update when first city is founded
        void analyseSpecialists();

        void update(const boost::shared_ptr<IEvent<NullRecv> >& event);

        const boost::shared_ptr<MapAnalysis>& getMapAnalysis() const
        {
            return pMapAnalysis_;
        }

        const boost::shared_ptr<WorkerAnalysis>& getWorkerAnalysis() const
        {
            return pWorkerAnalysis_;
        }

        const boost::shared_ptr<GreatPeopleAnalysis>& getGreatPeopleAnalysis() const
        {
            return pGreatPeopleAnalysis_;
        }

        const boost::shared_ptr<MilitaryAnalysis>& getMilitaryAnalysis() const
        {
            return pMilitaryAnalysis_;
        }

        ResearchTech getResearchTech(TechTypes ignoreTechType = NO_TECH);

        void recalcTechDepths();
        int getTechResearchDepth(TechTypes techType) const;
        std::vector<TechTypes> getTechsWithDepth(int depth) const;

        int getTimeHorizon() const
        {
            return timeHorizon_;
        }

        int getNumSimTurns() const;

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
            if (!playerTactics_->isInitialised())
            {
                playerTactics_->init();
            }
            return playerTactics_;
        }

        const boost::shared_ptr<UnitAnalysis>& getUnitAnalysis() const
        {
            return pUnitAnalysis_;
        }

        const boost::shared_ptr<PlotUpdates>& getMapDelta() const
        {
            return pMapDelta_;
        }

        boost::shared_ptr<UnitInfo> getUnitInfo(UnitTypes unitType) const;
        boost::shared_ptr<BuildingInfo> getBuildingInfo(BuildingTypes buildingType) const;
        boost::shared_ptr<BuildingInfo> getSpecialBuildingInfo(BuildingTypes buildingType) const;
        boost::shared_ptr<ProjectInfo> getProjectInfo(ProjectTypes projectType) const;
        boost::shared_ptr<TechInfo> getTechInfo(TechTypes techType) const;
        boost::shared_ptr<CivicInfo> getCivicInfo(CivicTypes civicType) const;
        boost::shared_ptr<ResourceInfo> getResourceInfo(BonusTypes bonusType) const;

        SpecialistTypes getBestSpecialist(OutputTypes outputType) const;
        SpecialistTypes getBestSpecialist(const std::vector<OutputTypes>& outputTypes) const;
        std::vector<SpecialistTypes> getBestSpecialists(const std::vector<OutputTypes>& outputTypes, size_t count) const;
        std::vector<SpecialistTypes> getMixedSpecialistTypes() const;

        int getPlayerUnitProductionModifier(UnitTypes unitType) const;
        int getPlayerBuildingProductionModifier(BuildingTypes buildingType) const;

        std::vector<UnitTypes> getSpecialBuildingUnits(BuildingTypes buildingType) const;

        int getUnitLevel(int experience) const;
        int getRequiredUnitExperience(int level) const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        void updateTechDepths_();
        bool sanityCheckTech_(TechTypes techType) const;

        Player& player_;
        boost::shared_ptr<MapAnalysis> pMapAnalysis_;
        boost::shared_ptr<MilitaryAnalysis> pMilitaryAnalysis_;
        boost::shared_ptr<WorkerAnalysis> pWorkerAnalysis_;
        boost::shared_ptr<GreatPeopleAnalysis> pGreatPeopleAnalysis_;
        boost::shared_ptr<PlotUpdates> pMapDelta_;
        int timeHorizon_;
        std::vector<int> techDepths_;

        boost::shared_ptr<PlayerTactics> playerTactics_;
        boost::shared_ptr<UnitAnalysis> pUnitAnalysis_;

        void analyseUnits_();
        void analyseBuildings_();
        void analyseProjects_();
        void analyseTechs_();
        void analyseCivics_();
        void analyseResources_();
        void analyseExperienceLevels_();

        std::map<UnitTypes, boost::shared_ptr<UnitInfo> > unitsInfo_;
        std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> > buildingsInfo_;
        std::map<BuildingTypes, boost::shared_ptr<BuildingInfo> > specialBuildingsInfo_;
        std::map<ProjectTypes, boost::shared_ptr<ProjectInfo> > projectsInfo_;
        std::map<TechTypes, boost::shared_ptr<TechInfo> > techsInfo_;
        std::map<CivicTypes, boost::shared_ptr<CivicInfo> > civicsInfo_;
        std::map<BonusTypes, boost::shared_ptr<ResourceInfo> > resourcesInfo_;

        std::map<UnitTypes, int> unitProductionModifiersMap_;
        std::map<BuildingTypes, int> buildingProductionModifiersMap_;

        std::map<OutputTypes, SpecialistTypes> bestSpecialistTypesMap_;
        std::vector<SpecialistTypes> mixedSpecialistTypes_;
        std::map<BuildingTypes, std::vector<UnitTypes> > unitSpecialBuildingsMap_;

        std::map<int, int> levelExperienceMap_;  // level -> exp
        std::map<int, int> experienceLevelsMap_;  // exp -> level
    };
}