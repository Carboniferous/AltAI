#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    typedef std::set<PromotionTypes> Promotions;

    class CityDefenceUnitTactic : public ICityUnitTactic
    {
    public:
        CityDefenceUnitTactic() {}
        explicit CityDefenceUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;

    private:
        Promotions promotions_;
    };

    class CityAttackUnitTactic : public ICityUnitTactic
    {
    public:
        CityAttackUnitTactic() {}
        explicit CityAttackUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 1;

    private:
        Promotions promotions_;
    };

    class CollateralUnitTactic : public ICityUnitTactic
    {
    public:
        CollateralUnitTactic() {}
        explicit CollateralUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 2;

    private:
        Promotions promotions_;
    };

    class FieldDefenceUnitTactic : public ICityUnitTactic
    {
    public:
        FieldDefenceUnitTactic() {}
        explicit FieldDefenceUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 3;

    private:
        Promotions promotions_;
    };

    class FieldAttackUnitTactic : public ICityUnitTactic
    {
    public:
        FieldAttackUnitTactic() {}
        explicit FieldAttackUnitTactic(const Promotions& promotions);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 4;

    private:
        Promotions promotions_;
    };

    class BuildCityUnitTactic : public ICityUnitTactic
    {
    public:
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 5;
    };

    class BuildImprovementsUnitTactic : public ICityUnitTactic
    {
    public:
        BuildImprovementsUnitTactic() {}
        explicit BuildImprovementsUnitTactic(const std::vector<BuildTypes>& buildTypes);
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 6;

    private:
        std::vector<BuildTypes> buildTypes_;
    };
}