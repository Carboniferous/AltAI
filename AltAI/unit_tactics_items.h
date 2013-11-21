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
        BuildImprovementsUnitTactic() : hasConsumedBuilds_(false) {}
        explicit BuildImprovementsUnitTactic(const std::vector<BuildTypes>& buildTypes);
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 6;

    private:
        std::vector<BuildTypes> buildTypes_;
        bool hasConsumedBuilds_;
    };

    class SeaAttackUnitTactic : public ICityUnitTactic
    {
    public:
        SeaAttackUnitTactic() {}
        explicit SeaAttackUnitTactic(const Promotions& promotions);
        virtual void debug(std::ostream& os) const;
        virtual void apply(const ICityUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 7;

    private:
        Promotions promotions_;
    };

    // specialist tactics:

    class DiscoverTechUnitTactic : public IUnitTactic
    {
    public:
        DiscoverTechUnitTactic() {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const IUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;
    };

    class BuildSpecialBuildingUnitTactic : public IUnitTactic
    {
    public:
        BuildSpecialBuildingUnitTactic() {}
        explicit BuildSpecialBuildingUnitTactic(BuildingTypes buildingType);

        virtual void debug(std::ostream& os) const;
        virtual void apply(const IUnitTacticsPtr& pUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 1;

    private:
        BuildingTypes buildingType_;
    };

    class CreateGreatWorkUnitTactic : public IUnitTactic
    {
    public:
        CreateGreatWorkUnitTactic() {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const IUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 2;
    };

    class TradeMissionUnitTactic : public IUnitTactic
    {
    public:
        TradeMissionUnitTactic() {}

        virtual void debug(std::ostream& os) const;
        virtual void apply(const IUnitTacticsPtr& pCityUnitTactics, TacticSelectionData& selectionData);

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 3;
    };
}