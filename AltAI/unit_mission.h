#pragma once

#include "./utils.h"

namespace AltAI
{
    class IUnitMission
    {
    public:
        virtual ~IUnitMission() = 0 {}

        virtual void update() = 0;

        virtual const CvUnitAI* getUnit() const = 0;
        virtual CvPlot* getTarget() const = 0;
        virtual bool isComplete() const = 0;
        virtual CvCity* getTargetCity() const = 0;
        virtual UnitAITypes getAIType() const = 0;
    };

    typedef boost::shared_ptr<IUnitMission> UnitMissionPtr;

    struct HasTarget
    {
        explicit HasTarget(const CvPlot* pPlot_) : pPlot(pPlot_)
        {
        }

        bool operator() (const UnitMissionPtr& pMission) const
        {
            return pMission->getTarget() == pPlot;
        }

        const CvPlot* pPlot;
    };

    struct IsCloserThan
    {
        IsCloserThan(const CvPlot* pPlot_, const int minDistance_) : pPlot(pPlot_), minDistance(minDistance_)
        {
        }

        bool operator() (const UnitMissionPtr& pMission) const
        {
            return stepDistance(pMission->getTarget()->getX(), pMission->getTarget()->getY(), pPlot->getX(), pPlot->getY()) < minDistance;
        }

        const CvPlot* pPlot;
        const int minDistance;
    };

    struct LandWorkerPred
    {
        explicit LandWorkerPred(const CvArea* pArea) : areaID(pArea->getID())
        {
        }

        bool operator() (const UnitMissionPtr& pMission) const
        {
            return pMission->getAIType() == UNITAI_WORKER && 
                pMission->getTarget() && 
                pMission->getTarget()->area()->getID() == areaID;
        }

        const int areaID;
    };
}
