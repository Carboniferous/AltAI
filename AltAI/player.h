#pragma once

#include "boost/shared_ptr.hpp"
#include <set>

#include "CvGameCoreDLL.h"
#include "CvStructs.h"

#include "./unit.h"
#include "./tactic_actions.h"

class CvPlayer;
class CvCity;
class CvUnit;
class CvPlot;

namespace AltAI
{
    class PlayerAnalysis;
    class CivHelper;
    class IPlotEvent;
    class City;
    class SettlerManager;

    class Player
    {
    public:
        Player() : pPlayer_(0) {}
        explicit Player(CvPlayer* pPlayer);

        void init();

        void doTurn();

        void addCity(CvCity* pCity);
        void deleteCity(CvCity* pCity);
        City& getCity(int ID);
        const City& getCity(int ID) const;

        void addUnit(CvUnitAI* pUnit);
        void deleteUnit(CvUnitAI* pUnit);
        void moveUnit(CvUnitAI* pUnit, CvPlot* pFromPlot, CvPlot* pToPlot);
        void movePlayerUnit(CvUnitAI* pUnit, CvPlot* pPlot);
        void hidePlayerUnit(CvUnitAI* pUnit, CvPlot* pOldPlot);
        
        const CvPlayer* getCvPlayer() const;
        CvPlayer* getCvPlayer();
        PlayerTypes getPlayerID() const;
        TeamTypes getTeamID() const;

        const boost::shared_ptr<PlayerAnalysis>& getAnalysis() const;
        const boost::shared_ptr<CivHelper>& getCivHelper() const;

        std::vector<BuildTypes> addAdditionalPlotBuilds(const CvPlot* pPlot, BuildTypes buildType) const;
        bool checkResourcesOutsideCities(CvUnitAI* pUnit) const;
        int getNumWorkersAtPlot(const CvPlot* pTargetPlot) const;
        int getNumSettlersTargetingPlot(CvUnit* pIgnoreUnit, const CvPlot* pTargetPlot) const;

        void logMission(CvSelectionGroup* pGroup, MissionData missionData, MissionAITypes eMissionAI, CvPlot* pMissionAIPlot, CvUnit* pMissionAIUnit) const;
        void logClearMissions(const CvSelectionGroup* pGroup) const;
        void logScrapUnit(const CvUnit* pUnit) const;

        void addTech(TechTypes techType);
        void pushPlotEvent(const boost::shared_ptr<IPlotEvent>& pPlotEvent);
        void updatePlotFeature(const CvPlot* pPlot, FeatureTypes oldFeatureType);
        void updatePlotCulture(const CvPlot* pPlot, bool remove);

        void notifyReligionFounded(ReligionTypes religionType, bool isOurs);

        void setWorkingCityOverride(const CvPlot* pPlot, const CvCity* pOldCity, const CvCity* pNewCity);

        void setCityDirty(IDInfo city);
        void updateCityData();

        void updatePlotValues();
        const boost::shared_ptr<SettlerManager>& getSettlerManager() const;
        std::vector<int> getBestCitySites(int minValue, int count);
        std::set<BonusTypes> getBonusesForSites(int siteCount) const;

        CvPlot* getBestPlot(CvUnitAI* pUnit, int subAreaID) const;

        void logCitySites() const;

        void logSettlerMission(const CvPlot* pBestPlot, const CvPlot* pBestFoundPlot, int bestFoundValue, int pathTurns, bool foundSite) const;
        void logUnitAIChange(const CvUnitAI* pUnit, UnitAITypes oldAI, UnitAITypes newAI) const;

        std::pair<int, int> getCityRank(IDInfo city, OutputTypes outputType) const;

        TechTypes getResearchTech(TechTypes ignoreTechType = NO_TECH);
        void logBestResearchTech(TechTypes techType) const;
        void logPushResearchTech(TechTypes techType) const;
        int getTechResearchDepth(TechTypes techType) const;
        ResearchTech getCurrentResearchTech() const;
        void notifyFirstToTechDiscovered(TeamTypes teamType, TechTypes techType);

        void logStuckSelectionGroup(CvUnit* pHeadUnit) const;
        void logInvalidUnitBuild(const CvUnit* pUnit, BuildTypes buildType) const;

        int getMaxResearchRate() const;
        int getMaxResearchRateWithProcesses() const;
        int getMaxResearchRate(std::pair<int, int> fixedIncomeAndExpenses) const;

        int getUnitCount(UnitTypes unitType) const;
        int getUnitCount(const std::vector<UnitAITypes>& AITypes, int militaryFlags) const;
        int getCollateralUnitCount(int militaryFlags) const;
        int getCombatUnitCount(DomainTypes domainType, bool inProduction) const;
        int getScoutUnitCount(DomainTypes domainType, bool inProduction) const;

        // save/load functions
        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        void calcMaxResearchRate_();
        void calcCivics_();

        typedef std::map<int, City> CityMap;
        CityMap cities_;
        std::set<int> citiesToInit_;
        std::set<Unit> units_;
        typedef std::set<Unit>::const_iterator UnitsCIter;
        CvPlayer* pPlayer_;
        boost::shared_ptr<PlayerAnalysis> pPlayerAnalysis_;
        boost::shared_ptr<CivHelper> pCivHelper_;
        boost::shared_ptr<SettlerManager> pSettlerManager_;

        ResearchTech researchTech_;

        std::vector<std::pair<CivicOptionTypes, std::vector<std::pair<CivicTypes, TotalOutput> > > > bestCivics_;

        std::set<IDInfo> cityFlags_;
        int maxRate_, maxRateWithProcesses_;
    };
}