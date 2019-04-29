#include "AltAI.h"

#include "./sub_area.h"
#include "./utils.h"
#include "./iters.h"

namespace AltAI
{
    namespace
    {
        template <typename PlotIter>
            void addBorderSubAreas(const CvPlot* pPlot, const SubAreaGraph::NodeSetIter& nodeIter)
        {
            PlotIter iter(pPlot);
            int subAreaID = pPlot->getSubArea();

            while (IterPlot pLoopPlot = iter())
            {
                if (pLoopPlot.valid())
                {
                    int loopPlotSubAreaID = pLoopPlot->getSubArea();
                    if (loopPlotSubAreaID != subAreaID)
                    {
                        nodeIter->borderingSubAreas.insert(loopPlotSubAreaID);
                    }
                }
                else
                {
                    nodeIter->bordersMapEdge = true;
                }
            }
        }
    }

    int SubArea::nextID_(1);

    SubArea::SubArea(bool isWater, bool isImpassable, int areaID) : isWater_(isWater), isImpassable_(isImpassable), ID_(nextID_++), areaID_(areaID), numTiles_(0)
    {
    }

    void SubArea::resetNextID()
    {
        nextID_ = 1;
    }

    void SubAreaGraph::build()
    {
        const CvMap& theMap = gGlobals.getMap();
        const int plotCount = theMap.numPlots();

        for (int plotIndex = 0; plotIndex < plotCount; ++plotIndex)
        {
            CvPlot* pPlot = theMap.plotByIndex(plotIndex);
            int subAreaID = pPlot->getSubArea();
            int areaID = pPlot->getArea();

            NodeSetIter nodeIter = nodes_.insert(SubAreaGraphNode(subAreaID)).first;

            boost::shared_ptr<SubArea> pSubArea = theMap.getSubArea(subAreaID);

            if (!pSubArea)
            {
                continue;
            }
            else if (pSubArea->isImpassable() || pSubArea->isWater())
            {
                addBorderSubAreas<CardinalPlotIter>(pPlot, nodeIter);
            }
            else
            {
                addBorderSubAreas<NeighbourPlotIter>(pPlot, nodeIter);
            }
        }

        while (true)
        {
            bool foundNewEnclosedSubArea = false;
            for (NodeSetIter nodeIter(nodes_.begin()), endIter(nodes_.end()); nodeIter != endIter; ++nodeIter)
            {
                if (nodeIter->borderingSubAreas.size() - nodeIter->enclosedSubAreas.size() == 1)
                {
                    int subAreaID = *(nodeIter->borderingSubAreas.begin());
                    // find sub areas which border this sub area
                    NodeSetIter borderIter(nodes_.find(SubAreaGraphNode(subAreaID)));
                    if (borderIter != nodes_.end())
                    {
                        foundNewEnclosedSubArea = borderIter->enclosedSubAreas.insert(subAreaID).second;
                    }
                }
            }

            if (!foundNewEnclosedSubArea)
            {
                break;
            }
        }
    }

    SubAreaGraphNode SubAreaGraph::getNode(int ID) const
    {
        NodeSetConstIter iter(nodes_.find(SubAreaGraphNode(ID)));

        return iter == nodes_.end() ? SubAreaGraphNode(FFreeList::INVALID_INDEX) : *iter;
    }
}