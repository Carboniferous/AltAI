#include "./utils.h"
#include "./map_analysis.h"

namespace AltAI
{
    class TechManager
    {
    public:
        explicit TechManager(const boost::shared_ptr<MapAnalysis>& pMapAnalysis);

        TechTypes getBestWorkerTech() const;
    private:
        boost::shared_ptr<MapAnalysis> pMapAnalysis_;
        PlayerTypes playerType_;
        TeamTypes teamType_;
    };
}