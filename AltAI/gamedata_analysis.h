#pragma once

#include "./utils.h"
#include "./tech_info.h"
#include "./resource_info.h"
#include "./plot_info.h"
#include "./plot_data.h"

namespace AltAI
{
    class Player;
    struct ConditionalPlotYieldEnchancingBuilding;
    // data derived from static game data
    // and functions which use that analysis
    class GameDataAnalysis
    {
    public:
        static boost::shared_ptr<GameDataAnalysis> getInstance();
        void analyse();
        void analyseForPlayer(const Player& player);

        std::pair<bool, int> isUniqueUnit(PlayerTypes playerID, int unitClassID) const;

        // todo - do these need to be cleared on reinit of game in case loaded mod changes?
        static TechTypes getCanWorkWaterTech();
        static std::vector<TechTypes> getIgnoreIrrigationTechs();
        static std::vector<TechTypes> getCarriesIrrigationTechs();

        static BuildTypes getBuildTypeForImprovementType(ImprovementTypes improvementType);
        static BuildTypes getBuildTypeForRouteType(RouteTypes routeType);
        static std::vector<BonusTypes> getBonusTypesForBuildType(BuildTypes buildType);

        static bool doesBuildTypeRemoveFeature(BuildTypes buildType, FeatureTypes featureType);
        static bool isBadFeature(FeatureTypes featureType);

        static TechTypes getTechTypeForBuildType(BuildTypes buildType);
        static TechTypes getTechTypeForRouteType(RouteTypes routeType);
        static BuildTypes getBuildTypeToRemoveFeature(FeatureTypes featureType);
        static TechTypes getTechTypeToRemoveFeature(FeatureTypes featureType);

        static GreatPersonOutput getSpecialistUnitTypeAndOutput(SpecialistTypes specialistType, PlayerTypes playerType);

        std::vector<ConditionalPlotYieldEnchancingBuilding> getConditionalPlotYieldEnhancingBuildings(PlayerTypes playerType, const CvCity* pCity = NULL) const;

    private:
        void analysePlots_(PlayerTypes playerID);

        void findUniqueUnits_(PlayerTypes playerID);

        static boost::shared_ptr<GameDataAnalysis> instance_;

        static TechTypes getCanWorkWaterTech_();
        static std::vector<TechTypes> getIgnoreIrrigationTechs_();
        static std::vector<TechTypes> getCarriesIrrigationTechs_();
        typedef std::multimap<BuildTypes, BonusTypes> BuildsResourcesMap;
        static BuildsResourcesMap getBonusTypesForBuildTypes_();

        struct PlayerData
        {
            // class, info id
            std::vector<std::pair<int, int> > uniqueUnits;
            std::vector<std::pair<BuildingClassTypes, BuildingTypes> > uniqueBuildings;

            std::vector<ConditionalPlotYieldEnchancingBuilding> conditionalPlotYieldEnhancingBuildings;
        };

        std::map<PlayerTypes, PlayerData> playerData_;
    };
}