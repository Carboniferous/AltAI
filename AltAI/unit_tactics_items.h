#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class CityDefenceUnitTactic : public ICityUnitTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;
    };

    class CityAttackUnitTactic : public ICityUnitTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 1;
    };

    class CollateralUnitTactic : public ICityUnitTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 2;
    };

    class FieldDefenceUnitTactic : public ICityUnitTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 3;
    };

    class FieldAttackUnitTactic : public ICityUnitTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 4;
    };
}