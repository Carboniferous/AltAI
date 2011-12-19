#include "./building_helper.h"
#include "./religion_helper.h"
#include "./bonus_helper.h"
#include "./civ_log.h"

namespace AltAI
{
    BuildingHelper::BuildingHelper(const CvCity* pCity) : pCity_(pCity)
    {
        owner_ = pCity->getOwner();

        const int buildingCount = gGlobals.getNumBuildingInfos();
        buildings_.resize(buildingCount, 0);
        freeBuildings_.resize(buildingCount, 0);
        buildingOriginalOwners_.resize(buildingCount);

        for (int i = 0; i < buildingCount; ++i)
        {
            buildings_[i] = pCity_->getNumRealBuilding((BuildingTypes)i);
            freeBuildings_[i] = pCity_->getNumFreeBuilding((BuildingTypes)i);
            // oddly, ownership is per building type, not per building (not big deal as only matters for culture buildings, and these are mostly destroyed on capture)
            buildingOriginalOwners_[i] = (PlayerTypes)pCity->getBuildingOriginalOwner((BuildingTypes)i);
        }

        for (int i = 0, count = gGlobals.getNumBuildingClassInfos(); i < count; ++i)
        {
            PlotYield plotYield;
            for (int j = 0; j < NUM_YIELD_TYPES; ++j)
            {
                plotYield[j] = pCity_->getBuildingYieldChange((BuildingClassTypes)i, (YieldTypes)j);
            }
            buildingYieldsMap_[(BuildingClassTypes)i] = plotYield;

            Commerce commerce;
            for (int j = 0; j < NUM_COMMERCE_TYPES; ++j)
            {
                commerce[j] = pCity_->getBuildingCommerceChange((BuildingClassTypes)i, (CommerceTypes)j);
            }
            buildingCommerceMap_[(BuildingClassTypes)i] = commerce;
        }
    }

    int BuildingHelper::getNumBuildings(BuildingTypes buildingType) const
    {
        return getNumRealBuildings(buildingType) + getNumFreeBuildings(buildingType);
    }

    int BuildingHelper::getNumFreeBuildings(BuildingTypes buildingType) const
    {
        return freeBuildings_[buildingType];
    }

    int BuildingHelper::getNumRealBuildings(BuildingTypes buildingType) const
    {
        return buildings_[buildingType];
    }

    PlayerTypes BuildingHelper::getBuildingOriginalOwner(BuildingTypes buildingType) const
    {
        return buildingOriginalOwners_[buildingType];
    }

    void BuildingHelper::setBuildingOriginalOwner(BuildingTypes buildingType)
    {
        buildingOriginalOwners_[buildingType] = owner_;
    }

    void BuildingHelper::changeNumRealBuildings(BuildingTypes buildingType)
    {
        ++buildings_[buildingType];
        setBuildingOriginalOwner(buildingType);
    }

    void BuildingHelper::changeNumFreeBuildings(BuildingTypes buildingType)
    {
        ++freeBuildings_[buildingType];
        setBuildingOriginalOwner(buildingType);
    }

    PlotYield BuildingHelper::getBuildingYieldChange(BuildingClassTypes buildingClassType) const
    {
        std::map<BuildingClassTypes, PlotYield>::const_iterator ci(buildingYieldsMap_.find(buildingClassType));
        return ci == buildingYieldsMap_.end() ? PlotYield() : ci->second;
    }

    void BuildingHelper::setBuildingYieldChange(BuildingClassTypes buildingClassType, PlotYield plotYield)
    {
        // ok, so we don't set the yieldrate here, but compare this to CvCity::setBuildingYieldChange() (~40 lines of code)
        // whoever wrote that clearly doesn't understand the -> operator, but wrote conditions to avoid accidental assignment - weird!
        buildingYieldsMap_[buildingClassType] = plotYield;
    }

    void BuildingHelper::changeBuildingYieldChange(BuildingClassTypes buildingClassType, PlotYield plotYield)
    {
        buildingYieldsMap_[buildingClassType] += plotYield;
    }

    Commerce BuildingHelper::getBuildingCommerceChange(BuildingClassTypes buildingClassType) const
    {
        std::map<BuildingClassTypes, Commerce>::const_iterator ci(buildingCommerceMap_.find(buildingClassType));
        return ci == buildingCommerceMap_.end() ? Commerce() : ci->second;
    }

    void BuildingHelper::setBuildingCommerceChange(BuildingClassTypes buildingClassType, Commerce commerce)
    {
        buildingCommerceMap_[buildingClassType] = commerce;
    }

    void BuildingHelper::changeBuildingCommerceChange(BuildingClassTypes buildingClassType, Commerce commerce)
    {
        buildingCommerceMap_[buildingClassType] += commerce;
    }

