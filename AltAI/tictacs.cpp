#include "./tictacs.h"
#include "./tactic_actions.h"
#include "./tactic_streams.h"
#include "./tech_tactics.h"
#include "./building_tactics.h"
#include "./city_tactics.h"
#include "./unit_tactics.h"
#include "./building_tactics_visitors.h"
#include "./tech_tactics_visitors.h"
#include "./player.h"
#include "./player_analysis.h"
#include "./map_analysis.h"
#include "./city.h"
#include "./city_simulator.h"
#include "./iters.h"
#include "./civ_log.h"
#include "./save_utils.h"

namespace AltAI
{
    void PlayerTactics::init()
    {
        possibleTechTactics_ = makeTechTactics(player);
        possibleUnitTactics_ = makeUnitTactics(player);
        possibleBuildingTactics_ = makeBuildingTactics(player);

        selectCityTactics();

        debugTactics();
    }

    void PlayerTactics::updateBuildingTactics()
    {
        possibleBuildingTactics_ = makeBuildingTactics(player);
        selectBuildingTactics();
    }

    void PlayerTactics::updateTechTactics()
    {
        possibleTechTactics_ = makeTechTactics(player);
        selectTechTactics();
    }

    void PlayerTactics::updateUnitTactics()
    {
        possibleUnitTactics_ = makeUnitTactics(player);
        selectUnitTactics();
    }

