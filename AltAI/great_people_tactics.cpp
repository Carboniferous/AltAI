#include "AltAI.h"

#include "./great_people_tactics.h"
#include "./city.h"
#include "./religion_helper.h"
#include "./specialist_helper.h"
#include "./civ_log.h"
#include "./player_analysis.h"
#include "./tictacs.h"
#include "./city_unit_tactics.h"
#include "./tech_info_visitors.h"

namespace AltAI
{
    namespace
    {
        struct CityGPPData
        {
            explicit CityGPPData(City& city)
            {
                CityDataPtr pCityData = city.getCityData();
                cityGPPModifier = pCityData->getSpecialistHelper()->getPlayerGPPModifier()
                    + pCityData->getSpecialistHelper()->getCityGPPModifier();
                if (pCityData->getReligionHelper()->hasStateReligion())
                {
                    cityGPPModifier += pCityData->getSpecialistHelper()->getStateReligionGPPModifier();
                }

                for (int specialistType = 0, count = gGlobals.getNumSpecialistInfos(); specialistType < count; ++specialistType)
                {
                    slotsMap[(SpecialistTypes)specialistType] = pCityData->getNumPossibleSpecialists((SpecialistTypes)specialistType);
                }

                // todo - wire through city data as we should be simulating it anyway
                currentGPPProgress = city.getCvCity()->getGreatPeopleProgress();
            }

            int cityGPPModifier;
            std::map<SpecialistTypes, int> slotsMap;
            int currentGPPProgress;
        };
    }

    class GreatPeopleAnalysisImpl
    {
    public:
        explicit GreatPeopleAnalysisImpl(Player& player) : player_(player)
        {
            baseGPPModifier_ = player.getCvPlayer()->getGreatPeopleRateModifier();
            currentGPPThreshold_ = player.getCvPlayer()->greatPeopleThreshold(false);
        }

        void updateCity(const CvCity* pCity, bool remove)
        {
            if (remove)
            {
                cityData_.erase(pCity->getIDInfo());
            }
            else
            {
                std::map<IDInfo, CityGPPData>::iterator iter = cityData_.find(pCity->getIDInfo());
                if (iter == cityData_.end())
                {
                    cityData_.insert(std::make_pair(pCity->getIDInfo(), CityGPPData(player_.getCity(pCity))));
                }
                else
                {
                    iter->second = CityGPPData(player_.getCity(pCity));
                }
            }
        }

    private:
        Player& player_;
        int baseGPPModifier_;
        int currentGPPThreshold_;
        std::map<IDInfo, CityGPPData> cityData_;
    };


    GreatPeopleAnalysis::GreatPeopleAnalysis(Player& player)
    {
        pImpl_ = boost::shared_ptr<GreatPeopleAnalysisImpl>(new GreatPeopleAnalysisImpl(player));
    }

    void GreatPeopleAnalysis::updateCity(const CvCity* pCity, bool remove)
    {
        pImpl_->updateCity(pCity, remove);
    }