    int BuildingHelper::getProductionModifier(BuildingTypes buildingType, const boost::shared_ptr<BonusHelper>& bonusHelper, const boost::shared_ptr<ReligionHelper>& religionHelper) const
    {
        const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);
        const CvPlayer& player = CvPlayerAI::getPlayer(owner_);

        int multiplier = player.getProductionModifier(buildingType);  // this is constant afaict

	    for (int bonusType = 0, count = gGlobals.getNumBonusInfos(); bonusType < count; ++bonusType)
	    {
		    if (bonusHelper->getNumBonuses((BonusTypes)bonusType) > 0)
		    {
			    multiplier += buildingInfo.getBonusProductionModifier(bonusType);
		    }
        }

        ReligionTypes stateReligion = religionHelper->getStateReligion();
	    if (stateReligion != NO_RELIGION && religionHelper->isHasReligion(stateReligion))
    	{
			multiplier += player.getStateReligionBuildingProductionModifier(); // todo - use civic helper
		}

	    return std::max<int>(0, multiplier);
    }

    Commerce BuildingHelper::getBuildingCommerce(BuildingTypes buildingType, const boost::shared_ptr<ReligionHelper>& religionHelper) const
    {
        //std::ostream& os = CivLog::getLog(CvPlayerAI::getPlayer(owner_))->getStream();

        Commerce totalCommerce;
        const int buildingCount = getNumBuildings(buildingType);
        const int activeBuildingCount = pCity_->getNumActiveBuilding(buildingType);  // todo
        const int originalBuildTime = pCity_->getBuildingOriginalTime(buildingType);  // todo
        const int gameTurnYear = gGlobals.getGame().getGameTurnYear(); // todo

        ReligionTypes stateReligion = CvPlayerAI::getPlayer(owner_).getStateReligion();

	    if (buildingCount > 0)
	    {
		    const CvBuildingInfo& buildingInfo = gGlobals.getBuildingInfo(buildingType);
            //os << "\nCalculating commerce for building: " << buildingInfo.getType();

            // TODO - check this
            totalCommerce += (Commerce(buildingInfo.getCommerceChangeArray()) + getBuildingCommerceChange((BuildingClassTypes)buildingInfo.getBuildingClassType())) * activeBuildingCount;
            //os << " total = " << totalCommerce;
            
            ReligionTypes buildingReligionType = (ReligionTypes)buildingInfo.getReligionType();

            if (buildingInfo.getGlobalReligionCommerce() != NO_RELIGION)  // shrine
    	    {
                ReligionTypes religionType = (ReligionTypes)buildingInfo.getGlobalReligionCommerce();
			    totalCommerce += Commerce(gGlobals.getReligionInfo(religionType).getGlobalReligionCommerceArray()) * religionHelper->getReligionCount(religionType) * activeBuildingCount;
		    }

            for (int commerceType = 0; commerceType < NUM_COMMERCE_TYPES; ++commerceType)
            {
                int commerce = 0;
		        if (!(buildingInfo.isCommerceChangeOriginalOwner(commerceType)) || getBuildingOriginalOwner(buildingType) == owner_)
		        {
			        commerce += buildingInfo.getObsoleteSafeCommerceChange(commerceType) * buildingCount;

			        if (activeBuildingCount > 0)
			        {
				        if (buildingReligionType != NO_RELIGION && buildingReligionType == stateReligion)  // todo ? make isStateReligionBuilding() fn in ReligionHelper?
					    {
                            commerce += CvPlayerAI::getPlayer(owner_).getStateReligionBuildingCommerce((CommerceTypes)commerceType) * activeBuildingCount;
					    }
				    }

                    // todo corps
				    //if (GC.getBuildingInfo(buildingType).getGlobalCorporationCommerce() != NO_CORPORATION)
				    //{
					//    iCommerce += (GC.getCorporationInfo((CorporationTypes)(GC.getBuildingInfo(buildingType).getGlobalCorporationCommerce())).getHeadquarterCommerce(eIndex) * GC.getGameINLINE().countCorporationLevels((CorporationTypes)(GC.getBuildingInfo(buildingType).getGlobalCorporationCommerce()))) * getNumActivbuildingType(buildingType);
				    //}
			    }

			    if (buildingInfo.getCommerceChangeDoubleTime(commerceType) != 0 && originalBuildTime != MIN_INT &&
				    gameTurnYear - originalBuildTime >= buildingInfo.getCommerceChangeDoubleTime(commerceType))
			    {
				    commerce *= 2;
			    }
                //os << " total = " << commerce;
                totalCommerce[commerceType] += commerce;
		    }
	    }

	    return totalCommerce;
    }
}