#pragma once

#include "./utils.h"
#include "./tactics_interfaces.h"

namespace AltAI
{
    class CityUnitTactics : public boost::enable_shared_from_this<CityUnitTactics>
    {
    public:
        CityUnitTactics() : unitType_(NO_UNIT) {}
        CityUnitTactics(UnitTypes unitType, IDInfo city);

        IDInfo getCity() const;
        void addTactic(const ICityUnitTacticPtr& pBuildingTactic);
        void addDependency(const IDependentTacticPtr& pDependentTactic);
        const std::vector<IDependentTacticPtr>& getDependencies() const;
        void update(Player& player, const CityDataPtr& pCityData);
        void updateDependencies(const Player& player, const CvCity* pCity);
        bool areDependenciesSatisfied(int depTacticFlags) const;
        void apply(TacticSelectionDataMap& tacticSelectionDataMap, int depTacticFlags);
        void apply(TacticSelectionData& selectionData, int depTacticFlags);
        std::list<ICityUnitTacticPtr> getUnitTactics() const;

        UnitTypes getUnitType() const;
        ProjectionLadder getProjection() const;
        CityDataPtr getCityData() const;

        void debug(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        std::vector<DependencyItem> getDepItems(int depTacticFlags) const;

        static CityUnitTacticsPtr factoryRead(FDataStreamBase* pStream);

        static const int CityUnitTacticsID = 0;

    private:
        void apply_(TacticSelectionData& selectionData);

        std::vector<IDependentTacticPtr> dependentTactics_;
        std::list<ICityUnitTacticPtr> unitTactics_;
        ProjectionLadder projection_;
        CityDataPtr pCityData_;
        UnitTypes unitType_;
        IDInfo city_;
    };


    class UnitTactics : public boost::enable_shared_from_this<UnitTactics>
    {
    public:
        UnitTactics() : playerType_(NO_PLAYER), unitType_(NO_UNIT) {}
        UnitTactics(PlayerTypes playerType, UnitTypes unitType);

        void addTactic(const IBuiltUnitTacticPtr& pPlayerTactic);
        void addTactic(const ICityUnitTacticPtr& pBuildingTactic);
        void addDependency(const IDependentTacticPtr& pDependentTactic);
        void addTechDependency(const ResearchTechDependencyPtr& pTechDependency);
        void update(Player& player);
        void updateDependencies(const Player& player);
        void addCityTactic(IDInfo city, const CityUnitTacticsPtr& pCityTactic);
        CityUnitTacticsPtr getCityTactics(IDInfo city) const;
        bool areDependenciesSatisfied(const Player& player, int depTacticFlags) const;
        bool areTechDependenciesSatisfied(const Player& player) const;
        const std::vector<ResearchTechDependencyPtr>& getTechDependencies() const;
        void apply(TacticSelectionDataMap& tacticSelectionDataMap, int depTacticFlags);
        void apply(TacticSelectionData& selectionData);
        void removeCityTactics(IDInfo city);
        bool empty() const;

        UnitTypes getUnitType() const;
        PlayerTypes getPlayerType() const;

        void debug(std::ostream& os) const;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

        static const int UnitTacticsID = 0;
        static UnitTacticsPtr factoryRead(FDataStreamBase* pStream);

    private:
        PlayerTypes playerType_;
        UnitTypes unitType_;
        typedef std::map<IDInfo, CityUnitTacticsPtr> CityTacticsMap;
        typedef std::list<IBuiltUnitTacticPtr> BuiltUnitTacticsList;
        std::vector<ResearchTechDependencyPtr> techDependencies_;
        CityTacticsMap cityTactics_;
        BuiltUnitTacticsList playerUnitTacticsList_;
    };
}