    void PlayerTactics::updateFirstToTechTactics(TechTypes techType)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        std::list<ResearchTech>::iterator iter(possibleTechTactics_.begin()), iterEnd(possibleTechTactics_.end());
        while (iter != iterEnd)
        {
            if (iter->techFlags & TechFlags::Free_Tech)
            {
                iter->techFlags &= ~TechFlags::Free_Tech;
#ifdef ALTAI_DEBUG
                os << "\n(updateFirstToTechTactics) Removing free tech flag from research tech tactic: " << *iter;
#endif
            }

            if (iter->techFlags & TechFlags::Free_GP)
            {
                iter->techFlags &= ~TechFlags::Free_GP;
#ifdef ALTAI_DEBUG
                os << "\n(updateFirstToTechTactics) Removing free GP flag from research tech tactic: " << *iter;
#endif
            }

            if (iter->isEmpty())
            {
#ifdef ALTAI_DEBUG
                os << "\n(updateFirstToTechTactics) Removing research tech tactic: " << *iter;
#endif
                possibleTechTactics_.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
    }

    ResearchTech PlayerTactics::getResearchTech(TechTypes ignoreTechType)
    {
        if (selectedTechTactics_.empty() || gGlobals.getGame().getGameTurn() == 0)
        {
            selectTechTactics();
        }
        return AltAI::getResearchTech(*this, ignoreTechType);
    }

    ResearchTech PlayerTactics::getResearchTechData(TechTypes techType) const
    {
        for (std::list<ResearchTech>::const_iterator ci(selectedTechTactics_.begin()), ciEnd(selectedTechTactics_.end()); ci != ciEnd; ++ci)
        {
            if (ci->techType == techType)
            {
                return *ci;
            }
        }
        return ResearchTech();
    }

    ConstructItem PlayerTactics::getBuildItem(const City& city)
    {
        if (selectedTechTactics_.empty() || gGlobals.getGame().getGameTurn() == 0)
        {
            updateTechTactics();
        }
        updateUnitTactics();
        //updateBuildingTactics();
        debugTactics();
        return AltAI::getConstructItem(*this, city);
    }

    void PlayerTactics::selectTechTactics()
    {
        selectedTechTactics_.clear();

        std::list<ResearchTech>::iterator iter(possibleTechTactics_.begin());
        while (iter != possibleTechTactics_.end())
        {
            ResearchTech selectedTech = selectReligionTechTactics(player, *iter);
            if (selectedTech.techType != NO_TECH)
            {
                selectedTechTactics_.push_back(selectedTech);
            }

            selectedTech = selectWorkerTechTactics(player, *iter);
            if (selectedTech.techType != NO_TECH)
            {
                selectedTechTactics_.push_back(selectedTech);
            }

            selectedTech = selectExpansionTechTactics(player, *iter);
            if (selectedTech.techType != NO_TECH)
            {
                selectedTechTactics_.push_back(selectedTech);
            }
            ++iter;
        }

        //debugTactics();
    }

    void PlayerTactics::selectUnitTactics()
    {
        selectedUnitTactics_.clear();

        ConstructListIter iter(possibleUnitTactics_.begin());
        while (iter != possibleUnitTactics_.end())
        {
            ConstructItem selectedUnit = selectExpansionUnitTactics(player, *iter);
            if (selectedUnit.unitType != NO_UNIT)
            {
                selectedUnitTactics_.push_back(selectedUnit);
            }
            ++iter;
        }

        //debugTactics();
    }

    void PlayerTactics::selectBuildingTactics()
    {
        const CvPlayer* pPlayer = player.getCvPlayer();

        CityIter cityIter(*pPlayer);
        CvCity* pCity;
        while (pCity = cityIter())
        {
            selectBuildingTactics(player.getCity(pCity->getID()));
        }
    }

    void PlayerTactics::selectBuildingTactics(const City& city)
    {
        selectedCityBuildingTactics_[city.getCvCity()->getIDInfo()].clear();
        ConstructListIter iter(possibleBuildingTactics_.begin());
        while (iter != possibleBuildingTactics_.end())
        {
            ConstructItem selectedBuilding = selectExpansionBuildingTactics(player, city, *iter);
            if (selectedBuilding.buildingType != NO_BUILDING || selectedBuilding.processType != NO_PROCESS)
            {
                selectedCityBuildingTactics_[city.getCvCity()->getIDInfo()].push_back(selectedBuilding);
            }

            selectedBuilding = selectExpansionMilitaryBuildingTactics(player, city, *iter);
            if (selectedBuilding.buildingType != NO_BUILDING)
            {
                selectedCityBuildingTactics_[city.getCvCity()->getIDInfo()].push_back(selectedBuilding);
            }

            ++iter;
        }

        //debugTactics();           
    }

    void PlayerTactics::selectCityTactics()
    {
        typedef std::map<YieldTypes, std::multimap<int, IDInfo, std::greater<int> > > CityYieldMap;
        CityYieldMap cityYieldMap;

        const int foodPerPop = gGlobals.getFOOD_CONSUMPTION_PER_POPULATION();
        boost::shared_ptr<MapAnalysis> pMapAnalysis = player.getAnalysis()->getMapAnalysis();
        YieldPriority yieldP = makeYieldP(YIELD_FOOD);
        YieldWeights yieldW = makeYieldW(0, 2, 1);
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
#endif
        int bestFoodDelta = 0;
        IDInfo bestCity;

        CityIter iter(*player.getCvPlayer());
        while (CvCity* pCity = iter())
        {
            const CityImprovementManager& improvementManager = pMapAnalysis->getImprovementManager(pCity->getIDInfo());

            const int size = pCity->getPopulation() + pCity->happyLevel() - pCity->unhappyLevel();
            PlotYield projectedYield = improvementManager.getProjectedYield(size, yieldP, yieldW);

            int thisFoodDelta = projectedYield[YIELD_FOOD] - foodPerPop * size;
            if (thisFoodDelta > bestFoodDelta)
            {
                bestFoodDelta = thisFoodDelta;
                bestCity = pCity->getIDInfo();
            }
#ifdef ALTAI_DEBUG
            os << "\nCity: " << narrow(pCity->getName()) << ", projected yield = " << projectedYield << " for size: " << size;
#endif
            CityImprovementManager testImprovementManager(pCity->getIDInfo(), true);
            for (int i = 0; i < NUM_YIELD_TYPES; ++i)
            {
                testImprovementManager.calcImprovements(std::vector<YieldTypes>(1, (YieldTypes)i), size, 3);
                PlotYield projectedYield = testImprovementManager.getProjectedYield(size, yieldP, yieldW);
#ifdef ALTAI_DEBUG
                os << "\nCity: " << narrow(pCity->getName()) << ", projected yield = " << projectedYield << " for yieldtype: " << i;
#endif
                cityYieldMap[(YieldTypes)i].insert(std::make_pair(projectedYield[i], pCity->getIDInfo()));
            }
        }

        if (bestCity.eOwner != NO_PLAYER)
        {
#ifdef ALTAI_DEBUG
            os << "\nBest city = " << narrow(getCity(bestCity)->getName()) << " with delta = " << bestFoodDelta;
#endif
        }

#ifdef ALTAI_DEBUG
        for (CityYieldMap::const_iterator ci(cityYieldMap.begin()), ciEnd(cityYieldMap.end()); ci != ciEnd; ++ci)
        {
            os << "\nYieldtype : " << ci->first;
            for (std::multimap<int, IDInfo, std::greater<int> >::const_iterator mi(ci->second.begin()), miEnd(ci->second.end()); mi != miEnd; ++mi)
            {
                os << " " << narrow(getCity(mi->second)->getName()) << " = " << mi->first;
            }
        }
#endif
    }

    void PlayerTactics::write(FDataStreamBase* pStream) const
    {
        writeComplexList(pStream, possibleTechTactics_);
        writeComplexList(pStream, selectedTechTactics_);
        writeComplexList(pStream, possibleUnitTactics_);
        writeComplexList(pStream, selectedUnitTactics_);
        writeComplexList(pStream, possibleBuildingTactics_);

        pStream->Write(selectedCityBuildingTactics_.size());
        for (std::map<IDInfo, ConstructList>::const_iterator ci(selectedCityBuildingTactics_.begin()), ciEnd(selectedCityBuildingTactics_.end()); ci != ciEnd; ++ci)
        {
            ci->first.write(pStream);
            writeComplexList(pStream, ci->second);
        }
    }

    void PlayerTactics::read(FDataStreamBase* pStream)
    {
        readComplexList(pStream, possibleTechTactics_);
        readComplexList(pStream, selectedTechTactics_);
        readComplexList(pStream, possibleUnitTactics_);
        readComplexList(pStream, selectedUnitTactics_);
        readComplexList(pStream, possibleBuildingTactics_);

        size_t size;
        pStream->Read(&size);
        for (size_t i = 0; i < size; ++i)
        {
            IDInfo city;
            city.read(pStream);
            ConstructList constructList;
            readComplexList(pStream, constructList);

            selectedCityBuildingTactics_.insert(std::make_pair(city, constructList));
        }
    }

    void PlayerTactics::debugTactics()
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*player.getCvPlayer())->getStream();
        os << "\n\nPlayerTactics::debugTactics():\nPossible tactics: (turn = " << gGlobals.getGame().getGameTurn() << ") ";

