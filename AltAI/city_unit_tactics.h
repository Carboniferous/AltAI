#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class CityUnitTactic : public ICityUnitTactics, public boost::enable_shared_from_this<CityUnitTactic>
    {
    public:
        CityUnitTactic() : unitType_(NO_UNIT) {}
        CityUnitTactic(UnitTypes unitType, IDInfo city);

        virtual IDInfo getCity() const;
        virtual void addTactic(const ICityUnitTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual const std::vector<IDependentTacticPtr>& getDependencies() const;
        virtual void update(const Player& player, const CityDataPtr& pCityData);
        virtual void updateDependencies(const Player& player, const CvCity* pCity);
        virtual bool areDependenciesSatisfied(int ignoreFlags) const;
        virtual void apply(TacticSelectionDataMap& tacticSelectionDataMap, int ignoreFlags);
        virtual void apply(TacticSelectionData& selectionData);

        virtual UnitTypes getUnitType() const;
        virtual ProjectionLadder getProjection() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;

    private:
        void apply_(TacticSelectionData& selectionData);

        std::vector<IDependentTacticPtr> dependentTactics_;
        std::list<ICityUnitTacticPtr> unitTactics_;
        ProjectionLadder projection_;
        UnitTypes unitType_;
        IDInfo city_;
    };


    class UnitTactic : public IUnitTactics, public boost::enable_shared_from_this<UnitTactic>
    {
    public:
        UnitTactic() : unitType_(NO_UNIT) {}
        explicit UnitTactic(UnitTypes unitType);

        virtual void addTactic(const IUnitTacticPtr& pPlayerTactic);
        virtual void addTactic(const ICityUnitTacticPtr& pBuildingTactic);
        virtual void addDependency(const IDependentTacticPtr& pDependentTactic);
        virtual void addTechDependency(const ResearchTechDependencyPtr& pTechDependency);
        virtual void update(const Player& player);
        virtual void updateDependencies(const Player& player);
        virtual void addCityTactic(IDInfo city, const ICityUnitTacticsPtr& pCityTactic);
        virtual ICityUnitTacticsPtr getCityTactics(IDInfo city) const;
        virtual bool areDependenciesSatisfied(const Player& player, int ignoreFlags) const;
        virtual const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const;
        virtual void apply(TacticSelectionDataMap& tacticSelectionDataMap, int ignoreFlags);
        virtual void apply(TacticSelectionData& selectionData);
        virtual void removeCityTactics(IDInfo city);
        virtual bool empty() const;

        virtual UnitTypes getUnitType() const;

        virtual void debug(std::ostream& os) const;

        virtual void write(FDataStreamBase* pStream) const;
        virtual void read(FDataStreamBase* pStream);

        static const int ID = 0;

    private:
        UnitTypes unitType_;
        typedef std::map<IDInfo, ICityUnitTacticsPtr> CityTacticsMap;
        typedef std::list<IUnitTacticPtr> PlayerUnitTacticsList;
        std::vector<ResearchTechDependencyPtr> techDependencies_;
        CityTacticsMap cityTactics_;
        PlayerUnitTacticsList playerUnitTacticsList_;
    };
}