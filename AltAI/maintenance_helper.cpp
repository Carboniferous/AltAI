#include "./maintenance_helper.h"
#include "./iters.h"
#include "./helper_fns.h"
#include "./city_data.h"

namespace AltAI
{
    MaintenanceHelper::MaintenanceHelper(const CvCity* pCity) : coords_(pCity->getX(), pCity->getY()), player_(CvPlayerAI::getPlayer(pCity->getOwner()))
    {
        population_ = pCity->getPopulation();
        cityModifier_ = pCity->getMaintenanceModifier();
        numCities_ = player_.getNumCities();

        init_();
    }

    MaintenanceHelper::MaintenanceHelper(const XYCoords coords, PlayerTypes playerType) : coords_(coords), player_(CvPlayerAI::getPlayer(playerType))
    {
        population_ = 1;
        cityModifier_ = 0;
        numCities_ = player_.getNumCities() + 1;

        init_();
    }

   /* MaintenanceHelper::MaintenanceHelper(const MaintenanceHelper& other)
        : coords_(other.coords_), player_(other.player_), cityModifier_(other.cityModifier_), population_(other.population_),
          numCities_(other.numCities_), numVassalCitiesModifier_(other.numVassalCitiesModifier_),
          MAX_DISTANCE_CITY_MAINTENANCE_(other.MAX_DISTANCE_CITY_MAINTENANCE_), distanceMaintenancePercent_(other.distanceMaintenancePercent_),
          distanceHandicapMaintenancePercent_(other.distanceHandicapMaintenancePercent_), distanceMaintenanceModifier_(other.distanceMaintenanceModifier_),
          maxPlotDistance_(other.maxPlotDistance_), numCitiesMaintenancePercent_(other.numCitiesMaintenancePercent_),
          numCitiesHandicapMaintenancePercent_(other.numCitiesHandicapMaintenancePercent_), maxNumCitiesMaintenance_(other.maxNumCitiesMaintenance_),
          numCitiesMaintenanceModifier_(other.numCitiesMaintenanceModifier_)
    {
    }*/

    MaintenanceHelperPtr MaintenanceHelper::clone() const
    {
        MaintenanceHelperPtr copy = MaintenanceHelperPtr(new MaintenanceHelper(*this));
        return copy;
    }

    void MaintenanceHelper::init_()
    {
        numVassalCitiesModifier_ = getNumVassalCities(player_);
        if (numVassalCitiesModifier_ > 0)
        {
            numVassalCitiesModifier_ /= std::max<int>(1, CvTeamAI::getTeam(player_.getTeam()).getNumMembers());
        }

        MAX_DISTANCE_CITY_MAINTENANCE_ = gGlobals.getDefineINT("MAX_DISTANCE_CITY_MAINTENANCE");  // 25 in standard game
        distanceMaintenancePercent_ = gGlobals.getWorldInfo(gGlobals.getMap().getWorldSize()).getDistanceMaintenancePercent();
        distanceHandicapMaintenancePercent_ = gGlobals.getHandicapInfo(player_.getHandicapType()).getDistanceMaintenancePercent();
        distanceMaintenanceModifier_ = std::max<int>(0, 100 + player_.getDistanceMaintenanceModifier());  // potentially affected by civic changes
        maxPlotDistance_ = gGlobals.getMap().maxPlotDistance();

        numCitiesMaintenancePercent_ = gGlobals.getWorldInfo(gGlobals.getMap().getWorldSize()).getNumCitiesMaintenancePercent();
        numCitiesHandicapMaintenancePercent_ = gGlobals.getHandicapInfo(player_.getHandicapType()).getNumCitiesMaintenancePercent();
        maxNumCitiesMaintenance_ = 100 * gGlobals.getHandicapInfo(player_.getHandicapType()).getMaxNumCitiesMaintenance();
        numCitiesMaintenanceModifier_ = std::max<int>(0, 100 + player_.getNumCitiesMaintenanceModifier());  // potentially affected by civic changes

        CityIter iter(player_);
        while (CvCity* pCity = iter())
        {
            if (pCity->isGovernmentCenter())
            {
                governmentCentres_.insert(pCity->getIDInfo());
            }
        }
    }

