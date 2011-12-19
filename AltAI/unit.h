#pragma once

class CvUnit;

namespace AltAI
{
    class Unit
    {
    public:
        explicit Unit(CvUnitAI* pUnit);

        bool operator < (const Unit& other) const;

        UnitTypes getUnitType() const
        {
            return pUnit_->getUnitType();
        }

        const CvUnitAI* getUnit() const
        {
            return pUnit_;
        }

    private:
        CvUnitAI* pUnit_;
    };
}

