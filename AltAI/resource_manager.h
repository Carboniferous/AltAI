#include "./utils.h"
#include "./map_analysis.h"

namespace AltAI
{
    class ResourceManager
    {
    public:
        explicit ResourceManager(const boost::shared_ptr<MapAnalysis>& pMapAnalysis);

    private:
        boost::shared_ptr<MapAnalysis> pMapAnalysis_;
        PlayerTypes playerType_;
        TeamTypes teamType_;
    };
}