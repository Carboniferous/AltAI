#pragma once

#include "./utils.h"

namespace AltAI
{
    // same logic as code in CvCity, but this class is designed for simulating population, modifier, etc.. changing
    class MaintenanceHelper
    {
    public:
        explicit MaintenanceHelper(const CvCity* pCity);
        MaintenanceHelper(const XYCoords coords, PlayerTypes playerType);
        MaintenanceHelper(const MaintenanceHelper& other);

        int getMaintenance() const;
        int getMaintenanceWithCity(const XYCoords coords);

        void changeModifier(int modifier)
        {
            cityModifier_ += modifier;
        }

        void changeDistanceModifier(int change)
        {
            distanceMaintenanceModifier_ += change;
        }

        void changeNumCitiesModifier(int change)
        {
            numCitiesMaintenanceModifier_ += change;
        }

        void setPopulation(int population)
        {
            population_ = population;
        }

    private:
        void init_();
        int calcDistanceMaintenance_() const;
        int calcCityDistanceMaintenance_(const XYCoords otherCityCoords) const;
        int calcNumCitiesMaintenance_() const;
        int calcColonyMaintenance_() const;
        int calcCorpMaintenance_() const;

        XYCoords coords_;
        const CvPlayer& player_;
        int cityModifier_, population_;
        int numCities_;

        int numVassalCitiesModifier_;

        int MAX_DISTANCE_CITY_MAINTENANCE_, distanceMaintenancePercent_, distanceHandicapMaintenancePercent_, distanceMaintenanceModifier_, maxPlotDistance_;
        int numCitiesMaintenancePercent_, numCitiesHandicapMaintenancePercent_, maxNumCitiesMaintenance_, numCitiesMaintenanceModifier_;
    };
}