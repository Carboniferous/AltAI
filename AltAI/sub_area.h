#pragma once

#include "FFreeListArraybase.h"
#include <set>

namespace AltAI
{
    class SubArea
    {
    public:
        SubArea() : isWater_(false), isImpassable_(false), ID_(FFreeList::INVALID_INDEX), areaID_(FFreeList::INVALID_INDEX), numTiles_(0)
        {
        }

        SubArea(bool isWater, bool isImpassable, int areaID);

        int getID() const
        {
            return ID_;
        }

        int getAreaID() const
        {
            return areaID_;
        }

        bool isWater() const
        {
            return isWater_;
        }

        bool isImpassable() const
        {
            return isImpassable_;
        }

        int getNumTiles() const
        {
            return numTiles_;
        }

        void setNumTiles(int count)
        {
            numTiles_ = count;
        }

        static void resetNextID();

    private:
        bool isWater_, isImpassable_;
        int ID_, areaID_;
        int numTiles_;

        static int nextID_;
    };

    struct SubAreaGraphNode
    {
        explicit SubAreaGraphNode(int ID_) : ID(ID_), bordersMapEdge(false) {}
        int ID;
        std::set<int> enclosedSubAreas;
        std::set<int> borderingSubAreas;
        bool bordersMapEdge;

        bool operator < (const SubAreaGraphNode& other) const
        {
            return ID < other.ID;
        }
    };

    class SubAreaGraph
    {
    public:
        typedef std::set<SubAreaGraphNode> NodeSet;
        typedef NodeSet::iterator NodeSetIter;
        typedef NodeSet::const_iterator NodeSetConstIter;

        void build();
        SubAreaGraphNode getNode(int ID) const;

    private:
        NodeSet nodes_;
    };
}