        os << "\nPossible tech tactics:\n";
        for (std::list<ResearchTech>::const_iterator ci(possibleTechTactics_.begin()), ciEnd(possibleTechTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nPossible building tactics:\n";
        for (ConstructListConstIter ci(possibleBuildingTactics_.begin()), ciEnd(possibleBuildingTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nSelected building tactics:\n";
        for (std::map<IDInfo, ConstructList >::const_iterator ci(selectedCityBuildingTactics_.begin()), ciEnd(selectedCityBuildingTactics_.end()); ci != ciEnd; ++ci)
        {
            const CvCity* pCity = getCity(ci->first);
            if (pCity)
            {
                os << "\nCity: " << narrow(pCity->getName()) << "\n";
            }

            for (ConstructListConstIter ci2(ci->second.begin()), ci2End(ci->second.end()); ci2 != ci2End; ++ci2)
            {
                os << *ci2 << "\n";
            }
        }

        os << "\nPossible unit tactics:\n";
        for (ConstructListConstIter ci(possibleUnitTactics_.begin()), ciEnd(possibleUnitTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nSelected tech tactics:\n";
        for (std::list<ResearchTech>::const_iterator ci(selectedTechTactics_.begin()), ciEnd(selectedTechTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }

        os << "\nSelected general unit tactics:\n";
        for (ConstructListConstIter ci(selectedUnitTactics_.begin()), ciEnd(selectedUnitTactics_.end()); ci != ciEnd; ++ci)
        {
            os << *ci << "\n";
        }
        os << "\n";
#endif
    }
}