#pragma once

#include "./unit.h"
#include "./tactic_actions.h"

class CvPlayer;
class CvCity;
class CvUnit;
class CvPlot;

namespace AltAI
{
    class PlayerAnalysis;
    class OpponentsAnalysis;
    class CivHelper;
    class AreaHelper;
    class IPlotEvent;
    class City;
    typedef boost::shared_ptr<City> CityPtr;
    class SettlerManager;
    struct HurryData;

    class Player;
    typedef boost::shared_ptr<Player> PlayerPtr;

    class Player
    {
    public:
        Player() : pPlayer_(0) {}
        explicit Player(CvPlayer* pPlayer);

        void init();

        void doTurn();

        void addCity(CvCity* pCity);
        void deleteCity(CvCity* pCity);
        void initCities();
        void recalcPlotInfo();
        void reinitDotMap();
        void reinitPlotKeys();

        City& getCity(const CvCity* pCity);
        const City& getCity(const CvCity* pCity) const;

        bool isCity(const int ID) const;
        City& getCity(const int ID);
        const City& getCity(const int ID) const;

        void addOurUnit(CvUnitAI* pUnit, const CvUnit* pUpgradingUnit = (const CvUnit*)0, bool loading = false);
        void deleteOurUnit(CvUnit* pUnit, const CvPlot* pPlot);        
        void moveOurUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot);
        void withdrawOurUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot);
        CvUnit* getNextAttackUnit();
        std::pair<IDInfo, UnitTypes> getPriorityUnitBuild(IDInfo city);
        Unit& getOurUnit(CvUnitAI* pUnit);
        Unit& getOurUnit(IDInfo unit);

        void addPlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot);
        void deletePlayerUnit(CvUnitAI* pUnit, const CvPlot* pPlot);
        void movePlayerUnit(CvUnitAI* pUnit, const CvPlot* pFromPlot, const CvPlot* pToPlot);
        void hidePlayerUnit(CvUnitAI* pUnit, const CvPlot* pOldPlot, bool moved);
        void withdrawPlayerUnit(CvUnitAI* pUnit, const CvPlot* pAttackPlot);

        std::vector<IUnitEventGeneratorPtr> getCityUnitEvents(IDInfo city);
        
        const CvPlayer* getCvPlayer() const;
        CvPlayer* getCvPlayer();
        PlayerTypes getPlayerID() const;
        TeamTypes getTeamID() const;

        const boost::shared_ptr<PlayerAnalysis>& getAnalysis() const;
        const boost::shared_ptr<CivHelper>& getCivHelper() const;
        const boost::shared_ptr<AreaHelper>& getAreaHelper(int areaID);

        CvCity* getNextCityForWorkerToImprove(const CvCity* pCurrentCity) const;

        std::vector<BuildTypes> addAdditionalPlotBuilds(const CvPlot* pPlot, BuildTypes buildType) const;
        bool checkResourcesOutsideCities(CvUnitAI* pUnit, const std::multimap<int, const CvPlot*>& resourceHints);
        bool checkResourcesPlot(CvUnitAI* pUnit, const CvPlot* pPlot);
        int getNumWorkersTargetingPlot(XYCoords targetCoords) const;

        int getNumWorkersTargetingCity(const CvCity* pCity) const;
        int getNumSettlersTargetingPlot(CvUnit* pIgnoreUnit, const CvPlot* pTargetPlot) const;
        int getNumActiveWorkers(UnitTypes unitType) const;
        std::vector<std::pair<UnitTypes, std::vector<Unit::WorkerMission> > > getWorkerMissionsForCity(const CvCity* pCity) const;
        std::vector<std::pair<std::pair<int, UnitTypes>, std::vector<Unit::WorkerMission> > > getWorkerMissions() const;
        void debugWorkerMissions(std::ostream& os) const;

        std::vector<IDInfo> getCitiesTargetingPlot(UnitTypes unitType, XYCoords buildTarget) const;

        bool doSpecialistMove(CvUnitAI* pUnit);

        void logMission(CvSelectionGroup* pGroup, MissionData missionData, MissionAITypes eMissionAI, CvPlot* pMissionAIPlot, CvUnit* pMissionAIUnit, const char* callingFunction) const;
        void logClearMissions(const CvSelectionGroup* pGroup, const std::string& caller) const;
        void logScrapUnit(const CvUnit* pUnit) const;
        void logFailedPath(const CvSelectionGroup* pGroup, const CvPlot* pFromPlot, const CvPlot* pToPlot, int iFlags) const;
        void logEmphasis(IDInfo city, EmphasizeTypes eIndex, bool bNewValue) const;
        
        void updateMilitaryAnalysis();
        void updateWorkerAnalysis();

        void pushWorkerMission(CvUnitAI* pUnit, const CvCity* pCity, const CvPlot* pTargetPlot,
            MissionTypes missionType, BuildTypes buildType, int iFlags = 0, const std::string& caller = "");
        //void updateWorkerMission(CvUnitAI* pUnit, BuildTypes buildType);
        void clearWorkerMission(CvUnitAI* pUnit);
        
        bool hasMission(CvUnitAI* pUnit);
        void pushMission(CvUnitAI* pUnit, const UnitMissionPtr& pMission);
        void updateMission(CvUnitAI* pUnit, CvPlot* pOldPlot, CvPlot* pNewPlot);
        bool executeMission(CvUnitAI* pUnit);
        PromotionTypes promoteUnit(CvUnitAI* pUnit);

        template <typename P>
            std::vector<UnitMissionPtr> getMissions(P pred, const CvUnitAI* pIgnoreUnit = NULL) const
        {
            std::vector<UnitMissionPtr> missions;

            for (UnitsCIter iter(units_.begin()), endIter(units_.end()); iter != endIter; ++iter)
            {
                if (pIgnoreUnit && iter->second.getUnit() == pIgnoreUnit)
                {
                    continue;
                }

                for (size_t i = 0, count = iter->second.getMissions().size(); i < count; ++i)
                {
                    if (pred(iter->second.getMissions()[i]))
                    {
                        missions.push_back(iter->second.getMissions()[i]);
                    }
                }
            }

            return missions;
        }

        void pushExploreMission(CvUnitAI* pUnit, const CvCity* pCity);

        void addTech(TechTypes techType, TechSources source);
        void gaveTech(TechTypes techType, PlayerTypes fromPlayer, PlayerTypes toPlayer);

        void meetTeam(TeamTypes teamType);

        void updatePlotInfo(const CvPlot* pPlot, bool isNew, const std::string& caller);
        //void pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pPlotEvent);
        void updatePlotRevealed(const CvPlot* pPlot, bool isNew, bool isRevealed);
        void updatePlotBonus(const CvPlot* pPlot, BonusTypes revealedBonusType);
        void updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType);
        void updatePlotCulture(const CvPlot* pPlot, PlayerTypes previousRevealedOwner, PlayerTypes newRevealedOwner);
        void updatePlotImprovement(const CvPlot* pPlot, ImprovementTypes oldImprovementType);
        void updateCityBonusCount(const CvCity* pCity, BonusTypes bonusType, int delta);

        void updateCityGreatPeople(IDInfo city);

        void eraseLimitedBuildingTactics(BuildingTypes buildingType);

        void notifyReligionFounded(ReligionTypes religionType, bool isOurs);
        void notifyCommerceRateChanged(CommerceTypes commerceType);

        bool isSharedPlot(const CvPlot* pPlot) const;
        const CvCity* getSharedPlotAssignedCity(const CvPlot* pPlot) const;
        void setWorkingCityOverride(const CvPlot* pPlot, const CvCity* pOldCity, const CvCity* pNewCity);

        void setCityDirty(IDInfo city);
        void updateCityData();

        void updatePlotValues();
        const boost::shared_ptr<SettlerManager>& getSettlerManager() const;
        std::vector<int /* plot num */> getBestCitySites(int minValue, int count);
        //std::set<BonusTypes> getBonusesForSites(int siteCount) const;

        CvPlot* getBestPlot(CvUnit* pUnit, int subAreaID) const;

        void logCitySites() const;

        void logSettlerMission(const CvPlot* pBestPlot, const CvPlot* pBestFoundPlot, int bestFoundValue, int pathTurns, bool foundSite) const;
        void logUnitAIChange(const CvUnitAI* pUnit, UnitAITypes oldAI, UnitAITypes newAI) const;

        std::pair<int, int> getCityRank(IDInfo city, OutputTypes outputType) const;

        TechTypes getResearchTech(TechTypes ignoreTechType = NO_TECH);
        CivicTypes chooseCivic(CivicOptionTypes civicOptionType);

        void logBestResearchTech(TechTypes techType) const;
        void logPushResearchTech(TechTypes techType) const;
        int getTechResearchDepth(TechTypes techType) const;
        ResearchTech getCurrentResearchTech() const;
        void notifyFirstToTechDiscovered(TeamTypes teamType, TechTypes techType);

        void logHurry(const CvCity* pCity, const HurryData& hurryData) const;

        void logStuckSelectionGroup(CvUnit* pHeadUnit) const;
        void logInvalidUnitBuild(const CvUnit* pUnit, BuildTypes buildType) const;

        CommerceModifier getCommercePercentages() const;

        int getMaxResearchPercent() const;
        int getMaxResearchPercentWithProcesses() const;
        int getMaxResearchPercent(std::pair<int, int> fixedIncomeAndExpenses) const;        

        int getMaxGoldRate() const;
        int getMaxGoldRateWithProcesses() const;

        const std::vector<std::pair<int, int> >& getGoldAndResearchRates() const;

        TotalOutput getCurrentOutput();
        TotalOutput getCurrentProjectedOutput();

        int getUnitCount(UnitTypes unitType, bool includeUnderConstruction = false) const;
        int getUnitCount(UnitAITypes unitAIType) const;
        int getCollateralUnitCount(int militaryFlags) const;
        int getCombatUnitCount(DomainTypes domainType, bool inProduction) const;
        int getScoutUnitCount(DomainTypes domainType, bool inProduction) const;

        void notifyHaveReligion(ReligionTypes religionType);
        void notifyLostReligion(ReligionTypes religionType);

        int getNumKnownPlayers() const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        void calcMaxResearchRate_();
        //void calcCivics_();        

        typedef std::map<int, CityPtr> CityMap;
        CityMap cities_;
        std::set<int> citiesToInit_;
        typedef std::map<IDInfo, Unit> UnitMap;
        UnitMap units_;
        typedef UnitMap::iterator UnitsIter;
        typedef UnitMap::const_iterator UnitsCIter;
        CvPlayer* pPlayer_;
        boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis_;
        boost::shared_ptr<OpponentsAnalysis> pOpponentsAnalysis_;
        boost::shared_ptr<CivHelper> pCivHelper_;
        std::map<int, boost::shared_ptr<AreaHelper> > areaHelpersMap_;
        boost::shared_ptr<SettlerManager> pSettlerManager_;

        ResearchTech researchTech_;

        std::vector<std::pair<CivicOptionTypes, std::vector<std::pair<CivicTypes, TotalOutput> > > > bestCivics_;

        std::set<IDInfo> cityFlags_;
        int maxRate_, maxRateWithProcesses_;
        int maxGold_, maxGoldWithProcesses_;
        std::vector<std::pair<int, int> > goldAndResearchOutputsByCommerceRate_;
    };
}