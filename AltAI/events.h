#pragma once

#include <istream>

namespace AltAI
{
    // base class for all events
    // these are not game events, but a mechanism
    // to link changes associated with game events like, e.g.
    // the production increase associated with discovering a new tech

    template <class Recv>
        class IEvent
    {
    public:
        virtual ~IEvent() {}
        virtual void handleEvent(Recv& receiver) = 0;
        virtual void stream(std::ostream& os) = 0;
        static void registerEvents();
    };

    struct NullRecv
    {
    };

    class DiscoverTech : public IEvent<NullRecv>
    {
    public:
        explicit DiscoverTech(int index) : data_(index) {}
        virtual void handleEvent(NullRecv&);
        virtual void stream(std::ostream& os)
        {
            os << "DiscoverTech " << data_;
        }

    private:
        int data_;
    };

    class BuildingBuilt : public IEvent<NullRecv>
    {
    public:
        BuildingBuilt(int buildingIndex, int cityIndex) : buildingIndex_(buildingIndex), cityIndex_(cityIndex) {}
        virtual void handleEvent(NullRecv&);
        virtual void stream(std::ostream& os)
        {
            os << "BuildingBuilt " << buildingIndex_;
        }

    private:
        int buildingIndex_, cityIndex_;
    };
}