    bool getSpecialistBuild(const PlayerTactics& playerTactics, CvUnitAI* pUnit)
    {
#ifdef ALTAI_DEBUG
        std::ostream& os = CivLog::getLog(*playerTactics.player.getCvPlayer())->getStream();
#endif
        PlayerTactics::UnitTacticsMap::const_iterator unitIter = playerTactics.specialUnitTacticsMap_.find(pUnit->getUnitType());
        TacticSelectionData selectionData;
        
        if (unitIter != playerTactics.specialUnitTacticsMap_.end())
        {
            unitIter->second->update(playerTactics.player);
            unitIter->second->apply(selectionData);
#ifdef ALTAI_DEBUG
            os << "\nSpec selection tactics: ";
            unitIter->second->debug(os);
            os << "\nSpec selection data:";
            selectionData.debug(os);
#endif
            TotalOutputWeights weights = makeOutputW(3, 4, 3, 3, 1, 1);
            TotalOutputValueFunctor valueF(weights);
            int bestBuildingValue = 0;
            IDInfo bestBuildingCity;
            BuildingTypes bestBuilding = NO_BUILDING;

            for (std::set<EconomicBuildingValue>::const_iterator ci(selectionData.economicBuildings.begin()), ciEnd(selectionData.economicBuildings.end()); ci != ciEnd; ++ci)
            {
                int thisValue = valueF(ci->output) / std::max<int>(1, ci->nTurns);
                if (thisValue > bestBuildingValue)
                {
                    bestBuildingCity = ci->city;
                    bestBuildingValue = thisValue;
                    bestBuilding = ci->buildingType;
                }
#ifdef ALTAI_DEBUG
                os << "\n(Economic Building): " << gGlobals.getBuildingInfo(ci->buildingType).getType()
                    << " city: " << narrow(::getCity(ci->city)->getName()) << " turns = " << ci->nTurns << ", delta = " << ci->output << " value = " << thisValue;
#endif
            }

            int bestSpecValue = 0;
            IDInfo bestSettleCity;
            SpecialistTypes bestSettledSpecialist = NO_SPECIALIST;
            for (std::multiset<SettledSpecialistValue>::const_iterator ci(selectionData.settledSpecialists.begin()), ciEnd(selectionData.settledSpecialists.end()); ci != ciEnd; ++ci)
            {
                int thisValue = valueF(ci->output);
                if (thisValue > bestSpecValue)
                {
                    bestSettleCity = ci->city;
                    bestSpecValue = thisValue;
                    bestSettledSpecialist = ci->specType;
                }
#ifdef ALTAI_DEBUG
                os << "\n(Settled Spec): " << gGlobals.getSpecialistInfo(ci->specType).getType()
                    << " city: " << narrow(::getCity(ci->city)->getName()) << ci->output << " value = " << thisValue;
#endif
            }

            int techValue = weights[OUTPUT_RESEARCH] * calculateTechResearchCost(selectionData.possibleFreeTech, playerTactics.player.getPlayerID()) * playerTactics.player.getAnalysis()->getNumSimTurns();
#ifdef ALTAI_DEBUG
            os << "\nBest building: " << (bestBuilding == NO_BUILDING ? " none " : gGlobals.getBuildingInfo(bestBuilding).getType())
                << " city: " << (bestBuildingCity == IDInfo() ? " none " : narrow(::getCity(bestBuildingCity)->getName()))
                << " value = " << bestBuildingValue
                << " best settle city: " << (bestSettleCity == IDInfo() ? " none " : narrow(::getCity(bestSettleCity)->getName()))
                << " value = " << bestSpecValue
                << " discover tech value: " << techValue;
#endif
            if (techValue > bestBuildingValue && techValue > bestSpecValue)
            {
                pUnit->getGroup()->pushMission(MISSION_DISCOVER, -1, 0, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                return true;
            }

            if (bestBuildingValue > bestSpecValue && bestBuildingValue > techValue)
            {
                CvPlot* pBestConstructPlot = ::getCity(bestBuildingCity)->plot();
                if (pUnit->atPlot(pBestConstructPlot))
                {
                    pUnit->getGroup()->pushMission(MISSION_CONSTRUCT, bestBuilding, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                    return true;
                }
                else
                {
                    pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBestConstructPlot->getX(), pBestConstructPlot->getY(), 0, false, false, MISSIONAI_CONSTRUCT, pBestConstructPlot, 0, __FUNCTION__);
                    return true;
                }
            }

            if (bestSpecValue > bestBuildingValue && bestSpecValue > techValue)
            {
                CvPlot* pBestSettlePlot = ::getCity(bestSettleCity)->plot();
                if (pUnit->atPlot(pBestSettlePlot))
                {
                    pUnit->getGroup()->pushMission(MISSION_JOIN, bestSettledSpecialist, -1, 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                    return true;
                }
                else
                {
                    pUnit->getGroup()->pushMission(MISSION_MOVE_TO, pBestSettlePlot->getX(), pBestSettlePlot->getY(), 0, false, false, NO_MISSIONAI, 0, 0, __FUNCTION__);
                    return true;
                }
            }
        }
        return false;
    }
}