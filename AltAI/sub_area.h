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

        void write(FDataStreamBase* pStream) const
        {
            pStream->Write(isWater_);
            pStream->Write(isImpassable_);
            pStream->Write(ID_);
            pStream->Write(areaID_);
            pStream->Write(numTiles_);
        }

        static void writeID(FDataStreamBase* pStream)
        {
            pStream->Write(nextID_);
        }

        void read(FDataStreamBase* pStream)
        {
            pStream->Read(&isWater_);
            pStream->Read(&isImpassable_);
            pStream->Read(&ID_);
            pStream->Read(&areaID_);
            pStream->Read(&numTiles_);
        }

        static void readID(FDataStreamBase* pStream)
        {
            pStream->Read(&nextID_);
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
        SubAreaGraphNode() : ID(-1), bordersMapEdge(false) {}
        explicit SubAreaGraphNode(int ID_) : ID(ID_), bordersMapEdge(false) {}

        int ID;
        std::set<int> enclosedSubAreas;
        std::set<int> borderingSubAreas;
        bool bordersMapEdge;

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

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

        void write(FDataStreamBase* pStream) const;
        void read(FDataStreamBase* pStream);

    private:
        NodeSet nodes_;
    };
}