    int MaintenanceHelper::getMaintenance() const
    {
        // todo - cache more of calculation
        return (std::max<int>(100 + cityModifier_, 0) * (calcDistanceMaintenance_() + calcNumCitiesMaintenance_() + calcColonyMaintenance_() + calcCorpMaintenance_())) / 100;
    }

    int MaintenanceHelper::getMaintenanceWithCity(const XYCoords coords)
    {
        ++numCities_;
        // don't bother redoing calculation of distance maintenance with extra city as the answer is only affected if we have no capital
        // (then iWorstCityMaintenance may change - even then it is unlikely a new city changes the answer)
        int newMaintenance = getMaintenance();
        --numCities_;

        return newMaintenance;
    }

    void MaintenanceHelper::addGovernmentCentre(IDInfo city)
    {
        governmentCentres_.insert(city);
    }

    void MaintenanceHelper::removeGovernmentCentre(IDInfo city)
    {
        governmentCentres_.erase(city);
    }

    int MaintenanceHelper::calcDistanceMaintenance_() const
    {
        int iWorstCityMaintenance = 0, iBestCapitalMaintenance = std::numeric_limits<int>::max();

        CityIter iter(player_);
        CvCity* pLoopCity;
        while (pLoopCity = iter())
        {
            int iTempMaintenance = calcCityDistanceMaintenance_(XYCoords(pLoopCity->getX(), pLoopCity->getY()));

		    iWorstCityMaintenance = std::max<int>(iWorstCityMaintenance, iTempMaintenance);

            if (governmentCentres_.find(pLoopCity->getIDInfo()) != governmentCentres_.end())
            {
            	iBestCapitalMaintenance = std::min<int>(iBestCapitalMaintenance, iTempMaintenance);
		    }
        }

        return std::min<int>(iWorstCityMaintenance, iBestCapitalMaintenance);
    }

    int MaintenanceHelper::calcCityDistanceMaintenance_(const XYCoords otherCityCoords) const
    {
        // this is equivalent to the code in CvCity, but I wonder if they meant to write:
        // int iTempMaintenance = 100 * std::min<int>(MAX_DISTANCE_CITY_MAINTENANCE_, plotDistance(xCoord, yCoord, pLoopCity->getX(), pLoopCity->getY()));
        // or more likely still (but indicating two bugs, not one):
        // int iTempMaintenance = 100 * std::min<int>(maxPlotDistance_, plotDistance(xCoord, yCoord, pLoopCity->getX(), pLoopCity->getY()));
        // and later divide by MAX_DISTANCE_CITY_MAINTENANCE_, not maxPlotDistance_
        int iMaintenance = 100 * MAX_DISTANCE_CITY_MAINTENANCE_ * plotDistance(coords_.iX, coords_.iY, otherCityCoords.iX, otherCityCoords.iY);
		iMaintenance *= (population_ + 7);
		iMaintenance /= 10;

        applyPercentModifier(iMaintenance, distanceMaintenanceModifier_);
        applyPercentModifier(iMaintenance, distanceMaintenancePercent_);
        applyPercentModifier(iMaintenance, distanceHandicapMaintenancePercent_);

		iMaintenance /= maxPlotDistance_;

        return iMaintenance;
    }

    int MaintenanceHelper::calcNumCitiesMaintenance_() const
    {
        int iNumCitiesPercent = (100 * (population_ + 17)) / 18;

        applyPercentModifier(iNumCitiesPercent, numCitiesMaintenancePercent_);
        applyPercentModifier(iNumCitiesPercent, numCitiesHandicapMaintenancePercent_);

        int iNumCitiesMaintenance = std::min<int>((numCities_ + numVassalCitiesModifier_) * iNumCitiesPercent, maxNumCitiesMaintenance_);
	    applyPercentModifier(iNumCitiesMaintenance, numCitiesMaintenanceModifier_);

	    return iNumCitiesMaintenance;
    }

    int MaintenanceHelper::calcColonyMaintenance_() const
    {
        // TODO
        return 0;
    }

    int MaintenanceHelper::calcCorpMaintenance_() const
    {
        // TODO
        return 0;
    }
}