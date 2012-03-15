#pragma once

#include "./utils.h"
#include "./city_projections.h"

namespace AltAI
{
    class Player;
    class City;
    class CityData;
    typedef boost::shared_ptr<CityData> CityDataPtr;

    class ICityBuildingTactics
    {
    public:
        virtual ~ICityBuildingTactics() = 0 {}
        virtual void update(const Player& player, const CityDataPtr& pCityData) = 0;
        virtual TotalOutput getProjection() const = 0;
        virtual void debug(std::ostream& os) const = 0;
    };

    class CityScienceBuilding : public ICityBuildingTactics
    {
    public:
        explicit CityScienceBuilding(BuildingTypes buildingType);
        virtual void update(const Player& player, const CityDataPtr& pCityData);
        virtual TotalOutput getProjection() const;
        virtual void debug(std::ostream& os) const;

    private:
        ProjectionLadder projection_;
        BuildingTypes buildingType_;
    };

    //class CityCultureBuilding : public ICityBuildingTactics
    //{
    //public:
    //    //virtual TotalOutput getOutputDelta();
    //    //virtual bool select(const City& city);

    //private:
    //    Commerce commerce_;
    //    CommerceModifier modifier_;
    //};
}