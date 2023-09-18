// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin grid engine query processing
 */
// ======================================================================

#pragma once

#include "GridInterface.h"
#include "Plugin.h"

namespace SmartMet
{
namespace Engine
{
namespace Grid
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace TimeSeries
{
class GridEngineQuery
{
 public:
  GridEngineQuery(const Plugin& thePlugin);

  bool processGridEngineQuery(const State& state,
                              Query& query,
                              TS::OutputData& outputData,
                              const QueryServer::QueryStreamer_sptr& queryStreamer,
                              const AreaProducers& areaproducers,
                              const ProducerDataPeriod& producerDataPeriod) const;
  bool isGridEngineQuery(const AreaProducers& theProducers, const Query& theQuery) const;

 private:
  void getLocationDefinition(Spine::LocationPtr& loc,
                             std::vector<std::vector<T::Coordinate>>& polygonPath,
                             const Spine::TaggedLocation& tloc,
                             Query& query) const;

  const Plugin& itsPlugin;

  std::unique_ptr<GridInterface> itsGridInterface;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
