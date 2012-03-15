#include "./building_tactics_items.h"
#include "./city_data.h"
#include "./player.h"
#include "./city.h"
#include "./player_analysis.h"

namespace AltAI
{
    CityScienceBuilding::CityScienceBuilding(BuildingTypes buildingType) : buildingType_(buildingType)
    {
    }

    void CityScienceBuilding::update(const Player& player, const CityDataPtr& pCityData)
    {
        projection_ = getProjectedOutput(player, pCityData->clone(), player.getAnalysis()->getBuildingInfo(buildingType_), 50);
    }

    TotalOutput CityScienceBuilding::getProjection() const
    {
        return projection_.getOutput();
    }

    void CityScienceBuilding::debug(std::ostream& os) const
    {
#ifdef ALTAI_DEBUG
        os << "\nCity science building: " << gGlobals.getBuildingInfo(buildingType_).getType() << " projection: ";
        projection_.debug(os);
#endif
    }
}