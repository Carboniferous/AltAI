#pragma once

#include "FFreeListArraybase.h"

namespace AltAI
{
    class IrrigatableArea
    {
    public:
        IrrigatableArea() : isIrrigatable_(false), hasFreshWaterAccess_(false), 
            ID_(FFreeList::INVALID_INDEX), subAreaID_(FFreeList::INVALID_INDEX), numTiles_(0)
        {
        }

        IrrigatableArea(bool isIrrigatable, bool hasFreshWaterAccess, int subAreaID);

        int getID() const
        {
            return ID_;
        }

        int getSubAreaID() const
        {
            return subAreaID_;
        }

        bool isIrrigatable() const
        {
            return isIrrigatable_;
        }

        bool hasFreshWaterAccess() const
        {
            return hasFreshWaterAccess_;
        }

        int getNumTiles() const
        {
            return numTiles_;
        }

        void setHasFreshWaterAccess()
        {
            hasFreshWaterAccess_ = true;
        }

        void setNumTiles(int count)
        {
            numTiles_ = count;
        }

        void write(FDataStreamBase* pStream) const
        {
            pStream->Write(isIrrigatable_);
            pStream->Write(hasFreshWaterAccess_);
            pStream->Write(ID_);
            pStream->Write(subAreaID_);
            pStream->Write(numTiles_);
        }

        static void writeID(FDataStreamBase* pStream)
        {
            pStream->Write(nextID_);
        }

        void read(FDataStreamBase* pStream)
        {
            pStream->Read(&isIrrigatable_);
            pStream->Read(&hasFreshWaterAccess_);
            pStream->Read(&ID_);
            pStream->Read(&subAreaID_);
            pStream->Read(&numTiles_);
        }

        static void readID(FDataStreamBase* pStream)
        {
            pStream->Read(&nextID_);
        }

        static void resetNextID();

    private:
        bool isIrrigatable_, hasFreshWaterAccess_;
        int ID_, subAreaID_;
        int numTiles_;

        static int nextID_;
    };
}
