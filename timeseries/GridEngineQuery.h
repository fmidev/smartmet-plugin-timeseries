// ======================================================================
/*!
 * \brief Smartmet TimeSeries plugin grid engine query processing
 */
// ======================================================================

#pragma once

#include "GridInterface.h"
#include "Producers.h"
#include <grid-content/queryServer/definition/QueryStreamer.h>
#include <spine/Location.h>
#include <timeseries/TimeSeriesInclude.h>
#include <timeseries/TimeSeriesUtility.h>
#include <memory>
#include <vector>

namespace SmartMet::Plugin::TimeSeries
{
class Plugin;
class ProducerDataPeriod;
class State;
struct Query;
}  // namespace SmartMet::Plugin::TimeSeries
namespace SmartMet::T
{
class Coordinate;
}  // namespace SmartMet::T

namespace SmartMet
{

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
                             const Query& query) const;
  void getWktLocationDefinition(Spine::LocationPtr& loc,
                                std::vector<std::vector<T::Coordinate>>& polygonPath,
                                const Spine::TaggedLocation& tloc,
                                const Query& query) const;
  void getBBoxLocationDefinition(Spine::LocationPtr& loc,
                                 std::vector<std::vector<T::Coordinate>>& polygonPath,
                                 const Spine::TaggedLocation& tloc,
                                 const Query& query) const;

  const Plugin& itsPlugin;

  std::unique_ptr<GridInterface> itsGridInterface;
};

}  // namespace TimeSeries
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
